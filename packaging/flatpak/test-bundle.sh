#!/usr/bin/env bash
# Installs a chip8 Flatpak bundle the way a user would and runs it.
#
# usage: test-bundle.sh <bundle.flatpak> <rom dir>
#
# Runs on a plain Ubuntu runner with flatpak and Xvfb installed. The runtime
# comes from Flathub through the bundle's runtime repository, exactly as for
# `flatpak install --user ./chip8-<version>-<arch>.flatpak`. Then, inside the
# sandbox: every library of /app/bin/chip8 resolves; the repository's own
# frontend smoke test (dummy SDL drivers, 180 frames, BMP screenshot)
# passes; and the real X11 window stays up under Xvfb for 20 seconds, with
# the ROM read through the app's home:ro permission.
set -euo pipefail
[ $# -eq 2 ] || { echo "usage: $0 <bundle.flatpak> <rom dir>" >&2; exit 2; }
bundle="$(readlink -f "$1")"
roms="$(cd "$2" && pwd)"
app=com.dhivalabs.chip8

flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y --noninteractive "$bundle"
flatpak info --user "$app"

echo "::group::ldd /app/bin/chip8 in the sandbox"
libs="$(flatpak run --command=ldd "$app" /app/bin/chip8)"
echo "$libs"
echo "::endgroup::"
if grep 'not found' <<< "$libs"; then exit 1; fi

smoke="$HOME/chip8-smoke"
mkdir -p "$smoke"
cp "$roms/2-ibm-logo.ch8" "$smoke/"

echo "::group::frontend smoke test (dummy SDL drivers)"
flatpak run --env=SDL_VIDEODRIVER=dummy --env=SDL_AUDIODRIVER=dummy \
  --filesystem="$smoke" "$app" \
  --scale 1 --frames 180 --screenshot "$smoke/frontend.bmp" "$smoke/2-ibm-logo.ch8"
ls -l "$smoke/frontend.bmp"
test "$(wc -c < "$smoke/frontend.bmp")" -gt 8000
echo "::endgroup::"

echo "::group::X11 window under Xvfb for 20 seconds"
cat > "$RUNNER_TEMP/smoke.sh" <<'EOS'
log="$RUNNER_TEMP/flatpak-smoke.log"
"$@" > "$log" 2>&1 &
pid=$!
for _ in $(seq 1 20); do
  kill -0 "$pid" 2> /dev/null || break
  sleep 1
done
if kill -0 "$pid" 2> /dev/null; then
  echo "still running after 20s"
  kill "$pid"
  ok=1
else
  wait "$pid" && rc=0 || rc=$?
  echo "exited early with status $rc"
  ok=0
fi
cat "$log"
if grep -E 'error while loading shared libraries|cannot open shared object|undefined symbol' "$log"; then ok=0; fi
[ "$ok" = 1 ]
EOS
xvfb-run -a -s '-screen 0 1280x800x24' dbus-run-session -- \
  bash "$RUNNER_TEMP/smoke.sh" flatpak run "$app" --mute "$smoke/2-ibm-logo.ch8"
echo "::endgroup::"
echo "flatpak: OK"
