---
name: cpp-submodules
description: Git submodule management automation for ScreenMonitor external dependencies (opencv, imgui, googletest, etc.)
---

#!/bin/bash
# ScreenMonitor Git Submodule Management
# Comprehensive automation for managing external dependencies

set -e  # Exit on error

echo "📦 ScreenMonitor Submodule Management"
echo "===================================="

# Phase 1: Environment Verification
echo ""
echo "🔍 Phase 1: Environment Check"
echo "-----------------------------"

# Validate project structure
if [ ! -f ".gitmodules" ]; then
    echo "❌ Error: .gitmodules not found"
    echo "   Please run this command from the C_capture project root"
    exit 1
fi

if [ ! -d "ScreenMonitor/external" ]; then
    echo "❌ Error: ScreenMonitor/external directory not found"
    exit 1
fi

echo "✅ Project structure validated"

# Parse command line arguments
ACTION="status"
FORCE=false
RECURSIVE=true
SPECIFIC_MODULE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --status|-s)
            ACTION="status"
            shift
            ;;
        --init|-i)
            ACTION="init"
            shift
            ;;
        --update|-u)
            ACTION="update"
            shift
            ;;
        --reset|-r)
            ACTION="reset"
            shift
            ;;
        --sync|--sync-urls)
            ACTION="sync"
            shift
            ;;
        --clean|-c)
            ACTION="clean"
            shift
            ;;
        --force|-f)
            FORCE=true
            shift
            ;;
        --no-recursive)
            RECURSIVE=false
            shift
            ;;
        --module|-m)
            SPECIFIC_MODULE="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: /cpp-submodules [ACTION] [OPTIONS]"
            echo ""
            echo "Actions:"
            echo "  -s, --status        Show submodule status (default)"
            echo "  -i, --init          Initialize submodules"
            echo "  -u, --update        Update submodules to latest"
            echo "  -r, --reset         Reset submodules to committed versions"
            echo "      --sync          Sync submodule URLs with .gitmodules"
            echo "  -c, --clean         Clean submodule directories"
            echo ""
            echo "Options:"
            echo "  -f, --force         Force operation (use with caution)"
            echo "      --no-recursive  Don't process nested submodules"
            echo "  -m, --module NAME   Target specific submodule"
            echo "  -h, --help          Show this help message"
            echo ""
            echo "Examples:"
            echo "  /cpp-submodules --status"
            echo "  /cpp-submodules --update --module opencv"
            echo "  /cpp-submodules --init --force"
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "📋 Operation Configuration:"
echo "   - Action: $ACTION"
echo "   - Force Mode: $FORCE"
echo "   - Recursive: $RECURSIVE"
echo "   - Specific Module: ${SPECIFIC_MODULE:-'(all)'}"

# Phase 2: Submodule Discovery and Analysis
echo ""
echo "🔍 Phase 2: Submodule Analysis"
echo "------------------------------"

# Get list of all submodules
SUBMODULES=($(git config --file .gitmodules --get-regexp path | awk '{ print $2 }'))
echo "📋 Registered submodules: ${#SUBMODULES[@]}"

# Critical submodules that must be available
CRITICAL_MODULES=("ScreenMonitor/external/opencv" "ScreenMonitor/external/imgui" "ScreenMonitor/external/googletest" "ScreenMonitor/external/nlohmann_json" "ScreenMonitor/external/screen_capture_lite")

echo ""
echo "🔍 Submodule Status Overview:"
echo "----------------------------"

