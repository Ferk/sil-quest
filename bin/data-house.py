#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the house.txt data file.

1. Validates the format and integrity of lib/edit/house.txt.
2. Exports the house records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: house number : house name
- A: alternate house name
- B: short house name
- F: house flag
- S: str : dex : con : gra
- D: description

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

# JSON-compatible types
type JsonValue = str | int | float | bool | None | list[JsonValue] | dict[str, JsonValue]
type JsonDict = dict[str, JsonValue]

# Valid house flags
VALID_FLAGS = {
    "SMT_AFFINITY",
    "WIL_AFFINITY",
    "PER_AFFINITY",
    "SNG_AFFINITY",
    "EVN_AFFINITY",
    "STL_AFFINITY",
    "MEL_AFFINITY",
    "ARC_AFFINITY",
}


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

    max_houses: int  # M:C value

    @property
    def max_house_id(self) -> int:
        """Maximum valid house ID (0-indexed, so max_houses - 1)."""
        return self.max_houses - 1


@dataclass
class House:
    """A house record parsed from house.txt."""

    id: int
    name: str
    alternate_name: str | None = None
    short_name: str | None = None
    flag: str | None = None
    stats: list[int] = field(default_factory=list)
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_houses = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:C:"):
            # M:C:11 - Maximum number of player houses
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_houses = int(parts[2])

    if max_houses is None:
        return None

    return Limits(max_houses=max_houses)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> int | None:
    """Validate N: line format and return the ID."""
    parts = line.split(":")
    if len(parts) < 3:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected at least 3: {line}")
        return None

    id_str = parts[1]
    if not id_str.isdigit():
        result.error(f"Line {lineno}: N: house number is not numeric: {id_str}")
        return None

    return int(id_str)


def validate_s_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate S: line format (str : dex : con : gra)."""
    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: S: line has {len(parts)} fields, expected 5: {line}")
        return

    for i, name in enumerate(["str", "dex", "con", "gra"], start=1):
        val = parts[i]
        # Stats can be negative, so check for optional minus sign
        if not (val.isdigit() or (val.startswith("-") and val[1:].isdigit())):
            result.error(f"Line {lineno}: S: {name} is not numeric: {val}")


def validate_f_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate F: line format (house flag)."""
    parts = line.split(":")
    if len(parts) < 2:
        result.error(f"Line {lineno}: F: line missing flag: {line}")
        return

    flag = parts[1].strip()
    if flag and flag not in VALID_FLAGS:
        result.warning(f"Line {lineno}: F: unrecognized flag '{flag}'")


def parse_houses(filepath: Path) -> list[House]:
    """Parse house.txt and return a list of House objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    houses: list[House] = []
    current_house: House | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new house
        if line.startswith("N:"):
            # Save previous house
            if current_house is not None:
                houses.append(current_house)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_house = House(id=int(parts[1]), name=":".join(parts[2:]))
            continue

        if current_house is None:
            continue

        # A: alternate name
        if line.startswith("A:"):
            current_house.alternate_name = line[2:].strip()

        # B: short name
        elif line.startswith("B:"):
            current_house.short_name = line[2:].strip()

        # F: flag
        elif line.startswith("F:"):
            current_house.flag = line[2:].strip()

        # S: stats
        elif line.startswith("S:"):
            parts = line.split(":")
            if len(parts) == 5:
                try:
                    current_house.stats = [int(parts[i]) for i in range(1, 5)]
                except ValueError:
                    pass

        # D: description
        elif line.startswith("D:"):
            content = line[2:]
            if current_house.description is None:
                current_house.description = content
            else:
                current_house.description += " " + content

    if current_house is not None:
        houses.append(current_house)

    return houses


def export_houses_to_json(houses: list[House]) -> str:
    """Export houses to a JSON string."""

    def clean_dict(d: JsonDict) -> JsonDict:
        """Recursively remove None values and empty collections from a dict."""
        result: JsonDict = {}
        for k, v in d.items():
            if v is None or v == [] or v == "":
                continue
            if isinstance(v, dict):
                cleaned = clean_dict(v)
                if cleaned:
                    result[k] = cleaned
            elif isinstance(v, list):
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

    def house_to_dict(house: House) -> JsonDict:
        """Convert a House to a dict, removing None values."""
        return clean_dict(asdict(house))

    data = {"houses": [house_to_dict(h) for h in houses]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_house_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire house.txt file.

    Args:
        filepath: Path to house.txt file.
        limits: Optional limits from limits.txt.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"House file not found: {filepath}")
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

        # N: line - start of house entry
        if line.startswith("N:"):
            house_id = validate_n_line(line, lineno, result)
            if house_id is not None:
                # Check for duplicates
                if house_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {house_id} "
                        + f"(first seen at line {ids_seen[house_id]})"
                    )
                else:
                    ids_seen[house_id] = lineno

                # Check IDs are increasing
                if house_id <= prev_id:
                    result.warning(
                        f"Line {lineno}: ID {house_id} is not greater than previous ID {prev_id}"
                    )
                prev_id = house_id

                # Check ID against limit
                if limits and house_id > limits.max_house_id:
                    result.error(
                        f"Line {lineno}: House ID {house_id} exceeds maximum allowed ID "
                        + f"{limits.max_house_id} (from limits.txt M:C:{limits.max_houses})"
                    )
            continue

        # Other line types
        if line.startswith("A:"):
            pass  # Alternate name, no strict validation
        elif line.startswith("B:"):
            pass  # Short name, no strict validation
        elif line.startswith("F:"):
            validate_f_line(line, lineno, result)
        elif line.startswith("S:"):
            validate_s_line(line, lineno, result)
        elif line.startswith("D:"):
            pass  # Description, no strict validation
        else:
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

    # Check total house count against limit
    if limits:
        house_count = len(ids_seen)
        if house_count > limits.max_houses:
            result.error(
                f"Total house count ({house_count}) exceeds maximum allowed "
                + f"({limits.max_houses}) from limits.txt M:C"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/house.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                     # Validate the file
  %(prog)s --validate lib/edit/house.txt  # Validate a specific file
  %(prog)s --export-json                  # Export to JSON (stdout)
  %(prog)s --export-json > houses.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to house.txt file (default: lib/edit/house.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the house.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all house records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(house_file: Path, limits_file: Path) -> int:
    """Run validation on the house file."""
    print("=" * 60)
    print(f"Validating: {house_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max houses = {limits.max_houses} (max ID = {limits.max_house_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_house_file(house_file, limits)

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


def run_export_json(house_file: Path) -> int:
    """Export houses to JSON on stdout."""
    houses = parse_houses(house_file)

    if not houses:
        print(f"ERROR: No houses found in {house_file}", file=sys.stderr)
        return 1

    print(export_houses_to_json(houses))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        house_file = file_arg
    else:
        house_file = project_dir / "lib" / "edit" / "house.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(house_file, limits_file)

    if export_json:
        return run_export_json(house_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
