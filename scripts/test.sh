#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=========================================================="
echo " Running Automated Test Suites (Backend + Frontend)       "
echo "=========================================================="

# 1. Backend GoogleTest Unit & Integration Tests
echo ""
echo "[1/2] Executing Backend GoogleTest Suites (ptrace + models + replay)..."
if [ ! -f "${ROOT_DIR}/build/backend/ttd_tests" ]; then
    echo "[!] Test binary not found. Building backend test target..."
    cmake --build "${ROOT_DIR}/build" --target ttd_tests -j2
fi

"${ROOT_DIR}/build/backend/ttd_tests" --gtest_color=yes

# 2. Frontend Type and Build Tests
echo ""
echo "[2/2] Validating Frontend TypeScript Compilation & Bundling..."
cd "${ROOT_DIR}/frontend"
npm run build

echo ""
echo "=========================================================="
echo " All Tests Passed Successfully!                           "
echo "=========================================================="

