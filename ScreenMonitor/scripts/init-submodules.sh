#!/bin/bash

# Git Submodule Initialization Script
# Initializes all submodules for fresh clone

echo "🚀 Initializing Git Submodules..."

# Initialize all submodules recursively
git submodule update --init --recursive

echo ""
echo "📊 Verifying Submodule Status:"
git submodule status

echo ""
echo "🔍 Checking Key Files:"
check_file() {
    if [ -f "$1" ]; then
        echo "✅ $1"
    else
        echo "❌ $1 (Missing!)"
    fi
}

check_file "external/opencv/CMakeLists.txt"
check_file "external/imgui/imgui.cpp"
check_file "external/googletest/CMakeLists.txt"
check_file "external/nlohmann_json/single_include/nlohmann/json.hpp"
check_file "external/screen_capture_lite/CMakeLists.txt"

echo ""
echo "📦 Directory Sizes:"
du -sh external/* 2>/dev/null | sort -hr

echo ""
echo "✅ Submodule initialization completed!"
echo "💡 You can now run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release"