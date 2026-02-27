#!/usr/bin/env bash
#
# File: smoke-tests.sh
#
# Runs basic "smoke" tests to verify that a generated `sil` binary works as
# expected.
#
# Only Linux and (non-Cocoa) macOS builds are currently supported.

set -euo pipefail

###############################################################################
# Configuration
###############################################################################

# shellcheck disable=SC2155
readonly SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
# shellcheck disable=SC2155
readonly PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd -P)
readonly DEFINES_H="$PROJECT_DIR/src/defines.h"
readonly SIL_BINARY="$PROJECT_DIR/sil"
readonly CMAKE_FILE="$PROJECT_DIR/CMakeLists.txt"

###############################################################################
# Test 1: CMakeLists.txt full version matches src/defines.h "fallback" ?
###############################################################################

# Extract version from CMakeLists.txt project() directive (may be multi-line)
cmake_project_version=$(grep -A3 'project(Sil' "$CMAKE_FILE" | sed -nE 's/.*VERSION[[:space:]]+([0-9.]+).*/\1/p')
if [[ -z "$cmake_project_version" ]]; then
    echo "[ERROR] Could not read project VERSION from $CMAKE_FILE" >&2
    exit 1
fi
# Extract version suffix from CMakeLists.txt (suffix may be empty)
cmake_version_suffix=$(grep -E 'set\s*\(\s*PROJECT_VERSION_SUFFIX' "$CMAKE_FILE" | sed -nE 's/.*"(.*)".*/\1/p')
# Combine to get full CMake version string
cmake_version_string="${cmake_project_version}${cmake_version_suffix}"

# Get expected version from src/defines.h
# shellcheck disable=SC2155
readonly defines_version_string=$(grep '#define VERSION_STRING' "$DEFINES_H" | sed -E 's/.*"(.*)".*/\1/')
if [[ -z "$defines_version_string" ]]; then
    echo "[ERROR] Could not read VERSION_STRING from $DEFINES_H" >&2
    exit 1
fi

# Compare CMakeLists.txt version against defines.h
if [[ "$cmake_version_string" != "$defines_version_string" ]]; then
    echo "[ERROR] Version mismatch between CMakeLists.txt and defines.h" >&2
    echo -e "  CMakeLists.txt: $cmake_version_string" >&2
    echo -e "  defines.h:      $defines_version_string" >&2
    exit 1
fi

###############################################################################
# Test 2: CMakeLists.txt version components match src/defines.h "fallback" ?
###############################################################################

# Split cmake_project_version (e.g., "1.5.1.0") into components
IFS='.' read -r cmake_major cmake_minor cmake_patch cmake_extra <<<"$cmake_project_version"

# Extract individual version constants from defines.h
defines_major=$(grep '#define VERSION_MAJOR' "$DEFINES_H" | sed -E 's/.*VERSION_MAJOR[[:space:]]+([0-9]+).*/\1/')
defines_minor=$(grep '#define VERSION_MINOR' "$DEFINES_H" | sed -E 's/.*VERSION_MINOR[[:space:]]+([0-9]+).*/\1/')
defines_patch=$(grep '#define VERSION_PATCH' "$DEFINES_H" | sed -E 's/.*VERSION_PATCH[[:space:]]+([0-9]+).*/\1/')
defines_extra=$(grep '#define VERSION_EXTRA' "$DEFINES_H" | sed -E 's/.*VERSION_EXTRA[[:space:]]+([0-9]+).*/\1/')

# Compare individual version components
version_mismatch=0
if [[ "$cmake_major" != "$defines_major" ]]; then
    echo "[ERROR] VERSION_MAJOR mismatch: CMakeLists.txt=$cmake_major, defines.h=$defines_major" >&2
    version_mismatch=1
fi
if [[ "$cmake_minor" != "$defines_minor" ]]; then
    echo "[ERROR] VERSION_MINOR mismatch: CMakeLists.txt=$cmake_minor, defines.h=$defines_minor" >&2
    version_mismatch=1
fi
if [[ "$cmake_patch" != "$defines_patch" ]]; then
    echo "[ERROR] VERSION_PATCH mismatch: CMakeLists.txt=$cmake_patch, defines.h=$defines_patch" >&2
    version_mismatch=1
fi
if [[ "$cmake_extra" != "$defines_extra" ]]; then
    echo "[ERROR] VERSION_EXTRA mismatch: CMakeLists.txt=$cmake_extra, defines.h=$defines_extra" >&2
    version_mismatch=1
fi
if [[ "$version_mismatch" -eq 1 ]]; then
    exit 1
fi

###############################################################################
# Test 3: `sil -v` version matches CMakeLists.txt ?
#         (also verifies whether the `sil` executable can run at all)
###############################################################################

# Get actual version by running sil and capturing its version output.
readonly print_version_command="$SIL_BINARY -v"
# shellcheck disable=SC2155
readonly print_version_command_output=$(eval "$print_version_command")
readonly exe_version_string=${print_version_command_output#"Sil-Q version "}

if [[ "$exe_version_string" != "$cmake_version_string" ]]; then
    echo "[ERROR] Version mismatch between CMakeLists.txt and sil binary" >&2
    echo -e "  CMakeLists.txt:  $cmake_version_string" >&2
    echo -e "  sil -v:          $exe_version_string" >&2
    exit 1
fi

# All tests passed.
echo "[SUCCESS] All tests passed"
exit 0
