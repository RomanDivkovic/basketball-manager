#!/bin/bash
# Builds the native BMCore library for one or all platforms and copies it into the Unity plugin folders.
# Usage: scripts/build-native.sh [mac|win|android|ios|all]   (default: all)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGINS="$ROOT/UI-unity-bm26/Assets/Plugins"
UNITY_EDITOR="${UNITY_EDITOR:-/Applications/Unity/Hub/Editor/6000.6.7f1}"
NDK="${ANDROID_NDK:-$UNITY_EDITOR/PlaybackEngines/AndroidPlayer/NDK}"
MAC_SDK="$(xcrun --sdk macosx --show-sdk-path)"
JOBS="$(sysctl -n hw.ncpu)"
TARGET="${1:-all}"

mac() {
    cmake -S "$ROOT" -B /tmp/bm-mac -DCMAKE_BUILD_TYPE=Release -DBM_BUILD_TOOLS=OFF \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_SYSROOT="$MAC_SDK" > /dev/null
    cmake --build /tmp/bm-mac -j"$JOBS"
    mkdir -p "$PLUGINS/macOS"
    cp /tmp/bm-mac/lib/libBMCore.dylib "$PLUGINS/macOS/libBMCore.dylib"
}

win() {
    cmake -S "$ROOT" -B /tmp/bm-win -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/windows-x64-zig.cmake" \
        -DCMAKE_BUILD_TYPE=Release -DBM_BUILD_TOOLS=OFF > /dev/null
    cmake --build /tmp/bm-win -j"$JOBS"
    mkdir -p "$PLUGINS/Windows/x86_64"
    cp /tmp/bm-win/bin/BMCore.dll "$PLUGINS/Windows/x86_64/BMCore.dll"
}

android() {
    cmake -S "$ROOT" -B /tmp/bm-android -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Release -DBM_BUILD_TOOLS=OFF > /dev/null
    cmake --build /tmp/bm-android -j"$JOBS"
    mkdir -p "$PLUGINS/Android/arm64-v8a"
    cp /tmp/bm-android/lib/libBMCore.so "$PLUGINS/Android/arm64-v8a/libBMCore.so"
    "$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" --strip-unneeded "$PLUGINS/Android/arm64-v8a/libBMCore.so" || true
}

ios() {
    cmake -S "$ROOT" -B /tmp/bm-ios -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_BUILD_TYPE=Release -DBM_STATIC=ON -DBM_BUILD_TOOLS=OFF > /dev/null
    cmake --build /tmp/bm-ios -j"$JOBS"
    mkdir -p "$PLUGINS/iOS"
    libtool -static -o "$PLUGINS/iOS/libBMCore.a" /tmp/bm-ios/bm-core/libBMCore.a /tmp/bm-ios/bm-core/libbm_lua.a /tmp/bm-ios/bm-core/libbm_sqlite.a
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
