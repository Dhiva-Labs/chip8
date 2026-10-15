#!/usr/bin/env bash
# Headless regression suite. Runs the Timendus test ROMs through chip8_run_rom
# and compares each final framebuffer with tests/expected/<name>.txt, then
# smoke-tests the SDL frontend under SDL's dummy video/audio drivers.
#
#   tests/run_tests.sh [BUILD_DIR]            check (default BUILD_DIR: build)
#   tests/run_tests.sh --update [BUILD_DIR]   rewrite tests/expected/*.txt
#
# Fetch the ROMs first: tests/roms/fetch_roms.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
update=0
if [ "${1:-}" = "--update" ]; then update=1; shift; fi
build="$(cd "${1:-$root/build}" && pwd)"
# Everything below runs from the repo root with relative paths, so the
# (possibly native Windows) executables never see MSYS-style absolute paths.
cd "$root"

# Locate a built executable (single-config, or multi-config like MSVC's Release/).
find_bin() {
  local name="$1" dir
  for dir in "$build" "$build/Release" "$build/RelWithDebInfo"; do
    for ext in "" ".exe"; do
      if [ -f "$dir/$name$ext" ]; then echo "$dir/$name$ext"; return 0; fi
    done
  done
  echo "error: $name not found under $build (build the project first)" >&2
  return 1
}

run_rom="$(find_bin chip8_run_rom)"
frontend="$(find_bin chip8)"
roms=tests/roms
expected=tests/expected

if [ ! -f "$roms/1-chip8-logo.ch8" ]; then
  echo "error: test ROMs missing; run tests/roms/fetch_roms.sh first" >&2
  exit 2
fi

failures=0
total=0

# check NAME ROM [chip8_run_rom args...]
check() {
  local name="$1" rom="$2"; shift 2
  local out actual diffout
  total=$((total + 1))
  if ! out="$("$run_rom" "$roms/$rom" "$@" | tr -d '\r')"; then
    echo "FAIL  $name (runner exited non-zero)"; failures=$((failures + 1)); return
  fi
  if grep -q '^WARNING' <<<"$out"; then
    echo "FAIL  $name"; grep '^WARNING' <<<"$out"; failures=$((failures + 1)); return
  fi
  actual="$(grep -E '^[+|]' <<<"$out")"
  # The expected file is CR-stripped too, in case git checked it out with CRLF.
  if [ "$update" = 1 ]; then
    printf '%s\n' "$actual" >"$expected/$name.txt"
    echo "WROTE $name"
  elif diffout="$(diff -u <(tr -d '\r' <"$expected/$name.txt") <(printf '%s\n' "$actual"))"; then
    echo "PASS  $name"
  else
    echo "FAIL  $name (framebuffer differs from tests/expected/$name.txt)"
    printf '%s\n' "$diffout"; failures=$((failures + 1))
  fi
}

check 1-chip8-logo   1-chip8-logo.ch8 --frames 120
check 2-ibm-logo     2-ibm-logo.ch8   --frames 120
check 3-corax-plus   "3-corax+.ch8"   --frames 480
check 4-flags        4-flags.ch8      --frames 480
check 5-quirks-vip   5-quirks.ch8     --poke 0x1FF=1 --frames 600
# Keypad: key 5 held from frame 60 (never released) for the EX9E / EXA1 menus;
# pressed and released for the FX0A menu.
check 6-keypad-ex9e  6-keypad.ch8     --poke 0x1FF=1 --key 5:60:1000 --frames 180
check 6-keypad-exa1  6-keypad.ch8     --poke 0x1FF=2 --key 5:60:1000 --frames 180
check 6-keypad-fx0a  6-keypad.ch8     --poke 0x1FF=3 --key 5:120:180 --frames 360

# SDL frontend smoke test: real SDL init, window, renderer and BMP export,
# headless via the dummy drivers (needs the SDL2 runtime to load).
if [ "$update" = 0 ]; then
  total=$((total + 1))
  mkdir -p tests/out
  shot=tests/out/frontend.bmp
  rm -f "$shot"
  if SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$frontend" --scale 1 --frames 180 \
       --screenshot "$shot" "$roms/2-ibm-logo.ch8" && [ "$(wc -c <"$shot")" -gt 8000 ]; then
    echo "PASS  frontend (dummy SDL video+audio drivers, screenshot written)"
  else
    echo "FAIL  frontend smoke test"; failures=$((failures + 1))
  fi
fi

echo "$((total - failures))/$total passed"
[ "$failures" = 0 ]
