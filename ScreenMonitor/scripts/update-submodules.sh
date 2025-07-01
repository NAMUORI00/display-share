#!/bin/bash

# Git Submodule Management Script
# Updates all submodules to their latest versions

echo "🔄 Updating Git Submodules..."

# Update all submodules to latest commit on their default branch
git submodule update --remote

echo "📊 Submodule Status:"
git submodule status

echo ""
echo "📦 Submodule Summary:"
echo "- OpenCV: $(cd external/opencv && git describe --tags 2>/dev/null || echo 'Latest')"
echo "- ImGui: $(cd external/imgui && git describe --tags 2>/dev/null || echo 'Latest')"
echo "- GoogleTest: $(cd external/googletest && git describe --tags 2>/dev/null || echo 'Latest')"
echo "- nlohmann/json: $(cd external/nlohmann_json && git describe --tags 2>/dev/null || echo 'Latest')"
echo "- screen_capture_lite: $(cd external/screen_capture_lite && git describe --tags 2>/dev/null || echo 'Latest')"

echo ""
echo "✅ Submodule update completed!"
echo "💡 Run 'git add external/' and commit if you want to lock these versions."