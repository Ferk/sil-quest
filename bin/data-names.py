#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the names.txt data file.

1. Validates the format and integrity of lib/edit/names.txt.
2. Exports the name records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: name

Additional constraints from comments:
- Every word (data record) must be unique.
- Data records should be in alphabetical order.

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
class Name:
    """A name record parsed from names.txt."""

    name: str


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> str | None:
    """Validate N: line format and return the name."""
    parts = line.split(":", 1)  # Split only on first colon
    if len(parts) < 2:
        result.error(f"Line {lineno}: N: line missing name: {line}")
        return None

    name = parts[1].strip()
    if not name:
        result.error(f"Line {lineno}: N: line has empty name: {line}")
        return None

    return name


def parse_names(filepath: Path) -> list[Name]:
    """Parse names.txt and return a list of Name objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    names: list[Name] = []

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line
        if line.startswith("N:"):
            parts = line.split(":", 1)
            if len(parts) >= 2:
                name = parts[1].strip()
                if name:
                    names.append(Name(name=name))

    return names


def export_names_to_json(names: list[Name]) -> str:
    """Export names to a JSON string, sorted alphabetically."""
    sorted_names = sorted(n.name for n in names)
    data = {"names": sorted_names}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_names_file(filepath: Path) -> ValidationResult:
    """Validate the entire names.txt file."""
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Names file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state
    names_seen: dict[str, int] = {}  # Maps name -> line number
    prev_name: str | None = None
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

        # N: line
        if line.startswith("N:"):
            name = validate_n_line(line, lineno, result)
            if name is not None:
                # Check for duplicates
                if name in names_seen:
                    result.error(
                        f"Line {lineno}: Duplicate name '{name}' "
                        + f"(first seen at line {names_seen[name]})"
                    )
                else:
                    names_seen[name] = lineno

                # Check alphabetical ordering
                if prev_name is not None and name < prev_name:
                    result.warning(
                        f"Line {lineno}: Name '{name}' is not in alphabetical order "
                        + f"(comes after '{prev_name}')"
                    )
                prev_name = name
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

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/names.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                     # Validate the file
  %(prog)s --validate lib/edit/names.txt  # Validate a specific file
  %(prog)s --export-json                  # Export to JSON (stdout)
  %(prog)s --export-json > names.json     # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to names.txt file (default: lib/edit/names.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the names.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all name records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(names_file: Path) -> int:
    """Run validation on the names file."""
    print("=" * 60)
    print(f"Validating: {names_file}")

    # Validate the file
    result = validate_names_file(names_file)

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


def run_export_json(names_file: Path) -> int:
    """Export names to JSON on stdout."""
    names = parse_names(names_file)

    if not names:
        print(f"ERROR: No names found in {names_file}", file=sys.stderr)
        return 1

    print(export_names_to_json(names))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        names_file = file_arg
    else:
        names_file = project_dir / "lib" / "edit" / "names.txt"

    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(names_file)

    if export_json:
        return run_export_json(names_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
