---
name: cpp-analyze
description: Comprehensive code quality and performance analysis for ScreenMonitor including static analysis, metrics, and optimization recommendations
---

#!/bin/bash
# ScreenMonitor Code Analysis and Quality Assessment
# Comprehensive static analysis, metrics collection, and performance recommendations

set -e  # Exit on error

echo "🔍 ScreenMonitor Code Analysis Suite"
echo "===================================="

# Phase 1: Analysis Configuration
echo ""
echo "⚙️ Phase 1: Analysis Configuration"
echo "----------------------------------"

# Validate project structure
if [ ! -f "ScreenMonitor/CMakeLists.txt" ]; then
    echo "❌ Error: ScreenMonitor/CMakeLists.txt not found"
    echo "   Please run this command from the C_capture project root"
    exit 1
fi

# Parse command line arguments
ANALYZE_CODE=true
ANALYZE_BUILD=true
ANALYZE_DEPS=true
ANALYZE_PERF=false
ANALYZE_SECURITY=false
GENERATE_REPORT=true
OUTPUT_FORMAT="console"
DETAILED=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --code|-c)
            ANALYZE_CODE=true
            ANALYZE_BUILD=false
            ANALYZE_DEPS=false
            shift
            ;;
        --build|-b)
            ANALYZE_BUILD=true
            ANALYZE_CODE=false
            ANALYZE_DEPS=false
            shift
            ;;
        --dependencies|-d)
            ANALYZE_DEPS=true
            ANALYZE_CODE=false
            ANALYZE_BUILD=false
            shift
            ;;
        --performance|-p)
            ANALYZE_PERF=true
            shift
            ;;
        --security|-s)
            ANALYZE_SECURITY=true
            shift
            ;;
        --all|-a)
            ANALYZE_CODE=true
            ANALYZE_BUILD=true
            ANALYZE_DEPS=true
            ANALYZE_PERF=true
            ANALYZE_SECURITY=true
            shift
            ;;
        --detailed)
            DETAILED=true
            shift
            ;;
        --report|-r)
            GENERATE_REPORT=true
            OUTPUT_FORMAT="markdown"
            shift
            ;;
        --json)
            OUTPUT_FORMAT="json"
            shift
            ;;
        --help|-h)
            echo "Usage: /cpp-analyze [OPTIONS]"
            echo ""
            echo "Analysis Types:"
            echo "  -c, --code          Analyze source code quality (default)"
            echo "  -b, --build         Analyze build system and configuration"
            echo "  -d, --dependencies  Analyze external dependencies"
            echo "  -p, --performance   Analyze performance characteristics"
            echo "  -s, --security      Analyze security vulnerabilities"
            echo "  -a, --all           Run all analysis types"
            echo ""
            echo "Output Options:"
            echo "  --detailed          Generate detailed analysis output"
            echo "  -r, --report        Generate markdown report"
            echo "  --json              Output results in JSON format"
            echo "  -h, --help          Show this help message"
            echo ""
            echo "Examples:"
            echo "  /cpp-analyze                    # Basic code analysis"
            echo "  /cpp-analyze --all --detailed   # Comprehensive analysis"
            echo "  /cpp-analyze --build --report   # Build system analysis with report"
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "📋 Analysis Configuration:"
echo "   - Code Quality: $ANALYZE_CODE"
echo "   - Build System: $ANALYZE_BUILD"
echo "   - Dependencies: $ANALYZE_DEPS"
echo "   - Performance: $ANALYZE_PERF"
echo "   - Security: $ANALYZE_SECURITY"
echo "   - Detailed Output: $DETAILED"
echo "   - Output Format: $OUTPUT_FORMAT"

# Create analysis output directory
ANALYSIS_DIR="analysis_results"
mkdir -p "$ANALYSIS_DIR"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Phase 2: Project Structure Analysis
echo ""
echo "📊 Phase 2: Project Structure Analysis"
echo "--------------------------------------"

# Basic project metrics
echo "📈 Basic Project Metrics:"

# Count files by type
CPP_FILES=$(find ScreenMonitor -name "*.cpp" -o -name "*.cxx" -o -name "*.cc" | wc -l)
HEADER_FILES=$(find ScreenMonitor -name "*.h" -o -name "*.hpp" -o -name "*.hxx" | wc -l)
CMAKE_FILES=$(find ScreenMonitor -name "CMakeLists.txt" -o -name "*.cmake" | wc -l)
TEST_FILES=$(find ScreenMonitor -name "*test*.cpp" -o -name "*Test*.cpp" | wc -l)
JSON_FILES=$(find ScreenMonitor -name "*.json" | wc -l)

