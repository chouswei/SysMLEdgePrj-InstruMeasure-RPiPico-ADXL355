#!/usr/bin/env bash
# firmware-crossbuild.sh [build-dir] : Pico SDK cross build of firmware/, asserts a .uf2 exists.
# Without an argument a private mktemp dir is used and removed on exit; with one it is kept
# and reported. Safe to run several copies at the same time (no shared path).
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [ $# -ge 1 ]; then
  B="$1"; mkdir -p "$B" || exit 1; echo "build dir kept: $B"
else
  B="$(mktemp -d)" || exit 1
  trap 'rm -rf "$B"' EXIT
fi
# shellcheck disable=SC1091
. /workspace/pico355/code/toolchain.env
cmake -S "$ROOT/firmware" -B "$B" >"$B/configure.log" 2>&1 || { tail -n 40 "$B/configure.log"; echo "crossbuild: configure failed"; exit 2; }
cmake --build "$B" >"$B/build.log" 2>&1 || { tail -n 60 "$B/build.log"; echo "crossbuild: build failed"; exit 3; }
if ! find "$B" -name '*.uf2' | grep -q .; then echo "crossbuild: no .uf2 produced"; exit 4; fi
echo "crossbuild: ok (.uf2 produced)"
exit 0
