#!/usr/bin/env bash
# ci.sh [repo-root] : exit 0 only if .github/workflows/ci.yml meets the CI contract.
# Parse: python3 -I with PyYAML when importable (structural checks); otherwise a
# documented text fallback (grep/awk on the file; same rules, coarser parse).
set -u
ROOT="${1:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
WF="$ROOT/.github/workflows/ci.yml"
fail=0
bad() { echo "FAIL: $*"; fail=1; }
[ -f "$WF" ] || { echo "FAIL: $WF missing"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

# Text-level rules (both modes).
for s in 'bash tools/checks/firmware-crossbuild.sh' 'bash tools/checks/host-tests.sh' \
         'bash tools/checks/firmware-lint.sh' 'bash tools/checks/hmi.sh'; do
  grep -qF -- "$s" "$WF" || bad "no step runs: $s"
done
grep -qE 'pull_request_target' "$WF" && bad "pull_request_target used"
grep -qE '\$\{\{[[:space:]]*secrets\.' "$WF" && bad "secrets used"
grep -qE '^[[:space:]]*secrets:|secrets:[[:space:]]*inherit' "$WF" && bad "secrets key used"
grep -qE '^[[:space:]]*matrix:' "$WF" && bad "strategy.matrix used"
grep -qiE 'rust-cache|Swatinem|actions/cache|cargo' "$WF" || bad "no Rust caching"
grep -qiE 'cmake' "$WF" || bad "no CMake mention"
grep -qiE 'actions/cache|ccache|cmake-cache|cache:' "$WF" || bad "no cache step"
grep -qE 'cancel-in-progress:[[:space:]]*true' "$WF" || bad "concurrency cancel-in-progress true missing"
grep -qE '^concurrency:|^[[:space:]]+concurrency:' "$WF" || bad "no concurrency group"
jobs_n=$(grep -cE '^[[:space:]]+runs-on:' "$WF")
tmo_n=$(grep -cE '^[[:space:]]+timeout-minutes:' "$WF")
[ "$jobs_n" -ge 1 ] || bad "no jobs"
[ "$tmo_n" -ge "$jobs_n" ] || bad "timeout-minutes on every job ($tmo_n of $jobs_n)"

# Structural rules.
if python3 -I -c 'import yaml' 2>/dev/null; then
  cat > "$TMP/chk.py" <<'PY'
import sys, yaml
d = yaml.safe_load(open(sys.argv[1]))
e = []
if not isinstance(d, dict): print("FAIL: not a mapping"); sys.exit(1)
on = d.get("on", d.get(True))
if not isinstance(on, dict): e.append("on: must map pull_request and push")
else:
    need = ["firmware/**", "hmi/**", "tools/**", "docs/protocol.md", ".github/workflows/ci.yml"]
    for t in ("pull_request", "push"):
        p = (on.get(t) or {}).get("paths") if isinstance(on.get(t), dict) else None
        if not p: e.append(t + ": no paths filter")
        else:
            for n in need:
                if n not in p: e.append(t + ": paths lacks " + n)
c = d.get("concurrency")
if not (isinstance(c, dict) and c.get("group") and c.get("cancel-in-progress") is True):
    e.append("concurrency group + cancel-in-progress true")
p = d.get("permissions")
if not (isinstance(p, dict) and p.get("contents") == "read" and all(v == "read" or v == "none" for v in p.values())):
    e.append("permissions must be read-only contents")
jobs = d.get("jobs") or {}
if not jobs: e.append("no jobs")
for k, j in jobs.items():
    if "timeout-minutes" not in j: e.append("job %s: no timeout-minutes" % k)
    if "matrix" in (j.get("strategy") or {}): e.append("job %s: strategy.matrix" % k)
for x in e: print("FAIL:", x)
sys.exit(1 if e else 0)
PY
  python3 -I "$TMP/chk.py" "$WF" || fail=1
else
  echo "note: PyYAML absent; text fallback"
  grep -qE '^permissions:' "$WF" && grep -A3 -E '^permissions:' "$WF" | grep -qE 'contents:[[:space:]]*read' \
    || bad "permissions contents: read missing"
  for t in pull_request push; do
    awk -v t="$t" '$0 ~ "^[[:space:]]+"t":"{f=1;next} f&&/^[[:space:]]+[a-z_]+:/&&!/paths/{if($0 !~ /^[[:space:]]{6,}/)f=0} f&&/paths:/{p=1} END{exit p?0:1}' "$WF" || bad "$t: no paths filter"
  done
  for n in 'firmware/\*\*' 'hmi/\*\*' 'tools/\*\*' 'docs/protocol.md' '.github/workflows/ci.yml'; do
    [ "$(grep -cE -- "$n" "$WF")" -ge 2 ] || bad "paths lacks $n (needs it under both triggers)"
  done
fi
[ "$fail" -eq 0 ] && echo "ci.sh: OK" || echo "ci.sh: FAILED"
exit "$fail"
