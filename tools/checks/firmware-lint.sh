#!/usr/bin/env bash
# firmware-lint.sh : static rules for firmware/ (run from anywhere). Exit 0 only if all hold.
#  (a) no pico/, hardware/ or tusb include outside firmware/src/main.c
#  (b) every #define in adxl355_regs.h cites "no-OS H:<line>" / "no-OS C:<line>" (adxl355_regs.h pins
#      the commit) or "Datasheet Rev. D" + Section/Table, or is TODO(TBD)
#  (c) no numeric TODO(TBD) define outside config.h (adxl355_regs.h is the one other home:
#      its TBD entries are register sentinels/flags per the A1 contract, see docs/protocol.md)
#  (d) the DRDY pin is never set as output or driven
#  (e) forbidden-word check over firmware/ (both words; pattern assembled from fragments;
#      the repo-wide rule is code/checks/forbidden.sh)
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT/firmware" || exit 1
rc=0
bad() { echo "LINT $1: $2"; rc=1; }

# (a)
while IFS= read -r l; do bad a "SDK include outside main.c: $l"; done < <(
  grep -rnE '#[[:space:]]*include[[:space:]]*[<"](pico/|hardware/|tusb)' --include='*.[ch]' . | grep -v '^\./src/main\.c:')

# (b)
REGS=include/adxl355_regs.h
if [ -f "$REGS" ]; then
  while IFS=: read -r ln rest; do
    prev=$(sed -n "$((ln-1))p" "$REGS")
    echo "$rest$prev" | grep -qE 'TODO\(TBD\)|no-OS [HC]:[0-9]+|Datasheet Rev\. D.*([Ss]ection|[Tt]able)' || bad b "$REGS:$ln uncited define"
  done < <(grep -nE '^[[:space:]]*#[[:space:]]*define' "$REGS")
else
  bad b "$REGS missing"
fi

# (c) a #define whose value is a numeric literal and whose line says TODO(TBD)
while IFS= read -r l; do bad c "numeric TODO(TBD) define outside config.h: $l"; done < <(
  grep -rnE '^[[:space:]]*#[[:space:]]*define[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]+\(?-?(0[xX][0-9A-Fa-f]+|[0-9]+)[uUlL]*\)?[[:space:]]*(/\*.*)?TODO\(TBD\)' \
    --include='*.[ch]' . | grep -v -e '^\./include/config\.h:' -e '^\./include/adxl355_regs\.h:')

# (d) DRDY never output / driven (macro name or literal GP22)
D='(CFG_PIN_DRDY_IN|[A-Za-z0-9_]*DRDY[A-Za-z0-9_]*|\b22)'
while IFS= read -r l; do bad d "DRDY driven: $l"; done < <(
  grep -rnE "gpio_(put|set_function|set_dir|set_outover|set_dir_out_masked|set_mask|clr_mask)[[:space:]]*\([[:space:]]*$D[[:space:]]*[,)]" --include='*.[ch]' . \
    | grep -E 'gpio_put|gpio_set_function|set_outover|_masked|set_mask|clr_mask|GPIO_OUT')

# (e)
P1="Instru""Measure"; P2="\\bWe""ft"
while IFS= read -r l; do bad e "forbidden name: $l"; done < <(grep -rniIE "$P1|$P2" . | cut -d: -f1,2)

[ $rc -eq 0 ] && echo "firmware-lint: ok"
exit $rc
