---
name: cpp-build
description: Comprehensive C++ build automation for ScreenMonitor with Debug/Release configurations
---

#!/bin/bash
# ScreenMonitor C++ Build Automation
# Handles CMake configuration, compilation, and validation for both Debug and Release modes

set -e  # Exit on error

echo "🏗️ ScreenMonitor Build Automation Started"
echo "=============================================="

# Phase 1: Environment and Prerequisites Check
echo ""
echo "🔍 Phase 1: Prerequisites Check"
echo "--------------------------------"

# Validate we're in the correct directory
if [ ! -f "ScreenMonitor/CMakeLists.txt" ]; then
    echo "❌ Error: ScreenMonitor/CMakeLists.txt not found"
    echo "   Please run this command from the C_capture project root"
    exit 1
fi

# Check for required build tools
if ! command -v cmake &> /dev/null; then
    echo "❌ Error: CMake not found. Please install CMake 3.16+"
    exit 1
fi

echo "✅ Project structure validated"
echo "✅ CMake found: $(cmake --version | head -1)"

# Parse command line arguments
BUILD_TYPE="Release"
CLEAN_BUILD=false
PARALLEL_JOBS=$(nproc 2>/dev/null || echo "4")

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug|-d)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release|-r)
            BUILD_TYPE="Release" 
            shift
            ;;
        --clean|-c)
            CLEAN_BUILD=true
            shift
            ;;
        --jobs|-j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: /cpp-build [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  -r, --release   Build in Release mode"  
            echo "  -c, --clean     Clean build (remove existing build directory)"
            echo "  -j, --jobs N    Use N parallel jobs (default: auto-detect)"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "📋 Build Configuration:"
echo "   - Build Type: $BUILD_TYPE"
echo "   - Parallel Jobs: $PARALLEL_JOBS"
echo "   - Clean Build: $CLEAN_BUILD"

# Phase 2: Git Submodule Verification
echo ""
echo "📦 Phase 2: Git Submodule Verification"
echo "---------------------------------------"

# Check submodule status
SUBMODULE_STATUS=$(git submodule status)
if echo "$SUBMODULE_STATUS" | grep -q "^-"; then
    echo "⚠️  Uninitialized submodules detected. Initializing..."
    git submodule update --init --recursive
    echo "✅ Submodules initialized"
elif echo "$SUBMODULE_STATUS" | grep -q "^+"; then
    echo "⚠️  Submodule updates available. Consider running /cpp-submodules --update"
fi

echo "✅ Submodule verification complete"

# Check critical submodules
CRITICAL_MODULES=("ScreenMonitor/external/opencv" "ScreenMonitor/external/imgui" "ScreenMonitor/external/googletest")
for module in "${CRITICAL_MODULES[@]}"; do
    if [ ! -d "$module" ] || [ -z "$(ls -A "$module")" ]; then
        echo "❌ Critical submodule missing: $module"
        echo "   Running submodule update..."
        git submodule update --init --recursive
        break
    fi
done

# Phase 3: Build Directory Management
echo ""
echo "🗂️ Phase 3: Build Directory Setup"
echo "----------------------------------"

BUILD_DIR="ScreenMonitor/build-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"

if [ "$CLEAN_BUILD" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "🧹 Removing existing build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
echo "✅ Build directory ready: $BUILD_DIR"

# Phase 4: CMake Configuration  
echo ""
echo "⚙️ Phase 4: CMake Configuration"
echo "-------------------------------"

cd "$BUILD_DIR"

# Configure with appropriate settings for the build type
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DBUILD_TESTING=ON
)

# Add Debug-specific options
if [ "$BUILD_TYPE" = "Debug" ]; then
    CMAKE_ARGS+=(
        -DCMAKE_CXX_FLAGS_DEBUG="-g -O0 -DDEBUG"
        -DCMAKE_C_FLAGS_DEBUG="-g -O0 -DDEBUG"
    )
fi

# Add Release-specific options
if [ "$BUILD_TYPE" = "Release" ]; then
    CMAKE_ARGS+=(
        -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG"
        -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG"
    )
fi

echo "🔧 Running CMake configuration..."
cmake "${CMAKE_ARGS[@]}" ..

if [ $? -eq 0 ]; then
    echo "✅ CMake configuration successful"
else
    echo "❌ CMake configuration failed"
    exit 1
fi

# Phase 5: Compilation
echo ""
echo "🔨 Phase 5: Compilation"
echo "-----------------------"

echo "🚀 Starting compilation with $PARALLEL_JOBS parallel jobs..."
cmake --build . --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"

if [ $? -eq 0 ]; then
    echo "✅ Compilation successful"
else
    echo "❌ Compilation failed"
    exit 1
fi

# Phase 6: Build Verification
echo ""
echo "✅ Phase 6: Build Verification"
echo "------------------------------"

# Check if main executable was created
MAIN_EXECUTABLE=""
if [ "$BUILD_TYPE" = "Debug" ]; then
    MAIN_EXECUTABLE="bin/Debug/SmartScreenCapture.exe"
elif [ "$BUILD_TYPE" = "Release" ]; then
    MAIN_EXECUTABLE="bin/Release/SmartScreenCapture.exe"
fi

if [ -f "$MAIN_EXECUTABLE" ]; then
    echo "✅ Main executable created: $MAIN_EXECUTABLE"
    echo "   Size: $(du -h "$MAIN_EXECUTABLE" | cut -f1)"
else
    echo "⚠️  Main executable not found at expected location"
    echo "   Searching for executables..."
    find . -name "*.exe" -type f 2>/dev/null | head -5
fi

# Check for test executables
TEST_EXECUTABLES=$(find . -name "*test*.exe" -type f 2>/dev/null | wc -l)
echo "✅ Test executables found: $TEST_EXECUTABLES"

# Return to project root
cd - > /dev/null

echo ""
echo "🎉 ScreenMonitor Build Complete!"
echo "================================"
echo ""
echo "📖 Build Summary:"
echo "   - Configuration: $BUILD_TYPE"
echo "   - Build Directory: $BUILD_DIR"
echo "   - Parallel Jobs Used: $PARALLEL_JOBS"
echo "   - Main Executable: $MAIN_EXECUTABLE"
echo ""
echo "💡 Next Steps:"
echo "   - Run tests: /cpp-test"
echo "   - Execute application: ./$BUILD_DIR/$MAIN_EXECUTABLE"
echo "   - View build logs: less $BUILD_DIR/CMakeFiles/CMakeOutput.log"
echo ""
echo "🔧 Available Commands:"
echo "   - /cpp-test --all    # Run all tests"
echo "   - /cpp-clean         # Clean all build artifacts"
echo "   - /cpp-submodules    # Manage Git submodules"