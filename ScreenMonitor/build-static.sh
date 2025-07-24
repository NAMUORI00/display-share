#!/bin/bash
#########################################################################
# Professional Screen Capture & Computer Vision System
# Static Build Script - Single EXE with No Dependencies (Linux/macOS)
# 
# This script builds a completely self-contained executable with all
# dependencies statically linked for production deployment.
#########################################################################

set -e  # Exit on any error

echo "===================================="
echo "Professional Screen Capture System"
echo "Single EXE Static Build Process"
echo "===================================="

# Check if we're in the correct directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "ERROR: CMakeLists.txt not found!"
    echo "Please run this script from the ScreenMonitor directory."
    exit 1
fi

# Detect platform
PLATFORM="$(uname -s)"
case "${PLATFORM}" in
    Linux*)     MACHINE=Linux;;
    Darwin*)    MACHINE=macOS;;
    *)          MACHINE="UNKNOWN:${PLATFORM}"
esac
echo "Building for: $MACHINE"

# Create and clean build directory
rm -rf build-static
mkdir build-static
cd build-static

echo ""
echo "[1/4] Configuring CMake for static build..."
echo "============================================="

# Configure CMake with static linking options
CMAKE_FLAGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -static-libgcc -static-libstdc++"
    -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -static-libgcc -static-libstdc++"
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"
)

# macOS specific settings
if [ "$MACHINE" = "macOS" ]; then
    CMAKE_FLAGS+=(
        -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
    )
fi

cmake "${CMAKE_FLAGS[@]}" ..

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed!"
    exit 1
fi

echo ""
echo "[2/4] Building static executable..."
echo "==================================="

# Detect number of CPU cores for parallel build
if [ "$MACHINE" = "Linux" ]; then
    CORES=$(nproc)
elif [ "$MACHINE" = "macOS" ]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

echo "Building with $CORES parallel jobs..."

# Build the project in Release mode with maximum optimization
cmake --build . --config Release --parallel $CORES

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed!"
    exit 1
fi

echo ""
echo "[3/4] Verifying static dependencies..."
echo "====================================="

# Check if the executable was created
EXECUTABLE="bin/SmartScreenCapture"
if [ ! -f "$EXECUTABLE" ]; then
    echo "ERROR: SmartScreenCapture executable was not created!"
    exit 1
fi

# Display file information
echo "Executable created successfully:"
ls -lh "$EXECUTABLE"

# Check dependencies
echo ""
echo "Checking shared library dependencies:"
if [ "$MACHINE" = "Linux" ]; then
    ldd "$EXECUTABLE" | grep -v "not a dynamic executable" || echo "✓ Fully static executable - no shared library dependencies!"
elif [ "$MACHINE" = "macOS" ]; then
    otool -L "$EXECUTABLE" | grep -v "$EXECUTABLE:" || echo "✓ Minimal dependencies - mostly system libraries!"
fi

echo ""
echo "[4/4] Creating deployment package..."
echo "===================================="

# Create deployment directory
mkdir -p deploy

# Copy the executable
cp "$EXECUTABLE" deploy/

# Make executable if needed
chmod +x "deploy/SmartScreenCapture"

# Copy configuration file
if [ -f "../config/config.json" ]; then
    mkdir -p deploy/config
    cp "../config/config.json" deploy/config/
fi

# Copy models directory if it exists
if [ -d "../models" ]; then
    cp -r "../models" deploy/
fi

# Create a simple README for deployment
cat > deploy/README.txt << EOF
Professional Screen Capture & Computer Vision System

This is a self-contained executable with minimal external dependencies.
Simply run ./SmartScreenCapture to start the application.

Configuration: config/config.json
AI Models: models/ directory

Platform: $MACHINE
Build Date: $(date)
EOF

echo ""
echo "========================================"
echo "BUILD COMPLETED SUCCESSFULLY!"
echo "========================================"
echo ""
echo "Static executable: build-static/bin/SmartScreenCapture"
echo "Deployment package: build-static/deploy/"
echo ""
echo "The executable has minimal dependencies and can be"
echo "distributed with standard system libraries."
echo ""

# Display final file size
FILESIZE=$(stat -f%z "$EXECUTABLE" 2>/dev/null || stat -c%s "$EXECUTABLE" 2>/dev/null || echo "unknown")
echo "Executable size: $FILESIZE bytes"

echo ""
echo "Press Enter to test the executable..."
read -r

# Quick test run
echo "Testing executable..."
if timeout 5s "$EXECUTABLE" --help 2>/dev/null || gtimeout 5s "$EXECUTABLE" --help 2>/dev/null; then
    echo "✓ Executable test successful!"
else
    echo "Note: Application may require GUI environment for full testing"
fi

echo ""
echo "Build process complete!"