# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **Professional Screen Capture & Computer Vision System** built in C++17 that delivers enterprise-grade high-performance screen capture and computer vision processing. The project integrates the screen_capture_lite library to achieve 27,000+ FPS cross-platform screen capture with advanced real-time processing capabilities.

**Key Architecture Components:**
- **Core Application**: PerformanceMonitor class as main entry point
- **Screen Capture**: screen_capture_lite integration for cross-platform capture
- **Computer Vision**: HSV tracking, template matching, optical flow algorithms  
- **Professional Analytics**: Advanced performance monitoring and optimization tools
- **Configuration**: JSON-based config system with schema validation

## Build System & Dependencies

### Build Commands (Submodule-Based)
```bash
# Configure build (No vcpkg required!)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build project
cmake --build build --config Release --parallel

# Run main application
./build/bin/SmartScreenCapture
```

### Submodule Dependencies (external/)
- **opencv**: Computer vision (core, imgproc, imgcodecs, dnn modules)
- **imgui**: Professional GUI with GLFW and OpenGL3 backends
- **googletest**: Unit testing framework (gtest, gmock)
- **nlohmann_json**: JSON configuration management (header-only)
- **screen_capture_lite**: High-performance cross-platform capture
- **GLFW**: Cross-platform windowing (included in screen_capture_lite)

### Submodule Management
```bash
# Initialize all submodules
git submodule update --init --recursive

# Update submodules to latest versions
git submodule update --remote

# Check submodule status
git submodule status
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

- **Main Config**: `config/config.json` - Production system settings
- **Schema Validation**: Built-in schema validation in ConfigManager
- **Production Mode**: Optimized for "production_mode" deployment
- **Algorithm Selection**: Configurable vision algorithms (HSV, YOLO)
- **Performance Settings**: Target FPS, threading, GPU acceleration toggles

Key config sections: production_system, vision_algorithms, analytics, gui, performance

## Development Guidelines

### Code Style
- C++17 standard required
- Cross-platform compatibility (Windows/macOS/Linux)
- Production focus - all features optimized for enterprise deployment
- Performance-oriented with real-time metrics
- Schema-validated configuration system

### Build Troubleshooting
1. **vcpkg issues**: Ensure VCPKG_ROOT environment variable is set
2. **Submodule missing**: Run `git submodule update --init --recursive`
3. **OpenCV conflicts**: Use minimal feature set to avoid protobuf issues
4. **Memory issues**: Limit parallel build jobs on low-memory systems

### Testing
- Use Google Test framework for unit tests
- Focus on production system validation
- Performance benchmarking for capture system
- Cross-platform compatibility testing

## Production System Features

This is a **professional-grade system** designed for:
- High-performance real-time computer vision processing
- Enterprise-level screen capture and analysis
- Production-ready algorithm deployment
- Commercial applications and business intelligence
- Advanced performance monitoring and analytics

The system includes optimized processing modes, real-time analytics, and enterprise-grade performance monitoring specifically for production environments.