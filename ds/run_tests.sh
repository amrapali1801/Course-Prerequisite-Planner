#!/usr/bin/env bash
set -e

echo "============================================================"
echo "  Course Prerequisite Planner - Test Runner (KKWIEER 2023)"
echo "============================================================"

echo "[1/3] Compiling C++ Backend with g++ -std=c++17 -Wall -Wextra..."
g++ -std=c++17 -Wall -Wextra main.cpp -o planner
echo "[PASS] Compilation successful."

echo "[2/3] Exporting syllabus data to web/data.js..."
./planner --export-web

echo "[3/3] Running Tree Metrics Parity Test Suite (82 courses)..."
if command -v node >/dev/null 2>&1; then
    node tests/parity.js
elif command -v python3 >/dev/null 2>&1; then
    python3 tests/parity.py
elif command -v python >/dev/null 2>&1; then
    python tests/parity.py
else
    echo "Error: Neither node nor python found to execute parity test."
    exit 1
fi

echo "============================================================"
echo " TEST SUITE STATUS: ALL 82 COURSES VERIFIED (PASS)"
echo "============================================================"
