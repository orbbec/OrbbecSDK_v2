#!/usr/bin/env bash
set -euo pipefail

fail_health() {
    echo "[HEALTH FAIL] Device infrastructure not ready (infrastructure failure, not a test failure)."
    exit 1
}

if ! command -v lsusb >/dev/null 2>&1; then
    fail_health
fi

if ! lsusb | grep -q "2bc5"; then
    fail_health
fi

echo "[HEALTH OK] Device infrastructure is ready."
exit 0
