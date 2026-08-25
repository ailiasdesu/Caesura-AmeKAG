@echo off
REM ===========================================================================
REM  Caesura AmeKAG - Android Release Keystore Generator (Track A5 - Windows)
REM
REM  Generates a standard PKCS12 release keystore using JDK keytool.
REM  Supports interactive creation, custom parameter flags, and --test mode.
REM ===========================================================================
setlocal enabledelayedexpansion

set "KEYSTORE=caesura-release.keystore"
set "ALIAS=caesura"
set "STOREPASS="
set "KEYPASS="
set "DNAME=CN=Caesura Game, OU=Release, O=CaesuraEngine, C=JP"
set "VALIDITY=10000"
set "KEYSIZE=2048"
set "IS_TEST=0"

:parse_args
if "%~1"=="" goto validate_args
if /i "%~1"=="--keystore" (
    set "KEYSTORE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--alias" (
    set "ALIAS=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--storepass" (
    set "STOREPASS=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--keypass" (
    set "KEYPASS=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--dname" (
    set "DNAME=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--validity" (
    set "VALIDITY=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--keysize" (
    set "KEYSIZE=%~2"
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--test" (
    set "IS_TEST=1"
    shift
    goto parse_args
)
if /i "%~1"=="-h" goto show_help
if /i "%~1"=="--help" goto show_help
echo ERROR: Unknown option %~1
exit /b 2

:show_help
echo Usage: scripts\generate_android_keystore.bat [options]
echo Options:
echo   --keystore ^<path^>    Output keystore path (default: caesura-release.keystore)
echo   --alias ^<alias^>      Key alias (default: caesura)
echo   --storepass ^<pass^>   Keystore password
echo   --keypass ^<pass^>     Key password
echo   --dname ^<dname^>      Distinguished name
echo   --validity ^<days^>    Validity in days (default: 10000)
echo   --keysize ^<bits^>     RSA key size (default: 2048)
echo   --test               Headless test mode with ephemeral credentials
exit /b 0

:validate_args
where keytool >nul 2>nul
if %errorlevel% neq 0 (
    if defined JAVA_HOME (
        if exist "%JAVA_HOME%\bin\keytool.exe" (
            set "KEYTOOL_CMD=%JAVA_HOME%\bin\keytool.exe"
        ) else (
            echo ERROR: 'keytool' not found in PATH or JAVA_HOME.
            exit /b 1
        )
    ) else (
        echo ERROR: 'keytool' not found in PATH or JAVA_HOME.
        exit /b 1
    )
) else (
    set "KEYTOOL_CMD=keytool"
)

if "%IS_TEST%"=="1" (
    if "%KEYSTORE%"=="caesura-release.keystore" set "KEYSTORE=caesura-test.keystore"
    if not defined ALIAS set "ALIAS=caesura-test"
    if not defined STOREPASS set "STOREPASS=caesura_test_pass"
    if not defined KEYPASS set "KEYPASS=caesura_test_pass"
    set "DNAME=CN=Caesura CI Test, OU=Engineering, O=Caesura, C=JP"
    echo === Generating Ephemeral Test Keystore (--test mode) ===
) else (
    echo === Generating Android PKCS12 Release Keystore ===
    if not defined STOREPASS (
        set /p "STOREPASS=Enter keystore password (min 6 characters): "
    )
    if not defined KEYPASS (
        set /p "KEYPASS=Enter key password (leave blank to use keystore password): "
    )
)

if not defined KEYPASS set "KEYPASS=%STOREPASS%"

if exist "%KEYSTORE%" (
    if "%IS_TEST%"=="1" (
        del /f "%KEYSTORE%"
    ) else (
        echo Keystore "%KEYSTORE%" already exists. Overwrite? (y/N)
        set /p "OVERWRITE="
        if /i "!OVERWRITE!" neq "y" (
            echo Aborted.
            exit /b 0
        )
        del /f "%KEYSTORE%"
    )
)

"%KEYTOOL_CMD%" -genkeypair -v ^
    -keystore "%KEYSTORE%" ^
    -alias "%ALIAS%" ^
    -keyalg RSA ^
    -keysize %KEYSIZE% ^
    -validity %VALIDITY% ^
    -storetype PKCS12 ^
    -storepass "%STOREPASS%" ^
    -keypass "%KEYPASS%" ^
    -dname "%DNAME%"

if %errorlevel% neq 0 (
    echo ERROR: keytool failed to generate keystore.
    exit /b %errorlevel%
)

for %%F in ("%KEYSTORE%") do set "FULL_KEYSTORE=%%~fF"

echo.
echo === Keystore successfully created ===
echo   Path     : %FULL_KEYSTORE%
echo   Alias    : %ALIAS%
echo   Type     : PKCS12
echo   Validity : %VALIDITY% days
echo.
echo Set environment variables before building:
echo   set CAESURA_ANDROID_KEYSTORE=%FULL_KEYSTORE%
echo   set CAESURA_ANDROID_KEYSTORE_PASS=%STOREPASS%
echo   set CAESURA_ANDROID_KEY_ALIAS=%ALIAS%
echo   set CAESURA_ANDROID_KEY_PASS=%KEYPASS%
echo.
exit /b 0
