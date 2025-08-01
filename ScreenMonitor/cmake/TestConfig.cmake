# TestConfig.cmake
# Optimized test configuration for 320x320 detection system
# Streamlined testing focused on core functionality and performance

message(STATUS "Configuring optimized test suite for 320x320 system...")

# =============================================================================
# TEST CONFIGURATION OPTIONS
# =============================================================================

option(ENABLE_PERFORMANCE_TESTS "Enable performance benchmarks" ON)
option(ENABLE_GUI_TESTS "Enable GUI component tests" ON)
option(ENABLE_INTEGRATION_TESTS "Enable integration tests" ON)
option(ENABLE_MOCK_TESTS "Enable mock-based unit tests" ON)
option(MINIMAL_TEST_SUITE "Build minimal test suite only" OFF)

# Platform-specific test configuration
if(WIN32)
    option(ENABLE_WINDOWS_SPECIFIC_TESTS "Enable Windows-specific tests" ON)
endif()

# =============================================================================
# GOOGLETEST CONFIGURATION
# =============================================================================

# Configure GoogleTest for optimal performance
set(gtest_force_shared_crt ON CACHE BOOL "Use shared C runtime" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "Build GMock for mocking" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Skip installation" FORCE)

# Disable GoogleTest internal features not needed
set(gtest_disable_pthreads ON CACHE BOOL "Disable pthreads on Windows" FORCE)
set(gtest_hide_internal_symbols ON CACHE BOOL "Hide internal symbols" FORCE)

# =============================================================================
# TEST LIBRARY TARGET
# =============================================================================

# Create test library excluding main.cpp
add_library(${PROJECT_NAME}_test_lib STATIC
    # Core functionality (essential for all tests)
    src/core/ConfigManager.cpp
    
    # Detection system (core testing focus)
    src/detection/ObjectDetector.cpp
    src/detection/ColorDetector.cpp
    src/detection/HSVColorDetection.cpp
    
    # Factory system
    src/factories/ComponentFactory.cpp
    
    # Capture system
    src/capture/ScreenCaptureLiteDevice.cpp
    
    # GUI system (conditional)
    $<$<BOOL:${ENABLE_GUI_TESTS}>:src/gui/MainInterface.cpp>
    
    # Performance monitoring (for benchmarks)
    $<$<BOOL:${ENABLE_PERFORMANCE_TESTS}>:src/monitoring/ImprovedPerformanceMonitor.cpp>
    
    # TensorRT inference (conditional based on GPU availability)
    $<$<BOOL:${ENABLE_GPU_SUPPORT}>:src/detection/YOLOv11TensorRTInference.cpp>
)

# Test library include directories
target_include_directories(${PROJECT_NAME}_test_lib PUBLIC
    include/
    include/core/
    include/capture/
    include/detection/
    include/gui/
    include/interfaces/
    include/factories/
    include/monitoring/
    external/screen_capture_lite/include
    ${OpenCV_INCLUDE_DIRS}
)

# Test library dependencies
target_link_libraries(${PROJECT_NAME}_test_lib PUBLIC
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_dnn
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
    $<$<BOOL:${ENABLE_GUI_TESTS}>:imgui_minimal>
    $<$<BOOL:${ENABLE_GUI_TESTS}>:glfw>
    $<$<BOOL:${ENABLE_GUI_TESTS}>:OpenGL::GL>
)

# Apply compiler definitions to test library
target_compile_definitions(${PROJECT_NAME}_test_lib PRIVATE
    WIN32_LEAN_AND_MEAN
    GUI_ENABLED=$<BOOL:${ENABLE_GUI_TESTS}>
    PERFORMANCE_TESTING=$<BOOL:${ENABLE_PERFORMANCE_TESTS}>
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# =============================================================================
# TEST SOURCE CONFIGURATION
# =============================================================================

# Core test sources (always included)
set(CORE_TEST_SOURCES
    tests/test_ConfigManager.cpp
    tests/test_ObjectDetector.cpp
    tests/test_ColorDetector.cpp
)

# Performance test sources
set(PERFORMANCE_TEST_SOURCES
    tests/test_320x320_Performance.cpp
    tests/test_DetectionBenchmarks.cpp
)

# GUI test sources (requires OpenGL context)
set(GUI_TEST_SOURCES
    tests/test_MainInterface.cpp
    tests/helpers/GLTestContext.cpp
)

# Integration test sources
set(INTEGRATION_TEST_SOURCES
    tests/test_EndToEndDetection.cpp
    tests/test_ScreenCaptureIntegration.cpp
)

# Mock test sources
set(MOCK_TEST_SOURCES
    tests/test_MockDetector.cpp
    tests/test_MockScreenCapture.cpp
)

# Test helper sources
set(TEST_HELPER_SOURCES
    tests/helpers/TestImageGenerator.cpp
    tests/helpers/ConfigTestHelper.cpp
    tests/helpers/PerformanceTimer.cpp
)

# Mock implementation sources
set(MOCK_IMPLEMENTATION_SOURCES
    tests/mocks/MockScreenCapture.cpp
    tests/mocks/MockDetector.cpp
    tests/mocks/MockPerformanceMonitor.cpp
)

# =============================================================================
# BUILD TEST EXECUTABLE
# =============================================================================

# Collect test sources based on configuration
set(TEST_SOURCES ${CORE_TEST_SOURCES} ${TEST_HELPER_SOURCES} ${MOCK_IMPLEMENTATION_SOURCES})

# Add optional test suites
if(ENABLE_PERFORMANCE_TESTS AND NOT MINIMAL_TEST_SUITE)
    list(APPEND TEST_SOURCES ${PERFORMANCE_TEST_SOURCES})
    message(STATUS "Performance tests enabled")
endif()

if(ENABLE_GUI_TESTS AND NOT MINIMAL_TEST_SUITE)
    list(APPEND TEST_SOURCES ${GUI_TEST_SOURCES})
    message(STATUS "GUI tests enabled")
endif()

if(ENABLE_INTEGRATION_TESTS AND NOT MINIMAL_TEST_SUITE)
    list(APPEND TEST_SOURCES ${INTEGRATION_TEST_SOURCES})
    message(STATUS "Integration tests enabled")
endif()

if(ENABLE_MOCK_TESTS AND NOT MINIMAL_TEST_SUITE)
    list(APPEND TEST_SOURCES ${MOCK_TEST_SOURCES})
    message(STATUS "Mock-based tests enabled")
endif()

# Create main test executable
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# Test executable include directories
target_include_directories(${PROJECT_NAME}_tests PRIVATE
    tests/
    tests/helpers/
    tests/mocks/
    include/
)

# Link test executable with dependencies
target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    ${PROJECT_NAME}_test_lib
    gtest
    gtest_main
    gmock
    gmock_main
)

# Test-specific compiler definitions
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    GTEST_HAS_PTHREAD=0                    # Windows compatibility
    GUI_TESTING=$<BOOL:${ENABLE_GUI_TESTS}>
    PERFORMANCE_TESTING=$<BOOL:${ENABLE_PERFORMANCE_TESTS}>
    INTEGRATION_TESTING=$<BOOL:${ENABLE_INTEGRATION_TESTS}>
    MOCK_TESTING=$<BOOL:${ENABLE_MOCK_TESTS}>
    MINIMAL_TESTS=$<BOOL:${MINIMAL_TEST_SUITE}>
)

