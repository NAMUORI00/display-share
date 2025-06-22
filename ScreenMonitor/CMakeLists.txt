cmake_minimum_required(VERSION 3.20)

# Set vcpkg toolchain if not already set
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(SmartScreenCapture VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define WIN32_LEAN_AND_MEAN and UNICODE preprocessor directives globally
add_compile_definitions(WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

# Add screen_capture_lite as subdirectory
set(BUILD_EXAMPLE OFF CACHE BOOL "Build screen_capture_lite examples" FORCE)
set(BUILD_CSHARP OFF CACHE BOOL "Build screen_capture_lite C#" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build screen_capture_lite as static library" FORCE)
add_subdirectory(external/screen_capture_lite)

# Create bin directory in build folder for output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Use vcpkg for dependency management
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Smart Screen Capture Application
# High-performance cross-platform screen capture with HSV detection and YOLO support

# Create main executable
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
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    glfw
    OpenGL::GL
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    screen_capture_lite_static
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
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)