@echo off
REM ============================================================================
REM Caesura (AmeKAG) - Shader Compilation Script (Windows)
REM ============================================================================
REM Compiles bgfx shaderc-format (.sc) GLSL shaders into platform-specific
REM binary shaders using bgfx`s shaderc tool.
REM
REM Usage:
REM   compile_shaders.bat [platform]
REM
REM Platforms:
REM   linux   - OpenGL (GLSL 120)
REM   windows - Direct3D 11 (DXBC via HLSL)
REM   macos   - Metal (MSL)
REM   all     - Compile for all platforms (default)
REM
REM Prerequisites:
REM   - bgfx shaderc.exe built and available in PATH or set SHADERC env var
REM   - Or set BGFX_DIR to bgfx root
REM ============================================================================

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "GLSL_DIR=%SCRIPT_DIR%glsl"
set "OUT_DIR=%SCRIPT_DIR%compiled"
set "VARYING_DEF=%SCRIPT_DIR%varying.def"

if not defined SHADERC (
    if defined BGFX_DIR (
        set "SHADERC=%BGFX_DIR%\.build\win64_vs2022\bin\shadercRelease.exe"
    ) else (
        set "SHADERC=shaderc.exe"
    )
)

set "PLATFORM=%1"
if "%PLATFORM%"=="" set "PLATFORM=all"

REM -- Create varying.def if not exists ---------------------------------------
if not exist "%VARYING_DEF%" (
    (
        echo vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0^);
        echo.
        echo vec2 a_position  : POSITION;
        echo vec2 a_texcoord0 : TEXCOORD0;
    ) > "%VARYING_DEF%"
    echo [shaders] Created varying.def
)

REM -- Compile for a specific platform ----------------------------------------
goto :main

:compile_platform
set "PLAT=%1"
set "OUT_SUBDIR=%OUT_DIR%\%PLAT%"
if not exist "%OUT_SUBDIR%" mkdir "%OUT_SUBDIR%"

set "PROFILE_ARGS="
set "EXT=.bin"

if "%PLAT%"=="linux" (
    set "PROFILE_ARGS=--platform linux --profile 120"
)
if "%PLAT%"=="windows" (
    set "PROFILE_ARGS=--platform windows --profile vs_4_0 -O 3"
)
if "%PLAT%"=="macos" (
    set "PROFILE_ARGS=--platform osx --profile metal"
    set "EXT=.metal.bin"
)

echo.
echo === Compiling shaders for %PLAT% ===

REM Vertex shaders
for %%S in (vs_sprite vs_fullscreen stretch_blt_vs affine_blt_vs minigame_vs) do (
    set "SRC=%GLSL_DIR%\%%S.sc"
    set "OUT=%OUT_SUBDIR%\%%S!EXT!"
    if exist "!SRC!" (
        echo   [VS] %%S
        "%SHADERC%" -f "!SRC!" -o "!OUT!" --type vertex --varyingdef "%VARYING_DEF%" !PROFILE_ARGS! --depends
    ) else (
        echo   [SKIP] !SRC! not found
    )
)

REM Fragment shaders
for %%S in (fs_texture fs_blend fs_transition fs_vfx stretch_blt_fs affine_blt_fs minigame_fs) do (
    set "SRC=%GLSL_DIR%\%%S.sc"
    set "OUT=%OUT_SUBDIR%\%%S!EXT!"
    if exist "!SRC!" (
        echo   [FS] %%S
        "%SHADERC%" -f "!SRC!" -o "!OUT!" --type fragment --varyingdef "%VARYING_DEF%" !PROFILE_ARGS! --depends
    ) else (
        echo   [SKIP] !SRC! not found
    )
)

echo === %PLAT% shaders compiled to %OUT_SUBDIR% ===
exit /b 0

:main
echo ==============================================
echo  Caesura ^(AmeKAG^) Shader Compiler
echo ==============================================
echo Source dir : %GLSL_DIR%
echo Output dir : %OUT_DIR%
echo.

if "%PLATFORM%"=="all" (
    call :compile_platform linux
    call :compile_platform windows
    call :compile_platform macos
) else (
    call :compile_platform %PLATFORM%
)

echo.
echo All shaders compiled successfully.
echo Output: %OUT_DIR%

endlocal

