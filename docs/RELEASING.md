# Releasing RigelEngine: Proton

1. Update version.json (including a strictly increasing Android version code),
   android/app/AndroidManifest.xml, and the numeric CMake project version.
   Run python3 tools/release_version.py. Tags must match the full version string.
2. Commit and push master. Wait for both CI and Bundling on that exact commit.
   CI tests Linux x64/ARM64 and Windows x64 Debug/Release and builds Android.
   Bundling produces Linux x64/ARM64 OpenGL and GLES deb/tar packages; CI produces
   the Windows Release ZIP. No game data is packaged.
3. Download and test the artifacts: Windows and Linux desktop smoke tests, Linux
   ARM64 on a Pi, and a locally release-signed Android APK on a handheld.
   The Ubuntu 24.04 Linux packages need compatible runtime libraries.
4. Push an annotated tag, e.g. v0.10.0-beta.1, pointing to the tested commit.
   The Release workflow validates the tag, signs Android with the permanent key,
   retrieves desktop packages from successful workflows for the exact tagged commit,
   and creates a prerelease with checksums. It refuses to publish without those checks.
   Do not manually create a release first: the workflow creates it.
5. For a failed publication, fix the problem and use a new version/tag if source changed.
   A manual Release dispatch can retry an existing tag if no release was created.

The repository secrets ANDROID_RELEASE_KEYSTORE_BASE64 and
ANDROID_RELEASE_KEYSTORE_PASSWORD must be configured before tagging. Keep an offline
backup of the keystore and password; replacing the key breaks ordinary Android updates.
Never put either value in an issue, commit, build artifact or release notes.

All tags currently produce prereleases, even without a beta suffix. After validation,
a maintainer can edit the GitHub release to remove its prerelease status.
Development CI APKs have disposable signing keys; the public APK is the one attached
to the release. The package ID stays org.rigelengine.android for update compatibility.
