#!/usr/bin/env bash
# HMI check: build and run every test; exit non-zero on any failure.
# cargo uses the crate's own target dir under the caller's worktree.
set -euo pipefail
cd "$(dirname "$0")/../../hmi"
cargo build
cargo test
cd ..
bash tools/checks/gui-docs.sh
