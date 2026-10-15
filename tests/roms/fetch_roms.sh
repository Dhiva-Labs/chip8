#!/usr/bin/env bash
# Fetches the Timendus chip8-test-suite ROMs used by the test procedure in the
# README. The ROMs are gitignored; run this once before testing.
#
# The suite is pinned to a known commit so the framebuffers in tests/expected
# stay valid. Override with TIMENDUS_REF=<branch|tag|sha> to try another
# revision.
set -euo pipefail
cd "$(dirname "$0")"

ref="${TIMENDUS_REF:-742e9eac9f5d44466c1c9d83fcaa8158c6683cb1}"
base="https://raw.githubusercontent.com/Timendus/chip8-test-suite/$ref/bin"
roms=(
  1-chip8-logo.ch8
  2-ibm-logo.ch8
  3-corax+.ch8
  4-flags.ch8
  5-quirks.ch8
  6-keypad.ch8
  7-beep.ch8
)

for rom in "${roms[@]}"; do
  echo "fetching $rom"
  curl -fsSL -o "$rom" "$base/$rom"
done
echo "done: ${#roms[@]} ROMs in $(pwd)"
