RigelEngine: Proton brings Duke Nukem I and II together with a shared launcher,
modern HUD and menus, controller support, widescreen rendering and persistent saves.

Downloads:
- Android ARM64 APK (Android 8.0+, permanent project signature).
- Windows x64 ZIP (extract it; install the Microsoft VC++ 2015–2022 x64 runtime if needed).
- Linux x64/ARM64 deb and tar.gz packages, built on Ubuntu 24.04; compatible system
  SDL2/SDL2_mixer libraries are required. The x64 packages use OpenGL; on ARM64
  choose gl for OpenGL or gles for OpenGL ES (Raspberry Pi and similar boards).

**Supply your own DOS game files.** No game data is included. On Android select a
normal folder using Choose game folder. Files may be mixed together or inside DN1/
DN2 subfolders. The app imports recognized data without changing your originals;
use GAME FILES to reimport after updating the source folder. No ADB setup is needed.
Desktop builds prompt for the data path. See README for required episode files.
Either game works independently. The Android package is org.rigelengineproton.android;
it installs separately from earlier private test builds.

This is a beta. A controller is recommended on Android; touch controls are incomplete.
Keep save backups. Report problems with the platform, version and reproduction steps.
SHA256SUMS verifies downloads; COMMIT identifies the source used.
