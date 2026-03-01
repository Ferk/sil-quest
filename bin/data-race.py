#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the race.txt data file.

1. Validates the format and integrity of lib/edit/race.txt.
2. Exports the race records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: race number : race name
- S: str : dex : con : gra
- I: history : agebase : agemax
- H: hgt : modhgt
- W: wgt : modwgt
- C: house | house | etc
- F: racial flags
- E: tval : sval : min : max
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

    max_player_races: int  # M:P value (note: limits.txt stores max+1)

    @property
    def max_race_id(self) -> int:
        """Maximum valid race ID (0-indexed, so max_player_races - 1)."""
        return self.max_player_races - 1


@dataclass
class Equipment:
    """Starting equipment entry (E: line)."""

    tval: int
    sval: int
    min_amount: int
    max_amount: int


@dataclass
class Race:
    """A race record parsed from race.txt."""

    id: int
    name: str
    str_mod: int | None = None
    dex_mod: int | None = None
    con_mod: int | None = None
    gra_mod: int | None = None
    history: int | None = None
    age_base: int | None = None
    age_max: int | None = None
    height_base: int | None = None
    height_mod: int | None = None
    weight_base: int | None = None
    weight_mod: int | None = None
    houses: list[int] = field(default_factory=list)
    flags: list[str] = field(default_factory=list)
    equipment: list[Equipment] = field(default_factory=list)
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_player_races = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:P:"):
            # M:P:4 - Maximum number of player races
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_player_races = int(parts[2])

    if max_player_races is None:
        return None

    return Limits(max_player_races=max_player_races)


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


