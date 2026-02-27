#!/usr/bin/env bash
#
# File: verify-tutorial.sh
#
# Verifies that the tutorial savefile's version header matches the expected
# game version from src/defines.h.

set -euo pipefail

###############################################################################
# Configuration
###############################################################################

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd -P)
readonly DEFINES_H="$PROJECT_DIR/src/defines.h"
readonly TUTORIAL_SAVEFILE="$PROJECT_DIR/lib/xtra/tutorial"

###############################################################################
# Verify the tutorial savefile
###############################################################################

# Get expected a.b.c.d version from src/defines.h.
expected_major=$(grep '#define VERSION_MAJOR ' "$DEFINES_H" | sed 's/.*MAJOR[[:space:]]*//')
expected_minor=$(grep '#define VERSION_MINOR ' "$DEFINES_H" | sed 's/.*MINOR[[:space:]]*//')
expected_patch=$(grep '#define VERSION_PATCH ' "$DEFINES_H" | sed 's/.*PATCH[[:space:]]*//')
expected_extra=$(grep '#define VERSION_EXTRA ' "$DEFINES_H" | sed 's/.*EXTRA[[:space:]]*//')
expected_version="${expected_major}.${expected_minor}.${expected_patch}.${expected_extra}"

# Get actual a.b.c.d version from tutorial savefile.
# shellcheck disable=SC2155
readonly actual_version=$("$SCRIPT_DIR/savefile.sh" "$TUTORIAL_SAVEFILE" | sed -n 's/.*: \([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*\).*/\1/p')

echo "Tutorial savefile version: expected=$expected_version, actual=$actual_version"

if [ "$expected_version" != "$actual_version" ]; then
    echo "ERROR: Tutorial savefile version mismatch (expected $expected_version, was $actual_version)" >&2
    exit 1
fi

echo "OK"
