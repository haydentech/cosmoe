#!/bin/bash
# Profile a Cosmoe application with OProfile
# Usage: sudo ./profile_app.sh <app-name>
# Example: sudo ./profile_app.sh showcase

if [ "$EUID" -ne 0 ]; then 
   echo "Please run with sudo"
   exit 1
fi

APP=${1:-showcase}

# Find the actual executable by looking in the app's directory
APP_DIR="builddir/src/apps/${APP}"
if [ ! -d "$APP_DIR" ]; then
    echo "App directory not found: $APP_DIR"
    echo "Available apps:"
    ls builddir/src/apps/
    exit 1
fi

# Find the executable (skip .rsrc and .o files)
APP_PATH=$(find "$APP_DIR" -maxdepth 1 -type f -executable ! -name "*.rsrc" ! -name "*.o" | head -1)

if [ -z "$APP_PATH" ] || [ ! -f "$APP_PATH" ]; then
    echo "Executable not found in: $APP_DIR"
    echo "Files in directory:"
    ls -la "$APP_DIR"
    exit 1
fi

echo "Found executable: $APP_PATH"

cd /home/billh/git/cosmoe

echo "Resetting OProfile data..."
operf --reset

echo ""
echo "Starting $APP under OProfile..."
echo "Use the app normally, then close it when done profiling"
echo ""

LD_LIBRARY_PATH=builddir:$LD_LIBRARY_PATH operf "$APP_PATH"

echo ""
echo "=== Top Functions by CPU Usage ==="
opreport --symbols --threshold 0.2 | head -40

echo ""
echo ""
echo "Available commands:"
echo "  opreport --symbols                    # Full symbol report"
echo "  opreport --symbols --threshold 1      # Functions using >1% CPU"
echo "  opannotate --source                   # Annotated source code"
echo "  opannotate --source --threshold 1     # Source with >1% samples"
echo "  opreport --callgraph                  # Call graph"
