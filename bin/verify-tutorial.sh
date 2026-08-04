#!/usr/bin/env bash
#
# File: verify-tutorial.sh
#
# Verifies the authored tutorial scenario data used by CI.

set -euo pipefail

###############################################################################
# Configuration
###############################################################################

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd -P)
readonly QUEST_FILE="$PROJECT_DIR/lib/edit/quest.txt"
readonly TUTORIAL_SCENARIO_REL="quests/tutorial_scenario.txt"
readonly TUTORIAL_SCENARIO="$PROJECT_DIR/lib/edit/$TUTORIAL_SCENARIO_REL"

###############################################################################
# Verify the tutorial scenario
###############################################################################

if [[ ! -f "$QUEST_FILE" ]]; then
    echo "ERROR: quest definition file not found: $QUEST_FILE" >&2
    exit 1
fi

if [[ ! -f "$TUTORIAL_SCENARIO" ]]; then
    echo "ERROR: tutorial scenario file not found: $TUTORIAL_SCENARIO" >&2
    exit 1
fi

required_quest_patterns=(
    '^N:1:Tutorial$'
    '^G:-1$'
    "^S:SCENARIO:$TUTORIAL_SCENARIO_REL$"
)

for pattern in "${required_quest_patterns[@]}"; do
    if ! grep -Eq "$pattern" "$QUEST_FILE"; then
        echo "ERROR: tutorial quest definition missing pattern: $pattern" >&2
        exit 1
    fi
done

required_scenario_patterns=(
    '^V:[0-9]+$'
    '^P:NAME:Laurilfea$'
    '^P:RACE:Noldor$'
    '^P:HOUSE:House of Finarfin$'
    '^MAP:Tutorial$'
    '^MAP_SIZE:[0-9]+:[0-9]+$'
    '^E:[0-9]+:[0-9]+:PLAYER'
)

for pattern in "${required_scenario_patterns[@]}"; do
    if ! grep -Eq "$pattern" "$TUTORIAL_SCENARIO"; then
        echo "ERROR: tutorial scenario missing pattern: $pattern" >&2
        exit 1
    fi
done

if grep -Eq '^S:SAVEFILE:' "$QUEST_FILE"; then
    echo "ERROR: tutorial should use scenario data, not a legacy savefile" >&2
    exit 1
fi

echo "Tutorial scenario: $TUTORIAL_SCENARIO_REL"
echo "OK"
