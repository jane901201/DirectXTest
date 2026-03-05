@echo off
setlocal

REM =====================================
REM vcpkg_update.bat
REM Update existing vcpkg only
REM =====================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "VCPKG_DIR=%ROOT%\vcpkg"

pushd "%ROOT%" || exit /b 1

echo.
echo [1/3] Checking vcpkg...

if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo ERROR: vcpkg not found at "%VCPKG_DIR%"
    popd
    exit /b 1
)

echo.
echo [2/3] Updating vcpkg repository...

pushd "%VCPKG_DIR%"
git pull
if errorlevel 1 (
    echo ERROR: git pull failed
    popd
    popd
    exit /b 1
)
popd

echo.
echo [3/3] Rebootstrapping vcpkg...

call "%VCPKG_DIR%\bootstrap-vcpkg.bat"
if errorlevel 1 (
    echo ERROR: bootstrap failed
    popd
    exit /b 1
)

echo.
echo DONE: vcpkg updated successfully.

popd
endlocal