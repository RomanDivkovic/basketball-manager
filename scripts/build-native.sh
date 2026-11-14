#!/bin/bash
# Builds the native BMCore library for one or all platforms and copies it into the Unity plugin folders.
# Usage: scripts/build-native.sh [mac|win|android|ios|all]   (default: all)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGINS="$ROOT/UI-unity-bm26/Assets/Plugins"
UNITY_EDITOR="${UNITY_EDITOR:-/Applications/Unity/Hub/Editor/6000.6.7f1}"
NDK="${ANDROID_NDK:-$UNITY_EDITOR/PlaybackEngines/AndroidPlayer/NDK}"
MAC_SDK="$(xcrun --sdk macosx --show-sdk-path)"
JOBS="${BM_JOBS:-4}"
TARGET="${1:-all}"
NATIVE_BUILD_ROOT="${BM_NATIVE_BUILD_ROOT:-$ROOT/build/native}"

mac() {
    cmake -S "$ROOT" -B "$NATIVE_BUILD_ROOT/mac" -DCMAKE_BUILD_TYPE=Release -DBM_BUILD_TOOLS=OFF \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_SYSROOT="$MAC_SDK" > /dev/null
    cmake --build "$NATIVE_BUILD_ROOT/mac" -j"$JOBS"
    mkdir -p "$PLUGINS/macOS"
    cp "$NATIVE_BUILD_ROOT/mac"/lib/libBMCore.dylib "$PLUGINS/macOS/libBMCore.dylib"
    install_name_tool -id @rpath/libBMCore.dylib "$PLUGINS/macOS/libBMCore.dylib"
    codesign --force --sign - "$PLUGINS/macOS/libBMCore.dylib"
}

win() {
    cmake -S "$ROOT" -B "$NATIVE_BUILD_ROOT/win" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/windows-x64-zig.cmake" \
        -DCMAKE_BUILD_TYPE=Release -DBM_BUILD_TOOLS=OFF > /dev/null
    cmake --build "$NATIVE_BUILD_ROOT/win" -j"$JOBS"
    mkdir -p "$PLUGINS/Windows/x86_64"
    cp "$NATIVE_BUILD_ROOT/win"/bin/BMCore.dll "$PLUGINS/Windows/x86_64/BMCore.dll"
}

android() {
    cmake -S "$ROOT" -B "$NATIVE_BUILD_ROOT/android" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_BUILD_TYPE=Release -DBM_BUILD_TOOLS=OFF > /dev/null
    cmake --build "$NATIVE_BUILD_ROOT/android" -j"$JOBS"
    mkdir -p "$PLUGINS/Android/arm64-v8a"
    cp "$NATIVE_BUILD_ROOT/android"/lib/libBMCore.so "$PLUGINS/Android/arm64-v8a/libBMCore.so"
    "$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" --strip-unneeded "$PLUGINS/Android/arm64-v8a/libBMCore.so" || true
}

ios() {
    cmake -S "$ROOT" -B "$NATIVE_BUILD_ROOT/ios" -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_BUILD_TYPE=Release -DBM_STATIC=ON -DBM_BUILD_TOOLS=OFF > /dev/null
    cmake --build "$NATIVE_BUILD_ROOT/ios" -j"$JOBS"
    mkdir -p "$PLUGINS/iOS"
    libtool -static -o "$PLUGINS/iOS/libBMCore.a" "$NATIVE_BUILD_ROOT/ios"/bm-core/libBMCore.a "$NATIVE_BUILD_ROOT/ios"/bm-core/libbm_lua.a "$NATIVE_BUILD_ROOT/ios"/bm-core/libbm_sqlite.a
}

case "$TARGET" in
    mac) mac ;;
    win) win ;;
    android) android ;;
    ios) ios ;;
    all) mac; win; android; ios ;;
    *) echo "unknown target $TARGET"; exit 1 ;;
esac
echo "Done: $TARGET"
