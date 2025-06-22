# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an **Educational Computer Vision Framework** built in C++17 that demonstrates high-performance screen capture and computer vision algorithms for learning purposes. The project integrates the screen_capture_lite library to achieve 27,000+ FPS cross-platform screen capture with educational overlays for algorithm comparison.

**Key Architecture Components:**
- **Core Application**: PerformanceMonitor class as main entry point
- **Screen Capture**: screen_capture_lite integration for cross-platform capture
- **Computer Vision**: HSV tracking, template matching, optical flow algorithms  
- **Educational Framework**: Algorithm comparison and performance analysis tools
- **Configuration**: JSON-based config system with schema validation

## Build System & Dependencies

### Build Commands
```bash
# Configure build (requires vcpkg)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

# Build project
cmake --build build --config Release --parallel

# Run main application
./build/bin/SmartScreenCapture
```

### Required Dependencies (vcpkg.json)
- **opencv4**: Computer vision (minimal: jpeg, png, tiff features only)
- **imgui**: Educational GUI with GLFW and OpenGL3 bindings
- **glfw3**: Cross-platform windowing
- **nlohmann-json**: Configuration management
- **gtest**: Unit testing framework
- **screen_capture_lite**: High-performance capture (Git submodule)

### Submodule Setup
```bash
git submodule update --init --recursive
```

## Project Structure

```
src/
├── main.cpp                     # Entry point - initializes PerformanceMonitor
├── core/ConfigManager.cpp       # JSON config with schema validation
├── capture/                     # Screen capture implementations
│   ├── HighSpeedCapture.cpp     # High-performance capture interface
│   └── CrossPlatformCapture.cpp # Platform abstraction layer
├── detection/                   # Computer vision algorithms
│   ├── ColorDetector.cpp        # HSV-based color tracking  
│   └── ObjectDetector.cpp       # Template matching & YOLO integration
├── monitoring/PerformanceMonitor.cpp # Main application logic & GUI
└── gui/MainInterface.cpp        # ImGui educational interface

external/screen_capture_lite/    # Git submodule for capture library
config/config.json              # Main configuration file
```

## Configuration System

The project uses a robust JSON configuration system with schema validation:

- **Main Config**: `config/config.json` - Educational framework settings
- **Schema Validation**: Built-in schema validation in ConfigManager
- **Educational Mode**: Hardcoded to "educational_only" mode
- **Algorithm Selection**: Configurable vision algorithms (HSV, YOLO, template matching)
- **Performance Settings**: Target FPS, threading, GPU acceleration toggles

Key config sections: educational_framework, vision_algorithms, simulation, analytics, gui, performance, data_export, educational_content

## Development Guidelines

### Code Style
- C++17 standard required
- Cross-platform compatibility (Windows/macOS/Linux)
- Educational focus - all features designed for learning
- Performance-oriented with real-time metrics
- Schema-validated configuration system

### Build Troubleshooting
1. **vcpkg issues**: Ensure VCPKG_ROOT environment variable is set
2. **Submodule missing**: Run `git submodule update --init --recursive`
3. **OpenCV conflicts**: Use minimal feature set to avoid protobuf issues
4. **Memory issues**: Limit parallel build jobs on low-memory systems

### Testing
- Use Google Test framework for unit tests
- Focus on educational component validation
- Performance benchmarking for capture system
- Cross-platform compatibility testing

## Educational Framework Notes

This is explicitly an **educational tool** designed for:
- Learning computer vision algorithms
- Algorithm performance comparison  
- Real-time processing demonstration
- Academic research and teaching

The framework includes simulation modes, step-by-step algorithm demonstrations, and performance analytics specifically for educational purposes.