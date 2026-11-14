#!/bin/bash
# Zips the finished desktop builds for upload (itch.io, testers). Run after the Unity builds.
# Usage: scripts/package-release.sh <version>      e.g. scripts/package-release.sh 1.0.0-beta1
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILDS="$ROOT/UI-unity-bm26/Builds"
VERSION="${1:?version missing}"
OUT="$ROOT/release/$VERSION"
mkdir -p "$OUT"

if [ -d "$BUILDS/macOS/Basketball Manager 26.app" ]; then
    # ditto keeps the code signature and extended attributes intact
    ditto -c -k --sequesterRsrc --keepParent "$BUILDS/macOS/Basketball Manager 26.app" "$OUT/BasketballManager26-$VERSION-mac.zip"
fi
if [ -d "$BUILDS/Windows" ]; then
    (cd "$BUILDS/Windows" && rm -rf "Basketball Manager 26_BackUpThisFolder_ButDontShipItWithYourGame" \
        && zip -qr "$OUT/BasketballManager26-$VERSION-windows.zip" . -x "*.pdb")
fi
[ -f "$BUILDS/Android/BasketballManager26.apk" ] && cp "$BUILDS/Android/BasketballManager26.apk" "$OUT/BasketballManager26-$VERSION.apk"
[ -f "$BUILDS/Android/BasketballManager26.aab" ] && cp "$BUILDS/Android/BasketballManager26.aab" "$OUT/BasketballManager26-$VERSION.aab"
ls -la "$OUT"
