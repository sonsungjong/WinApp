@echo off
setlocal

set "SDK_DIR=%~dp0ThirdParty\libmpv"
set "ARCHIVE_URL=https://github.com/zhongfly/mpv-winbuild/releases/download/2026-11-04-858e0e5087/mpv-dev-lgpl-x86_64-20261104-git-858e0e5087.7z"
set "ARCHIVE_FILE=%SDK_DIR%\mpv-dev-lgpl.7z"

if /I not "%~1"=="--force" (
    if exist "%SDK_DIR%\include\mpv\client.h" (
        if exist "%SDK_DIR%\libmpv-2.dll" (
            if exist "%SDK_DIR%\libmpv.lib" (
                echo [setup_sdk] libmpv SDK is already installed at "%SDK_DIR%".
                exit /b 0
            )
        )
    )
)

echo [setup_sdk] Preparing directory "%SDK_DIR%"...
if not exist "%SDK_DIR%" mkdir "%SDK_DIR%"

echo [setup_sdk] Downloading libmpv SDK...
curl.exe -L --fail "%ARCHIVE_URL%" -o "%ARCHIVE_FILE%"
if errorlevel 1 (
    echo [setup_sdk] ERROR: Failed to download libmpv archive.
    exit /b 1
)

echo [setup_sdk] Extracting archive...
tar.exe -xf "%ARCHIVE_FILE%" -C "%SDK_DIR%"
if errorlevel 1 (
    echo [setup_sdk] ERROR: Failed to extract libmpv archive.
    exit /b 1
)

del /f /q "%ARCHIVE_FILE%"

if exist "%SDK_DIR%\libmpv.dll.a" (
    copy /y "%SDK_DIR%\libmpv.dll.a" "%SDK_DIR%\libmpv.lib" >nul
)

echo [setup_sdk] Successfully set up libmpv SDK in "%SDK_DIR%".
exit /b 0
