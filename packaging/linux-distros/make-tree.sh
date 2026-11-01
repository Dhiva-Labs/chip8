#!/usr/bin/env bash
# Lays out the installed file tree that pack.sh and the Flatpak are built
# from, using the chip8 binary the build job compiled and tested (there is no
# .deb to unpack).
#
# usage: make-tree.sh <chip8 binary> <root>
set -euo pipefail
[ $# -eq 2 ] || { echo "usage: $0 <chip8 binary> <root>" >&2; exit 2; }
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="$1"
root="$2"

install -Dm755 "$bin" "$root/usr/bin/chip8"
install -Dm644 "$repo/packaging/linux/chip8.desktop" "$root/usr/share/applications/chip8.desktop"
install -Dm644 "$repo/packaging/linux/chip8.png" "$root/usr/share/icons/hicolor/256x256/apps/chip8.png"
# .ch8 files get their own MIME type, so file managers offer chip8 for them.
install -Dm644 "$repo/packaging/linux/chip8-mime.xml" "$root/usr/share/mime/packages/chip8.xml"
install -Dm644 "$repo/README.md" "$root/usr/share/doc/chip8/README.md"
find "$root" | sort
