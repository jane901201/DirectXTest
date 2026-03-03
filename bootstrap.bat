@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "VCPKG_DIR=%ROOT%\vcpkg"

pushd "%ROOT%" || exit /b 1

echo [1/2] Checking vcpkg...
if not exist "%VCPKG_DIR%\bootstrap-vcpkg.bat" (
  if exist "%ROOT%\.gitmodules" (
    git config -f "%ROOT%\.gitmodules" --get submodule.vcpkg.path >nul 2>&1
    if not errorlevel 1 (
      echo vcpkg submodule detected. Initializing...
      git submodule update --init --recursive vcpkg
      if errorlevel 1 (
        echo Submodule initialization failed. Falling back to git clone...
        git clone https://github.com/microsoft/vcpkg "%VCPKG_DIR%"
        if errorlevel 1 exit /b 1
      )
    ) else (
      echo vcpkg not found. Cloning...
      git clone https://github.com/microsoft/vcpkg "%VCPKG_DIR%"
      if errorlevel 1 exit /b 1
    )
  ) else (
    echo vcpkg not found. Cloning...
    git clone https://github.com/microsoft/vcpkg "%VCPKG_DIR%"
    if errorlevel 1 exit /b 1
  )
)

if not exist "%VCPKG_DIR%\bootstrap-vcpkg.bat" (
  echo ERROR: vcpkg bootstrap script not found at "%VCPKG_DIR%".
  popd
  exit /b 1
)

echo [2/2] Bootstrapping vcpkg...
call "%VCPKG_DIR%\bootstrap-vcpkg.bat"
if errorlevel 1 exit /b 1

echo vcpkg setup complete.
popd
endlocal
