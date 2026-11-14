#!/bin/bash
# Package completed builds without deleting backups or existing release folders.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${1:?Usage: scripts/package-release.sh VERSION}"
[[ "$VERSION" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || { echo "Invalid version" >&2; exit 1; }
BUILDS="${BM_BUILDS:-$ROOT/UI-unity-bm26/Builds/Release/$VERSION}"
OUT="$ROOT/release/$VERSION"
mkdir -p "$OUT"
if [ -d "$BUILDS/macOS/Basketball Manager 26.app" ]; then
    ditto -c -k --sequesterRsrc --keepParent "$BUILDS/macOS/Basketball Manager 26.app" "$OUT/BasketballManager26-$VERSION-mac.zip"
fi
if [ -f "$BUILDS/Windows/Basketball Manager 26.exe" ]; then
    (cd "$BUILDS/Windows" && zip -qr "$OUT/BasketballManager26-$VERSION-windows.zip" . -x '*.pdb' '*BackUpThisFolder_ButDontShipItWithYourGame*' '*BurstDebugInformation_DoNotShip*')
fi
for ext in apk aab; do
    if [ -f "$BUILDS/Android/BasketballManager26.$ext" ]; then cp "$BUILDS/Android/BasketballManager26.$ext" "$OUT/BasketballManager26-$VERSION.$ext"; fi
done
(cd "$OUT" && find . -maxdepth 1 -type f ! -name SHA256SUMS -exec shasum -a 256 {} \; > SHA256SUMS)
