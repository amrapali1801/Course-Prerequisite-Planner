@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo   Course Prerequisite Planner - Test Runner (KKWIEER 2023)
echo ============================================================

echo [1/3] Compiling C++ Backend with g++ -std=c++17 -Wall -Wextra...
g++ -std=c++17 -Wall -Wextra main.cpp -o planner.exe
if %ERRORLEVEL% neq 0 (
    echo [FAIL] C++ Compilation failed!
    exit /b 1
)
echo [PASS] Compilation successful with 0 errors.

echo [2/3] Exporting syllabus data to web/data.js...
planner.exe --export-web
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Export to web/data.js failed!
    exit /b 1
)

echo [3/3] Running Tree Metrics Parity Test Suite (82 courses)...
where node >nul 2>nul
if %ERRORLEVEL% equ 0 (
    node tests/parity.js
    set TEST_RES=!ERRORLEVEL!
) else (
    where python >nul 2>nul
    if !ERRORLEVEL! equ 0 (
        python tests/parity.py
        set TEST_RES=!ERRORLEVEL!
    ) else (
        echo [!] Neither node nor python found in PATH to execute parity test.
        set TEST_RES=1
    )
)

if !TEST_RES! equ 0 (
    echo ============================================================
    echo  TEST SUITE STATUS: ALL 82 COURSES VERIFIED (PASS)
    echo ============================================================
    exit /b 0
) else (
    echo ============================================================
    echo  TEST SUITE STATUS: FAILED
    echo ============================================================
    exit /b 1
)
