# chip8

[![ARM64 builds](https://github.com/Dhiva-Labs/chip8/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Dhiva-Labs/chip8/actions/workflows/ci.yml)
![Linux x64 | arm64](https://img.shields.io/badge/Linux-x64%20%7C%20arm64-informational?logo=linux&logoColor=white)
![Windows x64 | arm64](https://img.shields.io/badge/Windows-x64%20%7C%20arm64-informational?logo=windows&logoColor=white)

A CHIP-8 interpreter in C++20 with an SDL2 frontend, validated against
[Timendus's chip8-test-suite](https://github.com/Timendus/chip8-test-suite)
rather than eyeballed. Phase 1 of a two-phase emulator project (Phase 2 is a
Game Boy emulator); the fetch/decode/execute loop and the CPU-vs-timer
decoupling here are the patterns the Game Boy core builds on.

## Build

Requires CMake ≥ 3.16, a C++20 compiler, and SDL2 development headers
(`libsdl2-dev` on Debian/Ubuntu).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Targets:

- `build/chip8` — the SDL2 frontend
- `build/chip8_run_rom` — headless test runner (see Testing)

### Windows

Use MSVC (Visual Studio 2022) and install SDL2 with [vcpkg](https://vcpkg.io).
The triplet picks the architecture (`x64-windows` or `arm64-windows`):

```
vcpkg install sdl2:x64-windows
cmake -S . -B build -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

For ARM64 use `sdl2:arm64-windows`, `-A ARM64` and `-DVCPKG_TARGET_TRIPLET=arm64-windows`.
The executables and `SDL2.dll` end up in `build\Release\`.

### Prebuilt binaries

Every release carries four archives, built and tested by CI on native runners:

| Platform | Archive |
|---|---|
| Linux x86-64 | `chip8-linux-x64.tar.gz` |
| Linux ARM64 (aarch64) | `chip8-linux-arm64.tar.gz` |
| Windows x64 | `chip8-windows-x64.zip` |
| Windows ARM64 | `chip8-windows-arm64.zip` |

The Linux archives are built on Ubuntu 24.04 and need the SDL2 runtime
(`sudo apt install libsdl2-2.0-0`). The Windows archives include `SDL2.dll`
and need the Microsoft Visual C++ Redistributable (2015-2022, same
architecture as the archive).

## Run

```
./build/chip8 [options] rom.ch8
```

| Option | Meaning |
|---|---|
| `--ips N` | Instructions per second (default 540). ROMs tuned for other interpreters may want 700–1000. |
| `--scale N` | Window scale factor (default 12 → 768×384) |
| `--profile vip\|modern` | Quirk profile (default `vip`, see Quirks) |
| `--mute` | Disable the beeper |
| `--frames N` | Exit after N 60Hz frames (for automated testing) |
| `--screenshot PATH` | Save a BMP screenshot on exit |

### Keypad mapping

The 16-key hex keypad maps to the left of a QWERTY keyboard:

```
CHIP-8          Keyboard
1 2 3 C         1 2 3 4
4 5 6 D    →    Q W E R
7 8 9 E         A S D F
A 0 B F         Z X C V
```

`Space` pauses, `Esc` quits.

## Timing model

CPU speed and timer speed are fully decoupled:

- The **delay and sound timers** decrement at a fixed 60Hz driven by a
  wall-clock accumulator (`SDL_GetPerformanceCounter`), regardless of how
  many instructions execute.
- The **instruction rate** is a separate accumulator at `--ips` instructions
  per second (default 540 ≈ COSMAC VIP speed, i.e. ~9 instructions per frame).
- **Display wait** (see Quirks) couples `DXYN` to the 60Hz tick the same way
  the VIP's display interrupt did: each frame grants one sprite draw; a
  `DXYN` with no quota available halts the CPU until the next tick and the
  remaining cycles of that frame are lost. The wait happens *before* the
  draw, so code following the draw runs in the same frame — the Timendus
  quirks test measures exactly this and reports `SLOW` if you halt after
  drawing instead.

## Quirks

CHIP-8 "quirks" are the behaviors where the original COSMAC VIP interpreter
and later SUPER-CHIP-era interpreters disagree. The default profile is the
**original COSMAC VIP**, verified by the Timendus quirks test:

| Behavior | VIP default (`--profile vip`) | `--profile modern` |
|---|---|---|
| `8XY1/8XY2/8XY3` | reset VF to 0 | leave VF alone |
| `8XY6/8XYE` | shift **VY** into VX | shift VX in place |
| `FX55/FX65` | leave `I = I + X + 1` | leave `I` unchanged |
| `BNNN` | jump to `NNN + V0` | `BXNN`: jump to `NNN + VX` |
| `DXYN` edges | start coords wrap mod 64/32, sprite pixels **clip** at right/bottom | same (clipping) |
| `DXYN` timing | waits for the 60Hz tick (max ~60 draws/sec) | no wait |
| `FX0A` | completes on key **release** | same |

Flag ordering: arithmetic ops write the result before the flag, so `VF`
holds the flag (not the result) when `X` is `F` — the flags test checks
every one of these.

Only the VIP profile is validated by the test suite; `modern` is a
convenience for ROMs written against SUPER-CHIP-style interpreters.

## Testing

Correctness is proven by test ROMs, not visual inspection. Fetch the
Timendus suite (ROMs are gitignored; the script pins the suite to the
revision the expected output was recorded against):

```
./tests/roms/fetch_roms.sh
```

`tests/run_tests.sh [BUILD_DIR]` runs the suite below headlessly, compares
every final framebuffer with `tests/expected/*.txt`, and smoke-tests the SDL
frontend under SDL's dummy video/audio drivers. CI runs it on Linux and
Windows, x64 and ARM64. `--update` rewrites the expected files (review the
diff before committing).

`chip8_run_rom` executes a ROM headlessly for a fixed number of simulated
frames and dumps the framebuffer as ASCII art (plus `--bmp PATH` for an
image). The Timendus ROMs read a menu selection from memory address `0x1FF`,
which `--poke` supplies, and `--key K:DOWN:UP` scripts a key press/release
for the keypad tests:

```
./build/chip8_run_rom tests/roms/1-chip8-logo.ch8 --frames 120
./build/chip8_run_rom tests/roms/2-ibm-logo.ch8   --frames 120
./build/chip8_run_rom "tests/roms/3-corax+.ch8"   --frames 480
./build/chip8_run_rom tests/roms/4-flags.ch8      --frames 480
./build/chip8_run_rom tests/roms/5-quirks.ch8     --poke 0x1FF=1 --frames 600
./build/chip8_run_rom tests/roms/6-keypad.ch8     --poke 0x1FF=3 --key 5:120:180 --frames 360
```

Suite status (all verified against the rendered pass/fail screens):

| Test | Result |
|---|---|
| 1 CHIP-8 logo | ✅ renders |
| 2 IBM logo | ✅ renders |
| 3 corax+ opcode test | ✅ all 24 checks tick |
| 4 flags test | ✅ all checks tick |
| 5 quirks test (CHIP-8/VIP profile, `0x1FF=1`) | ✅ VF RESET ON, MEMORY ON, DISP.WAIT ON, CLIPPING ON, SHIFTING OFF, JUMPING OFF — all ticks |
| 6 keypad test (`0x1FF=1/2/3`) | ✅ EX9E, EXA1, FX0A "ALL GOOD" |
| 7 beep | 🔊 manual — beeps SOS with the speaker icon (audio can't be asserted headlessly) |
| 8 scrolling | n/a — SUPER-CHIP/XO-CHIP scrolling opcodes, out of scope |

## Layout

```
src/
  cpu.{h,cpp}       registers, stack, timers, opcode dispatch (table on the
                    high nibble, sub-dispatch for 0/8/E/F groups)
  memory.{h,cpp}    4KB RAM, hex font at 0x050, ROM at 0x200
  display.{h,cpp}   64x32 XOR framebuffer + SDL2 renderer
  input.{h,cpp}     keypad state, FX0A release latch, SDL scancode map
  rom_loader.{h,cpp}
main.cpp            SDL2 window, wall-clock timing loop, beeper
tests/run_rom.cpp   headless deterministic runner
tests/roms/         test ROMs (gitignored; fetch_roms.sh)
```

## Out of scope

SUPER-CHIP / XO-CHIP extensions (hires, scrolling, planes), save states,
and configurable palettes. The sound is a fixed 440Hz square wave.
