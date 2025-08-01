---
name: cpp-dev-setup
description: Complete development environment setup automation for ScreenMonitor including dependencies, submodules, and initial build
---

#!/bin/bash
# ScreenMonitor Development Environment Setup
# Complete automation for new developer onboarding and environment preparation

set -e  # Exit on error

echo "🚀 ScreenMonitor Development Environment Setup"
echo "=============================================="

# Phase 1: System Requirements Check
echo ""
echo "🔍 Phase 1: System Requirements Verification"
echo "--------------------------------------------"

# Parse command line arguments
SKIP_SYSTEM_CHECK=false
FORCE_SETUP=false
BUILD_TYPE="Release"
INIT_SUBMODULES=true
RUN_TESTS=true

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-system-check)
            SKIP_SYSTEM_CHECK=true
            shift
            ;;
        --force|-f)
            FORCE_SETUP=true
            shift
            ;;
        --debug|-d)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-submodules)
            INIT_SUBMODULES=false
            shift
            ;;
        --no-tests)
            RUN_TESTS=false
            shift
            ;;
        --help|-h)
            echo "Usage: /cpp-dev-setup [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --skip-system-check   Skip system requirements verification"
            echo "  -f, --force           Force setup even if already configured"
            echo "  -d, --debug           Setup for Debug development (default: Release)"
            echo "  --no-submodules       Skip Git submodule initialization"
            echo "  --no-tests            Skip running tests after setup"
            echo "  -h, --help            Show this help message"
            echo ""
            echo "This command will:"
            echo "  1. Verify system requirements (CMake, Git, compilers)"
            echo "  2. Initialize and update Git submodules"
            echo "  3. Configure and build the project"
            echo "  4. Run basic tests to verify setup"
            echo "  5. Generate development environment report"
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Validate project structure
if [ ! -f "ScreenMonitor/CMakeLists.txt" ]; then
    echo "❌ Error: ScreenMonitor/CMakeLists.txt not found"
    echo "   Please run this command from the C_capture project root"
    exit 1
fi

echo "📋 Setup Configuration:"
echo "   - Skip System Check: $SKIP_SYSTEM_CHECK"
echo "   - Force Setup: $FORCE_SETUP"
echo "   - Build Type: $BUILD_TYPE"
echo "   - Initialize Submodules: $INIT_SUBMODULES"
echo "   - Run Tests: $RUN_TESTS"

# System Requirements Check
if [ "$SKIP_SYSTEM_CHECK" = false ]; then
    echo ""
    echo "🔧 System Requirements Check:"
    echo "----------------------------"
    
    REQUIREMENTS_MET=true
    
    # Check Git
    if command -v git &> /dev/null; then
        GIT_VERSION=$(git --version | cut -d' ' -f3)
        echo "   ✅ Git: $GIT_VERSION"
    else
        echo "   ❌ Git: Not found"
        REQUIREMENTS_MET=false
    fi
    
    # Check CMake
    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -1 | cut -d' ' -f3)
        CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
        CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)
        
        if [ "$CMAKE_MAJOR" -ge 3 ] && [ "$CMAKE_MINOR" -ge 16 ]; then
            echo "   ✅ CMake: $CMAKE_VERSION"
        else
            echo "   ⚠️  CMake: $CMAKE_VERSION (recommend 3.16+)"
        fi
    else
        echo "   ❌ CMake: Not found (required)"
        REQUIREMENTS_MET=false
    fi
    
    # Check C++ Compiler
    CXX_FOUND=false
    if command -v g++ &> /dev/null; then
        GXX_VERSION=$(g++ --version | head -1)
        echo "   ✅ g++: $GXX_VERSION"
        CXX_FOUND=true
    fi
    
    if command -v clang++ &> /dev/null; then
        CLANG_VERSION=$(clang++ --version | head -1)
        echo "   ✅ clang++: $CLANG_VERSION"
        CXX_FOUND=true
    fi
    
    # Check for MSVC on Windows
    if command -v cl &> /dev/null; then
        echo "   ✅ MSVC: Available"
        CXX_FOUND=true
    fi
    
    if [ "$CXX_FOUND" = false ]; then
        echo "   ❌ C++ Compiler: Not found (g++, clang++, or MSVC required)"
        REQUIREMENTS_MET=false
    fi
    
    # Check optional tools
    echo ""
    echo "🔧 Optional Tools:"
    echo "-----------------"
    
    if command -v ninja &> /dev/null; then
        echo "   ✅ Ninja: $(ninja --version) (faster builds)"
    else
        echo "   ⚠️  Ninja: Not found (recommended for faster builds)"
    fi
    
    if command -v ccache &> /dev/null; then
        echo "   ✅ ccache: $(ccache --version | head -1) (build caching)"
    else
        echo "   ⚠️  ccache: Not found (recommended for build caching)"
    fi
    
    if [ "$REQUIREMENTS_MET" = false ]; then
        echo ""
        echo "❌ System requirements not met!"
        echo "   Please install missing tools and run again"
        echo "   Or use --skip-system-check to proceed anyway"
        exit 1
    fi
    
    echo ""
    echo "✅ All system requirements satisfied"
