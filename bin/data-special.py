#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the special.txt data file.

1. Validates the format and integrity of lib/edit/special.txt.
2. Exports the special item records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: serial number : special type
- C: max att : plus damage dice : plus damage sides : max evn :
     plus prot dice : plus prot sides : pval
- W: depth : rarity : max_depth : cost
- T: tval : min_sval : max_sval
- B: skill_id / ability_id
- F: flag | flag | etc

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

###
### Regex patterns for validation
###
# Regex pattern for ability format
ABILITY_PATTERN = re.compile(r"^\d+/\d+$")

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

    max_special_types: int  # M:E value (note: limits.txt stores max+1)
    max_abilities: int  # M:B value

    @property
    def max_special_id(self) -> int:
        """Maximum valid special item ID (0-indexed, so max_special_types - 1)."""
        return self.max_special_types - 1

    @property
    def max_ability_id(self) -> int:
        """Maximum valid ability ID."""
        return self.max_abilities - 1


@dataclass
class TvalRange:
    """A tval/sval range entry (T: line)."""

    tval: int
    min_sval: int
    max_sval: int


@dataclass
class AbilityRef:
    """An ability reference (B: line)."""

    skill_id: int
    ability_id: int


@dataclass
class Special:
    """A special item record parsed from special.txt."""

    id: int
    name: str
    max_att: int | None = None
    plus_damage_dice: int | None = None
    plus_damage_sides: int | None = None
    max_evn: int | None = None
    plus_prot_dice: int | None = None
    plus_prot_sides: int | None = None
    pval: int | None = None
    depth: int | None = None
    rarity: int | None = None
    max_depth: int | None = None
    cost: int | None = None
    tval_ranges: list[TvalRange] = field(default_factory=list)
    abilities: list[AbilityRef] = field(default_factory=list)
    flags: list[str] = field(default_factory=list)


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_special_types = None
    max_abilities = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:E:"):
            # M:E:145 - Maximum number of special item types
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_special_types = int(parts[2])
        elif line.startswith("M:B:"):
            # M:B:240 - Maximum number of abilities
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_abilities = int(parts[2])

    if max_special_types is None or max_abilities is None:
        return None

    return Limits(max_special_types=max_special_types, max_abilities=max_abilities)


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