echo "   - C++ Source Files: $CPP_FILES"
echo "   - Header Files: $HEADER_FILES"
echo "   - CMake Files: $CMAKE_FILES"
echo "   - Test Files: $TEST_FILES"
echo "   - Configuration Files: $JSON_FILES"

# Calculate lines of code
if command -v wc &> /dev/null; then
    TOTAL_LOC=$(find ScreenMonitor -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo "0")
    SOURCE_LOC=$(find ScreenMonitor -name "*.cpp" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo "0")
    HEADER_LOC=$(find ScreenMonitor -name "*.h" -o -name "*.hpp" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo "0")
    
    echo "   - Total Lines of Code: $TOTAL_LOC"
    echo "   - Source LOC: $SOURCE_LOC"
    echo "   - Header LOC: $HEADER_LOC"
fi

# Directory structure analysis
echo ""
echo "📁 Directory Structure:"
echo "   - Source modules: $(find ScreenMonitor/src -maxdepth 1 -type d | wc -l)"
echo "   - Include modules: $(find ScreenMonitor/include -maxdepth 1 -type d | wc -l)"
echo "   - External dependencies: $(find ScreenMonitor/external -maxdepth 1 -type d | wc -l)"

# Phase 3: Code Quality Analysis
if [ "$ANALYZE_CODE" = true ]; then
    echo ""
    echo "🔍 Phase 3: Code Quality Analysis"
    echo "---------------------------------"
    
    echo "📊 Code Quality Metrics:"
    
    # Function and class analysis
    if command -v grep &> /dev/null; then
        FUNCTION_COUNT=$(grep -r "^[[:space:]]*[a-zA-Z_][a-zA-Z0-9_]*::[a-zA-Z_][a-zA-Z0-9_]*(" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
        CLASS_COUNT=$(grep -r "^[[:space:]]*class[[:space:]]\+[a-zA-Z_]" ScreenMonitor/include/ ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
        INTERFACE_COUNT=$(grep -r "^[[:space:]]*class[[:space:]]\+I[A-Z]" ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
        
        echo "   - Estimated Functions: $FUNCTION_COUNT"
        echo "   - Classes: $CLASS_COUNT"
        echo "   - Interfaces: $INTERFACE_COUNT"
    fi
    
    # Include analysis
    INCLUDE_STATEMENTS=$(grep -r "#include" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
    SYSTEM_INCLUDES=$(grep -r "#include <" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
    LOCAL_INCLUDES=$(grep -r "#include \"" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Total Includes: $INCLUDE_STATEMENTS"
    echo "   - System Includes: $SYSTEM_INCLUDES"
    echo "   - Local Includes: $LOCAL_INCLUDES"
    
    # Comment analysis
    COMMENT_LINES=$(grep -r "//" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
    BLOCK_COMMENTS=$(grep -r "/\*" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Line Comments: $COMMENT_LINES"
    echo "   - Block Comments: $BLOCK_COMMENTS"
    
    # Code patterns analysis
    echo ""
    echo "🔍 Code Pattern Analysis:"
    
    # Modern C++ features
    AUTO_USAGE=$(grep -r "\bauto\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    SMART_PTRS=$(grep -r "std::\(unique_ptr\|shared_ptr\|weak_ptr\)" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    LAMBDA_USAGE=$(grep -r "\[\]" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Auto keyword usage: $AUTO_USAGE"
    echo "   - Smart pointer usage: $SMART_PTRS"
    echo "   - Lambda expressions: $LAMBDA_USAGE"
    
    # Error handling patterns
    TRY_CATCH=$(grep -r "try\|catch" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    ASSERTIONS=$(grep -r "assert\|ASSERT" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Exception handling: $TRY_CATCH"
    echo "   - Assertions: $ASSERTIONS"
    
    # Potential issues detection
    echo ""
    echo "⚠️  Potential Issues Detection:"
    
    # Check for common issues
    TODO_COMMENTS=$(grep -r "TODO\|FIXME\|HACK" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | wc -l || echo "0")
    MAGIC_NUMBERS=$(grep -r "\b[0-9]\{3,\}\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    LONG_LINES=$(find ScreenMonitor -name "*.cpp" -o -name "*.h" | xargs awk 'length > 120 {count++} END {print count+0}' 2>/dev/null || echo "0")
    
    echo "   - TODO/FIXME comments: $TODO_COMMENTS"
    echo "   - Potential magic numbers: $MAGIC_NUMBERS"
    echo "   - Long lines (>120 chars): $LONG_LINES"
    
    if [ "$DETAILED" = true ]; then
        echo ""
        echo "📝 Detailed Code Issues:"
        
        if [ $TODO_COMMENTS -gt 0 ]; then
            echo "   🔍 TODO/FIXME locations:"
            grep -rn "TODO\|FIXME\|HACK" ScreenMonitor/src/ ScreenMonitor/include/ 2>/dev/null | head -10
        fi
        
        if [ $LONG_LINES -gt 0 ]; then
            echo ""
            echo "   📏 Long lines (sample):"
            find ScreenMonitor -name "*.cpp" -o -name "*.h" | xargs awk 'length > 120 {print FILENAME ":" NR ": " $0}' 2>/dev/null | head -5
        fi
    fi
fi

# Phase 4: Build System Analysis
if [ "$ANALYZE_BUILD" = true ]; then
    echo ""
    echo "🏗️ Phase 4: Build System Analysis"
    echo "---------------------------------"
    
    echo "📊 CMake Configuration Analysis:"
    
    # Analyze CMakeLists.txt
    if [ -f "ScreenMonitor/CMakeLists.txt" ]; then
        CMAKE_VERSION_REQ=$(grep "cmake_minimum_required" ScreenMonitor/CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/' || echo "Unknown")
        CXX_STANDARD=$(grep "CMAKE_CXX_STANDARD" ScreenMonitor/CMakeLists.txt | sed 's/.*CMAKE_CXX_STANDARD \([0-9]*\).*/\1/' || echo "Unknown")
        
        echo "   - CMake Version Required: $CMAKE_VERSION_REQ"
        echo "   - C++ Standard: $CXX_STANDARD"
    fi
    
    # Find all targets
    EXECUTABLE_TARGETS=$(grep -r "add_executable" ScreenMonitor/ 2>/dev/null | wc -l || echo "0")
    LIBRARY_TARGETS=$(grep -r "add_library" ScreenMonitor/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Executable targets: $EXECUTABLE_TARGETS"
    echo "   - Library targets: $LIBRARY_TARGETS"
    
    # Analyze dependencies
    FIND_PACKAGE_CALLS=$(grep -r "find_package" ScreenMonitor/ 2>/dev/null | wc -l || echo "0")
    TARGET_LINK_CALLS=$(grep -r "target_link_libraries" ScreenMonitor/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Find package calls: $FIND_PACKAGE_CALLS"
    echo "   - Target link libraries: $TARGET_LINK_CALLS"
    
    # Build directory analysis
    BUILD_DIRS=$(find ScreenMonitor -maxdepth 1 -name "build*" -type d 2>/dev/null | wc -l)
    echo "   - Build directories: $BUILD_DIRS"
    
    if [ $BUILD_DIRS -gt 0 ]; then
        echo ""
        echo "📁 Build Directory Analysis:"
        for build_dir in $(find ScreenMonitor -maxdepth 1 -name "build*" -type d 2>/dev/null); do
            if [ -d "$build_dir" ]; then
                BUILD_SIZE=$(du -sh "$build_dir" 2>/dev/null | cut -f1 || echo "Unknown")
                BUILD_FILES=$(find "$build_dir" -type f 2>/dev/null | wc -l || echo "0")
                echo "   - $build_dir: $BUILD_SIZE ($BUILD_FILES files)"
            fi
        done
    fi
    
    if [ "$DETAILED" = true ]; then
        echo ""
        echo "🔍 Detailed Build Analysis:"
        
        # Check for common CMake patterns
        if [ -f "ScreenMonitor/CMakeLists.txt" ]; then
            echo "   📋 CMake Features Used:"
            grep -o "target_[a-z_]*" ScreenMonitor/CMakeLists.txt 2>/dev/null | sort | uniq -c | head -10
        fi
    fi
fi

# Phase 5: Dependency Analysis
if [ "$ANALYZE_DEPS" = true ]; then
    echo ""
    echo "📦 Phase 5: Dependency Analysis"
    echo "-------------------------------"
    
    echo "🔗 Git Submodule Analysis:"
    
    # Submodule status
    TOTAL_SUBMODULES=$(git submodule status 2>/dev/null | wc -l || echo "0")
    INITIALIZED_SUBMODULES=$(git submodule status 2>/dev/null | grep -v "^-" | wc -l || echo "0")
    OUTDATED_SUBMODULES=$(git submodule status 2>/dev/null | grep "^+" | wc -l || echo "0")
    
    echo "   - Total submodules: $TOTAL_SUBMODULES"
    echo "   - Initialized: $INITIALIZED_SUBMODULES"
    echo "   - Outdated: $OUTDATED_SUBMODULES"
    
    if [ $TOTAL_SUBMODULES -gt 0 ]; then
        echo ""
        echo "📋 Submodule Details:"
        git submodule status 2>/dev/null | while read -r status_line; do
            STATUS_CHAR=$(echo "$status_line" | cut -c1)
            COMMIT=$(echo "$status_line" | awk '{print $1}' | sed 's/^.//')
            PATH=$(echo "$status_line" | awk '{print $2}')
            
            case $STATUS_CHAR in
                '-') STATUS_DESC="Not initialized" ;;
                '+') STATUS_DESC="Outdated" ;;
                ' ') STATUS_DESC="Up to date" ;;
                *) STATUS_DESC="Unknown" ;;
            esac
            
            printf "   %-35s %s\n" "$PATH" "$STATUS_DESC"
        done
    fi
    
    # Analyze external libraries
    echo ""
    echo "📚 External Library Analysis:"
    
    if [ -d "ScreenMonitor/external" ]; then
        EXTERNAL_LIBS=$(find ScreenMonitor/external -maxdepth 1 -type d | wc -l)
        echo "   - External library directories: $((EXTERNAL_LIBS - 1))"
        
        # Analyze each external library
        for lib_dir in ScreenMonitor/external/*/; do
            if [ -d "$lib_dir" ]; then
                LIB_NAME=$(basename "$lib_dir")
                LIB_SIZE=$(du -sh "$lib_dir" 2>/dev/null | cut -f1 || echo "Unknown")
                LIB_FILES=$(find "$lib_dir" -type f 2>/dev/null | wc -l || echo "0")
                
                echo "   - $LIB_NAME: $LIB_SIZE ($LIB_FILES files)"
            fi
        done
    fi
    
    # Check for version information
    if [ "$DETAILED" = true ]; then
        echo ""
        echo "🔍 Detailed Dependency Information:"
        
        # Try to extract version information from common locations
        for lib_dir in ScreenMonitor/external/*/; do
            if [ -d "$lib_dir" ]; then
                LIB_NAME=$(basename "$lib_dir")
                
                # Check for version files
                for version_file in "version.txt" "VERSION" "CMakeLists.txt" "package.json"; do
                    if [ -f "$lib_dir/$version_file" ]; then
                        echo "   📋 $LIB_NAME version info found in: $version_file"
                        break
                    fi
                done
            fi
        done
    fi
fi

# Phase 6: Performance Analysis
if [ "$ANALYZE_PERF" = true ]; then
    echo ""
    echo "⚡ Phase 6: Performance Analysis"
    echo "-------------------------------"
    
    echo "🎯 Performance Characteristics:"
    
    # Look for performance-related patterns
    STD_VECTOR_USAGE=$(grep -r "std::vector" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    STD_MAP_USAGE=$(grep -r "std::map" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    STD_UNORDERED_USAGE=$(grep -r "std::unordered" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - std::vector usage: $STD_VECTOR_USAGE"
    echo "   - std::map usage: $STD_MAP_USAGE"
    echo "   - Unordered containers: $STD_UNORDERED_USAGE"
    
    # Threading patterns
    THREAD_USAGE=$(grep -r "std::thread\|std::mutex\|std::atomic" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    ASYNC_USAGE=$(grep -r "std::async\|std::future" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Threading primitives: $THREAD_USAGE"
    echo "   - Async patterns: $ASYNC_USAGE"
    
    # Memory management patterns
    NEW_DELETE_USAGE=$(grep -r "\bnew\b\|\bdelete\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    MALLOC_FREE_USAGE=$(grep -r "\bmalloc\b\|\bfree\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Raw new/delete: $NEW_DELETE_USAGE"
    echo "   - malloc/free: $MALLOC_FREE_USAGE"
    
    # Build time analysis
    if [ -d "ScreenMonitor/build-release" ] || [ -d "ScreenMonitor/build-debug" ]; then
        echo ""
        echo "🕐 Build Time Estimates:"
        
        # Look for build logs or time information
        for build_dir in ScreenMonitor/build*/; do
            if [ -d "$build_dir" ]; then
                BUILD_NAME=$(basename "$build_dir")
                echo "   - $BUILD_NAME: Build artifacts present"
            fi
        done
    fi
fi

# Phase 7: Security Analysis
if [ "$ANALYZE_SECURITY" = true ]; then
    echo ""
    echo "🔒 Phase 7: Security Analysis"
    echo "-----------------------------"
    
    echo "🛡️ Security Pattern Analysis:"
    
    # Look for potentially dangerous functions
    STRCPY_USAGE=$(grep -r "\bstrcpy\b\|\bstrcat\b\|\bsprintf\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    GETS_USAGE=$(grep -r "\bgets\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    BUFFER_FUNCS=$(grep -r "\bmemcpy\b\|\bmemmove\b" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Unsafe string functions: $STRCPY_USAGE"
    echo "   - Gets usage: $GETS_USAGE"
    echo "   - Buffer manipulation: $BUFFER_FUNCS"
    
    # Input validation patterns
    INPUT_VALIDATION=$(grep -r "validate\|sanitize\|check.*input" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    BOUNDS_CHECKING=$(grep -r "bounds\|range.*check" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Input validation: $INPUT_VALIDATION"
    echo "   - Bounds checking: $BOUNDS_CHECKING"
    
    # Cryptographic usage
    CRYPTO_USAGE=$(grep -r "encrypt\|decrypt\|hash\|crypto" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    RANDOM_USAGE=$(grep -r "rand\|random" ScreenMonitor/src/ 2>/dev/null | wc -l || echo "0")
    
    echo "   - Cryptographic operations: $CRYPTO_USAGE"
    echo "   - Random number usage: $RANDOM_USAGE"
    
    if [ "$DETAILED" = true ] && [ $STRCPY_USAGE -gt 0 ]; then
        echo ""
        echo "⚠️  Potentially unsafe function usage:"
        grep -rn "\bstrcpy\b\|\bstrcat\b\|\bsprintf\b" ScreenMonitor/src/ 2>/dev/null | head -5
    fi
fi

# Phase 8: Report Generation
if [ "$GENERATE_REPORT" = true ]; then
    echo ""
    echo "📊 Phase 8: Report Generation"
    echo "-----------------------------"
    
    REPORT_FILE="$ANALYSIS_DIR/analysis_report_$TIMESTAMP"
    
    case $OUTPUT_FORMAT in
        "markdown")
            REPORT_FILE="$REPORT_FILE.md"
            cat > "$REPORT_FILE" << EOF
# ScreenMonitor Code Analysis Report

**Generated:** $(date)
**Analysis Types:** $([ "$ANALYZE_CODE" = true ] && echo "Code") $([ "$ANALYZE_BUILD" = true ] && echo "Build") $([ "$ANALYZE_DEPS" = true ] && echo "Dependencies") $([ "$ANALYZE_PERF" = true ] && echo "Performance") $([ "$ANALYZE_SECURITY" = true ] && echo "Security")

## Project Overview

- **C++ Source Files:** $CPP_FILES
- **Header Files:** $HEADER_FILES
- **Total Lines of Code:** $TOTAL_LOC
- **Test Files:** $TEST_FILES

## Quality Metrics

$([ "$ANALYZE_CODE" = true ] && cat << CODEEOF
### Code Quality
- **Functions:** $FUNCTION_COUNT
- **Classes:** $CLASS_COUNT
- **Interfaces:** $INTERFACE_COUNT
- **TODO/FIXME Comments:** $TODO_COMMENTS
- **Long Lines:** $LONG_LINES

CODEEOF
)

$([ "$ANALYZE_BUILD" = true ] && cat << BUILDEOF
### Build System
- **CMake Version Required:** $CMAKE_VERSION_REQ
- **C++ Standard:** $CXX_STANDARD
- **Executable Targets:** $EXECUTABLE_TARGETS
- **Library Targets:** $LIBRARY_TARGETS

BUILDEOF
)

$([ "$ANALYZE_DEPS" = true ] && cat << DEPSEOF
### Dependencies
- **Total Submodules:** $TOTAL_SUBMODULES
- **Initialized:** $INITIALIZED_SUBMODULES
- **Outdated:** $OUTDATED_SUBMODULES

DEPSEOF
)

## Recommendations

1. **Code Quality:** $([ $TODO_COMMENTS -gt 5 ] && echo "Address TODO/FIXME comments" || echo "Good documentation practice")
2. **Build System:** $([ "$CXX_STANDARD" = "Unknown" ] && echo "Specify C++ standard explicitly" || echo "Build configuration looks good")
3. **Dependencies:** $([ $OUTDATED_SUBMODULES -gt 0 ] && echo "Update outdated submodules" || echo "Dependencies are up to date")

---
*Generated by /cpp-analyze*
EOF
            ;;
            
        "json")
            REPORT_FILE="$REPORT_FILE.json"
            cat > "$REPORT_FILE" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "project": "ScreenMonitor",
  "analysis_types": {
    "code": $ANALYZE_CODE,
    "build": $ANALYZE_BUILD,
    "dependencies": $ANALYZE_DEPS,
    "performance": $ANALYZE_PERF,
    "security": $ANALYZE_SECURITY
  },
  "metrics": {
    "files": {
      "cpp_files": $CPP_FILES,
      "header_files": $HEADER_FILES,
      "test_files": $TEST_FILES,
      "cmake_files": $CMAKE_FILES
    },
    "code": {
      "total_loc": $TOTAL_LOC,
      "source_loc": $SOURCE_LOC,
      "header_loc": $HEADER_LOC,
      "functions": $FUNCTION_COUNT,
      "classes": $CLASS_COUNT,
      "interfaces": $INTERFACE_COUNT
    },
    "quality": {
      "todo_comments": $TODO_COMMENTS,
      "long_lines": $LONG_LINES,
      "comment_lines": $COMMENT_LINES
    },
    "dependencies": {
      "total_submodules": $TOTAL_SUBMODULES,
      "initialized_submodules": $INITIALIZED_SUBMODULES,
      "outdated_submodules": $OUTDATED_SUBMODULES
    }
  },
  "generated_by": "/cpp-analyze"
}
EOF
            ;;
    esac
    
    echo "📄 Analysis report generated: $REPORT_FILE"
fi

# Phase 9: Summary and Recommendations
echo ""
echo "🎉 Analysis Complete!"
echo "==================="
echo ""
echo "📊 Summary Statistics:"
echo "   - Total Files Analyzed: $((CPP_FILES + HEADER_FILES))"
echo "   - Lines of Code: $TOTAL_LOC"
echo "   - Code Quality Score: $([ $TODO_COMMENTS -lt 5 ] && echo "Good" || echo "Needs Attention")"
echo "   - Build System: $([ "$CXX_STANDARD" != "Unknown" ] && echo "Well Configured" || echo "Needs Review")"
echo "   - Dependencies: $([ $OUTDATED_SUBMODULES -eq 0 ] && echo "Up to Date" || echo "Updates Available")"

echo ""
echo "💡 Key Recommendations:"

# Generate smart recommendations based on analysis
RECOMMENDATIONS=()

if [ $TODO_COMMENTS -gt 10 ]; then
    RECOMMENDATIONS+=("Address TODO/FIXME comments for better code maintainability")
fi

if [ $LONG_LINES -gt 50 ]; then
    RECOMMENDATIONS+=("Consider breaking long lines for better readability")
fi

if [ $OUTDATED_SUBMODULES -gt 0 ]; then
    RECOMMENDATIONS+=("Update outdated Git submodules using /cpp-submodules --update")
fi

if [ "$CXX_STANDARD" = "Unknown" ]; then
    RECOMMENDATIONS+=("Explicitly specify C++ standard in CMakeLists.txt")
fi

if [ $NEW_DELETE_USAGE -gt 5 ] && [ "$ANALYZE_PERF" = true ]; then
    RECOMMENDATIONS+=("Consider using smart pointers instead of raw new/delete")
fi

if [ ${#RECOMMENDATIONS[@]} -eq 0 ]; then
    echo "   ✅ Project appears to be in good shape!"
    echo "   - Code quality is maintained"
    echo "   - Build system is properly configured"
    echo "   - Dependencies are up to date"
else
    for i in "${!RECOMMENDATIONS[@]}"; do
        echo "   $((i+1)). ${RECOMMENDATIONS[$i]}"
    done
fi

echo ""
echo "🔧 Next Steps:"
echo "   - Review analysis report: $REPORT_FILE"
echo "   - Address high-priority recommendations"
echo "   - Run specific analysis: /cpp-analyze --code --detailed"
echo "   - Update dependencies: /cpp-submodules --update"
echo "   - Build and test: /cpp-build && /cpp-test"

echo ""
echo "🔍 Available Analysis Commands:"
echo "   - /cpp-analyze --all --detailed  # Comprehensive analysis"
echo "   - /cpp-analyze --security        # Security-focused analysis"
echo "   - /cpp-analyze --performance     # Performance analysis"
echo "   - /cpp-build                     # Build project"
echo "   - /cpp-test                      # Run tests"