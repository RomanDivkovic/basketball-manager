#!/bin/zsh

# Basketball Manager Build Script for macOS M1
# Usage: ./build.sh [debug|release]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="${1:-Release}"

echo "🏀 Basketball Manager Build Script"
echo "   Platform: macOS M1 (ARM64)"
echo "   Build Type: $BUILD_TYPE"
echo "   Build Dir: $BUILD_DIR"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "🔨 Configuring CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
       -DCMAKE_OSX_ARCHITECTURES=arm64 \
       -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
       ..

# Build
echo ""
echo "🏗️  Building Basketball Manager..."
make -j4

# Build output
echo ""
echo "✅ Build complete!"
echo ""
echo "📦 Output files:"
echo "   Library: $BUILD_DIR/lib/libBMCore.dylib"
echo "   Binary: $BUILD_DIR/bin/bm-core"
echo ""
echo "🎮 Next steps:"
echo "   1. Create Unity project"
echo "   2. Add C# bindings to load libBMCore.dylib"
echo "   3. Integrate UI with C++ backend"
