#!/usr/bin/env python3
#
# SPDX-License-Identifier: EUPL-1.2
#
# Copyright (c) 2026 Fernando Carmona Varo
#
"""Validate Sil-Quest dialogue data files.

The dialogue format is intentionally small and semantic.  It is meant to back
future Morrowind-style NPC conversations while remaining usable from both the
terminal UI and the mobile/web frontend.
"""

from __future__ import annotations

import argparse
import json
import re
import signal
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

if hasattr(signal, "SIGPIPE"):
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)


PROJECT_DIR = Path(__file__).resolve().parents[1]
DEFAULT_REGISTRY = PROJECT_DIR / "lib" / "edit" / "dialogue.txt"

ID_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_/-]*$")
FLAG_RE = re.compile(r"^!?flag\.[A-Za-z][A-Za-z0-9_/-]*$")


@dataclass
class ValidationResult:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        return not self.errors and not self.warnings

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warning(self, message: str) -> None:
        self.warnings.append(message)


@dataclass
class Option:
    label: str
    target: str
    conditions: list[str] = field(default_factory=list)
    line: int = 0


@dataclass
class Node:
    node_id: str
    text: list[str] = field(default_factory=list)
    conditions: list[str] = field(default_factory=list)
    effects: list[str] = field(default_factory=list)
    options: list[Option] = field(default_factory=list)
    line: int = 0


@dataclass
class Dialogue:
    dialogue_id: str
    npc: str | None = None
    start: str | None = None
    nodes: dict[str, Node] = field(default_factory=dict)
    source: str = ""


@dataclass
class RegistryEntry:
    index: int
    dialogue_id: str
    description: str | None = None
    source: str | None = None


def clean_line(line: str) -> str:
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return ""
    return stripped


def parse_conditions(spec: str, result: ValidationResult, source: Path, line: int) -> list[str]:
    conditions = [part.strip() for part in spec.split(",") if part.strip()]
    for condition in conditions:
        if not FLAG_RE.match(condition):
            result.error(
                f"{source}:{line}: invalid condition '{condition}' "
                "(expected flag.name or !flag.name)"
            )
    return conditions


def parse_effect(spec: str, result: ValidationResult, source: Path, line: int) -> str | None:
    if "=" not in spec:
        result.error(f"{source}:{line}: malformed EFFECT entry: {spec}")
        return None

    kind, value = [part.strip() for part in spec.split("=", 1)]
    if kind not in {"SET_FLAG", "CLEAR_FLAG"}:
        result.error(f"{source}:{line}: unsupported EFFECT kind '{kind}'")
        return None

    if not ID_RE.match(value):
        result.error(f"{source}:{line}: invalid effect flag name '{value}'")
        return None

    return f"{kind}={value}"


def parse_option(line_text: str, result: ValidationResult, source: Path, line: int) -> Option | None:
    parts = line_text.split(":")
    if len(parts) < 3:
        result.error(f"{source}:{line}: OPTION requires label and target")
        return None

    label = parts[1].strip()
    target = parts[2].strip()
    if not label:
        result.error(f"{source}:{line}: OPTION label cannot be empty")
    if target != "END" and not ID_RE.match(target):
        result.error(f"{source}:{line}: invalid OPTION target '{target}'")

    option = Option(label=label, target=target, line=line)
    for modifier in parts[3:]:
        modifier = modifier.strip()
        if modifier.startswith("WHEN="):
            option.conditions.extend(parse_conditions(modifier[5:], result, source, line))
        else:
            result.error(f"{source}:{line}: unsupported OPTION modifier '{modifier}'")

    return option


def parse_dialogue_file(path: Path, result: ValidationResult) -> Dialogue | None:
    if not path.exists():
        result.error(f"{path}: dialogue file not found")
        return None

    dialogue: Dialogue | None = None
    current_node: Node | None = None
    seen_version = False

    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = clean_line(raw)
        if not line:
            continue

        if line.startswith("V:"):
            version = line[2:].strip()
            if version != "1":
                result.error(f"{path}:{line_number}: unsupported dialogue format version '{version}'")
            seen_version = True
            continue

        if line.startswith("DIALOG:"):
            dialogue_id = line[7:].strip()
            if dialogue is not None:
                result.error(f"{path}:{line_number}: duplicate DIALOG directive")
            if not ID_RE.match(dialogue_id):
                result.error(f"{path}:{line_number}: invalid dialogue id '{dialogue_id}'")
            dialogue = Dialogue(dialogue_id=dialogue_id, source=str(path.relative_to(PROJECT_DIR)))
            current_node = None
            continue

        if dialogue is None:
            result.error(f"{path}:{line_number}: directive before DIALOG")
            continue

        if line.startswith("NPC:"):
            dialogue.npc = line[4:].strip()
            if not dialogue.npc:
                result.error(f"{path}:{line_number}: NPC cannot be empty")
        elif line.startswith("START:"):
            dialogue.start = line[6:].strip()
            if not ID_RE.match(dialogue.start):
                result.error(f"{path}:{line_number}: invalid START node '{dialogue.start}'")
        elif line.startswith("NODE:"):
            node_id = line[5:].strip()
            if not ID_RE.match(node_id):
                result.error(f"{path}:{line_number}: invalid NODE id '{node_id}'")
            if node_id in dialogue.nodes:
                result.error(f"{path}:{line_number}: duplicate NODE '{node_id}'")
            current_node = Node(node_id=node_id, line=line_number)
            dialogue.nodes[node_id] = current_node
        elif line.startswith("WHEN:"):
            if current_node is None:
                result.error(f"{path}:{line_number}: WHEN requires an active NODE")
            else:
                current_node.conditions.extend(parse_conditions(line[5:].strip(), result, path, line_number))
        elif line.startswith("TEXT:"):
            if current_node is None:
                result.error(f"{path}:{line_number}: TEXT requires an active NODE")
            else:
                current_node.text.append(line[5:].strip())
        elif line.startswith("OPTION:"):
            if current_node is None:
                result.error(f"{path}:{line_number}: OPTION requires an active NODE")
            else:
                option = parse_option(line, result, path, line_number)
                if option is not None:
                    current_node.options.append(option)
        elif line.startswith("EFFECT:"):
            if current_node is None:
                result.error(f"{path}:{line_number}: EFFECT requires an active NODE")
            else:
                effect = parse_effect(line[7:].strip(), result, path, line_number)
                if effect is not None:
                    current_node.effects.append(effect)
        else:
            result.error(f"{path}:{line_number}: unknown dialogue directive '{line}'")

    if not seen_version:
        result.error(f"{path}: missing V: format version")
    if dialogue is None:
        result.error(f"{path}: missing DIALOG directive")
        return None

    if not dialogue.npc:
        result.error(f"{path}: missing NPC directive")
    if not dialogue.start:
        result.error(f"{path}: missing START directive")
    elif dialogue.start not in dialogue.nodes:
        result.error(f"{path}: START node '{dialogue.start}' is not defined")

    for node in dialogue.nodes.values():
        if not node.text:
            result.error(f"{path}:{node.line}: NODE '{node.node_id}' has no TEXT")
        if not node.options:
            result.warning(f"{path}:{node.line}: NODE '{node.node_id}' has no OPTION")
        for option in node.options:
            if option.target != "END" and option.target not in dialogue.nodes:
                result.error(
                    f"{path}:{option.line}: OPTION target '{option.target}' "
                    f"is not a NODE in dialogue '{dialogue.dialogue_id}'"
                )

    return dialogue


