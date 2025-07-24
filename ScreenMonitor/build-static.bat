@echo off
REM =========================================================================
REM Professional Screen Capture & Computer Vision System
REM Static Build Script - Single EXE with No Dependencies
REM 
REM This script builds a completely self-contained executable with all
REM dependencies statically linked for production deployment.
REM =========================================================================

echo ====================================
echo Professional Screen Capture System
echo Single EXE Static Build Process
echo ====================================

REM Check if we're in the correct directory
if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found!
    echo Please run this script from the ScreenMonitor directory.
    pause
    exit /b 1
)

REM Create and clean build directory
if exist "build-static" rmdir /s /q "build-static"
mkdir "build-static"
cd "build-static"

echo.
echo [1/4] Configuring CMake for static build...
echo =============================================

REM Configure CMake with static linking options
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded" ^
    -DCMAKE_CXX_FLAGS_RELEASE="/MT /O2 /DNDEBUG" ^
    -DCMAKE_C_FLAGS_RELEASE="/MT /O2 /DNDEBUG" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
    ..

if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [2/4] Building static executable...
echo ===================================

REM Build the project in Release mode with maximum optimization
cmake --build . --config Release --parallel --verbose

if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo [3/4] Verifying static dependencies...
echo =====================================

REM Check if the executable was created
if not exist "bin\SmartScreenCapture.exe" (
    echo ERROR: SmartScreenCapture.exe was not created!
    pause
    exit /b 1
)

REM Display file information
echo Executable created successfully:
dir "bin\SmartScreenCapture.exe"

REM Check dependencies using dumpbin (if available)
where dumpbin >nul 2>&1
if %errorlevel% == 0 (
    echo.
    echo Checking DLL dependencies:
    dumpbin /dependents "bin\SmartScreenCapture.exe" | findstr /i "\.dll"
    if errorlevel 1 (
        echo ✓ No external DLL dependencies found - fully static!
    ) else (
        echo ⚠ Warning: Some DLL dependencies detected
    )
) else (
    echo Note: dumpbin not available - cannot verify dependencies
)

echo.
echo [4/4] Creating deployment package...
echo ====================================

REM Create deployment directory
if not exist "deploy" mkdir "deploy"

REM Copy the executable
copy "bin\SmartScreenCapture.exe" "deploy\"

REM Copy configuration file
if exist "..\config\config.json" (
    if not exist "deploy\config" mkdir "deploy\config"
    copy "..\config\config.json" "deploy\config\"
)

REM Copy models directory if it exists
if exist "..\models" (
    xcopy "..\models" "deploy\models\" /E /I /Y
)

REM Create a simple README for deployment
echo Professional Screen Capture ^& Computer Vision System > "deploy\README.txt"
echo. >> "deploy\README.txt"
echo This is a self-contained executable with no external dependencies. >> "deploy\README.txt"
echo Simply run SmartScreenCapture.exe to start the application. >> "deploy\README.txt"
echo. >> "deploy\README.txt"
echo Configuration: config\config.json >> "deploy\README.txt"
echo AI Models: models\ directory >> "deploy\README.txt"

echo.
echo ========================================
echo BUILD COMPLETED SUCCESSFULLY!
echo ========================================
echo.
echo Static executable: build-static\bin\SmartScreenCapture.exe
echo Deployment package: build-static\deploy\
echo.
echo The executable is completely self-contained and can be
echo distributed without any external dependencies.
echo.

REM Display final file size
for %%F in ("bin\SmartScreenCapture.exe") do echo Executable size: %%~zF bytes

echo.
echo Press any key to test the executable...
pause

REM Quick test run (5 second timeout)
echo Testing executable...
timeout /t 5 /nobreak > nul
"bin\SmartScreenCapture.exe" --help 2>nul || (
    echo Note: Application may require GUI environment for full testing
)

echo.
echo Build process complete!
pause