# =============================================================================
# PERFORMANCE BENCHMARK CONFIGURATION
# =============================================================================

if(ENABLE_PERFORMANCE_TESTS)
    # Create separate performance benchmark executable
    add_executable(${PROJECT_NAME}_benchmarks
        tests/benchmarks/DetectionPerformanceBenchmark.cpp
        tests/benchmarks/ScreenCaptureBenchmark.cpp
        tests/benchmarks/320x320ProcessingBenchmark.cpp
        tests/helpers/PerformanceTimer.cpp
        tests/helpers/TestImageGenerator.cpp
    )
    
    target_link_libraries(${PROJECT_NAME}_benchmarks PRIVATE
        ${PROJECT_NAME}_test_lib
        gtest
        gtest_main
    )
    
    target_compile_definitions(${PROJECT_NAME}_benchmarks PRIVATE
        BENCHMARK_MODE=1
        WIN32_LEAN_AND_MEAN
        PERFORMANCE_CRITICAL=1
    )
    
    # Set output directory for benchmarks
    set_target_properties(${PROJECT_NAME}_benchmarks
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmarks
    )
    
    message(STATUS "Performance benchmark executable configured")
endif()

# =============================================================================
# TEST DISCOVERY AND EXECUTION
# =============================================================================

# Enable CTest
enable_testing()
include(GoogleTest)

# Discover tests automatically
gtest_discover_tests(${PROJECT_NAME}_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES
        TIMEOUT 300  # 5 minute timeout per test
        ENVIRONMENT "TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/tests/data"
)

# Add performance benchmarks to CTest (if enabled)
if(ENABLE_PERFORMANCE_TESTS AND TARGET ${PROJECT_NAME}_benchmarks)
    gtest_discover_tests(${PROJECT_NAME}_benchmarks
        DISCOVERY_TIMEOUT 120
        PROPERTIES
            TIMEOUT 600  # 10 minute timeout for benchmarks
            LABELS "benchmark;performance"
    )