fi

# Phase 2: Project Environment Analysis
echo ""
echo "📂 Phase 2: Project Environment Analysis"
echo "----------------------------------------"

# Check if this looks like a fresh setup or existing development environment
EXISTING_BUILD_DIRS=$(find ScreenMonitor -maxdepth 1 -name "build*" -type d 2>/dev/null | wc -l)
SUBMODULE_STATUS=$(git submodule status 2>/dev/null | wc -l || echo "0")
INITIALIZED_SUBMODULES=$(git submodule status 2>/dev/null | grep -v "^-" | wc -l || echo "0")

echo "📊 Environment Analysis:"
echo "   - Existing build directories: $EXISTING_BUILD_DIRS"
echo "   - Total submodules: $SUBMODULE_STATUS"
echo "   - Initialized submodules: $INITIALIZED_SUBMODULES"

FRESH_SETUP=true
if [ $EXISTING_BUILD_DIRS -gt 0 ] || [ $INITIALIZED_SUBMODULES -gt 0 ]; then
    FRESH_SETUP=false
fi

echo "   - Environment type: $([ "$FRESH_SETUP" = true ] && echo "Fresh setup" || echo "Existing development")"

if [ "$FRESH_SETUP" = false ] && [ "$FORCE_SETUP" = false ]; then
    echo ""
    echo "⚠️  Existing development environment detected!"
    echo "   This may overwrite existing build configurations"
    echo "   Use --force to proceed anyway"
    echo ""
    read -p "Continue with setup? [y/N]: " -n 1 -r
    echo
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ Setup cancelled by user"
        exit 0
    fi
fi

# Phase 3: Git Submodule Initialization
if [ "$INIT_SUBMODULES" = true ]; then
    echo ""
    echo "📦 Phase 3: Git Submodule Setup"
    echo "-------------------------------"
    
    echo "🔄 Initializing and updating submodules..."
    /cpp-submodules --init --force
    
    echo ""
    echo "🔧 Verifying critical submodule configurations..."
    
    # Ensure ImGui is on docking branch
    if [ -d "ScreenMonitor/external/imgui" ]; then
        cd "ScreenMonitor/external/imgui"
        CURRENT_BRANCH=$(git branch --show-current 2>/dev/null || echo "detached")
        if [ "$CURRENT_BRANCH" != "docking" ]; then
            echo "   🔧 Switching ImGui to docking branch..."
            git checkout docking 2>/dev/null || git checkout origin/docking 2>/dev/null || echo "   ⚠️  Could not switch to docking branch"
        else
            echo "   ✅ ImGui is on docking branch"
        fi
        cd - > /dev/null
    fi
else
    echo ""
    echo "⏭️  Phase 3: Skipping submodule initialization"
fi

# Phase 4: Initial Build Configuration
echo ""
echo "🏗️ Phase 4: Initial Build Setup"
echo "-------------------------------"

echo "🔧 Setting up build environment for $BUILD_TYPE..."

# Clean any existing build directories for fresh start
if [ "$FORCE_SETUP" = true ]; then
    echo "🧹 Cleaning existing build artifacts..."
    /cpp-clean --builds --cache --force
fi

# Perform initial build
echo "🚀 Performing initial build..."
if [ "$BUILD_TYPE" = "Debug" ]; then
    /cpp-build --debug
else
    /cpp-build --release
fi

# Phase 5: Development Environment Validation
echo ""
echo "✅ Phase 5: Environment Validation"
echo "----------------------------------"

BUILD_DIR="ScreenMonitor/build-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"

# Check build artifacts
echo "🔍 Validating build artifacts..."

MAIN_EXECUTABLE=""
if [ "$BUILD_TYPE" = "Debug" ]; then
    MAIN_EXECUTABLE="bin/Debug/SmartScreenCapture.exe"
elif [ "$BUILD_TYPE" = "Release" ]; then
    MAIN_EXECUTABLE="bin/Release/SmartScreenCapture.exe"
fi

cd "$BUILD_DIR"

if [ -f "$MAIN_EXECUTABLE" ]; then
    echo "   ✅ Main executable: $MAIN_EXECUTABLE"
    
    # Get executable info
    EXE_SIZE=$(du -h "$MAIN_EXECUTABLE" | cut -f1)
    echo "   📏 Executable size: $EXE_SIZE"
else
    echo "   ❌ Main executable not found!"
fi

# Check test executables
TEST_EXECUTABLES=$(find . -name "*test*.exe" -type f 2>/dev/null | wc -l)
echo "   🧪 Test executables: $TEST_EXECUTABLES"

