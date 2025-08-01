---
name: cpp-test
description: Comprehensive testing automation for ScreenMonitor with GoogleTest integration and coverage reporting
---

#!/bin/bash
# ScreenMonitor Test Automation
# Executes GoogleTest suites with detailed reporting and coverage analysis

set -e  # Exit on error

echo "🧪 ScreenMonitor Test Suite Automation"
echo "======================================"

# Phase 1: Test Environment Setup
echo ""
echo "🔍 Phase 1: Test Environment Check"
echo "----------------------------------"

# Validate project structure
if [ ! -f "ScreenMonitor/CMakeLists.txt" ]; then
    echo "❌ Error: ScreenMonitor/CMakeLists.txt not found"
    echo "   Please run this command from the C_capture project root"
    exit 1
fi

# Parse command line arguments
TEST_TYPE="all"
BUILD_TYPE="Release"
VERBOSE=false
PARALLEL=true
FILTER=""
COVERAGE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --all|-a)
            TEST_TYPE="all"
            shift
            ;;
        --unit|-u)
            TEST_TYPE="unit"
            shift
            ;;
        --integration|-i)
            TEST_TYPE="integration"
            shift
            ;;
        --performance|-p)
            TEST_TYPE="performance"
            shift
            ;;
        --debug|-d)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release|-r)
            BUILD_TYPE="Release"
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --sequential|-s)
            PARALLEL=false
            shift
            ;;
        --filter|-f)
            FILTER="$2"
            shift 2
            ;;
        --coverage|-c)
            COVERAGE=true
            BUILD_TYPE="Debug"  # Coverage requires Debug build
            shift
            ;;
        --help|-h)
            echo "Usage: /cpp-test [OPTIONS]"
            echo ""
            echo "Test Types:"
            echo "  -a, --all           Run all tests (default)"
            echo "  -u, --unit          Run unit tests only"
            echo "  -i, --integration   Run integration tests only"
            echo "  -p, --performance   Run performance/benchmark tests only"
            echo ""
            echo "Build Options:"
            echo "  -d, --debug         Use Debug build (default for coverage)"
            echo "  -r, --release       Use Release build (default)"
            echo ""
            echo "Execution Options:"
            echo "  -v, --verbose       Verbose test output"
            echo "  -s, --sequential    Run tests sequentially (not parallel)"
            echo "  -f, --filter PATTERN  Run tests matching pattern"
            echo "  -c, --coverage      Generate coverage report (forces Debug)"
            echo "  -h, --help          Show this help message"
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "📋 Test Configuration:"
echo "   - Test Type: $TEST_TYPE"
echo "   - Build Type: $BUILD_TYPE"
echo "   - Verbose Output: $VERBOSE"
echo "   - Parallel Execution: $PARALLEL"
echo "   - Filter: ${FILTER:-'(none)'}"
echo "   - Coverage Analysis: $COVERAGE"

# Phase 2: Build Verification
echo ""
echo "🏗️ Phase 2: Build Verification"
echo "------------------------------"

BUILD_DIR="ScreenMonitor/build-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"

if [ ! -d "$BUILD_DIR" ]; then
    echo "⚠️  Build directory not found: $BUILD_DIR"
    echo "🔧 Running build first..."
    
    if [ "$BUILD_TYPE" = "Debug" ]; then
        /cpp-build --debug
    else
        /cpp-build --release
    fi
else
    echo "✅ Build directory found: $BUILD_DIR"
fi

# Check for test executables
cd "$BUILD_DIR"
TEST_EXECUTABLES=$(find . -name "*test*.exe" -type f 2>/dev/null)
TEST_COUNT=$(echo "$TEST_EXECUTABLES" | grep -c '.exe' || echo "0")

if [ "$TEST_COUNT" -eq 0 ]; then
    echo "❌ No test executables found"
    echo "🔧 Rebuilding with tests enabled..."
    cd ..
    /cpp-build $([ "$BUILD_TYPE" = "Debug" ] && echo "--debug" || echo "--release")
    cd "$BUILD_DIR"
    TEST_EXECUTABLES=$(find . -name "*test*.exe" -type f 2>/dev/null)
    TEST_COUNT=$(echo "$TEST_EXECUTABLES" | grep -c '.exe' || echo "0")