for submodule in "${SUBMODULES[@]}"; do
    STATUS_CHAR=""
    STATUS_DESC=""
    
    if [ ! -d "$submodule" ]; then
        STATUS_CHAR="❌"
        STATUS_DESC="Directory missing"
    elif [ -z "$(ls -A "$submodule" 2>/dev/null)" ]; then
        STATUS_CHAR="⚠️ "
        STATUS_DESC="Empty directory"
    else
        # Check git status
        cd "$submodule"
        if git rev-parse --git-dir > /dev/null 2>&1; then
            # Check if submodule is on expected commit
            CURRENT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
            EXPECTED_COMMIT=$(cd .. && git ls-tree HEAD "$submodule" | awk '{print $3}' || echo "unknown")
            
            if [ "$CURRENT_COMMIT" = "$EXPECTED_COMMIT" ]; then
                STATUS_CHAR="✅"
                STATUS_DESC="Up to date"
            else
                STATUS_CHAR="🔄"
                STATUS_DESC="Needs update"
            fi
        else
            STATUS_CHAR="❌"
            STATUS_DESC="Not a git repository"
        fi
        cd - > /dev/null
    fi
    
    printf "   %s %-35s %s\n" "$STATUS_CHAR" "$submodule" "$STATUS_DESC"
done

echo ""

# Phase 3: Execute Requested Action
echo "⚡ Phase 3: Executing Action - $ACTION"
echo "$(printf '%*s' ${#ACTION} '' | tr ' ' '-')------------------"

case $ACTION in
    "status")
        echo "📊 Detailed Submodule Status:"
        echo ""
        
        if [ -n "$SPECIFIC_MODULE" ]; then
            if [[ " ${SUBMODULES[@]} " =~ " ${SPECIFIC_MODULE} " ]]; then
                git submodule status "$SPECIFIC_MODULE"
            else
                echo "❌ Module not found: $SPECIFIC_MODULE"
                exit 1
            fi
        else
            git submodule status
        fi
        
        echo ""
        echo "📋 Repository Information:"
        for submodule in "${SUBMODULES[@]}"; do
            if [ -d "$submodule/.git" ] || [ -f "$submodule/.git" ]; then
                cd "$submodule"
                BRANCH=$(git branch --show-current 2>/dev/null || echo "detached")
                REMOTE_URL=$(git config --get remote.origin.url 2>/dev/null || echo "no remote")
                printf "   %-35s Branch: %-15s Remote: %s\n" "$submodule" "$BRANCH" "$REMOTE_URL"
                cd - > /dev/null
            fi
        done
        ;;
        
    "init")
        echo "🚀 Initializing submodules..."
        
        INIT_ARGS=""
        if [ "$RECURSIVE" = true ]; then
            INIT_ARGS="$INIT_ARGS --recursive"
        fi
        
        if [ "$FORCE" = true ]; then
            INIT_ARGS="$INIT_ARGS --force"
        fi
        
        if [ -n "$SPECIFIC_MODULE" ]; then
            git submodule update --init $INIT_ARGS "$SPECIFIC_MODULE"
        else
            git submodule update --init $INIT_ARGS
        fi
        
        echo "✅ Submodule initialization complete"
        ;;
        
    "update")
        echo "🔄 Updating submodules..."
        
        # First, sync URLs in case they changed
        echo "🔗 Syncing submodule URLs..."
        git submodule sync
        
        UPDATE_ARGS="--remote"
        if [ "$RECURSIVE" = true ]; then
            UPDATE_ARGS="$UPDATE_ARGS --recursive"
        fi
        
        if [ "$FORCE" = true ]; then
            UPDATE_ARGS="$UPDATE_ARGS --force"
        fi
        
        if [ -n "$SPECIFIC_MODULE" ]; then
            git submodule update $UPDATE_ARGS "$SPECIFIC_MODULE"
        else
            git submodule update $UPDATE_ARGS
        fi
        
        echo ""
        echo "🔍 Checking for critical submodule branches..."
        
        # Ensure imgui is on docking branch
        if [ -d "ScreenMonitor/external/imgui" ]; then
            cd "ScreenMonitor/external/imgui"
            CURRENT_BRANCH=$(git branch --show-current 2>/dev/null || echo "detached")
            if [ "$CURRENT_BRANCH" != "docking" ]; then
                echo "🔧 Switching imgui to docking branch..."
                git checkout docking 2>/dev/null || git checkout origin/docking
            fi
            cd - > /dev/null
        fi
        
        echo "✅ Submodule update complete"
        ;;
        
    "reset")
        echo "🔄 Resetting submodules to committed versions..."
        
        RESET_ARGS=""
        if [ "$RECURSIVE" = true ]; then
            RESET_ARGS="$RESET_ARGS --recursive"
        fi
        
        if [ "$FORCE" = true ]; then
            RESET_ARGS="$RESET_ARGS --force"
        fi
        
        if [ -n "$SPECIFIC_MODULE" ]; then
            git submodule update $RESET_ARGS "$SPECIFIC_MODULE"
        else
            git submodule update $RESET_ARGS
        fi
        
        echo "✅ Submodule reset complete"
        ;;
        
    "sync")
        echo "🔗 Syncing submodule URLs..."
        
        if [ -n "$SPECIFIC_MODULE" ]; then
            git submodule sync "$SPECIFIC_MODULE"
        else
            git submodule sync
        fi
        
        echo "✅ Submodule URL sync complete"
        ;;
        
    "clean")
        echo "🧹 Cleaning submodule directories..."
        
        if [ "$FORCE" != true ]; then
            echo "⚠️  This will remove all submodule directories!"
            echo "   Use --force to confirm this action"
            exit 1
        fi
        
        for submodule in "${SUBMODULES[@]}"; do
            if [ -d "$submodule" ]; then
                echo "   Removing: $submodule"
                rm -rf "$submodule"
            fi
        done
        
        echo "✅ Submodule cleanup complete"
        echo "💡 Run /cpp-submodules --init to reinitialize"
        ;;
