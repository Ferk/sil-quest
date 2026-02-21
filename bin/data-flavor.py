#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the flavor.txt data file.

1. Validates the format and integrity of lib/edit/flavor.txt.
2. Exports the flavor records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: index : tval : sval
- G: char : attr
- D: text

Exit codes:
  0 - All validations passed
  1 - One or more validation errors found
"""

import argparse
import json
import signal
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import cast

# Handle broken pipe (e.g., when piping to head) - Unix only
if hasattr(signal, "SIGPIPE"):
    _ = signal.signal(signal.SIGPIPE, signal.SIG_DFL)

###
### Valid colors
###
# Valid base colors (16 colors)
# D - Dark Gray    w - White          s - Gray          o - Orange
# r - Red          g - Green          b - Blue          u - Brown
# d - Black        W - Light Gray     v - Violet        y - Yellow
# R - Light Red    G - Light Green    B - Light Blue    U - Light Brown
VALID_BASE_COLORS = set("DwsorgbudWvyRGBU")
# Extended colors observed in actual data (base color + numeric suffix)
VALID_EXTENDED_COLORS = {"b1", "g1", "v1", "r1", "G1", "U1", "D1", "W1", "u1", "B1", "y1"}
VALID_COLORS = VALID_BASE_COLORS | VALID_EXTENDED_COLORS

# JSON-compatible types
type JsonValue = str | int | float | bool | None | list[JsonValue] | dict[str, JsonValue]
type JsonDict = dict[str, JsonValue]


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
class Limits:
    """Limits parsed from lib/edit/limits.txt."""

    max_flavors: int  # M:L value

    @property
    def max_flavor_id(self) -> int:
        """Maximum valid flavor ID (0-indexed, so max_flavors - 1)."""
        return self.max_flavors - 1


@dataclass
class Flavor:
    """A flavor record parsed from flavor.txt."""

    id: int
    tval: int
    sval: int | None = None
    symbol: str | None = None
    color: str | None = None
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_flavors = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:L:"):
            # M:L:310 - Maximum number of flavors
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_flavors = int(parts[2])

    if max_flavors is None:
        return None

    return Limits(max_flavors=max_flavors)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> tuple[int, int] | None:
    """Validate N: line format and return the (ID, tval) tuple."""
    parts = line.split(":")
    # N:index:tval or N:index:tval:sval
    if len(parts) < 3:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected at least 3: {line}")
        return None

    id_str = parts[1]
    if not id_str.isdigit():
        result.error(f"Line {lineno}: N: index is not numeric: {id_str}")
        return None

    tval_str = parts[2]
    if not tval_str.isdigit():
        result.error(f"Line {lineno}: N: tval is not numeric: {tval_str}")
        return None

    # sval is optional (field 4)
    if len(parts) >= 4:
        sval_str = parts[3]
        if not sval_str.isdigit():
            result.error(f"Line {lineno}: N: sval is not numeric: {sval_str}")
            return None

    return (int(id_str), int(tval_str))


def validate_g_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate G: line format (char : attr)."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: G: line has {len(parts)} fields, expected 3: {line}")
        return

    symbol = parts[1]
    if len(symbol) != 1:
        result.error(f"Line {lineno}: G: symbol must be single character, got '{symbol}'")

    color = parts[2]
    if color not in VALID_COLORS:
        result.error(f"Line {lineno}: G: unrecognized color '{color}'")


def parse_flavors(filepath: Path) -> list[Flavor]:
    """Parse flavor.txt and return a list of Flavor objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    flavors: list[Flavor] = []
    current_flavor: Flavor | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new flavor
        if line.startswith("N:"):
            # Save previous flavor
            if current_flavor is not None:
                flavors.append(current_flavor)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit() and parts[2].isdigit():
                flavor_id = int(parts[1])
                tval = int(parts[2])
                sval = int(parts[3]) if len(parts) >= 4 and parts[3].isdigit() else None
                current_flavor = Flavor(id=flavor_id, tval=tval, sval=sval)
            continue

        if current_flavor is None:
            continue

        # G: char : attr
        if line.startswith("G:"):
            parts = line.split(":")
            if len(parts) >= 3:
                current_flavor.symbol = parts[1]
                current_flavor.color = parts[2]

        # D: description
        elif line.startswith("D:"):
            content = line[2:].strip()
            if current_flavor.description is None:
                current_flavor.description = content
            else:
                current_flavor.description += " " + content

    if current_flavor is not None:
        flavors.append(current_flavor)

    return flavors


