cmake_minimum_required(VERSION 3.28...3.30)

# Adopt newer policies explicitly when available (incrementally add as needed)
if(POLICY CMP0177)
    # Normalize install() DESTINATION paths (suppresses related dev warning)
    cmake_policy(SET CMP0177 NEW)
endif()

# Fallback: 만약 하위 서브프로젝트 재설정으로 CMP0177 미설정 경고 발생 시 해당 경고 자체를 억제
# (정책 동작은 위에서 NEW로 지정, 단지 "not set" 경고만 제거)
set(CMAKE_POLICY_WARNING_CMP0177 OFF CACHE BOOL "Suppress policy CMP0177 not-set warnings" FORCE)

# In case any deprecated features are still referenced by vendor packages, turn warnings off optionally.
# Comment out the next line if you prefer to see deprecation notices from vendored CMakeLists.
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress deprecated CMake feature warnings for clean configure output" FORCE)

# Submodule-based dependency management
# No vcpkg required - all dependencies managed as git submodules

project(SmartScreenCapture VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure MSVC treats sources as UTF-8 to suppress C4819 (character set) warnings
if(MSVC)
    add_compile_options(/utf-8)
endif()

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

# Create bin directory in build folder for output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# SUBMODULE DEPENDENCIES
# =============================================================================

# OpenGL (system dependency)
find_package(OpenGL REQUIRED)
message(STATUS "OpenGL found - GUI features enabled")

# Add screen_capture_lite as subdirectory
set(BUILD_EXAMPLE OFF CACHE BOOL "Build screen_capture_lite examples" FORCE)
set(BUILD_CSHARP OFF CACHE BOOL "Build screen_capture_lite C#" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build screen_capture_lite as static library" FORCE)
add_subdirectory(external/screen_capture_lite)

# GLFW Configuration (use embedded GLFW from screen_capture_lite)
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/screen_capture_lite/Example_OpenGL/glfw)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Set CMAKE policy to handle older CMake version in GLFW
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

add_subdirectory(${GLFW_DIR} glfw_build)

# GoogleTest with GMock (full testing framework)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "Build GMock for mocking support" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

# nlohmann/json (header-only)
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(external/nlohmann_json)

# OpenCV - Minimal build with core modules only
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
set(BUILD_opencv_video OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_gapi OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)

# Enable essential modules including DNN for YOLO support
set(BUILD_opencv_core ON CACHE BOOL "" FORCE)
set(BUILD_opencv_imgproc ON CACHE BOOL "" FORCE)
set(BUILD_opencv_imgcodecs ON CACHE BOOL "" FORCE)
set(BUILD_opencv_dnn ON CACHE BOOL "" FORCE)          # YOLO v11 지원을 위해 활성화
set(BUILD_opencv_videoio OFF CACHE BOOL "" FORCE)     # Windows GUID_NULL 에러로 임시 비활성화
set(BUILD_opencv_highgui OFF CACHE BOOL "" FORCE)     # Windows SDK 충돌로 비활성화 유지
set(BUILD_opencv_features2d OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_calib3d OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_flann OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_ml OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_photo OFF CACHE BOOL "" FORCE)

message(STATUS "OpenCV 최적화 구성 완료:")
message(STATUS "  - 필수 모듈: core, imgproc, imgcodecs")
message(STATUS "  - AI 모듈: DNN (YOLO v11 지원)")
message(STATUS "  - 임시 비활성화: videoio (Windows GUID_NULL 에러)")
message(STATUS "  - 비활성화된 모듈: highgui, features2d, calib3d, flann, ml, photo (Windows 호환성)")

# Override OpenCV's internal minimal CMake version.
# 이유: OpenCV 최상위 CMakeLists.txt 내부에서 별도 cmake_minimum_required(${MIN_VER_CMAKE}) 호출 시
# 해당 버전 이후 도입된 정책(CMP0177 등)은 "unset" 상태가 되어 install()에서 dev 경고가 발생.
# 3.28 이상으로 올리면 OpenCV 내부 호출이 CMP0177을 NEW로 자동 설정하여 경고 제거.
set(MIN_VER_CMAKE 3.28 CACHE STRING "Override OpenCV's minimum required CMake version (enables CMP0177)" FORCE)

# 서브모듈 수정 없이 CMP0177 기본값을 NEW로 강제 (정책 미설정 경고 방지)
if(NOT DEFINED CMAKE_POLICY_DEFAULT_CMP0177)
    set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
endif()
add_subdirectory(external/opencv)

# Set OpenCV include directories (source + generated)
set(OpenCV_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/external/opencv
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/imgproc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/imgcodecs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/dnn/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/videoio/include  # 임시 제거
)

# ImGui with GLFW and OpenGL3 backends (GUI enabled with docking)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
# Enable ImGui docking features
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_DOCKING)

# Link ImGui with GLFW and OpenGL
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
message(STATUS "ImGui GUI fully enabled with GLFW + OpenGL3")

# =============================================================================
# MAIN APPLICATION
# =============================================================================

# Build mode options
option(SC_RUN_PERF_TEST "Run performance test (instead of GUI loop)" OFF)
option(SC_FORCE_CONSOLE "Force console subsystem" OFF)