esac

# Phase 4: Post-Action Verification
echo ""
echo "✅ Phase 4: Post-Action Verification"
echo "------------------------------------"

# Check critical submodules
MISSING_CRITICAL=()
for critical in "${CRITICAL_MODULES[@]}"; do
    if [ ! -d "$critical" ] || [ -z "$(ls -A "$critical" 2>/dev/null)" ]; then
        MISSING_CRITICAL+=("$critical")
    fi
done

if [ ${#MISSING_CRITICAL[@]} -gt 0 ]; then
    echo "⚠️  Critical submodules missing:"
    for missing in "${MISSING_CRITICAL[@]}"; do
        echo "   - $missing"
    done
    echo ""
    echo "💡 Run: /cpp-submodules --init --force"
else
    echo "✅ All critical submodules are present"
fi

# Check ImGui docking branch
if [ -d "ScreenMonitor/external/imgui" ]; then
    cd "ScreenMonitor/external/imgui"
    IMGUI_BRANCH=$(git branch --show-current 2>/dev/null || echo "detached")
    if [ "$IMGUI_BRANCH" = "docking" ]; then
        echo "✅ ImGui is on docking branch"
    else
        echo "⚠️  ImGui branch: $IMGUI_BRANCH (should be 'docking')"
    fi
    cd - > /dev/null
fi

echo ""
echo "🎉 Submodule Management Complete!"
echo "================================="
echo ""
echo "📖 Operation Summary:"
echo "   - Action Performed: $ACTION"
echo "   - Total Submodules: ${#SUBMODULES[@]}"
echo "   - Critical Modules Missing: ${#MISSING_CRITICAL[@]}"
echo ""
echo "💡 Next Steps:"
if [ ${#MISSING_CRITICAL[@]} -eq 0 ]; then
    echo "   - Build project: /cpp-build"
    echo "   - Run tests: /cpp-test"
    echo "   - Check status: /cpp-submodules --status"
else
    echo "   - Initialize missing: /cpp-submodules --init --force"
    echo "   - Retry update: /cpp-submodules --update"
fi
echo ""
echo "🔧 Available Commands:"
echo "   - /cpp-submodules --status    # Check current status"
echo "   - /cpp-submodules --update    # Update all to latest"
echo "   - /cpp-build                  # Build with current submodules"