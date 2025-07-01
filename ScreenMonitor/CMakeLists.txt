cmake_minimum_required(VERSION 3.20)

# Submodule-based dependency management
# No vcpkg required - all dependencies managed as git submodules

project(SmartScreenCapture VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define preprocessor directives (UNICODE disabled for OpenCV compatibility)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
# Note: UNICODE disabled due to OpenCV compatibility issues

# =============================================================================
# STATIC LINKING CONFIGURATION (Single EXE without DLLs)
# =============================================================================

# Force static library builds globally
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

# Windows MSVC Runtime static linking
if(WIN32 AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    message(STATUS "MSVC Runtime: Static linking enabled")
endif()

# Add screen_capture_lite as subdirectory
set(BUILD_EXAMPLE OFF CACHE BOOL "Build screen_capture_lite examples" FORCE)
set(BUILD_CSHARP OFF CACHE BOOL "Build screen_capture_lite C#" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build screen_capture_lite as static library" FORCE)
add_subdirectory(external/screen_capture_lite)

# Create bin directory in build folder for output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# SUBMODULE DEPENDENCIES
# =============================================================================

# OpenGL (system dependency) - Make optional for WSL compatibility
find_package(OpenGL)
if(NOT OpenGL_FOUND)
    message(WARNING "OpenGL not found - GUI features will be disabled")
    set(DISABLE_GUI ON)
else()
    message(STATUS "OpenGL found - GUI features enabled")
    set(DISABLE_GUI OFF)
endif()

# GoogleTest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

# nlohmann/json (header-only)
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/nlohmann_json)

# OpenCV - Minimal build with core modules only (avoiding problematic modules)
message(STATUS "Enabling minimal OpenCV core modules for reliable build")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(WITH_IPP OFF CACHE BOOL "" FORCE)
set(WITH_ITT OFF CACHE BOOL "" FORCE)

# Disable problematic modules for Windows compatibility
set(BUILD_opencv_videoio OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_video OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_dnn OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_gapi OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)

# Enable only essential modules for static build (minimal set)
set(BUILD_opencv_core ON CACHE BOOL "" FORCE)
set(BUILD_opencv_imgproc ON CACHE BOOL "" FORCE)
set(BUILD_opencv_imgcodecs ON CACHE BOOL "" FORCE)
set(BUILD_opencv_highgui OFF CACHE BOOL "" FORCE)  # Disabled due to Windows SDK conflicts
set(BUILD_opencv_features2d OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_calib3d OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_flann OFF CACHE BOOL "" FORCE)     # Explicitly disable flann
set(BUILD_opencv_ml OFF CACHE BOOL "" FORCE)        # Disable ML module
set(BUILD_opencv_photo OFF CACHE BOOL "" FORCE)     # Disable photo module

add_subdirectory(external/opencv)

# Set OpenCV include directories (source + generated)
set(OpenCV_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/external/opencv
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/imgproc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/imgcodecs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/highgui/include
)

# GLFW Configuration (use vcpkg version for stability)
find_package(glfw3 CONFIG REQUIRED)
message(STATUS "Using vcpkg GLFW for GUI compatibility")

# ImGui with GLFW + OpenGL3 backends (GUI enabled with docking)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    # Enable GLFW + OpenGL3 backends
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
# Enable ImGui docking features
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_DOCKING)

# Link ImGui with GLFW and OpenGL
if(NOT DISABLE_GUI)
    target_link_libraries(imgui PUBLIC glfw)
    if(OpenGL_FOUND)
        target_link_libraries(imgui PUBLIC OpenGL::GL)
        message(STATUS "ImGui GUI fully enabled with GLFW + OpenGL3")
    endif()
else()
    message(STATUS "GUI disabled - ImGui core only")
endif()

# ONNX Runtime (optional - system installation required for DirectML)
# Note: ONNX Runtime is too large for submodule, install via package manager
find_package(onnxruntime CONFIG)
if(onnxruntime_FOUND)
    message(STATUS "ONNX Runtime found - GPU-only YOLO v11 support enabled")
    add_compile_definitions(HAVE_ONNXRUNTIME)
    add_compile_definitions(ONNX_GPU_ONLY)
    
    # Check for DirectML support on Windows
    if(WIN32)
        message(STATUS "DirectML GPU acceleration enabled")
        add_compile_definitions(HAVE_DIRECTML)
    endif()
else()
    message(STATUS "ONNX Runtime not found - using OpenCV DNN only")
    message(STATUS "Install ONNX Runtime with DirectML for GPU acceleration")
endif()

# Enable testing
enable_testing()
include(GoogleTest)

# Smart Screen Capture Application
# High-performance cross-platform screen capture with HSV detection and YOLO support

# Create main executable (complete application build)
add_executable(${PROJECT_NAME}
    # Core system files
    src/main.cpp
    src/core/ConfigManager.cpp
    
    # High-speed capture system
    src/capture/HighSpeedCapture.cpp
    src/capture/CrossPlatformCapture.cpp
    
    # Detection algorithms
    src/detection/ColorDetector.cpp
    src/detection/ObjectDetector.cpp
    
    # Performance monitoring
    src/monitoring/PerformanceMonitor.cpp
    
    # GUI interface
    src/gui/MainInterface.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include/
    include/core/
    include/capture/
    include/detection/
    include/monitoring/
    include/gui/
    external/screen_capture_lite/include
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries (submodule-based) - Full GUI enabled
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    glfw
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    # opencv_highgui  # Disabled due to Windows SDK conflicts
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
)

# Link OpenGL if available
if(NOT DISABLE_GUI AND OpenGL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
    message(STATUS "OpenGL linked to main executable")
endif()

# Add ONNX Runtime if available
if(onnxruntime_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft.ML.OnnxRuntime)
endif()

# =============================================================================
# COMPLETE STATIC LINKING CONFIGURATION
# =============================================================================

# GLFW static linking settings
set(GLFW_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# Windows-specific static linking
if(WIN32)
    # Link statically against Windows runtime
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Add Windows system libraries for static linking
    target_link_libraries(${PROJECT_NAME} PRIVATE
        kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 
        ole32 oleaut32 uuid odbc32 odbccp32
    )
    
    # Static linking flags for MinGW
    if(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE
            -static-libgcc -static-libstdc++ -static
        )
    endif()
endif()

# Linux static linking
if(UNIX AND NOT APPLE)
    # Static linking for system libraries
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc -static-libstdc++
    )
endif()

# Force static linking for all dependencies
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

# Windows specific settings
if(WIN32)
    # Console application for debugging
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# Add debug/release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
endif()

# Compiler definitions for ImGui OpenGL3 backend
# Windows에서 OpenGL 로더 필요 없음 (GLFW가 처리)
if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

# ============================================================================
# TEST CONFIGURATION
# ============================================================================

# Create a static library for testing (without main.cpp)
add_library(${PROJECT_NAME}_lib STATIC
    # Core system files
    src/core/ConfigManager.cpp
    
    # High-speed capture system
    src/capture/HighSpeedCapture.cpp
    src/capture/CrossPlatformCapture.cpp
    
    # Detection algorithms
    src/detection/ColorDetector.cpp
    src/detection/ObjectDetector.cpp
    
    # Performance monitoring
    src/monitoring/PerformanceMonitor.cpp
    
    # GUI interface
    src/gui/MainInterface.cpp
)

# Include directories for the library
target_include_directories(${PROJECT_NAME}_lib PUBLIC
    include/
    include/core/
    include/capture/
    include/detection/
    include/monitoring/
    include/gui/
    external/screen_capture_lite/include
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries for the library (submodule-based) - Full GUI enabled
target_link_libraries(${PROJECT_NAME}_lib PUBLIC
    imgui
    glfw
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    # opencv_highgui  # Disabled due to Windows SDK conflicts
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
)

# Link OpenGL to library if available
if(NOT DISABLE_GUI AND OpenGL_FOUND)
    target_link_libraries(${PROJECT_NAME}_lib PUBLIC OpenGL::GL)
endif()

# Add ONNX Runtime to library if available
if(onnxruntime_FOUND)
    target_link_libraries(${PROJECT_NAME}_lib PUBLIC Microsoft.ML.OnnxRuntime)
endif()

# Apply same compiler definitions to library
if(WIN32)
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE _DEBUG)
else()
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE NDEBUG)
endif()

if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

# Test executables
set(TEST_SOURCES
    tests/test_ConfigManager.cpp
    tests/test_ColorDetector.cpp
    tests/test_ObjectDetector.cpp
    tests/test_HighSpeedCapture.cpp
    tests/test_CrossPlatformCapture.cpp
    tests/test_MainInterface.cpp
    tests/test_IntegrationSystem.cpp
    tests/test_PerformanceBenchmark.cpp
    tests/test_YOLOv11.cpp
    tests/test_YOLOv11Integration.cpp
    tests/helpers/TestImageGenerator.cpp
    tests/helpers/ConfigTestHelper.cpp
    tests/helpers/GLTestContext.cpp
    tests/mocks/MockScreenCapture.cpp
    tests/mocks/MockDetector.cpp
)

# Create test executable
add_executable(${PROJECT_NAME}_tests
    ${TEST_SOURCES}
)

# Include directories for tests
target_include_directories(${PROJECT_NAME}_tests PRIVATE
    include/
    tests/
    tests/helpers/
    tests/mocks/
    external/screen_capture_lite/include
)

# Link test executable with library and GTest (submodule-based)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    ${PROJECT_NAME}_lib
    gtest
    gtest_main
    gmock
    gmock_main
)

