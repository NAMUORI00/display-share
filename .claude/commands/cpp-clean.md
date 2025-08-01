---
name: cpp-clean
description: Comprehensive cleanup automation for ScreenMonitor build artifacts, cache files, and temporary data
---

#!/bin/bash
# ScreenMonitor Cleanup Automation
# Removes build artifacts, CMake cache, and temporary files

set -e  # Exit on error

echo "🧹 ScreenMonitor Cleanup Automation"
echo "==================================="

# Phase 1: Cleanup Scope Analysis
echo ""
echo "🔍 Phase 1: Cleanup Scope Analysis"
echo "----------------------------------"

# Validate project structure
if [ ! -f "ScreenMonitor/CMakeLists.txt" ]; then
    echo "❌ Error: ScreenMonitor/CMakeLists.txt not found"
    echo "   Please run this command from the C_capture project root"
    exit 1
fi

# Parse command line arguments
CLEAN_BUILDS=true
CLEAN_CACHE=false
CLEAN_LOGS=false
CLEAN_TESTS=false
CLEAN_TEMP=true
CLEAN_SUBMODULES=false
CLEAN_ALL=false
DRY_RUN=false
FORCE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --builds|-b)
            CLEAN_BUILDS=true
            shift
            ;;
        --cache|-c)
            CLEAN_CACHE=true
            shift
            ;;
        --logs|-l)
            CLEAN_LOGS=true
            shift
            ;;
        --tests|-t)
            CLEAN_TESTS=true
            shift
            ;;
        --temp|-T)
            CLEAN_TEMP=true
            shift
            ;;
        --submodules|-s)
            CLEAN_SUBMODULES=true
            shift
            ;;
        --all|-a)
            CLEAN_ALL=true
            CLEAN_BUILDS=true
            CLEAN_CACHE=true
            CLEAN_LOGS=true
            CLEAN_TESTS=true
            CLEAN_TEMP=true
            shift
            ;;
        --dry-run|-n)
            DRY_RUN=true
            shift
            ;;
        --force|-f)
            FORCE=true
            shift
            ;;
        --help|-h)
            echo "Usage: /cpp-clean [OPTIONS]"
            echo ""
            echo "Cleanup Targets:"
            echo "  -b, --builds      Clean build directories (default)"
            echo "  -c, --cache       Clean CMake cache files"
            echo "  -l, --logs        Clean log files"
            echo "  -t, --tests       Clean test result files"
            echo "  -T, --temp        Clean temporary files (default)"
            echo "  -s, --submodules  Clean submodule build artifacts"
            echo "  -a, --all         Clean everything (except submodules)"
            echo ""
            echo "Options:"
            echo "  -n, --dry-run     Show what would be cleaned (don't delete)"
            echo "  -f, --force       Force cleanup without confirmation"
            echo "  -h, --help        Show this help message"
            echo ""
            echo "Examples:"
            echo "  /cpp-clean                    # Clean builds and temp files"
            echo "  /cpp-clean --all --dry-run    # Preview full cleanup"
            echo "  /cpp-clean --cache --force    # Force clean CMake cache"
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "📋 Cleanup Configuration:"
echo "   - Build Directories: $CLEAN_BUILDS"
echo "   - CMake Cache: $CLEAN_CACHE"
echo "   - Log Files: $CLEAN_LOGS"
echo "   - Test Results: $CLEAN_TESTS"
echo "   - Temporary Files: $CLEAN_TEMP"
echo "   - Submodule Artifacts: $CLEAN_SUBMODULES"
echo "   - Dry Run Mode: $DRY_RUN"
echo "   - Force Mode: $FORCE"

# Phase 2: Disk Usage Analysis
echo ""
echo "💾 Phase 2: Disk Usage Analysis"
echo "-------------------------------"

# Calculate current disk usage
if command -v du &> /dev/null; then
    TOTAL_SIZE_BEFORE=$(du -sh . 2>/dev/null | cut -f1 || echo "Unknown")
    echo "📊 Current project size: $TOTAL_SIZE_BEFORE"
    
    # Analyze individual components
    echo ""
    echo "📋 Component Analysis:"
    
    # Build directories
    BUILD_DIRS=$(find . -maxdepth 3 -name "build*" -type d 2>/dev/null || true)
    if [ -n "$BUILD_DIRS" ]; then
        BUILD_SIZE=$(du -sh $BUILD_DIRS 2>/dev/null | awk '{sum += $1} END {print sum}' || echo "0")
        echo "   - Build directories: $(echo "$BUILD_DIRS" | wc -l) dirs"
    fi
    
    # CMake cache files
    CMAKE_FILES=$(find . -name "CMakeCache.txt" -o -name "CMakeFiles" -type d 2>/dev/null | wc -l || echo "0")
    if [ "$CMAKE_FILES" -gt 0 ]; then
        echo "   - CMake cache files: $CMAKE_FILES items"
    fi
    
    # Log files
    LOG_FILES=$(find . -name "*.log" -o -name "*.out" 2>/dev/null | wc -l || echo "0")
    if [ "$LOG_FILES" -gt 0 ]; then
        echo "   - Log files: $LOG_FILES files"
    fi
    
    # Test result files
    TEST_FILES=$(find . -name "*test*.xml" -o -name "test_results" -type d 2>/dev/null | wc -l || echo "0")
    if [ "$TEST_FILES" -gt 0 ]; then
        echo "   - Test result files: $TEST_FILES items"
    fi