def export_flavors_to_json(flavors: list[Flavor]) -> str:
    """Export flavors to a JSON string."""

    def clean_dict(d: JsonDict) -> JsonDict:
        """Recursively remove None values and empty collections from a dict."""
        result: JsonDict = {}
        for k, v in d.items():
            if v is None or v == [] or v == "":
                continue
            if isinstance(v, dict):
                cleaned = clean_dict(v)
                if cleaned:  # Only include non-empty dicts
                    result[k] = cleaned
            elif isinstance(v, list):
                # Clean each item if it's a dict
                cleaned_list: list[JsonValue] = []
                for item in v:
                    if isinstance(item, dict):
                        cleaned_item = clean_dict(item)
                        if cleaned_item:
                            cleaned_list.append(cleaned_item)
                    elif item is not None:
                        cleaned_list.append(item)
                if cleaned_list:
                    result[k] = cleaned_list
            else:
                result[k] = v
        return result

    def flavor_to_dict(flavor: Flavor) -> JsonDict:
        """Convert a Flavor to a dict, removing None values."""
        return clean_dict(asdict(flavor))

    data = {"flavors": [flavor_to_dict(f) for f in flavors]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_flavor_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire flavor.txt file.

    Args:
        filepath: Path to flavor.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                flavor count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Flavor file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state. Maps id -> line number.
    ids_seen: dict[int, int] = {}

    prev_id = -1
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

        # N: line - start of flavor entry
        if line.startswith("N:"):
            parsed = validate_n_line(line, lineno, result)
            if parsed is not None:
                flavor_id, _ = parsed
                # Check for duplicates
                if flavor_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {flavor_id} "
                        + f"(first seen at line {ids_seen[flavor_id]})"
                    )
                else:
                    ids_seen[flavor_id] = lineno

                # Note: Flavor IDs can have gaps (not strictly increasing by 1)
                # but should generally increase
                if flavor_id < prev_id:
                    result.warning(
                        f"Line {lineno}: ID {flavor_id} is less than previous ID {prev_id} "
                        + "(IDs should generally increase)"
                    )
                prev_id = flavor_id

                # Check ID against limit
                if limits and flavor_id > limits.max_flavor_id:
                    result.error(
                        f"Line {lineno}: Flavor ID {flavor_id} exceeds maximum allowed ID "
                        + f"{limits.max_flavor_id} (from limits.txt M:L:{limits.max_flavors})"
                    )
            continue

        # Other line types
        if line.startswith("G:"):
            validate_g_line(line, lineno, result)
        elif line.startswith("D:"):
            pass  # D: lines are descriptions, no strict validation needed
        else:
            # Check for unknown line types (letter followed by colon)
            if len(line) >= 2 and line[1] == ":":
                result.error(f"Line {lineno}: Unknown line type '{line[0]}:' in line '{line}'")
            else:
                result.error(
                    f"Line {lineno}: Unrecognized line (missing '#' comment marker?): '{line}'"
                )

    # Check for required version stamp
    if not has_version:
        result.error("Missing required version stamp (V: line)")

    # Check total flavor count against limit
    if limits:
        flavor_count = len(ids_seen)
        if flavor_count > limits.max_flavors:
            result.error(
                f"Total flavor count ({flavor_count}) exceeds maximum allowed "
                + f"({limits.max_flavors}) from limits.txt M:L"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/flavor.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                      # Validate the file
  %(prog)s --validate lib/edit/flavor.txt  # Validate a specific file
  %(prog)s --export-json                   # Export to JSON (stdout)
  %(prog)s --export-json > flavors.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to flavor.txt file (default: lib/edit/flavor.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the flavor.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all flavor records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(flavor_file: Path, limits_file: Path) -> int:
    """Run validation on the flavor file."""
    print("=" * 60)
    print(f"Validating: {flavor_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max flavors = {limits.max_flavors} (max ID = {limits.max_flavor_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_flavor_file(flavor_file, limits)

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


def run_export_json(flavor_file: Path) -> int:
    """Export flavors to JSON on stdout."""
    flavors = parse_flavors(flavor_file)

    if not flavors:
        print(f"ERROR: No flavors found in {flavor_file}", file=sys.stderr)
        return 1

    print(export_flavors_to_json(flavors))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        flavor_file = file_arg
    else:
        flavor_file = project_dir / "lib" / "edit" / "flavor.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(flavor_file, limits_file)

    if export_json:
        return run_export_json(flavor_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
