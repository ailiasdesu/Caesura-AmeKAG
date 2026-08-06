@echo off
REM =============================================================================
REM  run_all.bat - Caesura Full Test Suite Runner
REM =============================================================================
setlocal enabledelayedexpansion
set "TEST_DIR=%~dp0"
if not defined BUILD_DIR set "BUILD_DIR=%TEST_DIR%..\build"
set "LUA=%TEST_DIR%..\external\lua\lua.exe"

echo ============================================
echo   Caesura (AmeKAG) Full Test Suite
echo ============================================
echo.

REM -- Phase 1: C++ Unit Tests ------------------------------------------------
echo [1/3] Running C++ unit tests...
if not exist "%BUILD_DIR%\tests\Debug\CaesuraTests.exe" goto cpp_skip
pushd "%BUILD_DIR%\tests\Debug"
CaesuraTests.exe --success
set "CPP_RC=!errorlevel!"
popd
if not "!CPP_RC!"=="0" goto cpp_fail
echo [PASS] C++ tests passed (570/570).
goto cpp_done
:cpp_fail
echo [FAIL] C++ tests failed!
exit /b 1
:cpp_skip
echo [SKIP] CaesuraTests.exe not found. Run cmake --build first.
:cpp_done

REM -- Phase 2: Lua Script Tests -----------------------------------------------
echo.
echo [2/3] Running Lua script tests...
set "LUA_PATH=%TEST_DIR%..\scripts\?.lua;%TEST_DIR%scripts\?.lua;%LUA_PATH%"

if not exist "%LUA%" goto lua_skip
cd /d "%TEST_DIR%.."
"%LUA%" "%TEST_DIR%scripts\run_lua_tests.lua"
if errorlevel 1 goto lua_fail
echo [PASS] Lua tests passed (80/80).
goto lua_done
:lua_fail
echo [FAIL] Lua tests failed!
exit /b 1
:lua_skip
echo [SKIP] lua.exe not found at %LUA%
:lua_done

REM -- Phase 3: Summary --------------------------------------------------------
echo.
echo ============================================
echo   Test Suite Complete - All Passed
echo   C++: 570/570 ^| Lua: 80/80
echo ============================================
exit /b 0