def validate_c_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate C: line format (7 creation bonus values)."""
    parts = line.split(":")
    if len(parts) != 8:
        result.error(f"Line {lineno}: C: line has {len(parts)} fields, expected 8: {line}")
        return

    field_names = [
        "max_att",
        "plus_damage_dice",
        "plus_damage_sides",
        "max_evn",
        "plus_prot_dice",
        "plus_prot_sides",
        "pval",
    ]
    for i, name in enumerate(field_names, start=1):
        val = parts[i]
        if not re.match(r"^-?\d+$", val):
            result.error(f"Line {lineno}: C: {name} is not a valid integer: {val}")


def validate_w_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate W: line format (depth : rarity : max_depth : cost)."""
    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: W: line has {len(parts)} fields, expected 5: {line}")
        return

    for i, name in enumerate(["depth", "rarity", "max_depth", "cost"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: W: {name} is not numeric: {val}")


def validate_t_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate T: line format (tval : min_sval : max_sval)."""
    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: T: line has {len(parts)} fields, expected 4: {line}")
        return

    for i, name in enumerate(["tval", "min_sval", "max_sval"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: T: {name} is not numeric: {val}")


def validate_b_line(line: str, lineno: int, result: ValidationResult, limits: Limits | None) -> None:
    """Validate B: line format (ability references)."""
    parts = line.split(":")
    if len(parts) != 2:
        result.error(f"Line {lineno}: B: line has {len(parts)} fields, expected 2: {line}")
        return

    ability_str = parts[1]
    if not ABILITY_PATTERN.match(ability_str):
        result.error(f"Line {lineno}: B: invalid ability format '{ability_str}', expected X/Y")
        return

    # Validate ability IDs are within limits
    if limits:
        _, ability_id = map(int, ability_str.split("/"))
        if ability_id > limits.max_ability_id:
            result.error(
                f"Line {lineno}: B: ability_id {ability_id} exceeds max {limits.max_ability_id}"
            )


def parse_ability(ability_str: str) -> AbilityRef | None:
    """Parse an ability string like "0/5" into an AbilityRef object."""
    if ABILITY_PATTERN.match(ability_str):
        skill_id, ability_id = map(int, ability_str.split("/"))
        return AbilityRef(skill_id=skill_id, ability_id=ability_id)
    return None


def parse_specials(filepath: Path) -> list[Special]:
    """Parse special.txt and return a list of Special objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    specials: list[Special] = []
    current_special: Special | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new special item
        if line.startswith("N:"):
            # Save previous special
            if current_special is not None:
                specials.append(current_special)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                # Join remaining parts for name (in case name contains colons)
                name = ":".join(parts[2:])
                current_special = Special(id=int(parts[1]), name=name)
            continue

        if current_special is None:
            continue

        # C: creation bonuses (7 values)
        if line.startswith("C:"):
            parts = line.split(":")
            if len(parts) >= 8:
                if re.match(r"^-?\d+$", parts[1]):
                    current_special.max_att = int(parts[1])
                if re.match(r"^-?\d+$", parts[2]):
                    current_special.plus_damage_dice = int(parts[2])
                if re.match(r"^-?\d+$", parts[3]):
                    current_special.plus_damage_sides = int(parts[3])
                if re.match(r"^-?\d+$", parts[4]):
                    current_special.max_evn = int(parts[4])
                if re.match(r"^-?\d+$", parts[5]):
                    current_special.plus_prot_dice = int(parts[5])
                if re.match(r"^-?\d+$", parts[6]):
                    current_special.plus_prot_sides = int(parts[6])
                if re.match(r"^-?\d+$", parts[7]):
                    current_special.pval = int(parts[7])

        # W: depth : rarity : max_depth : cost
        elif line.startswith("W:"):
            parts = line.split(":")
            if len(parts) >= 5:
                if parts[1].isdigit():
                    current_special.depth = int(parts[1])
                if parts[2].isdigit():
                    current_special.rarity = int(parts[2])
                if parts[3].isdigit():
                    current_special.max_depth = int(parts[3])
                if parts[4].isdigit():
                    current_special.cost = int(parts[4])

        # T: tval : min_sval : max_sval
        elif line.startswith("T:"):
            parts = line.split(":")
            if len(parts) >= 4:
                if parts[1].isdigit() and parts[2].isdigit() and parts[3].isdigit():
                    current_special.tval_ranges.append(
                        TvalRange(
                            tval=int(parts[1]),
                            min_sval=int(parts[2]),
                            max_sval=int(parts[3]),
                        )
                    )

        # B: ability reference
        elif line.startswith("B:"):
            parts = line.split(":")
            if len(parts) >= 2:
                ability = parse_ability(parts[1])
                if ability:
                    current_special.abilities.append(ability)

        # F: flags
        elif line.startswith("F:"):
            content = line[2:]  # Remove "F:"
            flags = [f.strip() for f in content.split("|") if f.strip()]
            current_special.flags.extend(flags)

    if current_special is not None:
        specials.append(current_special)

    return specials


def export_specials_to_json(specials: list[Special]) -> str:
    """Export specials to a JSON string."""

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

    def special_to_dict(special: Special) -> JsonDict:
        """Convert a Special to a dict, removing None values."""
        d = clean_dict(asdict(special))
        if "flags" in d and isinstance(d["flags"], list):
            d["flags"] = cast(JsonValue, sorted(cast(list[str], d["flags"])))
        return d

    data = {"specials": [special_to_dict(s) for s in specials]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_special_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire special.txt file.

    Args:
        filepath: Path to special.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                special item count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Special file not found: {filepath}")
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

        # N: line - start of special item entry
        if line.startswith("N:"):
            special_id = validate_n_line(line, lineno, result)
            if special_id is not None:
                # Check for duplicates
                if special_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {special_id} "
                        + f"(first seen at line {ids_seen[special_id]})"
                    )
                else:
                    ids_seen[special_id] = lineno

                # Check IDs are increasing
                if special_id <= prev_id:
                    result.error(
                        f"Line {lineno}: ID {special_id} is not greater than previous ID {prev_id} "
                        + "(IDs must be strictly increasing)"
                    )
                prev_id = special_id

                # Check ID against limit
                if limits and special_id > limits.max_special_id:
                    result.error(
                        f"Line {lineno}: Special ID {special_id} exceeds maximum allowed ID "
                        + f"{limits.max_special_id} (from limits.txt M:E:{limits.max_special_types})"
                    )
            continue

        # Other line types
        if line.startswith("C:"):
            validate_c_line(line, lineno, result)
        elif line.startswith("W:"):
            validate_w_line(line, lineno, result)
        elif line.startswith("T:"):
            validate_t_line(line, lineno, result)
        elif line.startswith("B:"):
            validate_b_line(line, lineno, result, limits)
        elif line.startswith("F:"):
            pass  # F: lines are free-form flags, no strict validation needed
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

    # Check total special count against limit
    if limits:
        special_count = len(ids_seen)
        if special_count > limits.max_special_types:
            result.error(
                f"Total special count ({special_count}) exceeds maximum allowed "
                + f"({limits.max_special_types}) from limits.txt M:E"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/special.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                       # Validate the file
  %(prog)s --validate lib/edit/special.txt  # Validate a specific file
  %(prog)s --export-json                    # Export to JSON (stdout)
  %(prog)s --export-json > specials.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to special.txt file (default: lib/edit/special.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the special.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all special item records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(special_file: Path, limits_file: Path) -> int:
    """Run validation on the special file."""
    print("=" * 60)
    print(f"Validating: {special_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max special types = {limits.max_special_types} (max ID = {limits.max_special_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_special_file(special_file, limits)

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


def run_export_json(special_file: Path) -> int:
    """Export specials to JSON on stdout."""
    specials = parse_specials(special_file)

    if not specials:
        print(f"ERROR: No specials found in {special_file}", file=sys.stderr)
        return 1

    print(export_specials_to_json(specials))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        special_file = file_arg
    else:
        special_file = project_dir / "lib" / "edit" / "special.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(special_file, limits_file)

    if export_json:
        return run_export_json(special_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
