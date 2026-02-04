#!/usr/bin/env bash
#
# File: create-version-file.sh
#
# Creates the top-level version file with version information from
# src/defines.h and the current git hash.

set -euo pipefail

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(readlink -f "$SCRIPT_DIR/..")
readonly VERSION_FILE="$PROJECT_DIR/VERSION.txt"
readonly DEFINES_H="$PROJECT_DIR/src/defines.h"

if ! command -v git &>/dev/null; then
    echo "ERROR: 'git' command not found"
    exit 1
fi

if [[ ! -f "$DEFINES_H" ]]; then
    echo "ERROR: '$DEFINES_H' not found"
    exit 1
fi

# Read version information from src/defines.h
# shellcheck disable=SC2155
readonly VERSION_STRING="$(grep '#define VERSION_STRING' "$DEFINES_H" | sed 's/.*"\(.*\)".*/\1/')"
# shellcheck disable=SC2155
readonly VERSION_MAJOR="$(grep '#define VERSION_MAJOR' "$DEFINES_H" | awk '{print $3}')"
# shellcheck disable=SC2155
readonly VERSION_MINOR="$(grep '#define VERSION_MINOR' "$DEFINES_H" | awk '{print $3}')"
# shellcheck disable=SC2155
readonly VERSION_PATCH="$(grep '#define VERSION_PATCH' "$DEFINES_H" | awk '{print $3}')"
# shellcheck disable=SC2155
readonly VERSION_EXTRA="$(grep '#define VERSION_EXTRA' "$DEFINES_H" | awk '{print $3}')"

# shellcheck disable=SC2155
readonly GIT_CURRENT_HASH="$(git rev-parse --short HEAD)"

echo "Creating version file '$VERSION_FILE'"
echo "Sil-Q version $VERSION_STRING ($VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH.$VERSION_EXTRA)" >"$VERSION_FILE"
echo "git commit: $GIT_CURRENT_HASH" >>"$VERSION_FILE"

echo "OK"
