@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "SDK_DIR=%PROJECT_DIR%ThirdParty\libmpv"
set "ARCHIVE_URL=https://github.com/zhongfly/mpv-winbuild/releases/download/2026-11-04-858e0e5087/mpv-dev-lgpl-x86_64-20261104-git-858e0e5087.7z"
set "ARCHIVE_FILE=%SDK_DIR%\mpv-dev-lgpl.7z"

set "NEED_EXTRACT=1"
if /I not "%~1"=="--force" (
    if exist "%SDK_DIR%\include\mpv\client.h" (
        if exist "%SDK_DIR%\libmpv-2.dll" (
            if exist "%SDK_DIR%\libmpv.lib" (
                set "NEED_EXTRACT=0"
            )
        )
    )
)

if "%NEED_EXTRACT%"=="1" (
    if not exist "%SDK_DIR%" mkdir "%SDK_DIR%"

    if /I "%~1"=="--force" (
        if exist "%ARCHIVE_FILE%" del /f /q "%ARCHIVE_FILE%"
    )

    if not exist "%ARCHIVE_FILE%" (
        echo [setup_sdk] Downloading libmpv LGPL SDK archive...
        curl.exe -L --fail "%ARCHIVE_URL%" -o "%ARCHIVE_FILE%"
        if errorlevel 1 (
            echo [setup_sdk] ERROR: Failed to download libmpv archive.
            exit /b 1
        )
    )

    echo [setup_sdk] Extracting "%ARCHIVE_FILE%"...
    tar.exe -xf "%ARCHIVE_FILE%" -C "%SDK_DIR%"
    if errorlevel 1 (
        echo [setup_sdk] ERROR: Failed to extract libmpv archive.
        exit /b 1
    )

    if exist "%SDK_DIR%\libmpv.dll.a" (
        copy /y "%SDK_DIR%\libmpv.dll.a" "%SDK_DIR%\libmpv.lib" >nul
    )
)

if not exist "%PROJECT_DIR%libmpv-2.dll" (
    if exist "%SDK_DIR%\libmpv-2.dll" (
        copy /y "%SDK_DIR%\libmpv-2.dll" "%PROJECT_DIR%libmpv-2.dll" >nul
    )
)

exit /b 0