fi

# Phase 3: Safety Confirmation
echo ""
echo "⚠️  Phase 3: Safety Confirmation"
echo "--------------------------------"

if [ "$DRY_RUN" = false ] && [ "$FORCE" = false ]; then
    echo "This will permanently delete the selected files and directories."
    echo ""
    echo "Items to be cleaned:"
    
    if [ "$CLEAN_BUILDS" = true ]; then
        echo "   ✅ Build directories (ScreenMonitor/build*)"
    fi
    
    if [ "$CLEAN_CACHE" = true ]; then
        echo "   ✅ CMake cache files (CMakeCache.txt, CMakeFiles/)"
    fi
    
    if [ "$CLEAN_LOGS" = true ]; then
        echo "   ✅ Log files (*.log, *.out)"
    fi
    
    if [ "$CLEAN_TESTS" = true ]; then
        echo "   ✅ Test result files (test_results/, *.xml)"
    fi
    
    if [ "$CLEAN_TEMP" = true ]; then
        echo "   ✅ Temporary files (*.tmp, *.temp, core dumps)"
    fi
    
    if [ "$CLEAN_SUBMODULES" = true ]; then
        echo "   ✅ Submodule build artifacts"
    fi
    
    echo ""
    read -p "Continue with cleanup? [y/N]: " -n 1 -r
    echo
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ Cleanup cancelled by user"
        exit 0
    fi
fi

# Phase 4: Cleanup Execution
echo ""
echo "🚀 Phase 4: Cleanup Execution"
echo "-----------------------------"

ITEMS_CLEANED=0
ERRORS=0

# Function to safely remove items
safe_remove() {
    local item="$1"
    local description="$2"
    
    if [ "$DRY_RUN" = true ]; then
        echo "   [DRY RUN] Would remove: $item"
        return 0
    fi
    
    if [ -e "$item" ]; then
        echo "   🗑️  Removing: $item"
        if rm -rf "$item" 2>/dev/null; then
            ((ITEMS_CLEANED++))
            echo "      ✅ Removed ($description)"
        else
            echo "      ❌ Failed to remove ($description)"
            ((ERRORS++))
        fi
    fi
}

# Clean build directories
if [ "$CLEAN_BUILDS" = true ]; then
    echo "🏗️ Cleaning build directories..."
    
    # Main build directories
    for build_dir in ScreenMonitor/build ScreenMonitor/build-debug ScreenMonitor/build-release; do
        if [ -d "$build_dir" ]; then
            safe_remove "$build_dir" "Build directory"
        fi
    done
    
    # Additional build patterns
    find ScreenMonitor -maxdepth 1 -name "build*" -type d 2>/dev/null | while read -r build_dir; do
        safe_remove "$build_dir" "Build directory"
    done
fi

# Clean CMake cache
if [ "$CLEAN_CACHE" = true ]; then
    echo "📝 Cleaning CMake cache..."
    
    # CMakeCache.txt files
    find . -name "CMakeCache.txt" -type f 2>/dev/null | while read -r cache_file; do
        safe_remove "$cache_file" "CMake cache file"
    done
    
    # CMakeFiles directories
    find . -name "CMakeFiles" -type d 2>/dev/null | while read -r cmake_dir; do
        safe_remove "$cmake_dir" "CMake files directory"
    done
    
    # CMake temporary files
    find . -name "cmake_install.cmake" -o -name "install_manifest.txt" 2>/dev/null | while read -r cmake_file; do
        safe_remove "$cmake_file" "CMake temporary file"
    done
fi

# Clean log files
if [ "$CLEAN_LOGS" = true ]; then
    echo "📋 Cleaning log files..."
    
    # Various log file patterns
    find . -name "*.log" -o -name "*.out" -o -name "*.err" 2>/dev/null | while read -r log_file; do
        safe_remove "$log_file" "Log file"
    done
    
    # Specific log directories
    if [ -d "logs" ]; then
        safe_remove "logs" "Log directory"
    fi
fi

