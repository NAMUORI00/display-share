# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **Professional Screen Capture & Computer Vision System** built in C++17 that delivers high-performance screen capture and real-time computer vision processing. The project is a Windows GUI application using ImGui with GLFW + OpenGL3 for a professional interface, integrated with screen_capture_lite for cross-platform capture capabilities.

**Key Architecture Components:**
- **Core Application**: MainInterface class as the GUI entry point with ImGui docking interface
- **Screen Capture**: ScreenCaptureLiteDevice integration for cross-platform capture  
- **Computer Vision**: HSV color detection, YOLO v11 object detection
- **GUI System**: ImGui-based interface with integrated console, docking panels, and stream redirection
- **Configuration**: JSON-based config system with schema validation

## Build System & Dependencies

### Build Commands (Submodule-Based)
```bash
# Configure build (Windows)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build project
cmake --build build --config Release --parallel

# Run main application (Windows GUI)
./build/bin/Release/SmartScreenCapture.exe

# Run tests
ctest -C Release --test-dir build --verbose
```

### Submodule Dependencies (external/)
- **opencv**: Computer vision (core, imgproc, imgcodecs modules only)
- **imgui**: Professional GUI with GLFW + OpenGL3 backends, docking branch required
- **googletest**: Unit testing framework (gtest, gmock)
- **nlohmann_json**: JSON configuration management (header-only)
- **screen_capture_lite**: High-performance cross-platform capture
- **GLFW**: Cross-platform windowing (embedded in screen_capture_lite)

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
├── main.cpp                     # Entry point - WinMain for Windows GUI
├── core/ConfigManager.cpp       # JSON config with schema validation
├── capture/                     # Screen capture implementations
│   └── ScreenCaptureLiteDevice.cpp # screen_capture_lite wrapper
├── detection/                   # Computer vision algorithms
│   ├── ColorDetector.cpp        # Basic color detection
│   ├── HSVColorDetection.cpp    # HSV-based color tracking  
│   ├── ObjectDetector.cpp       # Template matching
│   └── YOLOv11TensorRTInference.cpp # YOLO v11 object detection
├── factories/ComponentFactory.cpp   # Factory pattern implementation
└── gui/MainInterface.cpp        # ImGui interface with docking

include/
├── interfaces/                  # Abstract interfaces
│   ├── ICaptureDevice.h         # Capture device interface
│   ├── IDetectionAlgorithm.h    # Detection algorithm interface
│   └── IPerformanceObserver.h   # Performance monitoring interface
└── [corresponding headers]      # Headers mirror src structure

external/                        # Git submodules
├── opencv/                      # Computer vision library
├── imgui/                       # ImGui docking branch
├── googletest/                  # Testing framework
├── nlohmann_json/              # JSON library
└── screen_capture_lite/        # Screen capture library

config/config.json              # Main configuration file
tests/                          # GoogleTest test files
```

## Application Architecture

### Core Application Flow
The application is a Windows GUI application using WinMain entry point:

1. **Initialization**: MainInterface creates ImGui context with docking enabled
2. **GUI Setup**: 5-panel docking layout (Capture Settings, Detection Settings, Performance Monitor, Screen Preview, Console)
3. **Stream Redirection**: cout/cerr redirected to ImGui console panel via custom GuiStreamBuf
4. **Main Loop**: Event handling and rendering loop until window close

### Key Architectural Patterns

#### GUI Architecture (MainInterface)
- **ImGui Docking Interface**: 5-panel professional layout with persistent docking
- **Stream Redirection**: Custom GuiStreamBuf class redirects console output to GUI
- **OpenGL Integration**: GLFW + OpenGL3 for cross-platform rendering
- **Event-Driven**: ImGui immediate mode GUI with event handling

#### Factory Pattern (ComponentFactory)
- Creates detection algorithms and capture devices based on configuration
- Enables runtime algorithm switching and polymorphic behavior

#### Interface Segregation (interfaces/)
- ICaptureDevice: Abstract interface for screen capture implementations
- IDetectionAlgorithm: Common interface for vision algorithms
- IPerformanceObserver: Observer pattern for performance monitoring

#### Configuration System (ConfigManager)
- JSON-based configuration with schema validation
- Supports production_system, vision_algorithms, analytics, gui, and performance sections
- Runtime configuration changes reflected in GUI

## ImGui GUI System

### Docking Layout
The application uses ImGui docking to create a professional 5-panel interface:
- **Left Panels**: Capture Settings, Detection Settings
- **Center Panel**: Screen Preview (live video feed)  
- **Right Panel**: Performance Monitor (FPS graphs, metrics)
- **Bottom Panel**: Console (integrated cout/cerr output)

### Key GUI Features
- **Integrated Console**: All console output redirected to ImGui panel with timestamps and color coding
- **No External Console**: Pure GUI application with no separate console window
- **Real-time Updates**: Live FPS monitoring, detection results, performance metrics
- **Docking Persistence**: Layout saved/restored via imgui.ini

### Important Implementation Notes
- Requires `#include <imgui_internal.h>` for DockBuilder API access
- Uses ImGui docking branch, not master branch
- Custom GuiStreamBuf class handles stream redirection
- Korean language comments throughout codebase

