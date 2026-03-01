#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the vault.txt data file.

1. Validates the format and integrity of lib/edit/vault.txt.
2. Exports the vault records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: serial number : vault name
- X: room type : depth : rarity
- F: flag1 | flag2 | ...
- D: layout line (ASCII art defining the vault layout)

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

# Valid vault flags
VALID_FLAGS = {
    "TEST",
    "NO_ROTATION",
    "TRAPS",
    "WEBS",
    "LIGHT",
    "SURFACE",
}

# Valid room types
VALID_ROOM_TYPES = {
    6,   # interesting room
    7,   # lesser vault
    8,   # greater vault
    9,   # Morgoth's vault
    10,  # Gates of Angband
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

    max_vaults: int  # M:V value

    @property
    def max_vault_id(self) -> int:
        """Maximum valid vault ID (0-indexed, so max_vaults - 1)."""
        return self.max_vaults - 1


@dataclass
class Vault:
    """A vault record parsed from vault.txt."""

    id: int
    name: str
    room_type: int | None = None
    depth: int | None = None
    rarity: int | None = None
    flags: list[str] = field(default_factory=list)
    layout: list[str] = field(default_factory=list)


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_vaults = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:V:"):
            # M:V:500 - Maximum number of vaults
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_vaults = int(parts[2])

    if max_vaults is None:
        return None

    return Limits(max_vaults=max_vaults)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> int | None:
    """Validate N: line format and return the ID."""
    parts = line.split(":")
    if len(parts) < 3:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected at least 3: {line}")
        return None

    id_str = parts[1]
    if not id_str.isdigit():
        result.error(f"Line {lineno}: N: serial number is not numeric: {id_str}")
        return None

    return int(id_str)


def validate_x_line(line: str, lineno: int, result: ValidationResult) -> tuple[int, int, int] | None:
    """Validate X: line format and return (type, depth, rarity)."""
    parts = line.split(":")
    # X:type:depth:rarity
    if len(parts) != 4:
        result.error(f"Line {lineno}: X: line has {len(parts)} fields, expected 4: {line}")
        return None

    # Validate type
    type_str = parts[1]
    if not type_str.isdigit():
        result.error(f"Line {lineno}: X: room type is not numeric: {type_str}")
        return None
    room_type = int(type_str)
    if room_type not in VALID_ROOM_TYPES:
        result.warning(f"Line {lineno}: X: unknown room type {room_type}")

    # Validate depth
    depth_str = parts[2]
    if not depth_str.isdigit():
        result.error(f"Line {lineno}: X: depth is not numeric: {depth_str}")
        return None
    depth = int(depth_str)

    # Validate rarity
    rarity_str = parts[3]
    if not rarity_str.isdigit():
        result.error(f"Line {lineno}: X: rarity is not numeric: {rarity_str}")
        return None
    rarity = int(rarity_str)

    return (room_type, depth, rarity)


def validate_f_line(line: str, lineno: int, result: ValidationResult) -> list[str]:
    """Validate F: line format and return list of flags."""
    content = line[2:].strip()
    if not content:
        return []

    # Flags can be separated by | or spaces
    if "|" in content:
        flags = [f.strip() for f in content.split("|") if f.strip()]
    else:
        flags = content.split()

    for flag in flags:
        if flag not in VALID_FLAGS:
            result.warning(f"Line {lineno}: F: unknown flag '{flag}'")

    return flags


def validate_layout_dimensions(
    vault_id: int,
    vault_name: str,
    layout: list[str],
    result: ValidationResult,
) -> None:
    """Validate that all layout lines have consistent width."""
    if not layout:
        return

    line_widths = [len(line) for line in layout]

    # Check that all D: lines have consistent width
    if len(set(line_widths)) > 1:
        min_width = min(line_widths)
        max_width = max(line_widths)
        result.error(
            f"Vault {vault_id} ({vault_name}): layout has inconsistent line widths "
            f"(min={min_width}, max={max_width})"
        )


def parse_vaults(filepath: Path) -> list[Vault]:
    """Parse vault.txt and return a list of Vault objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    vaults: list[Vault] = []
    current_vault: Vault | None = None

    for line in lines:
        # Don't strip - preserve whitespace for D: lines
        stripped = line.strip()

        # Skip empty lines and comments
        if not stripped or stripped.startswith("#") or stripped.startswith("V:"):
            continue

        # N: line - start of new vault
        if stripped.startswith("N:"):
            # Save previous vault
            if current_vault is not None:
                vaults.append(current_vault)

            parts = stripped.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_vault = Vault(id=int(parts[1]), name=":".join(parts[2:]))
            continue

        if current_vault is None:
            continue

        # X: line
        if stripped.startswith("X:"):
            parts = stripped.split(":")
            if len(parts) >= 4:
                try:
                    current_vault.room_type = int(parts[1])
                    current_vault.depth = int(parts[2])
                    current_vault.rarity = int(parts[3])
                except ValueError:
                    pass

        # F: flags
        elif stripped.startswith("F:"):
            content = stripped[2:].strip()
            if "|" in content:
                current_vault.flags = [f.strip() for f in content.split("|") if f.strip()]
            else:
                current_vault.flags = content.split()

        # D: layout line - preserve exact content after "D:"
        elif stripped.startswith("D:"):
            # Use original line to preserve whitespace, just remove "D:" prefix
            if line.startswith("D:"):
                current_vault.layout.append(line[2:])
            else:
                # Handle case where line might have leading whitespace before D:
                idx = line.index("D:")
                current_vault.layout.append(line[idx + 2:])

    if current_vault is not None:
        vaults.append(current_vault)

    return vaults


def export_vaults_to_json(vaults: list[Vault]) -> str:
    """Export vaults to a JSON string, preserving exact layout whitespace."""

    def vault_to_dict(vault: Vault) -> JsonDict:
        """Convert a Vault to a dict."""
        d: JsonDict = {
            "id": vault.id,
            "name": vault.name,
        }
        if vault.room_type is not None:
            d["room_type"] = vault.room_type
        if vault.depth is not None:
            d["depth"] = vault.depth
        if vault.rarity is not None:
            d["rarity"] = vault.rarity
        if vault.flags:
            d["flags"] = vault.flags
        if vault.layout:
            d["layout"] = vault.layout
        return d

    data = {"vaults": [vault_to_dict(v) for v in vaults]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_vault_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire vault.txt file.

    Args:
        filepath: Path to vault.txt file.
        limits: Optional limits from limits.txt.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Vault file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state
    ids_seen: dict[int, int] = {}  # Maps id -> line number
    prev_id = -1
    has_version = False

    # Current vault tracking for layout validation
    current_vault_id: int | None = None
    current_vault_name: str = ""
    current_layout: list[str] = []

    for lineno, line in enumerate(lines, start=1):
        stripped = line.strip()

        # Skip empty lines and comments
        if not stripped or stripped.startswith("#"):
            continue

        # Version stamp
        if stripped.startswith("V:"):
            has_version = True
            continue

        # N: line - start of vault entry
        if stripped.startswith("N:"):
            # Validate previous vault's layout consistency
            if current_vault_id is not None and current_layout:
                validate_layout_dimensions(
                    current_vault_id,
                    current_vault_name,
                    current_layout,
                    result,
                )

            vault_id = validate_n_line(stripped, lineno, result)
            if vault_id is not None:
                # Check for duplicates
                if vault_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {vault_id} "
                        + f"(first seen at line {ids_seen[vault_id]})"
                    )
                else:
                    ids_seen[vault_id] = lineno

                # Check IDs are increasing
                if vault_id <= prev_id:
                    result.warning(
                        f"Line {lineno}: ID {vault_id} is not greater than previous ID {prev_id}"
                    )
                prev_id = vault_id

                # Check ID against limit
                if limits and vault_id > limits.max_vault_id:
                    result.error(
                        f"Line {lineno}: Vault ID {vault_id} exceeds maximum allowed ID "
                        + f"{limits.max_vault_id} (from limits.txt M:V:{limits.max_vaults})"
                    )

                # Reset for new vault
                current_vault_id = vault_id
                parts = stripped.split(":")
                current_vault_name = ":".join(parts[2:]) if len(parts) >= 3 else ""
                current_layout = []
            continue

        # X: line
        if stripped.startswith("X:"):
            _ = validate_x_line(stripped, lineno, result)
            continue

        # F: line
        if stripped.startswith("F:"):
            _ = validate_f_line(stripped, lineno, result)
            continue

        # D: line
        if stripped.startswith("D:"):
            # Preserve exact content after D:
            if line.startswith("D:"):
                current_layout.append(line[2:])
            else:
                idx = line.index("D:")
                current_layout.append(line[idx + 2:])
            continue

        # Unknown line types
        if len(stripped) >= 2 and stripped[1] == ":":
            result.error(f"Line {lineno}: Unknown line type '{stripped[0]}:' in line '{stripped}'")
        else:
            result.error(
                f"Line {lineno}: Unrecognized line (missing '#' comment marker?): '{stripped}'"
            )

    # Validate last vault's layout
    if current_vault_id is not None and current_layout:
        validate_layout_dimensions(
            current_vault_id,
            current_vault_name,
            current_layout,
            result,
        )

    # Check for required version stamp
    if not has_version:
        result.error("Missing required version stamp (V: line)")

    # Check total vault count against limit
    if limits:
        vault_count = len(ids_seen)
        if vault_count > limits.max_vaults:
            result.error(
                f"Total vault count ({vault_count}) exceeds maximum allowed "
                + f"({limits.max_vaults}) from limits.txt M:V"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/vault.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                     # Validate the file
  %(prog)s --validate lib/edit/vault.txt  # Validate a specific file
  %(prog)s --export-json                  # Export to JSON (stdout)
  %(prog)s --export-json > vaults.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to vault.txt file (default: lib/edit/vault.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the vault.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all vault records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(vault_file: Path, limits_file: Path) -> int:
    """Run validation on the vault file."""
    print("=" * 60)
    print(f"Validating: {vault_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max vaults = {limits.max_vaults} (max ID = {limits.max_vault_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_vault_file(vault_file, limits)

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


def run_export_json(vault_file: Path) -> int:
    """Export vaults to JSON on stdout."""
    vaults = parse_vaults(vault_file)

    if not vaults:
        print(f"ERROR: No vaults found in {vault_file}", file=sys.stderr)
        return 1

    print(export_vaults_to_json(vaults))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        vault_file = file_arg
    else:
        vault_file = project_dir / "lib" / "edit" / "vault.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(vault_file, limits_file)

    if export_json:
        return run_export_json(vault_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
