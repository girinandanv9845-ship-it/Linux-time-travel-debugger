#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=========================================================="
echo " Building Linux Process Time-Travel Debugger (All Targets) "
echo "=========================================================="

# 1. Build C++ Backend
echo ""
echo "[1/2] Configuring and Building C++ Backend with CMake..."
cd "${ROOT_DIR}"
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2

echo "[OK] Backend build succeeded: ./build/backend/ttd_backend and ./build/backend/ttd_tests"

# 2. Build Frontend
echo ""
echo "[2/2] Building React + Vite Frontend..."
cd "${ROOT_DIR}/frontend"
if [ ! -d "node_modules" ]; then
    npm install --no-audit --no-fund
fi
npm run build

echo "[OK] Frontend build succeeded: ./frontend/dist"
echo ""
echo "=========================================================="
echo " Complete Project Build Finished Successfully!            "
echo "=========================================================="

