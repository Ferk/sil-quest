#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the limits.txt data file.

1. Validates the format and integrity of lib/edit/limits.txt.
2. Exports the limit records to JSON format (stdout).

Data Validation
===============

Based on the inline comments in limits.txt:
- M:F:value - Maximum number of feature types
- M:K:value - Maximum number of object kinds
- M:B:value - Maximum number of abilities
- M:A:special:normal:random:self-made - Artifact limits (4 values)
- M:E:value - Maximum number of special item types
- M:R:value - Maximum number of monster races
- M:G:value - Maximum number of ghost templates
- M:V:value - Maximum number of vaults
- M:P:value - Maximum number of player races
- M:C:value - Maximum number of player houses
- M:H:value - Maximum number of player history lines
- M:Q:value - Maximum number of quests
- M:L:value - Maximum number of flavors
- M:O:value - Maximum number of objects on level
- M:N:value - Size of names array (bytes)
- M:T:value - Size of descriptions array (bytes)

Exit codes:
  0 - All validations passed
  1 - One or more validation errors found
"""

import argparse
import json
import signal
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast

# Handle broken pipe (e.g., when piping to head) - Unix only
if hasattr(signal, "SIGPIPE"):
    _ = signal.signal(signal.SIGPIPE, signal.SIG_DFL)

# JSON-compatible types
type JsonValue = str | int | float | bool | None | list[JsonValue] | dict[str, JsonValue]
type JsonDict = dict[str, JsonValue]

# Known limit codes and their expected field counts (after M:code:)
# Most have 1 value, M:A has 4 values
LIMIT_CODES: dict[str, tuple[str, int]] = {
    "F": ("feature_types", 1),
    "K": ("object_kinds", 1),
    "B": ("abilities", 1),
    "A": ("artefacts", 4),  # special:normal:random:self-made
    "E": ("special_items", 1),
    "R": ("monster_races", 1),
    "G": ("ghost_templates", 1),
    "V": ("vaults", 1),
    "P": ("player_races", 1),
    "C": ("player_houses", 1),
    "H": ("history_lines", 1),
    "Q": ("quests", 1),
    "L": ("flavors", 1),
    "O": ("objects_on_level", 1),
    "N": ("names_array_size", 1),
    "T": ("descriptions_array_size", 1),
}

# Artefact sub-field names for M:A line (special:normal:random:self-made)
ARTEFACT_FIELDS = ["special", "normal", "random", "selfmade"]


@dataclass
class ValidationResult:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    info: list[str] = field(default_factory=list)

    def error(self, msg: str) -> None:
        self.errors.append(msg)

    def warning(self, msg: str) -> None:
        self.warnings.append(msg)

    def log_info(self, msg: str) -> None:
        self.info.append(msg)

    @property
    def is_valid(self) -> bool:
        return len(self.errors) == 0 and len(self.warnings) == 0


@dataclass
class Limit:
    """A limit record parsed from limits.txt."""

    code: str
    name: str
    values: list[int]


def validate_m_line(line: str, lineno: int, result: ValidationResult) -> Limit | None:
    """Validate M: line format and return a Limit object."""
    parts = line.split(":")
    if len(parts) < 3:
        result.error(f"Line {lineno}: M: line has {len(parts)} fields, expected at least 3: {line}")
        return None

    code = parts[1]
    if code not in LIMIT_CODES:
        result.warning(f"Line {lineno}: M: unknown limit code '{code}': {line}")
        # Still try to parse it
        name = f"unknown_{code}"
        expected_count = len(parts) - 2
    else:
        name, expected_count = LIMIT_CODES[code]

    # Validate value count
    actual_count = len(parts) - 2
    if actual_count != expected_count:
        result.error(
            f"Line {lineno}: M:{code}: has {actual_count} values, expected {expected_count}: {line}"
        )
        return None

    # Validate all values are numeric
    values: list[int] = []
    for i, val in enumerate(parts[2:], start=1):
        if not val.isdigit():
            result.error(f"Line {lineno}: M:{code}: value {i} is not numeric: {val}")
            return None
        values.append(int(val))

    return Limit(code=code, name=name, values=values)


def parse_limits(filepath: Path) -> list[Limit]:
    """Parse limits.txt and return a list of Limit objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    limits: list[Limit] = []

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # M: line
        if line.startswith("M:"):
            parts = line.split(":")
            if len(parts) >= 3:
                code = parts[1]
                if code in LIMIT_CODES:
                    name, _ = LIMIT_CODES[code]
                else:
                    name = f"unknown_{code}"
                try:
                    values = [int(v) for v in parts[2:]]
                    limits.append(Limit(code=code, name=name, values=values))
                except ValueError:
                    pass

    return limits


