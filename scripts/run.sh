#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BACKEND_PORT="${PORT:-8080}"
FRONTEND_PORT="${FRONTEND_PORT:-3000}"

echo "=========================================================="
echo " Launching Web-Based Linux Process Time-Travel Debugger    "
echo "=========================================================="

if [ ! -f "${ROOT_DIR}/build/backend/ttd_backend" ]; then
    echo "[!] Backend binary not found. Running build..."
    "${ROOT_DIR}/scripts/build.sh"
fi

# Clean shutdown function
cleanup() {
    echo ""
    echo "[INFO] Shutting down services cleanly..."
    if [ -n "${BACKEND_PID:-}" ]; then
        kill -TERM "${BACKEND_PID}" 2>/dev/null || true
    fi
    if [ -n "${FRONTEND_PID:-}" ]; then
        kill -TERM "${FRONTEND_PID}" 2>/dev/null || true
    fi
    wait 2>/dev/null || true
    echo "[INFO] All services stopped."
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

# 1. Start C++ Backend Daemon
echo "[1/2] Starting C++ Backend on port ${BACKEND_PORT}..."
"${ROOT_DIR}/build/backend/ttd_backend" "${BACKEND_PORT}" &
BACKEND_PID=$!

# Wait briefly for backend to initialize
sleep 1

# 2. Start Frontend Dev Server
echo "[2/2] Starting Frontend Vite Server on port ${FRONTEND_PORT}..."
cd "${ROOT_DIR}/frontend"
npm run dev -- --port "${FRONTEND_PORT}" --host "0.0.0.0" &
FRONTEND_PID=$!

echo ""
echo "----------------------------------------------------------"
echo " Ready! Open your browser at: http://localhost:${FRONTEND_PORT}"
echo " Backend REST & WebSocket API: http://localhost:${BACKEND_PORT}/api/health"
echo " Press Ctrl+C to terminate both servers."
echo "----------------------------------------------------------"
echo ""

wait "${BACKEND_PID}" "${FRONTEND_PID}"

