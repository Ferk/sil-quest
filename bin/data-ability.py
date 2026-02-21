#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the ability.txt data file.

1. Validates the format and integrity of lib/edit/ability.txt.
2. Exports the ability records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: ability number : ability name
- I: skill number : ability value : level requirement
- P: prerequisite skill number / prerequisite ability value : ...
- T: tval : sval_min : sval_max
- D: description

Exit codes:
  0 - All validations passed
  1 - One or more validation errors found
"""

import argparse
import json
import re
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

    max_abilities: int  # M:B value

    @property
    def max_ability_id(self) -> int:
        """Maximum valid ability ID (0-indexed, so max_abilities - 1)."""
        return self.max_abilities - 1


@dataclass
class Prerequisite:
    """A prerequisite ability reference (P: line entry)."""

    skill_id: int
    ability_id: int


@dataclass
class ItemTypeRange:
    """An item type range (T: line entry)."""

    tval: int
    sval_min: int
    sval_max: int


@dataclass
class Ability:
    """An ability record parsed from ability.txt."""

    id: int
    name: str
    skill_id: int | None = None
    ability_value: int | None = None
    level_requirement: int | None = None
    prerequisites: list[Prerequisite] = field(default_factory=list)
    item_types: list[ItemTypeRange] = field(default_factory=list)
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_abilities = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:B:"):
            # M:B:240 - Maximum number of abilities
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_abilities = int(parts[2])

    if max_abilities is None:
        return None

    return Limits(max_abilities=max_abilities)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> int | None:
    """Validate N: line format and return the ID."""
    parts = line.split(":")
    if len(parts) < 3:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected at least 3: {line}")
        return None

    id_str = parts[1]
    if not id_str.isdigit():
        result.error(f"Line {lineno}: N: ID is not numeric: {id_str}")
        return None

    return int(id_str)


def validate_i_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate I: line format (skill number : ability value : level requirement)."""
    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: I: line has {len(parts)} fields, expected 4: {line}")
        return

    for i, name in enumerate(["skill number", "ability value", "level requirement"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: I: {name} is not numeric: {val}")


def validate_p_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate P: line format (prerequisite skill/ability pairs)."""
    content = line[2:]  # Remove "P:"

    # Standard format: skill/ability pairs separated by colons
    pairs = content.split(":")
    for pair in pairs:
        pair = pair.strip()
        if not re.match(r"^\d+/\d+$", pair):
            result.error(f"Line {lineno}: P: invalid prerequisite '{pair}' (expected format: skill/ability)")


def validate_t_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate T: line format (tval : sval_min : sval_max)."""
    # Remove inline comments
    if "#" in line:
        line = line[: line.index("#")]
    line = line.strip()

    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: T: line has {len(parts)} fields, expected 4: {line}")
        return

    for i, name in enumerate(["tval", "sval_min", "sval_max"], start=1):
        val = parts[i].strip()
        if not val.isdigit():
            result.error(f"Line {lineno}: T: {name} is not numeric: {val}")


def parse_prerequisites(line: str) -> list[Prerequisite]:
    """Parse a P: line into a list of Prerequisite objects."""
    content = line[2:]  # Remove "P:"
    prerequisites: list[Prerequisite] = []

    pairs = content.split(":")
    for pair in pairs:
        pair = pair.strip()
        match = re.match(r"^(\d+)/(\d+)$", pair)
        if match:
            prerequisites.append(Prerequisite(skill_id=int(match.group(1)), ability_id=int(match.group(2))))

    return prerequisites


def parse_item_type(line: str) -> ItemTypeRange | None:
    """Parse a T: line into an ItemTypeRange object."""
    # Remove inline comments
    if "#" in line:
        line = line[: line.index("#")]
    line = line.strip()

    parts = line.split(":")
    if len(parts) != 4:
        return None

    try:
        return ItemTypeRange(
            tval=int(parts[1].strip()),
            sval_min=int(parts[2].strip()),
            sval_max=int(parts[3].strip()),
        )
    except ValueError:
        return None


def parse_abilities(filepath: Path) -> list[Ability]:
    """Parse ability.txt and return a list of Ability objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    abilities: list[Ability] = []
    current_ability: Ability | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new ability
        if line.startswith("N:"):
            # Save previous ability
            if current_ability is not None:
                abilities.append(current_ability)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_ability = Ability(id=int(parts[1]), name=parts[2])
            continue

        if current_ability is None:
            continue

        # I: skill number : ability value : level requirement
        if line.startswith("I:"):
            parts = line.split(":")
            if len(parts) >= 4:
                if parts[1].isdigit():
                    current_ability.skill_id = int(parts[1])
                if parts[2].isdigit():
                    current_ability.ability_value = int(parts[2])
                if parts[3].isdigit():
                    current_ability.level_requirement = int(parts[3])

        # P: prerequisites
        elif line.startswith("P:"):
            prereqs = parse_prerequisites(line)
            current_ability.prerequisites.extend(prereqs)

        # T: item type range
        elif line.startswith("T:"):
            item_type = parse_item_type(line)
            if item_type:
                current_ability.item_types.append(item_type)

        # D: description
        elif line.startswith("D:"):
            content = line[2:].strip()  # Remove "D:" and strip whitespace
            if current_ability.description is None:
                current_ability.description = content
            else:
                current_ability.description += " " + content

    if current_ability is not None:
        abilities.append(current_ability)

    return abilities


def export_abilities_to_json(abilities: list[Ability]) -> str:
    """Export abilities to a JSON string."""

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

    def ability_to_dict(ability: Ability) -> JsonDict:
        """Convert an Ability to a dict, removing None values."""
        return clean_dict(asdict(ability))

    data = {"abilities": [ability_to_dict(a) for a in abilities]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_ability_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire ability.txt file.

    Args:
        filepath: Path to ability.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                ability count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Ability file not found: {filepath}")
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

        # N: line - start of ability entry
        if line.startswith("N:"):
            ability_id = validate_n_line(line, lineno, result)
            if ability_id is not None:
                # Check for duplicates
                if ability_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {ability_id} "
                        + f"(first seen at line {ids_seen[ability_id]})"
                    )
                else:
                    ids_seen[ability_id] = lineno

                # Note: Ability IDs can have gaps (not strictly increasing by 1)
                # but should generally increase
                if ability_id < prev_id:
                    result.warning(
                        f"Line {lineno}: ID {ability_id} is less than previous ID {prev_id} "
                        + "(IDs should generally increase)"
                    )
                prev_id = ability_id

                # Check ID against limit
                if limits and ability_id > limits.max_ability_id:
                    result.error(
                        f"Line {lineno}: Ability ID {ability_id} exceeds maximum allowed ID "
                        + f"{limits.max_ability_id} (from limits.txt M:B:{limits.max_abilities})"
                    )
            continue

        # Other line types
        if line.startswith("I:"):
            validate_i_line(line, lineno, result)
        elif line.startswith("P:"):
            validate_p_line(line, lineno, result)
        elif line.startswith("T:"):
            validate_t_line(line, lineno, result)
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

    # Check total ability count against limit
    if limits:
        ability_count = len(ids_seen)
        if ability_count > limits.max_abilities:
            result.error(
                f"Total ability count ({ability_count}) exceeds maximum allowed "
                + f"({limits.max_abilities}) from limits.txt M:B"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/ability.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                        # Validate the file
  %(prog)s --validate lib/edit/ability.txt   # Validate a specific file
  %(prog)s --export-json                     # Export to JSON (stdout)
  %(prog)s --export-json > abilities.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to ability.txt file (default: lib/edit/ability.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the ability.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all ability records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(ability_file: Path, limits_file: Path) -> int:
    """Run validation on the ability file."""
    print("=" * 60)
    print(f"Validating: {ability_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max abilities = {limits.max_abilities} (max ID = {limits.max_ability_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_ability_file(ability_file, limits)

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


def run_export_json(ability_file: Path) -> int:
    """Export abilities to JSON on stdout."""
    abilities = parse_abilities(ability_file)

    if not abilities:
        print(f"ERROR: No abilities found in {ability_file}", file=sys.stderr)
        return 1

    print(export_abilities_to_json(abilities))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        ability_file = file_arg
    else:
        ability_file = project_dir / "lib" / "edit" / "ability.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(ability_file, limits_file)

    if export_json:
        return run_export_json(ability_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