def export_limits_to_json(limits: list[Limit]) -> str:
    """Export limits to a JSON string."""
    data: JsonDict = {"limits": {}}
    limits_dict = cast(JsonDict, data["limits"])

    for limit in limits:
        if len(limit.values) == 1:
            limits_dict[limit.name] = limit.values[0]
        elif limit.name == "artefacts" and len(limit.values) == len(ARTEFACT_FIELDS):
            # Create nested artefacts map
            artefacts: JsonDict = {}
            for field_name, value in zip(ARTEFACT_FIELDS, limit.values, strict=True):
                artefacts[field_name] = value
            artefacts["total"] = sum(limit.values)
            limits_dict["artefacts"] = artefacts
        else:
            limits_dict[limit.name] = limit.values

    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_limits_file(filepath: Path) -> ValidationResult:
    """Validate the entire limits.txt file."""
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Limits file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state
    codes_seen: dict[str, int] = {}  # Maps code -> line number
    has_version = False

    for lineno, line in enumerate(lines, start=1):
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#"):
            continue

        # Version stamp
        if line.startswith("V:"):
            has_version = True
            continue

        # M: line
        if line.startswith("M:"):
            limit = validate_m_line(line, lineno, result)
            if limit is not None:
                # Check for duplicates
                if limit.code in codes_seen:
                    result.error(
                        f"Line {lineno}: Duplicate limit code '{limit.code}' "
                        + f"(first seen at line {codes_seen[limit.code]})"
                    )
                else:
                    codes_seen[limit.code] = lineno
            continue

        # Unknown line types
        if len(line) >= 2 and line[1] == ":":
            result.error(f"Line {lineno}: Unknown line type '{line[0]}:' in line '{line}'")
        else:
            result.error(
                f"Line {lineno}: Unrecognized line (missing '#' comment marker?): '{line}'"
            )

    # Check for required version stamp
    if not has_version:
        result.error("Missing required version stamp (V: line)")

    # Check that all expected limit codes are present
    for code in LIMIT_CODES:
        if code not in codes_seen:
            result.warning(f"Missing expected limit code 'M:{code}'")

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/limits.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                      # Validate the file
  %(prog)s --validate lib/edit/limits.txt  # Validate a specific file
  %(prog)s --export-json                   # Export to JSON (stdout)
  %(prog)s --export-json > limits.json     # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to limits.txt file (default: lib/edit/limits.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the limits.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all limit records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(limits_file: Path) -> int:
    """Run validation on the limits file."""
    print("=" * 60)
    print(f"Validating: {limits_file}")

    # Validate the file
    result = validate_limits_file(limits_file)

    # Print all messages of the validation result
    for msg in result.info:
        print(f"INFO: {msg}")

    for msg in result.warnings:
        print(f"WARNING: {msg}", file=sys.stderr)

    for msg in result.errors:
        print(f"ERROR: {msg}", file=sys.stderr)

    # Summary
    print("=" * 60)
    print(f"  Errors:   {len(result.errors)}")
    print(f"  Warnings: {len(result.warnings)}")
    print("=" * 60)

    if result.is_valid:
        print("OK")
    else:
        print("FAILED")

    return 0 if result.is_valid else 1


def run_export_json(limits_file: Path) -> int:
    """Export limits to JSON on stdout."""
    limits = parse_limits(limits_file)

    if not limits:
        print(f"ERROR: No limits found in {limits_file}", file=sys.stderr)
        return 1

    print(export_limits_to_json(limits))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        limits_file = file_arg
    else:
        limits_file = project_dir / "lib" / "edit" / "limits.txt"

    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(limits_file)

    if export_json:
        return run_export_json(limits_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
