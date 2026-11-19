#!/usr/bin/env bash
# readme.sh [repo-root] : exit 0 only if README.md has the "Build, flash and run" section
# with required content, a PICO355-R1..R9 trace table, existing paths, no forbidden words.
set -u
ROOT="${1:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
RD="$ROOT/README.md"
fail=0
bad() { echo "FAIL: $*"; fail=1; }
[ -f "$RD" ] || { echo "FAIL: README.md missing"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
# Section = from heading containing the phrase to the next heading of same or higher level.
awk '
  !f && /^#+[[:space:]].*Build, flash and run/ {f=1; match($0,/^#+/); lvl=RLENGTH; print; next}
  f { if (match($0,/^#+[[:space:]]/) && (RLENGTH-1) <= lvl) exit; print }
' "$RD" > "$TMP/sec"
[ -s "$TMP/sec" ] || { echo "FAIL: no heading containing 'Build, flash and run'"; exit 1; }
S="$TMP/sec"
grep -qiE 'firmware' "$S" && grep -qiE 'build' "$S" && grep -qiE 'cmake|pico[ -]?sdk' "$S" || bad "firmware build not covered"
grep -qiE 'UF2' "$S" && grep -qiE 'flash|BOOTSEL|drag|copy' "$S" || bad "flashing the UF2 not covered"
grep -qiE 'HMI' "$S" && grep -qiE 'cargo|run' "$S" || bad "running the HMI not covered"
grep -qF 'tools/checks' "$S" || bad "check scripts under tools/checks not covered"
grep -qE 'TBD' "$S" || bad "open TBD list missing"
for i in 1 2 3 4 5 6 7 8 9; do
  line=$(grep -E "PICO355-R${i}([^0-9]|$)" "$S" | grep -E '\|' | grep -E '[A-Za-z0-9_.-]+/[A-Za-z0-9_./*-]+|\.(c|h|rs|md|sh|toml|yml|sysml)\b' | head -1)
  [ -n "$line" ] || bad "trace table row for PICO355-R${i} naming a file missing"
done
# Every repo path mentioned (backticked, with a slash and a known top-level dir) must exist.
grep -oE '`[^` ]+`' "$S" | tr -d '`' | grep -E '^(firmware|hmi|tools|docs|sysml-models|\.github)/' \
  | sed 's/[,.;:]*$//' | sort -u > "$TMP/paths"
while IFS= read -r p; do
  case "$p" in *'*'*) ls -d $ROOT/$p >/dev/null 2>&1 || bad "path glob matches nothing: $p";; *) [ -e "$ROOT/$p" ] || bad "path missing: $p";; esac
done < "$TMP/paths"
# Forbidden words (assembled from fragments).
A="Instru"; B="Measure"; C="We"; D="ft"
if grep -qiE "${A}${B}|\\b${C}${D}" "$RD"; then bad "forbidden word in README.md"; fi
[ "$fail" -eq 0 ] && echo "readme.sh: OK" || echo "readme.sh: FAILED"
exit "$fail"
