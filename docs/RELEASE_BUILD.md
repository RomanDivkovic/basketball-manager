# Platform builds

Use Unity 6000.6.7f1 and its macOS, Windows, Android and iOS modules.
Run `scripts/build-native.sh all` before committing refreshed Unity plugins.
The native build uses bundled SQLite, Lua, Zig for Windows, and Unity NDK for Android.
Android native libraries support 16 KB pages. iOS links a combined static archive.

Build Unity with `-batchmode -projectPath UI-unity-bm26 -executeMethod METHOD`.
Methods: `BuildMac.Build`, `BuildPlatforms.Windows`, `BuildPlatforms.Android`,
and `BuildPlatforms.Ios`.
Set `BM_VERSION=1.0.0`, `BM_BUILD_NUMBER=2`, and `BM_BUILD_PATH` to an explicit output path.
Set `BM_AAB=1` for an Android bundle; otherwise Android produces an APK.
Android uses legacy input for the IMGUI runtime; desktop/iOS use Both.
Windows uses Mono because Windows IL2CPP requires a Windows build host.

Build from a separate project copy if the development editor is open. Copy only
Assets, Packages and ProjectSettings; never share a writable Library across editors.
Use `scripts/package-release.sh 1.0.0-rc2` to package completed builds under
`UI-unity-bm26/Builds/Release/1.0.0-rc2`, or set `BM_BUILDS`. Outputs and SHA256SUMS
go into ignored `release/1.0.0-rc2/`; old release folders are preserved.

## Signing

An ad-hoc signed macOS build is for internal use. Public direct distribution needs
a Developer ID Application certificate, notarization and stapling.
Android default debug signing is for local testing. Play upload requires a retained
release/upload keystore and the correct account configuration. Never commit keys
or passwords. iOS export creates an Xcode project. An unsigned archive can be
built with CODE_SIGNING_ALLOWED=NO, but App Store export requires the game owner
Apple Developer team, Apple Distribution signing and an appropriate profile.
Pass BM_APPLE_TEAM_ID only for the game owner team; never reuse unrelated profiles.
No build helper uploads or publishes to stores.