# Clean test results
if [ "$CLEAN_TESTS" = true ]; then
    echo "🧪 Cleaning test results..."
    
    # Test result files
    find . -name "*test*.xml" -o -name "test_results" -type d -o -name "coverage_raw.txt" 2>/dev/null | while read -r test_item; do
        safe_remove "$test_item" "Test result file/directory"
    done
    
    # Coverage files
    find . -name "*.gcov" -o -name "*.gcda" -o -name "*.gcno" 2>/dev/null | while read -r coverage_file; do
        safe_remove "$coverage_file" "Coverage file"
    done
fi

# Clean temporary files
if [ "$CLEAN_TEMP" = true ]; then
    echo "🗂️ Cleaning temporary files..."
    
    # Common temporary file patterns
    find . -name "*.tmp" -o -name "*.temp" -o -name "*.bak" -o -name "*~" 2>/dev/null | while read -r temp_file; do
        safe_remove "$temp_file" "Temporary file"
    done
    
    # Core dumps and crash files
    find . -name "core" -o -name "core.*" -o -name "*.dmp" 2>/dev/null | while read -r core_file; do
        safe_remove "$core_file" "Core dump file"
    done
    
    # IDE temporary files
    find . -name ".vs" -o -name "*.user" -o -name "*.suo" -type f 2>/dev/null | while read -r ide_file; do
        safe_remove "$ide_file" "IDE temporary file"
    done
fi

# Clean submodule build artifacts
if [ "$CLEAN_SUBMODULES" = true ]; then
    echo "📦 Cleaning submodule build artifacts..."
    
    SUBMODULE_DIRS=("ScreenMonitor/external/opencv" "ScreenMonitor/external/googletest" "ScreenMonitor/external/screen_capture_lite")
    
    for submodule in "${SUBMODULE_DIRS[@]}"; do
        if [ -d "$submodule" ]; then
            # Clean build directories in submodules
            find "$submodule" -name "build*" -type d 2>/dev/null | while read -r sub_build; do
                safe_remove "$sub_build" "Submodule build directory"
            done
            
            # Clean CMake cache in submodules
            find "$submodule" -name "CMakeCache.txt" -o -name "CMakeFiles" -type d 2>/dev/null | while read -r sub_cache; do
                safe_remove "$sub_cache" "Submodule CMake cache"
            done
        fi
    done
fi

# Phase 5: Post-Cleanup Analysis
echo ""
echo "📊 Phase 5: Post-Cleanup Analysis"
echo "---------------------------------"

if [ "$DRY_RUN" = false ]; then
    # Calculate disk usage after cleanup
    if command -v du &> /dev/null; then
        TOTAL_SIZE_AFTER=$(du -sh . 2>/dev/null | cut -f1 || echo "Unknown")
        echo "💾 Project size after cleanup: $TOTAL_SIZE_AFTER"
        echo "   (was: $TOTAL_SIZE_BEFORE)"
    fi
    
    echo "📈 Cleanup Statistics:"
    echo "   - Items cleaned: $ITEMS_CLEANED"
    echo "   - Errors encountered: $ERRORS"
    
    if [ $ERRORS -gt 0 ]; then
        echo "⚠️  Some items could not be removed (permissions or in use)"
    fi
else
    echo "🔍 Dry run completed - no files were actually removed"
fi

# Verify critical files still exist
echo ""
echo "✅ Critical File Verification:"
CRITICAL_FILES=("ScreenMonitor/CMakeLists.txt" "ScreenMonitor/src/main.cpp" ".gitmodules")
for critical_file in "${CRITICAL_FILES[@]}"; do
    if [ -f "$critical_file" ]; then
        echo "   ✅ $critical_file"
    else
        echo "   ❌ $critical_file (MISSING!)"
    fi
done

echo ""
echo "🎉 ScreenMonitor Cleanup Complete!"
echo "=================================="
echo ""

if [ "$DRY_RUN" = false ]; then
    echo "📖 Cleanup Summary:"
    echo "   - Operation: Cleanup completed"
    echo "   - Items removed: $ITEMS_CLEANED"
    echo "   - Errors: $ERRORS"
    echo ""
    echo "💡 Next Steps:"
    echo "   - Rebuild project: /cpp-build"
    echo "   - Initialize submodules: /cpp-submodules --init"
    echo "   - Check project status: git status"
else
    echo "📖 Dry Run Summary:"
    echo "   - Items that would be cleaned: $(grep -c "Would remove" <<< "$OUTPUT" || echo "0")"
    echo "   - Use without --dry-run to perform actual cleanup"
    echo ""
    echo "💡 To perform cleanup:"
    echo "   - /cpp-clean --force     # Skip confirmation"
    echo "   - /cpp-clean --all       # Clean everything"
fi

echo ""
echo "🔧 Available Commands:"
echo "   - /cpp-build             # Rebuild after cleanup"
echo "   - /cpp-submodules --init # Reinitialize submodules"
echo "   - /cpp-clean --help      # View all cleanup options"