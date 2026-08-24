#!/usr/bin/env bash
# Run ALL test suites (unit/hw/scenario/perf/destructive) and emit a test report.
# What actually runs is decided by tests/test_config.json 'suites'.
# Optional stages: health check (--health), clang-format check (--format), cmake install (--install).
# Usage: bash tests/scripts/run_tests.sh [options]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
if command -v cygpath >/dev/null 2>&1; then
    PROJECT_ROOT="$(cygpath -m "${PROJECT_ROOT}")"
fi

IS_WINDOWS=0
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=1 ;;
esac

BUILD_DIR="${PROJECT_ROOT}/build"
CONFIG="Release"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
DO_BUILD=1
DO_HEALTH=0
DO_FORMAT=0
DO_INSTALL=0

STEP_RESULTS=(
    "format|SKIP|not requested (use --format)"
    "health|SKIP|not requested (use --health)"
    "build|PENDING|"
    "ctest|PENDING|"
    "install|SKIP|not requested (use --install)"
)

update_step() {
    local name="$1" status="$2" detail="$3" entry i
    for i in "${!STEP_RESULTS[@]}"; do
        entry="${STEP_RESULTS[$i]}"
        if [ "${entry%%|*}" = "$name" ]; then
            STEP_RESULTS[$i]="${name}|${status}|${detail}"
            return
        fi
    done
}

for arg in "$@"; do
    case "$arg" in
        --no-build) DO_BUILD=0 ;;
        --health)   DO_HEALTH=1 ;;
        --format)   DO_FORMAT=1 ;;
        --install)  DO_INSTALL=1 ;;
        --config=*) CONFIG="${arg#*=}" ;;
        --build-dir=*) BUILD_DIR="${arg#*=}" ;;
        --jobs=*)   JOBS="${arg#*=}" ;;
        -h|--help)
            cat <<'HELP'
Usage: bash tests/scripts/run_tests.sh [options]

Options:
  --config=<TYPE>    Build type: Release|Debug|RelWithDebInfo (default: Release)
  --build-dir=<DIR>  CMake build directory (default: <root>/build)
  --jobs=<N>         Parallel build jobs
  --no-build         Skip configure & build, only run tests
  --health          Run device health check before tests
  --format          Run clang-format check on changed C++ files
  --install         Run cmake install and verify
  -h, --help        Show this help

Exit codes:
  0  All enabled suites passed
  1  One or more ctest suites failed
  2  Environment or configuration error
HELP
            exit 0
            ;;
        *) echo "Ignore unknown option: $arg" ;;
    esac
done

FULL_LOG="${BUILD_DIR}/test-results/run-all.log"
mkdir -p "$(dirname "${FULL_LOG}")"
exec > >(tee "${FULL_LOG}") 2>&1

require_cmd() {
    local name="$1"
    if ! command -v "$name" >/dev/null 2>&1; then
        echo "[ERROR] missing required command: $name" >&2
        exit 2
    fi
}

require_cmd ctest
if [ "$DO_BUILD" = "1" ] || [ "$DO_INSTALL" = "1" ]; then
    require_cmd cmake
fi
if [ "$DO_HEALTH" = "1" ]; then
    HEALTH_FILE="device_health_check.sh"
    if [ "$IS_WINDOWS" = "1" ]; then
        HEALTH_FILE="device_health_check.ps1"
    fi
    if [ ! -f "${SCRIPT_DIR}/${HEALTH_FILE}" ]; then
        echo "[ERROR] ${HEALTH_FILE} not found" >&2
        exit 2
    fi
fi

run_format() {
    echo "===== clang-format check ====="
    local CF
    CF="$(command -v clang-format || command -v clang-format-18 || true)"
    if [ -z "$CF" ]; then
        echo "[ERROR] clang-format (or clang-format-18) not found" >&2
        return 2
    fi
    "$CF" --version

    local TARGET_BRANCH="${CI_MERGE_REQUEST_TARGET_BRANCH_NAME:-develop}"
    if ! git -C "${PROJECT_ROOT}" remote get-url origin >/dev/null 2>&1; then
        echo "[WARN] no git remote origin; skipping diff-based format check."
        return 0
    fi
    if ! git -C "${PROJECT_ROOT}" fetch origin "${TARGET_BRANCH}"; then
        echo "[WARN] git fetch failed; skipping diff-based format check."
        return 0
    fi

    local CHANGED
    CHANGED="$(git -C "${PROJECT_ROOT}" diff --name-only --diff-filter=ACMR \
        "origin/${TARGET_BRANCH}...HEAD" \
        '*.cpp' '*.hpp' '*.h' '*.c' '*.cc' | \
        grep -E '^(src|include|tools|examples|tests)/' || true)"

    if [ -z "$CHANGED" ]; then
        echo "No changed C++ files in src/, include/ or tests/  - skipping"
        echo "clang-format-check: PASSED (no files to check)"
        return 0
    fi

    echo "Checking clang-format on changed files:"
    echo "$CHANGED"

    local DIFF_OUTPUT
    DIFF_OUTPUT="$(cd "${PROJECT_ROOT}" && echo "$CHANGED" | xargs "$CF" --dry-run --Werror 2>&1)"
    if [ "$?" -ne 0 ]; then
        echo "=== clang-format check failed ==="
        echo "$DIFF_OUTPUT"
        echo "[ERROR] clang-format: formatting errors found" >&2
        return 2
    fi

    echo "All changed files are properly formatted."
    echo "clang-format-check: PASSED"
}