fi

echo "✅ Test executables found: $TEST_COUNT"

# Phase 3: Test Classification and Filtering
echo ""
echo "🏷️ Phase 3: Test Classification"
echo "-------------------------------"

# Classify tests by type
UNIT_TESTS=()
INTEGRATION_TESTS=()
PERFORMANCE_TESTS=()

while IFS= read -r test_exe; do
    [ -z "$test_exe" ] && continue
    
    test_name=$(basename "$test_exe" .exe)
    
    if [[ "$test_name" == *"Integration"* ]] || [[ "$test_name" == *"integration"* ]]; then
        INTEGRATION_TESTS+=("$test_exe")
    elif [[ "$test_name" == *"Performance"* ]] || [[ "$test_name" == *"benchmark"* ]]; then
        PERFORMANCE_TESTS+=("$test_exe")
    else
        UNIT_TESTS+=("$test_exe")
    fi
done <<< "$TEST_EXECUTABLES"

echo "📊 Test Classification:"
echo "   - Unit Tests: ${#UNIT_TESTS[@]}"
echo "   - Integration Tests: ${#INTEGRATION_TESTS[@]}"
echo "   - Performance Tests: ${#PERFORMANCE_TESTS[@]}"

# Select tests to run based on test type
TESTS_TO_RUN=()

case $TEST_TYPE in
    "all")
        TESTS_TO_RUN=("${UNIT_TESTS[@]}" "${INTEGRATION_TESTS[@]}" "${PERFORMANCE_TESTS[@]}")
        ;;
    "unit")
        TESTS_TO_RUN=("${UNIT_TESTS[@]}")
        ;;
    "integration")
        TESTS_TO_RUN=("${INTEGRATION_TESTS[@]}")
        ;;
    "performance")
        TESTS_TO_RUN=("${PERFORMANCE_TESTS[@]}")
        ;;
esac

echo "🎯 Tests selected for execution: ${#TESTS_TO_RUN[@]}"

# Phase 4: Test Execution
echo ""
echo "🚀 Phase 4: Test Execution"
echo "--------------------------"

# Prepare test execution parameters
GTEST_ARGS=""
if [ "$VERBOSE" = true ]; then
    GTEST_ARGS="$GTEST_ARGS --gtest_output=xml:test_results.xml"
fi

if [ -n "$FILTER" ]; then
    GTEST_ARGS="$GTEST_ARGS --gtest_filter=$FILTER"
fi

# Create test results directory
mkdir -p test_results
TEST_RESULTS_DIR="$(pwd)/test_results"

# Execute tests
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
FAILED_TEST_NAMES=()

echo "📋 Starting test execution..."
echo ""

for test_exe in "${TESTS_TO_RUN[@]}"; do
    if [ ! -f "$test_exe" ]; then
        echo "⚠️  Test executable not found: $test_exe"
        continue
    fi
    
    test_name=$(basename "$test_exe" .exe)
    echo "🔍 Running: $test_name"
    
    # Prepare individual test result file
    RESULT_FILE="$TEST_RESULTS_DIR/${test_name}_result.xml"
    
    # Run the test
    if [ "$VERBOSE" = true ]; then
        "$test_exe" $GTEST_ARGS --gtest_output=xml:"$RESULT_FILE"
        TEST_EXIT_CODE=$?
    else
        "$test_exe" $GTEST_ARGS --gtest_output=xml:"$RESULT_FILE" 2>/dev/null
        TEST_EXIT_CODE=$?
    fi
    
    if [ $TEST_EXIT_CODE -eq 0 ]; then
        echo "   ✅ PASSED"
        ((PASSED_TESTS++))
    else
        echo "   ❌ FAILED (exit code: $TEST_EXIT_CODE)"
        ((FAILED_TESTS++))
        FAILED_TEST_NAMES+=("$test_name")
    fi
    
    ((TOTAL_TESTS++))