def parse_registry(path: Path, result: ValidationResult) -> list[RegistryEntry]:
    if not path.exists():
        result.error(f"{path}: dialogue registry not found")
        return []

    entries: list[RegistryEntry] = []
    current: RegistryEntry | None = None
    seen_version = False
    seen_ids: set[str] = set()
    seen_indexes: set[int] = set()

    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = clean_line(raw)
        if not line:
            continue

        if line.startswith("V:"):
            version = line[2:].strip()
            if version != "1":
                result.error(f"{path}:{line_number}: unsupported dialogue registry version '{version}'")
            seen_version = True
        elif line.startswith("N:"):
            parts = line.split(":", 2)
            if len(parts) != 3 or not parts[1].isdigit():
                result.error(f"{path}:{line_number}: malformed N record")
                current = None
                continue
            index = int(parts[1])
            dialogue_id = parts[2].strip()
            if index in seen_indexes:
                result.error(f"{path}:{line_number}: duplicate dialogue index {index}")
            if dialogue_id in seen_ids:
                result.error(f"{path}:{line_number}: duplicate dialogue id '{dialogue_id}'")
            if not ID_RE.match(dialogue_id):
                result.error(f"{path}:{line_number}: invalid dialogue id '{dialogue_id}'")
            seen_indexes.add(index)
            seen_ids.add(dialogue_id)
            current = RegistryEntry(index=index, dialogue_id=dialogue_id)
            entries.append(current)
        elif line.startswith("D:"):
            if current is None:
                result.error(f"{path}:{line_number}: D record before N record")
            else:
                current.description = line[2:].strip()
        elif line.startswith("S:"):
            if current is None:
                result.error(f"{path}:{line_number}: S record before N record")
            else:
                current.source = line[2:].strip()
        else:
            result.error(f"{path}:{line_number}: unknown registry directive '{line}'")

    if not seen_version:
        result.error(f"{path}: missing V: format version")

    for entry in entries:
        if not entry.description:
            result.error(f"{path}: dialogue '{entry.dialogue_id}' is missing a D record")
        if not entry.source:
            result.error(f"{path}: dialogue '{entry.dialogue_id}' is missing an S record")

    return entries


def validate(registry_path: Path) -> tuple[ValidationResult, list[Dialogue]]:
    result = ValidationResult()
    entries = parse_registry(registry_path, result)
    dialogues: list[Dialogue] = []

    for entry in entries:
        if not entry.source:
            continue
        source_path = PROJECT_DIR / "lib" / "edit" / entry.source
        dialogue = parse_dialogue_file(source_path, result)
        if dialogue is None:
            continue
        if dialogue.dialogue_id != entry.dialogue_id:
            result.error(
                f"{source_path}: DIALOG id '{dialogue.dialogue_id}' does not match "
                f"registry id '{entry.dialogue_id}'"
            )
        dialogues.append(dialogue)

    return result, dialogues


def print_result(result: ValidationResult) -> None:
    for warning in result.warnings:
        print(f"WARNING: {warning}", file=sys.stderr)
    for error in result.errors:
        print(f"ERROR: {error}", file=sys.stderr)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validate", action="store_true", help="validate dialogue data")
    parser.add_argument("--json", action="store_true", help="export parsed dialogue data as JSON")
    parser.add_argument(
        "--file",
        type=Path,
        default=DEFAULT_REGISTRY,
        help=f"dialogue registry to read (default: {DEFAULT_REGISTRY})",
    )
    args = parser.parse_args(argv)

    if not args.validate and not args.json:
        parser.error("choose --validate or --json")

    result, dialogues = validate(args.file)

    if args.json:
        payload: list[dict[str, Any]] = [asdict(dialogue) for dialogue in dialogues]
        print(json.dumps(payload, indent=2, ensure_ascii=False))

    if args.validate:
        print_result(result)
        if result.is_valid:
            print(f"OK: validated {len(dialogues)} dialogue file(s)")
            return 0
        return 1

    return 0 if result.is_valid else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