# Check libraries
LIBRARY_FILES=$(find . -name "*.lib" -o -name "*.a" -o -name "*.so" -o -name "*.dll" 2>/dev/null | wc -l)
echo "   📚 Library files: $LIBRARY_FILES"

cd - > /dev/null

# Phase 6: Initial Testing (if requested)
if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo "🧪 Phase 6: Initial Testing"
    echo "---------------------------"
    
    echo "🚀 Running basic tests to verify setup..."
    
    # Run a subset of tests to verify everything works
    if /cpp-test --unit 2>/dev/null; then
        echo "✅ Basic tests passed - environment is ready!"
    else
        echo "⚠️  Some tests failed - environment may need attention"
        echo "   This is not necessarily a problem for development"
    fi
else
    echo ""
    echo "⏭️  Phase 6: Skipping initial testing"
fi

# Phase 7: Development Environment Report
echo ""
echo "📊 Phase 7: Development Environment Report"
echo "------------------------------------------"

echo "🎉 Development Environment Setup Complete!"
echo ""
echo "📋 Environment Summary:"
echo "   - Project: ScreenMonitor (C_capture)"
echo "   - Build Type: $BUILD_TYPE"
echo "   - Build Directory: $BUILD_DIR"
echo "   - Submodules: $(git submodule status 2>/dev/null | wc -l || echo "0") initialized"
echo "   - Main Executable: $([ -f "$BUILD_DIR/$MAIN_EXECUTABLE" ] && echo "✅ Available" || echo "❌ Missing")"
echo "   - Test Suite: $([ $TEST_EXECUTABLES -gt 0 ] && echo "✅ $TEST_EXECUTABLES tests" || echo "❌ No tests")"

echo ""
echo "📁 Key Directories:"
echo "   - Source Code: ScreenMonitor/src/"
echo "   - Headers: ScreenMonitor/include/"
echo "   - Tests: ScreenMonitor/tests/"
echo "   - Config: ScreenMonitor/config/"
echo "   - External Dependencies: ScreenMonitor/external/"
echo "   - Build Output: $BUILD_DIR"

echo ""
echo "🔧 Available Development Commands:"
echo "   - /cpp-build           # Build the project"
echo "   - /cpp-test            # Run test suites" 
echo "   - /cpp-submodules      # Manage dependencies"
echo "   - /cpp-clean           # Clean build artifacts"

echo ""
echo "💡 Next Steps for Development:"
echo "   1. Explore the codebase: ScreenMonitor/src/"
echo "   2. Review architecture: ScreenMonitor/CLAUDE.md"
echo "   3. Run the application: ./$BUILD_DIR/$MAIN_EXECUTABLE"
echo "   4. Make changes and test: /cpp-build && /cpp-test"
echo "   5. Check documentation: All CLAUDE.md files in subdirectories"

echo ""
echo "🚨 Troubleshooting:"
echo "   - Build issues: /cpp-clean --all && /cpp-build"
echo "   - Submodule problems: /cpp-submodules --reset --force"
echo "   - Missing dependencies: /cpp-dev-setup --force"

# Create a development status file
DEV_STATUS_FILE="DEVELOPMENT_STATUS.md"
if [ "$FORCE_SETUP" = true ] || [ ! -f "$DEV_STATUS_FILE" ]; then
    cat > "$DEV_STATUS_FILE" << EOF
# ScreenMonitor Development Environment Status

**Setup Date:** $(date)
**Build Type:** $BUILD_TYPE
**Setup Script:** /cpp-dev-setup

## Environment Details
- Build Directory: $BUILD_DIR
- Main Executable: $([ -f "$BUILD_DIR/$MAIN_EXECUTABLE" ] && echo "✅ $MAIN_EXECUTABLE" || echo "❌ Not found")
- Test Executables: $TEST_EXECUTABLES
- Submodules Initialized: $(git submodule status 2>/dev/null | grep -v "^-" | wc -l || echo "0")

## Quick Commands
\`\`\`bash
# Build project
/cpp-build

# Run tests  
/cpp-test

# Clean and rebuild
/cpp-clean --all && /cpp-build

# Update dependencies
/cpp-submodules --update
\`\`\`

## Last Setup Command
\`\`\`bash
/cpp-dev-setup$([ "$BUILD_TYPE" = "Debug" ] && echo " --debug" || echo "")$([ "$INIT_SUBMODULES" = false ] && echo " --no-submodules" || echo "")$([ "$RUN_TESTS" = false ] && echo " --no-tests" || echo "")
\`\`\`
EOF
    
    echo ""
    echo "📝 Development status saved to: $DEV_STATUS_FILE"
fi

echo ""
echo "🎉 Welcome to ScreenMonitor Development!"
echo "======================================="
echo "Your development environment is ready for C++ development."