# Apply same compiler definitions to tests
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    GTEST_HAS_PTHREAD=0  # Disable pthread for Windows compatibility
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE _DEBUG)
else()
    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE NDEBUG)
endif()

if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME}_tests PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

# Discover tests
gtest_discover_tests(${PROJECT_NAME}_tests)

# Individual test executables for specific components
add_executable(test_configmanager tests/test_ConfigManager.cpp tests/helpers/ConfigTestHelper.cpp)
target_link_libraries(test_configmanager PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_configmanager PRIVATE include/ tests/ tests/helpers/)

add_executable(test_colordetector tests/test_ColorDetector.cpp tests/helpers/TestImageGenerator.cpp)
target_link_libraries(test_colordetector PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_colordetector PRIVATE include/ tests/ tests/helpers/)

add_executable(test_objectdetector tests/test_ObjectDetector.cpp tests/helpers/TestImageGenerator.cpp)
target_link_libraries(test_objectdetector PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_objectdetector PRIVATE include/ tests/ tests/helpers/)

add_executable(test_maininterface tests/test_MainInterface.cpp tests/helpers/GLTestContext.cpp)
target_link_libraries(test_maininterface PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_maininterface PRIVATE include/ tests/ tests/helpers/)

add_executable(test_integrationsystem tests/test_IntegrationSystem.cpp)
target_link_libraries(test_integrationsystem PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_integrationsystem PRIVATE include/ tests/)

add_executable(test_performancebenchmark tests/test_PerformanceBenchmark.cpp tests/helpers/TestImageGenerator.cpp)
target_link_libraries(test_performancebenchmark PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_performancebenchmark PRIVATE include/ tests/ tests/helpers/)

add_executable(test_yolov11 tests/test_YOLOv11.cpp tests/helpers/TestImageGenerator.cpp)
target_link_libraries(test_yolov11 PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_yolov11 PRIVATE include/ tests/ tests/helpers/)

add_executable(test_yolov11integration tests/test_YOLOv11Integration.cpp tests/helpers/TestImageGenerator.cpp)
target_link_libraries(test_yolov11integration PRIVATE ${PROJECT_NAME}_lib gtest gtest_main)
target_include_directories(test_yolov11integration PRIVATE include/ tests/ tests/helpers/)

# Set output directory for test executables
set_target_properties(${PROJECT_NAME}_tests test_configmanager test_colordetector test_objectdetector test_maininterface test_integrationsystem test_performancebenchmark test_yolov11 test_yolov11integration
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)