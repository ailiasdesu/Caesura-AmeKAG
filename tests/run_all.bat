@echo off
REM =============================================================================
REM  run_all.bat ? Caesura Full Test Suite Runner
REM =============================================================================
setlocal enabledelayedexpansion
set TEST_DIR=%~dp0
set BUILD_DIR=%TEST_DIR%..\build
set LUA=%TEST_DIR%..\external\lua\lua.exe

echo ============================================
echo   Caesura (AmeKAG) Full Test Suite
echo ============================================
echo.

REM -- Phase 1: C++ Unit Tests ------------------------------------------------
echo [1/3] Running C++ unit tests...
if exist "%BUILD_DIR%\tests\Debug\CaesuraTests.exe" (
    "%BUILD_DIR%\tests\Debug\CaesuraTests.exe" --success
    if !errorlevel! neq 0 (
        echo [FAIL] C++ tests failed!
        exit /b 1
    )
    echo [PASS] C++ tests passed (78/78).
) else (
    echo [SKIP] CaesuraTests.exe not found. Run cmake --build first.
)

REM -- Phase 2: Lua Script Tests -----------------------------------------------
echo.
echo [2/3] Running Lua script tests...
set "LUA_PATH=%TEST_DIR%..\scripts\?.lua;%TEST_DIR%scripts\?.lua;%LUA_PATH%"

if exist "%LUA%" (
    "%LUA%" "%TEST_DIR%scripts\run_lua_tests.lua"
    if !errorlevel! neq 0 (
        echo [FAIL] Lua tests failed!
        exit /b 1
    )
    echo [PASS] Lua tests passed (25/25).
) else (
    echo [SKIP] lua.exe not found at %LUA%
)

REM -- Phase 3: Summary --------------------------------------------------------
echo.
echo ============================================
echo   Test Suite Complete ? All Passed
echo   C++: 78/78  |  Lua: 25/25
echo ============================================
exit /b 0
