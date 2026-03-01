#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the artefact.txt data file.

1. Validates the format and integrity of lib/edit/artefact.txt.
2. Exports the artefact records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: serial number : item name
- G: char : attr
- I: tval : sval : pval
- B: skilltype / abilitynum : ...
- W: depth : rarity : weight : cost
- P: attack bonus : damage dice : evasion bonus : protection dice
- F: flag | flag | etc
- D: text

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
### Valid colors
###
# Valid base colors (16 colors)
# D - Dark Gray    w - White          s - Gray          o - Orange
# r - Red          g - Green          b - Blue          u - Brown
# d - Black        W - Light Gray     v - Violet        y - Yellow
# R - Light Red    G - Light Green    B - Light Blue    U - Light Brown
VALID_BASE_COLORS = set("DwsorgbudWvyRGBU")
# Extended colors observed in actual data (base color + numeric suffix)
VALID_EXTENDED_COLORS = {"b1"}
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

    special_artefacts: int  # M:A first value
    normal_artefacts: int  # M:A second value

    @property
    def max_artefact_id(self) -> int:
        """Maximum valid artefact ID (special + normal - 1, 0-indexed)."""
        return self.special_artefacts + self.normal_artefacts - 1

    @property
    def total_artefacts(self) -> int:
        """Total number of artefacts (special + normal)."""
        return self.special_artefacts + self.normal_artefacts


@dataclass
class Ability:
    """An ability reference (B: line entry)."""

    skill_id: int
    ability_id: int


@dataclass
class Artefact:
    """An artefact record parsed from artefact.txt."""

    id: int
    name: str
    symbol: str | None = None
    color: str | None = None
    tval: int | None = None
    sval: int | None = None
    pval: int | None = None
    depth: int | None = None
    rarity: int | None = None
    weight: int | None = None
    cost: int | None = None
    attack_bonus: int | None = None
    damage_dice: str | None = None
    evasion_bonus: int | None = None
    protection_dice: str | None = None
    abilities: list[Ability] = field(default_factory=list)
    flags: list[str] = field(default_factory=list)
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:A:"):
            # M:A:special:normal:random:self-made
            parts = line.split(":")
            if len(parts) >= 4 and parts[2].isdigit() and parts[3].isdigit():
                return Limits(
                    special_artefacts=int(parts[2]),
                    normal_artefacts=int(parts[3]),
                )

    return None


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


def validate_g_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate G: line format (char : attr)."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: G: line has {len(parts)} fields, expected 3: {line}")
        return

    color = parts[2]
    if color not in VALID_COLORS:
        result.error(f"Line {lineno}: G: unrecognized color '{color}'")


def validate_i_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate I: line format (tval : sval : pval)."""
    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: I: line has {len(parts)} fields, expected 4: {line}")
        return

    for i, name in enumerate(["tval", "sval", "pval"], start=1):
        val = parts[i]
        if not re.match(r"^-?\d+$", val):
            result.error(f"Line {lineno}: I: {name} is not a valid integer: {val}")


def validate_b_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate B: line format (skilltype/abilitynum pairs or flags)."""
    content = line[2:]  # Remove "B:"

    # Check if it looks like flags (contains | and uppercase letters)
    if "|" in content and re.search(r"[A-Z_]+", content):
        # This is the flag format (e.g., "STR | RES_FEAR | FREE_ACT")
        # This appears to be non-standard usage, but accept it
        result.warning(f"Line {lineno}: B: line uses flag format instead of ability references: {line}")
        return

    # Standard format: skill/ability pairs separated by colons
    pairs = content.split(":")
    for pair in pairs:
        pair = pair.strip()
        if not re.match(r"^\d+/\d+$", pair):
            result.error(f"Line {lineno}: B: invalid ability reference '{pair}' (expected format: skill/ability)")


