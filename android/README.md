# Android ARM64 build

The CI job cross-compiles on GitHub's Ubuntu runner and uploads an APK artifact on
pushes, pull requests and manual CI runs. It does not emulate Android, launch the
game, test a controller or certify frame rate. Existing native test jobs run
alongside it. No self-hosted runner or attached G2 is required.

## Reproduce locally on Linux

Initialize Git submodules. Install CMake, a C++ host toolchain, curl, Python 3 and
Java 17, plus the Android command-line tools. Install the pinned SDK components:

    sdkmanager "platforms;android-33" "build-tools;35.0.0" "ndk;27.2.12479018"
    export ANDROID_HOME=/path/to/android-sdk
    export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
    bash tools/build_android.sh

The script builds SDL 2.30.9 and SDL_mixer 2.8.0 from SHA-256-verified upstream
archives, then RigelEngine for arm64-v8a / minimum API 26 with OpenGL ES.
These versions and target API 33 match the existing device-tested setup.
It uses standard [SDK package installation](https://developer.android.com/tools/sdkmanager)
and [APK signing/verification](https://developer.android.com/tools/apksigner) tools.
Java compilation failures fail the build.

The current mixer configuration supports WAV/PC-speaker effects and the engine's
internal music synthesis. External compressed audio codecs remain disabled,
matching the current local build; this is not an optional-music packaging job.

Outputs are in build-android/output:

- RigelEngine-arm64-debug.apk: signed development/test APK.
- RigelEngine-arm64-unsigned.apk: aligned APK for separate signing.
- SHA256SUMS, COMMIT and SOURCE_STATE: artifact hashes and source revision/status.

Packaging checks an explicit file allowlist: manifest, generated Android resource table,
compiled Java, and the four required ARM64 shared libraries. It fails on missing, duplicate or extra
payloads and checks the native ELF architecture. No game EXEs, archives, maps,
art, audio, research files or signing keys enter the APK or uploaded artifact.

## Signing and device updates

Each fresh CI runner generates a disposable debug key. CI APKs are test builds,
not release builds, and successive runs do not have a stable signing identity.
An APK signed this way cannot update the existing G2 installation signed with a
different key. Do not uninstall/reset the player's app as a workaround.

For local in-place updates, set ANDROID_DEBUG_KEYSTORE to the existing local
debug keystore (alias rigel, debug password android). It is never copied into the
repository or artifact. For repeatable distribution, use the unsigned artifact
with your stable signing process; release signing is outside this CI job.

## Build-path overrides

- RIGEL_ANDROID_BUILD_DIR: separate build/cache/output directory.
- RIGEL_ANDROID_JOBS: parallel native builds, default 2.
- ANDROID_BUILD_TOOLS: direct build-tools directory.
- ANDROID_PLATFORM_JAR: direct platform android.jar.
- JAVA_HOME: selected Java installation.

The last two Android path overrides allow verification with an existing local
SDK layout. CI uses the normal SDK package layout. SDK licenses are accepted by
the Android setup action; no signing secrets or game downloads are used.
