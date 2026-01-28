@echo off
echo Checking for Visual Studio instances...
tasklist /FI "IMAGENAME eq devenv.exe" 2>NUL | find /I /N "devenv.exe" >NUL
if "%ERRORLEVEL%"=="0" (
    echo WARNING: Visual Studio appears to be running.
    echo Please close Visual Studio before running this script, or files may be locked.
    echo.
    pause
)

echo Cleaning up tmp64 directory...
if exist tmp64 (
    rmdir /s /q tmp64 2>nul
    if exist tmp64 (
        echo Warning: Could not fully delete tmp64. Please close Visual Studio and delete it manually.
        pause
        exit /b 1
    )
)

echo Running cmake...
cmake -B tmp64 -A x64
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo CMake failed. Common issues:
    echo 1. Make sure Visual Studio is completely closed
    echo 2. Make sure git is installed and in PATH
    echo 3. Make sure you have internet access for downloading vcpkg
    echo 4. Check that WXWIN environment variable is set to your wxWidgets installation
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo CMake configuration completed successfully!
echo You can now open tmp64\phd2.sln in Visual Studio
pause