def validate_w_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate W: line format (depth : rarity : weight : cost)."""
    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: W: line has {len(parts)} fields, expected 5: {line}")
        return

    for i, name in enumerate(["depth", "rarity", "weight", "cost"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: W: {name} is not numeric: {val}")


def validate_p_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate P: line format (attack bonus : damage dice : evasion bonus : protection dice).

    Note: Some items (crowns/light sources) have a 5th value (always 0).
    Bonuses can have a + prefix (e.g., +2, +11).
    """
    parts = line.split(":")
    if len(parts) not in (5, 6):
        result.error(f"Line {lineno}: P: line has {len(parts)} fields, expected 5 or 6: {line}")
        return

    # Attack bonus (can be negative or have + prefix)
    if not re.match(r"^[+-]?\d+$", parts[1]):
        result.error(f"Line {lineno}: P: attack bonus is not a valid integer: {parts[1]}")

    # Damage dice (NdM format)
    if not re.match(r"^\d+d\d+$", parts[2]):
        result.error(f"Line {lineno}: P: damage dice has invalid format: {parts[2]}")

    # Evasion bonus (can be negative or have + prefix)
    if not re.match(r"^[+-]?\d+$", parts[3]):
        result.error(f"Line {lineno}: P: evasion bonus is not a valid integer: {parts[3]}")

    # Protection dice (NdM format)
    if not re.match(r"^\d+d\d+$", parts[4]):
        result.error(f"Line {lineno}: P: protection dice has invalid format: {parts[4]}")


def parse_abilities(line: str) -> list[Ability]:
    """Parse a B: line into a list of Ability objects."""
    content = line[2:]  # Remove "B:"

    # Skip if it looks like flags
    if "|" in content and re.search(r"[A-Z_]+", content):
        return []

    abilities: list[Ability] = []
    pairs = content.split(":")
    for pair in pairs:
        pair = pair.strip()
        match = re.match(r"^(\d+)/(\d+)$", pair)
        if match:
            abilities.append(Ability(skill_id=int(match.group(1)), ability_id=int(match.group(2))))
    return abilities