done

# Phase 5: Test Results Analysis
echo ""
echo "📊 Phase 5: Test Results Analysis"
echo "---------------------------------"

# Parse XML results for detailed statistics
TOTAL_TEST_CASES=0
TOTAL_ASSERTIONS=0
FAILED_ASSERTIONS=0

if command -v xmllint &> /dev/null; then
    for xml_file in "$TEST_RESULTS_DIR"/*.xml; do
        if [ -f "$xml_file" ]; then
            # Extract test statistics from XML
            TEST_CASES=$(xmllint --xpath "count(//testcase)" "$xml_file" 2>/dev/null || echo "0")
            TOTAL_TEST_CASES=$((TOTAL_TEST_CASES + ${TEST_CASES%.*}))
        fi
    done
fi

echo "📈 Detailed Test Statistics:"
echo "   - Test Executables: $TOTAL_TESTS"
echo "   - Test Cases: $TOTAL_TEST_CASES"
echo "   - Passed Executables: $PASSED_TESTS"
echo "   - Failed Executables: $FAILED_TESTS"

if [ $FAILED_TESTS -gt 0 ]; then
    echo ""
    echo "❌ Failed Tests:"
    for failed_test in "${FAILED_TEST_NAMES[@]}"; do
        echo "   - $failed_test"
    done
fi

# Phase 6: Coverage Analysis (if requested)
if [ "$COVERAGE" = true ]; then
    echo ""
    echo "📊 Phase 6: Coverage Analysis"
    echo "-----------------------------"
    
    if command -v gcov &> /dev/null; then
        echo "🔍 Generating coverage report..."
        
        # Find .gcno and .gcda files
        GCNO_FILES=$(find . -name "*.gcno" | wc -l)
        GCDA_FILES=$(find . -name "*.gcda" | wc -l)
        
        echo "   - GCNO files found: $GCNO_FILES"
        echo "   - GCDA files found: $GCDA_FILES"
        
        if [ $GCDA_FILES -gt 0 ]; then
            gcov $(find . -name "*.gcda") > coverage_raw.txt 2>&1
            
            # Generate coverage summary
            echo "✅ Coverage analysis complete"
            echo "   - Raw coverage data: coverage_raw.txt"
        else
            echo "⚠️  No coverage data found. Ensure tests were built with coverage flags."
        fi
    else
        echo "⚠️  gcov not available. Coverage analysis skipped."
    fi
fi

# Return to project root
cd - > /dev/null

# Phase 7: Final Report
echo ""
echo "🎉 ScreenMonitor Test Suite Complete!"
echo "====================================="
echo ""
echo "📖 Test Summary:"
echo "   - Total Test Executables: $TOTAL_TESTS"
echo "   - Passed: $PASSED_TESTS"
echo "   - Failed: $FAILED_TESTS"
echo "   - Success Rate: $(( PASSED_TESTS * 100 / TOTAL_TESTS ))%"
echo "   - Test Results Directory: $BUILD_DIR/test_results"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo "✅ All tests passed successfully!"
    echo ""
    echo "💡 Next Steps:"
    echo "   - Run performance analysis: /cpp-test --performance"
    echo "   - Generate coverage report: /cpp-test --coverage"
    echo "   - Build release version: /cpp-build --release"
else
    echo "❌ Some tests failed. Review the failed tests above."
    echo ""
    echo "💡 Troubleshooting:"
    echo "   - Run failed tests individually with --verbose flag"
    echo "   - Check test logs in: $BUILD_DIR/test_results/"
    echo "   - Rebuild in debug mode: /cpp-build --debug"
    
    exit 1
fi

echo ""
echo "🔧 Available Commands:"
echo "   - /cpp-build         # Rebuild the project"
echo "   - /cpp-test --verbose # Run tests with detailed output"
echo "   - /cpp-clean         # Clean build artifacts"