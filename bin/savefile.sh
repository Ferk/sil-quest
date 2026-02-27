#!/usr/bin/env bash
#
# File: savefile.sh
#
# Reads or writes the game version in the file header of a Sil-Q savefile.

set -euo pipefail

###############################################################################
# Usage help
###############################################################################

usage() {
    echo "Usage: $(basename "$0") [-h|--help] <file> [<version>]"
    echo
    echo "Reads or writes the game version of a Sil-Q savefile stored in its"
    echo "file header (first four bytes)."
    echo
    echo "  -h, --help  Print this usage help and quit"
    echo "  <file>      Path to the savefile"
    echo "  <version>   Set (write) the version header, provided as a string in"
    echo "              MAJOR.MINOR.PATCH.EXTRA format, corresponding to"
    echo "              VERSION_{MAJOR,MINOR,PATCH,EXTRA} in src/defines.h)."
    echo "              Each component must be an integer in the range 0-255."
    echo "              If omitted, the current version is printed."
    echo
    echo "              WARNING: Modifying a savefile's version is a dangerous"
    echo "              operation. It is YOUR responsibility to ensure that the"
    echo "              modified savefile can be read by that Sil-Q version."
    echo
    echo "Examples:"
    echo
    echo "  # Print current version"
    echo "  $(basename "$0") /path/to/savefile"
    echo
    echo "  # Change version to 1.2.3.4"
    echo "  $(basename "$0") /path/to/savefile 1.2.3.4"
    echo
    exit 1
}

# Print usage if needed.
for arg in "$@"; do
    if [[ "$arg" == "-h" || "$arg" == "--help" ]]; then
        usage
    fi
done
if [[ $# -lt 1 || $# -gt 2 ]]; then
    usage
fi

###############################################################################
# Validate the environment
###############################################################################

# Verify environment and script arguments
if ! command -v dd &>/dev/null; then
    echo "ERROR: 'dd' not found in PATH" >&2
    exit 1
fi

file="$1"

if [[ ! -f "$file" ]]; then
    echo "ERROR: file not found: $file" >&2
    exit 1
fi

###############################################################################
# Read/modify the savefile
###############################################################################

read_version() {
    local bytes
    bytes=$(dd if="$1" bs=1 count=4 2>/dev/null | od -A n -t u1 | tr -s ' ')
    local b0 b1 b2 b3
    read -r b0 b1 b2 b3 <<<"$bytes"
    echo "${b0}.${b1}.${b2}.${b3}"
}

if [[ $# -eq 1 ]]; then
    echo "Savefile game version: $(read_version "$file")"
    exit 0
fi

version="$2"

IFS='.' read -r maj min pat ext <<<"$version"

if [[ -z "$maj" || -z "$min" || -z "$pat" || -z "$ext" ]]; then
    echo "ERROR: version must have four components (e.g. 1.2.3.4)" >&2
    exit 1
fi

for val in "$maj" "$min" "$pat" "$ext"; do
    if ! [[ "$val" =~ ^[0-9]+$ ]] || ((val < 0 || val > 255)); then
        echo "ERROR: version component '$val' is not an integer in 0-255" >&2
        exit 1
    fi
done

old_version=$(read_version "$file")

if [[ "$old_version" == "$version" ]]; then
    echo "savefile game version: $version (unchanged)"
    exit 0
fi

printf '%b' "\\x$(printf '%02x' "$maj")\\x$(printf '%02x' "$min")\\x$(printf '%02x' "$pat")\\x$(printf '%02x' "$ext")" | dd of="$file" bs=1 count=4 conv=notrunc 2>/dev/null

echo "savefile game version: changed $old_version => $version"
echo
echo "  *** WARNING WARNING WARNING *** "
echo
echo "It is YOUR responsibility to ensure that the modified savefile"
echo "can actually be read by Sil-Q version $version !"