run_install() {
    echo "===== cmake install (${CONFIG}) ====="
    cmake --install "${BUILD_DIR}" --prefix "${PROJECT_ROOT}/ob-install" --config "${CONFIG}" || return 2

    if [ ! -d "${PROJECT_ROOT}/cmake/verify" ]; then
        echo "cmake/verify not found  - skipping install verification."
        echo "install: PASSED (verify skipped)"
        return 0
    fi

    cmake -S "${PROJECT_ROOT}/cmake/verify" -B "${BUILD_DIR}/verify-build" \
        -DOrbbecSDK_DIR="${PROJECT_ROOT}/ob-install/lib" || return 2
    cmake --build "${BUILD_DIR}/verify-build" --config "${CONFIG}" || return 2
    echo "install: PASSED"
}

if [ "$DO_FORMAT" = "1" ]; then
    if run_format; then
        update_step format PASS ""
    else
        update_step format FAIL "clang-format check failed"
    fi
fi

CONFIG_FILE="${PROJECT_ROOT}/tests/test_config.json"
if [ -f "${CONFIG_FILE}" ]; then
    DEVICE_COUNT="$(grep -o '"count"[[:space:]]*:[[:space:]]*[0-9]*' "${CONFIG_FILE}" | head -n 1 || true)"
    echo "===== test_config.json ====="
    [ -n "${DEVICE_COUNT}" ] && echo "device: ${DEVICE_COUNT}"
    SELECTED=()
    for suite in unit hw scenario perf destructive; do
        if grep -q "\"${suite}\"[[:space:]]*:[[:space:]]*true" "${CONFIG_FILE}"; then
            SELECTED+=("${suite}")
        fi
    done
    if [ "${#SELECTED[@]}" -eq 0 ]; then
        echo "[WARNING] no suite enabled in test_config.json 'suites'"
        exit 0
    fi
    LABELS="$(IFS='|'; echo "${SELECTED[*]}")"
    echo "suites: ${LABELS}"
else
    echo "[WARNING] test_config.json not found: ${CONFIG_FILE}, using default labels"
    LABELS="unit|hw|scenario|perf|destructive"
fi

if [ "$DO_HEALTH" = "1" ]; then
    if ! echo "${LABELS}" | grep -qE 'hw|scenario|perf|destructive'; then
        echo "===== health ====="
        echo "No device-dependent suite selected - skipping health check"
        update_step health SKIP "no device suite selected"
    elif [ "$IS_WINDOWS" = "1" ]; then
        echo "===== health ====="
        HEALTH_SCRIPT="$(cygpath -w "${SCRIPT_DIR}/device_health_check.ps1" 2>/dev/null)"
        powershell -NoProfile -ExecutionPolicy Bypass -File "${HEALTH_SCRIPT}"
        HEALTH_RC=$?
        if [ "$HEALTH_RC" = "0" ]; then
            update_step health PASS ""
        else
            update_step health FAIL "device health check failed"
            echo "[ERROR] device health check failed, aborting." >&2
            exit 2
        fi
    else
        echo "===== health ====="
        bash "${SCRIPT_DIR}/device_health_check.sh"
        HEALTH_RC=$?
        if [ "$HEALTH_RC" = "0" ]; then
            update_step health PASS ""
        else
            update_step health FAIL "device health check failed"
            echo "[ERROR] device health check failed, aborting." >&2
            exit 2
        fi
    fi
fi

echo "===== build ====="
if [ "$DO_BUILD" = "1" ]; then
    if cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
        -DOB_BUILD_TESTS=ON \
        -DOB_BUILD_EXAMPLES=OFF \
        -DOB_BUILD_TOOLS=OFF \
        -DOB_BUILD_DOCS=OFF \
        -DCMAKE_BUILD_TYPE="${CONFIG}" \
        && cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel "${JOBS}"; then
        update_step build PASS ""
    else
        update_step build FAIL "configure or build failed"
    fi
else
    update_step build SKIP "--no-build"
fi

TEST_RESULTS="${BUILD_DIR}/test-results"
JUNIT="${TEST_RESULTS}/junit-all.xml"
mkdir -p "${TEST_RESULTS}"
export OB_TEST_LOG_DIR="${TEST_RESULTS}"

echo "===== ctest ====="
(cd "${BUILD_DIR}" && ctest -L "${LABELS}" -C "${CONFIG}" --output-on-failure --output-junit "${JUNIT}")
CTEST_RC=$?
if [ "$CTEST_RC" = "0" ]; then
    update_step ctest PASS ""
else
    update_step ctest FAIL "ctest rc=${CTEST_RC}"
fi

if [ "$DO_INSTALL" = "1" ]; then
    if run_install; then
        update_step install PASS ""
    else
        update_step install FAIL "install failed"
    fi
fi

echo "===== report ====="
echo "JUnit XML: ${JUNIT}"
echo "Full log:  ${FULL_LOG}"
echo "SDK log:   ${TEST_RESULTS}/OrbbecSDK.log.txt"
echo "===== Run Steps ====="
for st in "${STEP_RESULTS[@]}"; do
    name="${st%%|*}"
    rest="${st#*|}"
    status="${rest%%|*}"
    detail="${rest#*|}"
    if [ -n "$detail" ]; then
        echo "  ${name}: ${status}  ${detail}"
    else
        echo "  ${name}: ${status}"
    fi
done

FINAL_RC=0
for st in "${STEP_RESULTS[@]}"; do
    name="${st%%|*}"
    rest="${st#*|}"
    status="${rest%%|*}"
    if [ "$status" = "FAIL" ]; then
        if [ "$name" = "ctest" ]; then
            FINAL_RC=1
        else
            FINAL_RC=2
        fi
    fi
done
echo "===== DONE rc=${FINAL_RC} ====="
exit "${FINAL_RC}"
