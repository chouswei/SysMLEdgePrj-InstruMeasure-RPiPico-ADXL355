#!/usr/bin/env bash
# gui-docs.sh [repo-root] : exit 0 only if docs/protocol.md has an "HMI GUI" section with the
# required content and README.md mentions the GUI, --cli and .h5; no forbidden words.
set -u
ROOT="${1:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
DOC="$ROOT/docs/protocol.md"
RD="$ROOT/README.md"
fail=0
bad() { echo "FAIL: $*"; fail=1; }
[ -f "$DOC" ] || { echo "FAIL: docs/protocol.md missing"; exit 1; }
[ -f "$RD" ] || { echo "FAIL: README.md missing"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
# Section = from a heading containing "HMI GUI" to the next heading of same or higher level.
awk '
  !f && /^#+[[:space:]].*HMI GUI/ {f=1; match($0,/^#+/); lvl=RLENGTH; print; next}
  f { if (match($0,/^#+[[:space:]]/) && (RLENGTH-1) <= lvl) exit; print }
' "$DOC" > "$TMP/sec"
[ -s "$TMP/sec" ] || { echo "FAIL: docs/protocol.md has no heading containing 'HMI GUI'"; exit 1; }
S="$TMP/sec"
for s in 'X (g)' '3.9e-6' 'adxl355.h:62-65' 'accel_g' 'raw_counts' 'sample_index' '--cli' '2E8A' '000A' '(owner t43s2)'; do
  grep -qF -- "$s" "$S" || bad "section lacks: $s"
done
# near = same line or within 2 lines (case-insensitive second word).
near() { grep -n -i -C2 -F -- "$1" "$S" | grep -qi -- "$2"; }
near 'scale_lsb_per_g' 'derived' || bad "scale_lsb_per_g not marked derived"
near 'native file picker' 'TBD' || bad "native file picker not marked TBD"
# README mentions.
grep -qiE 'GUI|window' "$RD" || bad "README.md does not mention the GUI"
grep -qF -- '--cli' "$RD" || bad "README.md does not mention --cli"
grep -qF -- '.h5' "$RD" || bad "README.md does not mention .h5"
# Forbidden words (assembled from fragments).
A="Instru"; B="Measure"; C="We"; D="ft"
for f in "$DOC" "$RD"; do
  if grep -qiE "${A}${B}|\\b${C}${D}" "$f"; then bad "forbidden word in $f"; fi
done
[ "$fail" -eq 0 ] && echo "gui-docs.sh: OK" || echo "gui-docs.sh: FAILED"
exit "$fail"
