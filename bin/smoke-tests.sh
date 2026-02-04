#!/usr/bin/env bash
#
# File: smoke-tests.sh
#
# Runs basic "smoke" tests to verify that a generated `sil` binary works as
# expected.
#
# Only Linux and (non-Cocoa) macOS builds are currently supported.

set -euo pipefail

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(readlink -f "$SCRIPT_DIR/..")
readonly DEFINES_H="$PROJECT_DIR/src/defines.h"
readonly SIL_BINARY="$PROJECT_DIR/sil"

###
### Test 1: Check the `sil -v` version info against src/defines.h
###         (also verifies whether the `sil` executable can run at all)
###

# Get expected version from src/defines.h.
# shellcheck disable=SC2155
readonly expected_version_string=$(grep '#define VERSION_STRING' "$DEFINES_H" | sed 's/.*"\(.*\)"/\1/')
if [[ -z "$expected_version_string" ]]; then
    echo "[ERROR] Could not read VERSION_STRING from $DEFINES_H" >&2
    exit 1
fi

# Get actual version by running sil and capturing its version output.
readonly print_version_command="$SIL_BINARY -v"
# shellcheck disable=SC2155
readonly print_version_command_output=$(eval "$print_version_command")
readonly exe_version_string=${print_version_command_output#"Sil-Q version "}

if [[ "$exe_version_string" != "$expected_version_string" ]]; then
    echo "[ERROR] Version mismatch" >&2
    echo -e "  Expected: $expected_version_string\t(from $DEFINES_H)" >&2
    echo -e "  Actual:   $exe_version_string\t(from $print_version_command)" >&2
    exit 1
fi

# All tests passed.
echo "[SUCCESS] All tests passed"
exit 0