# Smart Screen Capture Application - Phase 2 GUI version
add_executable(${PROJECT_NAME}
    # Entry point (GUI / performance test selectable via SC_RUN_PERF_TEST)
    src/main.cpp

    # Core system files
    src/core/ConfigManager.cpp

    # Detection algorithms
    src/detection/HSVColorDetection.cpp
    src/detection/YOLOv11TensorRTInference.cpp

    # Monitoring
    src/monitoring/SimpleMetrics.cpp

    # GUI interface (enabled for persistent application)
    src/gui/MainInterface.cpp

    # Capture implementation
    src/capture/ScreenCaptureLiteDevice.cpp
    src/capture/CenterRegionCapture.cpp
)

# Set as console application for Phase 1 cleanup test
if(SC_FORCE_CONSOLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# ScreenCaptureLiteDevice.cpp는 완전히 구현되어 src/capture/ 디렉토리에 위치

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include/
    include/core/
    include/capture/
    include/detection/
    include/gui/
    src/gui/
    include/interfaces/
    include/factories/
    external/screen_capture_lite/include
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries (submodule-based) - Full GUI enabled
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    glfw
    OpenGL::GL
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_dnn          # YOLO v11 지원을 위해 추가
    # opencv_videoio    # Windows GUID_NULL 에러로 임시 제거
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
)

# =============================================================================
# WINDOWS GUI APPLICATION CONFIGURATION
# =============================================================================

# Windows-specific settings
if(WIN32)
    # Windows GUI application (no console window)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
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

# Force static linking for all dependencies
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

# Add debug/release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
endif()

# GUI 기능 활성화 매크로 정의
target_compile_definitions(${PROJECT_NAME} PRIVATE GUI_ENABLED=1)
if(SC_RUN_PERF_TEST)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RUN_PERFORMANCE_TEST=1)
endif()
message(STATUS "GUI_ENABLED 매크로 정의됨 - GUI 기능 완전 활성화")

# Compiler definitions for ImGui OpenGL3 backend
# Windows에서 OpenGL 로더 필요 없음 (GLFW가 처리)
if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

# =============================================================================
# TESTING CONFIGURATION (Simplified)
# =============================================================================

# Test build toggle
option(SC_ENABLE_TESTS "Build and discover GoogleTest tests" ON)
if(SC_ENABLE_TESTS)
    enable_testing()
    include(GoogleTest)
endif()

# Create a static library for testing (without main.cpp)
if(SC_ENABLE_TESTS)
add_library(${PROJECT_NAME}_lib STATIC
    # Core system files
    src/core/ConfigManager.cpp
    
    # Detection algorithms (cleaned and simplified)
    src/detection/HSVColorDetection.cpp
    src/detection/YOLOv11TensorRTInference.cpp  # DNN 모듈 활성화로 다시 포함
    
    # Simple monitoring system
    src/monitoring/SimpleMetrics.cpp
    
    
    # GUI interface (temporarily disabled for Phase 2 testing)
    # src/gui/MainInterface.cpp
    
    # New capture implementation
    src/capture/ScreenCaptureLiteDevice.cpp
    src/capture/CenterRegionCapture.cpp
)

# Include directories for the library
target_include_directories(${PROJECT_NAME}_lib PUBLIC
    include/
    include/core/
    include/capture/
    include/detection/
    include/gui/
    src/gui/
    include/interfaces/
    include/factories/
    external/screen_capture_lite/include
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries for the library (submodule-based) - Full GUI enabled
target_link_libraries(${PROJECT_NAME}_lib PUBLIC
    imgui
    glfw
    OpenGL::GL
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_dnn          # YOLO v11 지원을 위해 추가
    # opencv_videoio    # Windows GUID_NULL 에러로 임시 제거
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
)

# Apply same compiler definitions to library
if(SC_ENABLE_TESTS AND WIN32)
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE WIN32_LEAN_AND_MEAN)
endif()

if(SC_ENABLE_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE _DEBUG)
else()
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE NDEBUG)
endif()

# GUI 기능 활성화 매크로 정의 (라이브러리)
target_compile_definitions(${PROJECT_NAME}_lib PRIVATE GUI_ENABLED=1)

if(SC_ENABLE_TESTS AND NOT WIN32)
    target_compile_definitions(${PROJECT_NAME}_lib PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

# Main test executable - Phase 5 QA Integration Tests
add_executable(${PROJECT_NAME}_tests
    tests/test_ConfigManager.cpp
    tests/test_CenterRegionCapture.cpp
    tests/test_Phase5SystemValidation.cpp
    tests/helpers/TestImageGenerator.cpp
    tests/helpers/ConfigTestHelper.cpp
    tests/mocks/MockScreenCapture.cpp
)

# Include directories for tests
target_include_directories(${PROJECT_NAME}_tests PRIVATE
    include/
    tests/
    tests/helpers/
    tests/mocks/
    external/screen_capture_lite/include
)

# Link test executable with library, GTest and GMock
target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    ${PROJECT_NAME}_lib
    gtest
    gtest_main
    gmock
    gmock_main
)

# Apply same compiler definitions to tests
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    GTEST_HAS_PTHREAD=0  # Disable pthread for Windows compatibility
    GUI_ENABLED=1        # GUI 기능 활성화
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
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests DISCOVERY_MODE PRE_TEST)

# Set output directory for test executables
set_target_properties(${PROJECT_NAME}_tests
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)
endif() # SC_ENABLE_TESTS

message(STATUS "=== SmartScreenCapture Configuration Complete ===")
message(STATUS "GUI: Fully Enabled (GLFW + ImGui + OpenGL3)")
message(STATUS "Static Linking: Enabled (Single EXE)")
message(STATUS "OpenCV: Minimal Core Modules")
message(STATUS "Testing: Google Test Framework")
message(STATUS "=================================================")