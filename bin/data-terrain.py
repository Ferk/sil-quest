#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the terrain.txt data file.

1. Validates the format and integrity of lib/edit/terrain.txt.
2. Exports the terrain records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: serial number : terrain name
- G: symbol : color
- M: feature to mimic

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
VALID_EXTENDED_COLORS = {"G1", "v1", "B1", "U1", "D1", "W1", "y1"}
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

    max_terrain_features: int  # M:F value

    @property
    def max_terrain_id(self) -> int:
        """Maximum valid terrain ID (0-indexed, so max_terrain_features - 1)."""
        return self.max_terrain_features - 1


@dataclass
class Terrain:
    """A terrain record parsed from terrain.txt."""

    id: int
    name: str
    symbol: str | None = None
    color: str | None = None
    mimic: int | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_terrain_features = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:F:"):
            # M:F:86 - Maximum number of terrain features
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_terrain_features = int(parts[2])

    if max_terrain_features is None:
        return None

    return Limits(max_terrain_features=max_terrain_features)


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
    """Validate G: line format (symbol : color).

    Note: The symbol can be a colon itself, so G:::s means symbol=':' color='s'.
    """
    # Handle special case where symbol is a colon: G:::color
    if line.startswith("G::"):
        # Symbol is ':', color is everything after the third colon
        if len(line) >= 4 and line[3] == ":":
            color = line[4:]
        else:
            result.error(f"Line {lineno}: G: malformed line with colon symbol: {line}")
            return
    else:
        parts = line.split(":")
        if len(parts) != 3:
            result.error(f"Line {lineno}: G: line has {len(parts)} fields, expected 3: {line}")
            return
        color = parts[2]

    if color not in VALID_COLORS:
        result.error(f"Line {lineno}: G: unrecognized color '{color}'")


def validate_m_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate M: line format (feature to mimic)."""
    parts = line.split(":")
    if len(parts) != 2:
        result.error(f"Line {lineno}: M: line has {len(parts)} fields, expected 2: {line}")
        return

    mimic_str = parts[1]
    if not mimic_str.isdigit():
        result.error(f"Line {lineno}: M: mimic ID is not numeric: {mimic_str}")


def parse_terrains(filepath: Path) -> list[Terrain]:
    """Parse terrain.txt and return a list of Terrain objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    terrains: list[Terrain] = []
    current_terrain: Terrain | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new terrain
        if line.startswith("N:"):
            # Save previous terrain
            if current_terrain is not None:
                terrains.append(current_terrain)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_terrain = Terrain(id=int(parts[1]), name=parts[2])
            continue

        if current_terrain is None:
            continue

        # G: symbol : color
        # Note: Symbol can be a colon, so G:::s means symbol=':' color='s'
        if line.startswith("G:"):
            if line.startswith("G::") and len(line) >= 4 and line[3] == ":":
                # Symbol is ':'
                current_terrain.symbol = ":"
                current_terrain.color = line[4:]
            else:
                parts = line.split(":")
                if len(parts) >= 3:
                    current_terrain.symbol = parts[1]
                    current_terrain.color = parts[2]

        # M: mimic
        elif line.startswith("M:"):
            parts = line.split(":")
            if len(parts) >= 2 and parts[1].isdigit():
                current_terrain.mimic = int(parts[1])

    if current_terrain is not None:
        terrains.append(current_terrain)

    return terrains


def export_terrains_to_json(terrains: list[Terrain]) -> str:
    """Export terrains to a JSON string."""

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

    def terrain_to_dict(terrain: Terrain) -> JsonDict:
        """Convert a Terrain to a dict, removing None values."""
        return clean_dict(asdict(terrain))

    data = {"terrains": [terrain_to_dict(t) for t in terrains]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_terrain_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire terrain.txt file.

    Args:
        filepath: Path to terrain.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                terrain count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Terrain file not found: {filepath}")
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

        # N: line - start of terrain entry
        if line.startswith("N:"):
            terrain_id = validate_n_line(line, lineno, result)
            if terrain_id is not None:
                # Check for duplicates
                if terrain_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {terrain_id} "
                        + f"(first seen at line {ids_seen[terrain_id]})"
                    )
                else:
                    ids_seen[terrain_id] = lineno

                # Check IDs are increasing
                if terrain_id <= prev_id:
                    result.error(
                        f"Line {lineno}: ID {terrain_id} is not greater than previous ID {prev_id} "
                        + "(IDs must be strictly increasing)"
                    )
                prev_id = terrain_id

                # Check ID against limit
                if limits and terrain_id > limits.max_terrain_id:
                    result.error(
                        f"Line {lineno}: Terrain ID {terrain_id} exceeds maximum allowed ID "
                        + f"{limits.max_terrain_id} (from limits.txt M:F:{limits.max_terrain_features})"
                    )
            continue

        # Other line types
        if line.startswith("G:"):
            validate_g_line(line, lineno, result)
        elif line.startswith("M:"):
            validate_m_line(line, lineno, result)
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

    # Check total terrain count against limit
    if limits:
        terrain_count = len(ids_seen)
        if terrain_count > limits.max_terrain_features:
            result.error(
                f"Total terrain count ({terrain_count}) exceeds maximum allowed "
                + f"({limits.max_terrain_features}) from limits.txt M:F"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/terrain.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                       # Validate the file
  %(prog)s --validate lib/edit/terrain.txt  # Validate a specific file
  %(prog)s --export-json                    # Export to JSON (stdout)
  %(prog)s --export-json > terrains.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to terrain.txt file (default: lib/edit/terrain.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the terrain.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all terrain records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(terrain_file: Path, limits_file: Path) -> int:
    """Run validation on the terrain file."""
    print("=" * 60)
    print(f"Validating: {terrain_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max terrain features = {limits.max_terrain_features} (max ID = {limits.max_terrain_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_terrain_file(terrain_file, limits)

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


def run_export_json(terrain_file: Path) -> int:
    """Export terrains to JSON on stdout."""
    terrains = parse_terrains(terrain_file)

    if not terrains:
        print(f"ERROR: No terrains found in {terrain_file}", file=sys.stderr)
        return 1

    print(export_terrains_to_json(terrains))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        terrain_file = file_arg
    else:
        terrain_file = project_dir / "lib" / "edit" / "terrain.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(terrain_file, limits_file)

    if export_json:
        return run_export_json(terrain_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
