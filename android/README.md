# RigelEngine: Proton Android ARM64 build

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

- rigelengine-proton-arm64-debug.apk: signed development/test APK.
- rigelengine-proton-arm64-unsigned.apk: aligned APK for separate signing.
- SHA256SUMS, COMMIT and SOURCE_STATE: artifact hashes and source revision/status.

Packaging checks an explicit file allowlist: manifest, generated Android resource table,
compiled Java, and the four required ARM64 shared libraries. It fails on missing, duplicate or extra
payloads and checks the native ELF architecture. No game EXEs, archives, maps,
art, audio, research files or signing keys enter the APK or uploaded artifact.

## Signing and device updates

Public APKs on the repository Releases page use the permanent project signing key.
An update with the same package and certificate preserves saves/settings. The private
keystore and its password are backed up outside the checkout and stored as GitHub
Actions secrets; never commit them. Development CI APKs use disposable keys and cannot
update a release installation. Do not uninstall/reset a player's app as a workaround.

Local development updates can still use ANDROID_DEBUG_KEYSTORE with the original
local key. For release builds set RIGEL_ANDROID_RELEASE=1, ANDROID_KEYSTORE and
ANDROID_KEYSTORE_PASSWORD (and optionally ANDROID_KEY_PASSWORD and ANDROID_KEY_ALIAS).
The alias defaults to rigel; the key password defaults to the keystore password.
Passwords are passed through environment variables, not command arguments. A release
build fails before compilation if the permanent keystore or password is missing.
Its signed output is rigelengine-proton-arm64.apk.

The release workflow requires ANDROID_RELEASE_KEYSTORE_BASE64 and
ANDROID_RELEASE_KEYSTORE_PASSWORD repository secrets. Only that workflow receives
the permanent key; pull-request and development CI jobs do not. The key is decoded
under the runner's temporary directory and removed even if the job fails.

Version metadata lives in version.json. Keep the Android manifest and CMake numeric
version in sync; tools/release_version.py checks them. Increase android_version_code
for every published APK, including betas. The release tag must be v followed by the
exact version string. See [release process](../docs/RELEASING.md).

## Build-path overrides

- RIGEL_ANDROID_BUILD_DIR: separate build/cache/output directory.
- RIGEL_ANDROID_JOBS: parallel native builds, default 2.
- ANDROID_BUILD_TOOLS: direct build-tools directory.
- ANDROID_PLATFORM_JAR: direct platform android.jar.
- JAVA_HOME: selected Java installation.

The last two Android path overrides allow verification with an existing local
SDK layout. CI uses the normal SDK package layout. SDK licenses are accepted by
the Android setup action; development CI needs no signing secrets or game downloads.

## Player setup

The launcher is SetupActivity in org.rigelengineproton.android. Before SDL starts,
the player chooses a directory using ACTION_OPEN_DOCUMENT_TREE. The chosen tree's
read permission is persisted. Imports scan that directory and its immediate DN1/
Duke1 and DN2/Duke2 subfolders, case-insensitively, and accept only DN1 episode EXEs/
.DN1-.DN3 files and NUKEM2.CMP/NUKEM2.F1-F5. Mixed files work too. Duplicate names are
reported rather than silently choosing a copy. Each import is a replacement, so select
a folder containing every game you want available.

Recognized files are copied into the app's no-backup internal directory. They are
not accessed through guessed paths to document URIs, and the selected originals are
never modified. Native code receives the absolute imported path as its command-line
game directory. SDL's Android argv[0] is app_process and is explicitly accepted.
There are no storage-wide or MANAGE_EXTERNAL_STORAGE permissions.

A pending import is validated before replacing the active library; IO errors,
empty files, cancellation and size limits leave the old library available. An
interrupted directory swap is recovered on next startup. Imports have 64 MiB/file,
256 MiB/total and 4096-entry-per-directory bounds, well above the DOS distributions.
Detection is based on filenames and nonempty episode EXE/first map pairs or the CMP;
native loaders still validate actual file contents when used.

On the first launch, the setup screen offers a folder picker and starts SDL after
the player imports files and chooses Play. Later launches start the game automatically.
Choose GAME FILES from the launcher to reopen setup and reimport or
select a different folder. Imported data is excluded from Android backups and is
removed on uninstall. The user keeps the original shared-storage folder.
Older private package org.rigelengine.android is separate; no settings migration.

Test the importer without a device using bash tools/test_android_import.sh.
Tests use generated bytes only. The system picker, persisted grant, controller UI,
lifecycle and real game loading require the fresh-install manual test on Android.