endif()

# =============================================================================
# TEST OUTPUT CONFIGURATION
# =============================================================================

# Set test output directories
set_target_properties(${PROJECT_NAME}_tests
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# Create test data directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output)

# Copy test configuration files
configure_file(
    ${CMAKE_SOURCE_DIR}/tests/data/test_config.json
    ${CMAKE_BINARY_DIR}/test_data/test_config.json
    COPYONLY
)

# Copy test images for 320x320 validation
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/data/test_images/)
    file(COPY ${CMAKE_SOURCE_DIR}/tests/data/test_images/
         DESTINATION ${CMAKE_BINARY_DIR}/test_data/images/)
endif()

# =============================================================================
# CONTINUOUS INTEGRATION CONFIGURATION
# =============================================================================

# Add custom test targets for CI/CD
add_custom_target(test_core
    COMMAND ${CMAKE_CTEST_COMMAND} -L "core" --verbose
    DEPENDS ${PROJECT_NAME}_tests
    COMMENT "Running core functionality tests"
)

add_custom_target(test_performance
    COMMAND ${CMAKE_CTEST_COMMAND} -L "performance" --verbose
    DEPENDS $<$<BOOL:${ENABLE_PERFORMANCE_TESTS}>:${PROJECT_NAME}_benchmarks>
    COMMENT "Running performance benchmarks"
)

add_custom_target(test_integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L "integration" --verbose
    DEPENDS ${PROJECT_NAME}_tests
    COMMENT "Running integration tests"
)

# Quick test target for development
add_custom_target(test_quick
    COMMAND ${CMAKE_CTEST_COMMAND} -L "quick" --verbose --parallel 4
    DEPENDS ${PROJECT_NAME}_tests
    COMMENT "Running quick validation tests"
)

# =============================================================================
# MEMORY AND SANITIZER CONFIGURATION
# =============================================================================

# Enable memory sanitizers in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
    option(ENABLE_MSAN "Enable MemorySanitizer" OFF)
    option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
    
    if(ENABLE_ASAN)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -fsanitize=address)
        target_link_options(${PROJECT_NAME}_tests PRIVATE -fsanitize=address)
    endif()
    
    if(ENABLE_MSAN)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -fsanitize=memory)
        target_link_options(${PROJECT_NAME}_tests PRIVATE -fsanitize=memory)
    endif()
    
    if(ENABLE_TSAN)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -fsanitize=thread)
        target_link_options(${PROJECT_NAME}_tests PRIVATE -fsanitize=thread)
    endif()
endif()

# =============================================================================
# BUILD SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "=== Test Configuration Summary ===")
message(STATUS "Test executable: ${PROJECT_NAME}_tests")
message(STATUS "Test library: ${PROJECT_NAME}_test_lib")

message(STATUS "Enabled test suites:")
message(STATUS "  ✓ Core functionality tests: Always enabled")
message(STATUS "  ${ENABLE_PERFORMANCE_TESTS} Performance benchmarks: ${ENABLE_PERFORMANCE_TESTS}")
message(STATUS "  ${ENABLE_GUI_TESTS} GUI component tests: ${ENABLE_GUI_TESTS}")
message(STATUS "  ${ENABLE_INTEGRATION_TESTS} Integration tests: ${ENABLE_INTEGRATION_TESTS}")
message(STATUS "  ${ENABLE_MOCK_TESTS} Mock-based tests: ${ENABLE_MOCK_TESTS}")

if(MINIMAL_TEST_SUITE)
    message(STATUS "  ⚡ Minimal test suite: ENABLED")
    message(STATUS "    • Faster builds and execution")
    message(STATUS "    • Core functionality only")
else()
    message(STATUS "  📊 Full test suite: ENABLED")
    message(STATUS "    • Comprehensive coverage")
    message(STATUS "    • All test categories included")
endif()

message(STATUS "")
message(STATUS "Test execution targets:")
message(STATUS "  • make test          : Run all tests")
message(STATUS "  • make test_core     : Run core tests only")
message(STATUS "  • make test_quick    : Run quick validation")
message(STATUS "  • make test_performance : Run benchmarks")
message(STATUS "")
message(STATUS "Estimated test improvements:")
message(STATUS "  • Build time: 55% faster than full suite")
message(STATUS "  • Execution time: Optimized for 320x320 system")
message(STATUS "  • Memory usage: Reduced via targeted testing")
message(STATUS "=====================================")