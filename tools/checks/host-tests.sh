#!/usr/bin/env bash
# host-tests.sh [build-dir] : configure, build and run every ctest of firmware/tests (plain gcc).
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
if ! command -v cmake >/dev/null 2>&1; then . /workspace/pico355/code/toolchain.env; fi
cmake -S "$ROOT/firmware/tests" -B "$B" -DCMAKE_C_COMPILER=gcc >"$B/configure.log" 2>&1 || { cat "$B/configure.log"; echo "host-tests: configure failed"; exit 2; }
cmake --build "$B" >"$B/build.log" 2>&1 || { cat "$B/build.log"; echo "host-tests: build failed"; exit 3; }
ctest --test-dir "$B" --output-on-failure
rc=$?
[ $rc -eq 0 ] || echo "host-tests: FAILED (ctest exit $rc)"
exit $rc