## Configuration System

The project uses JSON configuration with these key sections:

- **production_system**: Application metadata and mode settings
- **vision_algorithms**: HSV tracking and YOLO v11 detection configuration
- **analytics**: Performance monitoring settings
- **gui**: UI scale and display options
- **performance**: FPS targets, threading, GPU acceleration settings

Key config locations:
- `config/config.json` - Main configuration file
- Algorithm parameters configurable via GUI in real-time

## Development Guidelines

### Code Style
- C++17 standard required
- Windows-focused (GUI application using WinMain)
- Korean language comments throughout codebase
- Static linking configuration for single EXE deployment
- ImGui immediate mode GUI patterns

### Build Configuration
- Static linking enabled for all dependencies
- Windows MSVC runtime static linking
- WIN32_EXECUTABLE property set for GUI application
- OpenGL 3.3+ requirement for ImGui rendering

### Testing
- GoogleTest framework for unit tests
- Test library (SmartScreenCapture_lib) excludes main.cpp
- Comprehensive mocks for capture devices and detectors
- Integration tests for GUI components

### Testing Commands
```bash
# Build and run all tests
cmake --build build --config Release
ctest -C Release --test-dir build --verbose

# Run specific test executable
./build/bin/tests/SmartScreenCapture_tests.exe

# Test individual components
./build/bin/tests/test_MainInterface.exe
./build/bin/tests/test_ConfigManager.exe
```

### Common Development Tasks

#### Adding New Detection Algorithms
1. Implement IDetectionAlgorithm interface
2. Add algorithm to ComponentFactory
3. Update config.json schema
4. Add GUI controls in MainInterface

#### Modifying GUI Layout
1. Update docking layout in MainInterface::Render()
2. Ensure window names match DockBuilderDockWindow calls
3. Test docking persistence via imgui.ini

#### Adding New Configuration Options
1. Update config.json schema
2. Add parsing in ConfigManager
3. Expose controls in appropriate GUI panel

### Build Troubleshooting
1. **ImGui DockBuilder errors**: Ensure `#include <imgui_internal.h>` is included
2. **Submodule missing**: Run `git submodule update --init --recursive`
3. **OpenCV minimal build**: Only core, imgproc, imgcodecs modules enabled
4. **Static linking issues**: Check MSVC runtime library settings
5. **GUI not showing**: Verify OpenGL 3.3+ support and GLFW initialization

## Production System Features

This is a **professional Windows application** designed for:
- High-performance real-time screen capture and computer vision processing
- Professional ImGui-based interface with advanced docking capabilities
- Enterprise-level monitoring and analytics
- Production-ready algorithm deployment with YOLO v11 support
- Integrated development experience with console-to-GUI redirection

The system provides a complete professional GUI framework with integrated console output, real-time performance monitoring, and modular computer vision algorithm support.