def parse_artefacts(filepath: Path) -> list[Artefact]:
    """Parse artefact.txt and return a list of Artefact objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    artefacts: list[Artefact] = []
    current_artefact: Artefact | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new artefact
        if line.startswith("N:"):
            # Save previous artefact
            if current_artefact is not None:
                artefacts.append(current_artefact)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_artefact = Artefact(id=int(parts[1]), name=parts[2])
            continue

        if current_artefact is None:
            continue

        # G: char : attr
        if line.startswith("G:"):
            parts = line.split(":")
            if len(parts) >= 3:
                current_artefact.symbol = parts[1]
                current_artefact.color = parts[2]

        # I: tval : sval : pval
        elif line.startswith("I:"):
            parts = line.split(":")
            if len(parts) >= 4:
                if re.match(r"^-?\d+$", parts[1]):
                    current_artefact.tval = int(parts[1])
                if re.match(r"^-?\d+$", parts[2]):
                    current_artefact.sval = int(parts[2])
                if re.match(r"^-?\d+$", parts[3]):
                    current_artefact.pval = int(parts[3])

        # B: abilities
        elif line.startswith("B:"):
            abilities = parse_abilities(line)
            current_artefact.abilities.extend(abilities)

        # W: depth : rarity : weight : cost
        elif line.startswith("W:"):
            parts = line.split(":")
            if len(parts) >= 5:
                if parts[1].isdigit():
                    current_artefact.depth = int(parts[1])
                if parts[2].isdigit():
                    current_artefact.rarity = int(parts[2])
                if parts[3].isdigit():
                    current_artefact.weight = int(parts[3])
                if parts[4].isdigit():
                    current_artefact.cost = int(parts[4])

        # P: attack bonus : damage dice : evasion bonus : protection dice
        elif line.startswith("P:"):
            parts = line.split(":")
            if len(parts) >= 5:
                if re.match(r"^-?\d+$", parts[1]):
                    current_artefact.attack_bonus = int(parts[1])
                current_artefact.damage_dice = parts[2]
                if re.match(r"^-?\d+$", parts[3]):
                    current_artefact.evasion_bonus = int(parts[3])
                current_artefact.protection_dice = parts[4]

        # F: flags
        elif line.startswith("F:"):
            content = line[2:]  # Remove "F:"
            flags = [f.strip() for f in content.split("|") if f.strip()]
            current_artefact.flags.extend(flags)

        # D: description
        elif line.startswith("D:"):
            content = line[2:].strip()  # Remove "D:" and strip whitespace
            if current_artefact.description is None:
                current_artefact.description = content
            else:
                current_artefact.description += " " + content

    if current_artefact is not None:
        artefacts.append(current_artefact)

    return artefacts


def export_artefacts_to_json(artefacts: list[Artefact]) -> str:
    """Export artefacts to a JSON string."""

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

    def artefact_to_dict(artefact: Artefact) -> JsonDict:
        """Convert an Artefact to a dict, removing None values."""
        d = clean_dict(asdict(artefact))
        if "flags" in d and isinstance(d["flags"], list):
            d["flags"] = cast(JsonValue, sorted(cast(list[str], d["flags"])))
        return d

    data = {"artefacts": [artefact_to_dict(a) for a in artefacts]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_artefact_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire artefact.txt file.

    Args:
        filepath: Path to artefact.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                artefact count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Artefact file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state. Maps id -> line number.
    ids_seen: dict[int, int] = {}

    prev_id = 0  # Artefacts start at 1
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

        # N: line - start of artefact entry
        if line.startswith("N:"):
            artefact_id = validate_n_line(line, lineno, result)
            if artefact_id is not None:
                # Check for duplicates
                if artefact_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {artefact_id} "
                        + f"(first seen at line {ids_seen[artefact_id]})"
                    )
                else:
                    ids_seen[artefact_id] = lineno

                # Check IDs are increasing
                if artefact_id <= prev_id:
                    result.error(
                        f"Line {lineno}: ID {artefact_id} is not greater than previous ID {prev_id} "
                        + "(IDs must be strictly increasing)"
                    )
                prev_id = artefact_id

                # Check ID against limit
                if limits and artefact_id > limits.max_artefact_id:
                    result.error(
                        f"Line {lineno}: Artefact ID {artefact_id} exceeds maximum allowed ID "
                        + f"{limits.max_artefact_id} (from limits.txt M:A)"
                    )
            continue

        # Other line types
        if line.startswith("G:"):
            validate_g_line(line, lineno, result)
        elif line.startswith("I:"):
            validate_i_line(line, lineno, result)
        elif line.startswith("B:"):
            validate_b_line(line, lineno, result)
        elif line.startswith("W:"):
            validate_w_line(line, lineno, result)
        elif line.startswith("P:"):
            validate_p_line(line, lineno, result)
        elif line.startswith("F:"):
            pass  # F: lines are free-form flags, no strict validation needed
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

    # Check total artefact count against limit
    if limits:
        artefact_count = len(ids_seen)
        if artefact_count > limits.total_artefacts:
            result.error(
                f"Total artefact count ({artefact_count}) exceeds maximum allowed "
                + f"({limits.total_artefacts}) from limits.txt M:A"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/artefact.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                         # Validate the file
  %(prog)s --validate lib/edit/artefact.txt   # Validate a specific file
  %(prog)s --export-json                      # Export to JSON (stdout)
  %(prog)s --export-json > artefacts.json     # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to artefact.txt file (default: lib/edit/artefact.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the artefact.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all artefact records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(artefact_file: Path, limits_file: Path) -> int:
    """Run validation on the artefact file."""
    print("=" * 60)
    print(f"Validating: {artefact_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max artefacts = {limits.total_artefacts} (max ID = {limits.max_artefact_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_artefact_file(artefact_file, limits)

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


def run_export_json(artefact_file: Path) -> int:
    """Export artefacts to JSON on stdout."""
    artefacts = parse_artefacts(artefact_file)

    if not artefacts:
        print(f"ERROR: No artefacts found in {artefact_file}", file=sys.stderr)
        return 1

    print(export_artefacts_to_json(artefacts))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        artefact_file = file_arg
    else:
        artefact_file = project_dir / "lib" / "edit" / "artefact.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(artefact_file, limits_file)

    if export_json:
        return run_export_json(artefact_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