def validate_s_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate S: line format (str : dex : con : gra)."""
    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: S: line has {len(parts)} fields, expected 5: {line}")
        return

    for i, name in enumerate(["str", "dex", "con", "gra"], start=1):
        val = parts[i]
        if not re.match(r"^-?\d+$", val):
            result.error(f"Line {lineno}: S: {name} is not a valid integer: {val}")


def validate_i_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate I: line format (history : agebase : agemax)."""
    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: I: line has {len(parts)} fields, expected 4: {line}")
        return

    for i, name in enumerate(["history", "agebase", "agemax"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: I: {name} is not numeric: {val}")


def validate_h_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate H: line format (hgt : modhgt)."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: H: line has {len(parts)} fields, expected 3: {line}")
        return

    for i, name in enumerate(["hgt", "modhgt"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: H: {name} is not numeric: {val}")


def validate_w_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate W: line format (wgt : modwgt)."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: W: line has {len(parts)} fields, expected 3: {line}")
        return

    for i, name in enumerate(["wgt", "modwgt"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: W: {name} is not numeric: {val}")


def validate_c_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate C: line format (pipe-separated house IDs)."""
    parts = line.split(":")
    if len(parts) != 2:
        result.error(f"Line {lineno}: C: line has {len(parts)} fields, expected 2: {line}")
        return

    house_str = parts[1]
    houses = house_str.split("|")
    for house in houses:
        if not house.isdigit():
            result.error(f"Line {lineno}: C: house ID is not numeric: {house}")


def validate_e_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate E: line format (tval : sval : min : max)."""
    # Remove inline comments
    if "#" in line:
        line = line[: line.index("#")]
    line = line.strip()

    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: E: line has {len(parts)} fields, expected 5: {line}")
        return

    for i, name in enumerate(["tval", "sval", "min", "max"], start=1):
        val = parts[i].strip()
        if not val.isdigit():
            result.error(f"Line {lineno}: E: {name} is not numeric: {val}")


def parse_equipment(line: str) -> Equipment | None:
    """Parse an E: line into an Equipment object."""
    # Remove inline comments
    if "#" in line:
        line = line[: line.index("#")]
    line = line.strip()

    parts = line.split(":")
    if len(parts) != 5:
        return None

    try:
        return Equipment(
            tval=int(parts[1].strip()),
            sval=int(parts[2].strip()),
            min_amount=int(parts[3].strip()),
            max_amount=int(parts[4].strip()),
        )
    except ValueError:
        return None


def parse_races(filepath: Path) -> list[Race]:
    """Parse race.txt and return a list of Race objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    races: list[Race] = []
    current_race: Race | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new race
        if line.startswith("N:"):
            # Save previous race
            if current_race is not None:
                races.append(current_race)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_race = Race(id=int(parts[1]), name=parts[2])
            continue

        if current_race is None:
            continue

        # S: str : dex : con : gra
        if line.startswith("S:"):
            parts = line.split(":")
            if len(parts) >= 5:
                if re.match(r"^-?\d+$", parts[1]):
                    current_race.str_mod = int(parts[1])
                if re.match(r"^-?\d+$", parts[2]):
                    current_race.dex_mod = int(parts[2])
                if re.match(r"^-?\d+$", parts[3]):
                    current_race.con_mod = int(parts[3])
                if re.match(r"^-?\d+$", parts[4]):
                    current_race.gra_mod = int(parts[4])

        # I: history : agebase : agemax
        elif line.startswith("I:"):
            parts = line.split(":")
            if len(parts) >= 4:
                if parts[1].isdigit():
                    current_race.history = int(parts[1])
                if parts[2].isdigit():
                    current_race.age_base = int(parts[2])
                if parts[3].isdigit():
                    current_race.age_max = int(parts[3])

        # H: hgt : modhgt
        elif line.startswith("H:"):
            parts = line.split(":")
            if len(parts) >= 3:
                if parts[1].isdigit():
                    current_race.height_base = int(parts[1])
                if parts[2].isdigit():
                    current_race.height_mod = int(parts[2])

        # W: wgt : modwgt
        elif line.startswith("W:"):
            parts = line.split(":")
            if len(parts) >= 3:
                if parts[1].isdigit():
                    current_race.weight_base = int(parts[1])
                if parts[2].isdigit():
                    current_race.weight_mod = int(parts[2])

        # C: house IDs
        elif line.startswith("C:"):
            parts = line.split(":")
            if len(parts) >= 2:
                houses = parts[1].split("|")
                for house in houses:
                    if house.isdigit():
                        current_race.houses.append(int(house))

        # F: flags
        elif line.startswith("F:"):
            content = line[2:]  # Remove "F:"
            flags = [f.strip() for f in content.split("|") if f.strip()]
            current_race.flags.extend(flags)

        # E: equipment
        elif line.startswith("E:"):
            equip = parse_equipment(line)
            if equip:
                current_race.equipment.append(equip)

        # D: description
        elif line.startswith("D:"):
            content = line[2:].strip()  # Remove "D:" and strip whitespace
            if current_race.description is None:
                current_race.description = content
            else:
                current_race.description += " " + content

    if current_race is not None:
        races.append(current_race)

    return races


def export_races_to_json(races: list[Race]) -> str:
    """Export races to a JSON string."""

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

    def race_to_dict(race: Race) -> JsonDict:
        """Convert a Race to a dict, removing None values."""
        d = clean_dict(asdict(race))
        if "flags" in d and isinstance(d["flags"], list):
            d["flags"] = cast(JsonValue, sorted(cast(list[str], d["flags"])))
        return d

    data = {"races": [race_to_dict(r) for r in races]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_race_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire race.txt file.

    Args:
        filepath: Path to race.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                race count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Race file not found: {filepath}")
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

        # N: line - start of race entry
        if line.startswith("N:"):
            race_id = validate_n_line(line, lineno, result)
            if race_id is not None:
                # Check for duplicates
                if race_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {race_id} "
                        + f"(first seen at line {ids_seen[race_id]})"
                    )
                else:
                    ids_seen[race_id] = lineno

                # Check IDs are increasing
                if race_id <= prev_id:
                    result.error(
                        f"Line {lineno}: ID {race_id} is not greater than previous ID {prev_id} "
                        + "(IDs must be strictly increasing)"
                    )
                prev_id = race_id

                # Check ID against limit
                if limits and race_id > limits.max_race_id:
                    result.error(
                        f"Line {lineno}: Race ID {race_id} exceeds maximum allowed ID "
                        + f"{limits.max_race_id} (from limits.txt M:P:{limits.max_player_races})"
                    )
            continue

        # Other line types
        if line.startswith("S:"):
            validate_s_line(line, lineno, result)
        elif line.startswith("I:"):
            validate_i_line(line, lineno, result)
        elif line.startswith("H:"):
            validate_h_line(line, lineno, result)
        elif line.startswith("W:"):
            validate_w_line(line, lineno, result)
        elif line.startswith("C:"):
            validate_c_line(line, lineno, result)
        elif line.startswith("F:"):
            pass  # F: lines are free-form flags, no strict validation needed
        elif line.startswith("E:"):
            validate_e_line(line, lineno, result)
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

    # Check total race count against limit
    if limits:
        race_count = len(ids_seen)
        if race_count > limits.max_player_races:
            result.error(
                f"Total race count ({race_count}) exceeds maximum allowed "
                + f"({limits.max_player_races}) from limits.txt M:P"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/race.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                    # Validate the file
  %(prog)s --validate lib/edit/race.txt  # Validate a specific file
  %(prog)s --export-json                 # Export to JSON (stdout)
  %(prog)s --export-json > races.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to race.txt file (default: lib/edit/race.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the race.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all race records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(race_file: Path, limits_file: Path) -> int:
    """Run validation on the race file."""
    print("=" * 60)
    print(f"Validating: {race_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max player races = {limits.max_player_races} (max ID = {limits.max_race_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_race_file(race_file, limits)

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


def run_export_json(race_file: Path) -> int:
    """Export races to JSON on stdout."""
    races = parse_races(race_file)

    if not races:
        print(f"ERROR: No races found in {race_file}", file=sys.stderr)
        return 1

    print(export_races_to_json(races))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        race_file = file_arg
    else:
        race_file = project_dir / "lib" / "edit" / "race.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(race_file, limits_file)

    if export_json:
        return run_export_json(race_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
