#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the history.txt data file.

1. Validates the format and integrity of lib/edit/history.txt.
2. Exports the history records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: primary index : secondary index : probability : house
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

    max_history_lines: int  # M:H value


@dataclass
class HistoryEntry:
    """A history record parsed from history.txt."""

    primary_index: int
    secondary_index: int
    probability: int
    house: int
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_history_lines = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:H:"):
            # M:H:165 - Maximum number of player history lines
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_history_lines = int(parts[2])

    if max_history_lines is None:
        return None

    return Limits(max_history_lines=max_history_lines)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> bool:
    """Validate N: line format. Returns True if valid."""
    parts = line.split(":")
    # N:primary:secondary:probability:house = 5 fields
    if len(parts) != 5:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected 5: {line}")
        return False

    field_names = ["primary index", "secondary index", "probability", "house"]
    for i, name in enumerate(field_names, start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: N: {name} is not numeric: {val}")
            return False

    # Validate probability is 1-100
    probability = int(parts[3])
    if probability < 1 or probability > 100:
        result.warning(f"Line {lineno}: N: probability {probability} is outside expected range 1-100")

    return True


def parse_history_entries(filepath: Path) -> list[HistoryEntry]:
    """Parse history.txt and return a list of HistoryEntry objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    entries: list[HistoryEntry] = []
    current_entry: HistoryEntry | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new entry
        if line.startswith("N:"):
            # Save previous entry
            if current_entry is not None:
                entries.append(current_entry)

            parts = line.split(":")
            if len(parts) == 5:
                try:
                    current_entry = HistoryEntry(
                        primary_index=int(parts[1]),
                        secondary_index=int(parts[2]),
                        probability=int(parts[3]),
                        house=int(parts[4]),
                    )
                except ValueError:
                    current_entry = None
            continue

        if current_entry is None:
            continue

        # D: description
        if line.startswith("D:"):
            content = line[2:]  # Keep spacing as-is per file comments
            if current_entry.description is None:
                current_entry.description = content
            else:
                current_entry.description += " " + content

    if current_entry is not None:
        entries.append(current_entry)

    return entries


def export_history_to_json(entries: list[HistoryEntry]) -> str:
    """Export history entries to a JSON string."""

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

    def entry_to_dict(entry: HistoryEntry) -> JsonDict:
        """Convert a HistoryEntry to a dict, removing None values."""
        return clean_dict(asdict(entry))

    data = {"history": [entry_to_dict(e) for e in entries]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_history_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire history.txt file.

    Args:
        filepath: Path to history.txt file.
        limits: Optional limits from limits.txt.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"History file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    entry_count = 0
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

        # N: line - start of history entry
        if line.startswith("N:"):
            if validate_n_line(line, lineno, result):
                entry_count += 1
            continue

        # D: line - description
        if line.startswith("D:"):
            # D: lines are descriptions, no strict validation needed
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

    # Check total entry count against limit
    if limits:
        if entry_count > limits.max_history_lines:
            result.error(
                f"Total history entry count ({entry_count}) exceeds maximum allowed "
                + f"({limits.max_history_lines}) from limits.txt M:H"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/history.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                       # Validate the file
  %(prog)s --validate lib/edit/history.txt  # Validate a specific file
  %(prog)s --export-json                    # Export to JSON (stdout)
  %(prog)s --export-json > history.json     # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to history.txt file (default: lib/edit/history.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the history.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all history records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(history_file: Path, limits_file: Path) -> int:
    """Run validation on the history file."""
    print("=" * 60)
    print(f"Validating: {history_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max history lines = {limits.max_history_lines}")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_history_file(history_file, limits)

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


def run_export_json(history_file: Path) -> int:
    """Export history entries to JSON on stdout."""
    entries = parse_history_entries(history_file)

    if not entries:
        print(f"ERROR: No history entries found in {history_file}", file=sys.stderr)
        return 1

    print(export_history_to_json(entries))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        history_file = file_arg
    else:
        history_file = project_dir / "lib" / "edit" / "history.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(history_file, limits_file)

    if export_json:
        return run_export_json(history_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
