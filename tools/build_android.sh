#!/usr/bin/env bash
# Linux-hosted ARM64 APK build; no game files are read or packaged.
set -euo pipefail

repo=$(cd -- "$(dirname -- "$0")/.." && pwd)
build=${RIGEL_ANDROID_BUILD_DIR:-"$repo/build-android"}
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
ndk=${ANDROID_NDK_HOME:-}
jobs=${RIGEL_ANDROID_JOBS:-2}
java_bin=${JAVA_HOME:+"$JAVA_HOME/bin/"}
tools=${ANDROID_BUILD_TOOLS:-"$sdk/build-tools/35.0.0"}
android_jar=${ANDROID_PLATFORM_JAR:-"$sdk/platforms/android-33/android.jar"}
[[ -n "$ndk" && -f "$ndk/build/cmake/android.toolchain.cmake" ]] ||
  { echo "Set ANDROID_NDK_HOME to NDK 27.2.12479018" >&2; exit 1; }
[[ -f "$android_jar" && -x "$tools/aapt2" ]] ||
  { echo "Android platform 33 and build-tools 35.0.0 are required" >&2; exit 1; }
grep -q '^Pkg.Revision = 27.2.12479018$' "$ndk/source.properties" ||
  { echo "This build pins NDK r27c (27.2.12479018)" >&2; exit 1; }

# Refuse a release without its permanent identity before doing any compilation.
release=${RIGEL_ANDROID_RELEASE:-0}
key=${ANDROID_KEYSTORE:-${ANDROID_DEBUG_KEYSTORE:-}}
if [[ "$release" == 1 ]]; then
  [[ -n "$key" && -f "$key" && -n "${ANDROID_KEYSTORE_PASSWORD:-}" ]] ||
    { echo "Release requires ANDROID_KEYSTORE and ANDROID_KEYSTORE_PASSWORD" >&2; exit 1; }
fi
if [[ -n "$key" && ! -f "$key" ]]; then
  echo "Specified signing keystore does not exist" >&2
  exit 1
fi
mapfile -t version < <(python3 "$repo/tools/release_version.py")
[[ ${#version[@]} == 2 ]] || { echo "Invalid release version" >&2; exit 1; }

mkdir -p "$build"
build=$(cd "$build" && pwd)
downloads="$build/downloads"
sources="$build/sources"
prefix="$build/prefix"
output="$build/output"
mkdir -p "$downloads" "$sources" "$output"

fetch() {
  local name=$1 url=$2 digest=$3
  if [[ ! -f "$downloads/$name" ]]; then
    curl --fail --location --retry 3 --output "$downloads/$name.part" "$url"
    mv "$downloads/$name.part" "$downloads/$name"
  fi
  printf '%s  %s\n' "$digest" "$downloads/$name" | sha256sum --check -
}

fetch SDL2-2.30.9.tar.gz \
  https://www.libsdl.org/release/SDL2-2.30.9.tar.gz \
  24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4
fetch SDL2_mixer-2.8.0.tar.gz \
  https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-2.8.0.tar.gz \
  1cfb34c87b26dbdbc7afd68c4f545c0116ab5f90bbfecc5aebe2a9cb4bb31549

for name in SDL2-2.30.9 SDL2_mixer-2.8.0; do
  if [[ ! -f "$sources/$name/CMakeLists.txt" ]]; then
    mkdir -p "$sources/$name"
    tar -xzf "$downloads/$name.tar.gz" -C "$sources/$name" --strip-components=1
  fi
done

common=(
  "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake"
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26
  -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  "-DCMAKE_INSTALL_PREFIX=$prefix"
)
cmake -S "$sources/SDL2-2.30.9" -B "$build/sdl2" "${common[@]}" \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
cmake --build "$build/sdl2" --parallel "$jobs"
cmake --install "$build/sdl2"
cmake -S "$sources/SDL2_mixer-2.8.0" -B "$build/mixer" "${common[@]}" \
  "-DCMAKE_PREFIX_PATH=$prefix" "-DSDL2_DIR=$prefix/lib/cmake/SDL2" \
  "-DSDL2_LIBRARY=$prefix/lib/libSDL2.so" "-DSDL2_INCLUDE_DIR=$prefix/include/SDL2" \
  -DBUILD_SHARED_LIBS=ON \
  -DSDL2MIXER_SAMPLES=OFF -DSDL2MIXER_VENDORED=OFF \
  -DSDL2MIXER_DEPS_SHARED=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_GME=OFF \
  -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_MOD=OFF -DSDL2MIXER_MP3=OFF \
  -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_VORBIS=OFF -DSDL2MIXER_WAVPACK=OFF \
  -DSDL2MIXER_WAVE=ON
cmake --build "$build/mixer" --parallel "$jobs"
cmake --install "$build/mixer"
cmake -S "$repo" -B "$build/engine" "${common[@]}" \
  "-DCMAKE_PREFIX_PATH=$prefix" \
  "-DSDL2_LIBRARY=$prefix/lib/libSDL2.so" \
  "-DSDL2_LIBRARY_RELEASE=$prefix/lib/libSDL2.so" \
  "-DSDL2MAIN_LIBRARY=$prefix/lib/libSDL2main.a" \
  "-DSDL2_INCLUDE_DIR=$prefix/include/SDL2" \
  "-DSDL2_MIXER_LIBRARY=$prefix/lib/libSDL2_mixer.so" \
  "-DSDL2_MIXER_INCLUDE_DIR=$prefix/include/SDL2" \
  -DUSE_GL_ES=ON -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF
cmake --build "$build/engine" --target RigelEngine --parallel "$jobs"

stage=$(mktemp -d "$build/package.XXXXXXXX")
mkdir -p "$stage/classes" "$stage/lib/arm64-v8a"
llvm="$ndk/toolchains/llvm/prebuilt/linux-x86_64"
"$llvm/bin/llvm-strip" --strip-unneeded -o "$stage/lib/arm64-v8a/libmain.so" \
  "$build/engine/src/libmain.so"
cp "$prefix/lib/libSDL2.so" "$prefix/lib/libSDL2_mixer.so" \
  "$llvm/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" \
  "$stage/lib/arm64-v8a/"
"${java_bin}javac" -encoding UTF-8 -source 8 -target 8 -Xlint:-options \
  -cp "$android_jar" -d "$stage/classes" \
  "$sources/SDL2-2.30.9/android-project/app/src/main/java/org/libsdl/app/"*.java \
  "$repo/android/app/java/org/rigelengineproton/android/"*.java
mapfile -d '' classes < <(find "$stage/classes" -name '*.class' -print0)
"${java_bin}java" -cp "$tools/lib/d8.jar" com.android.tools.r8.D8 \
  --lib "$android_jar" --min-api 26 --output "$stage" "${classes[@]}"
"$tools/aapt2" link -o "$stage/base.apk" \
  --manifest "$repo/android/app/AndroidManifest.xml" -I "$android_jar" \
  --min-sdk-version 26 --target-sdk-version 33 \
  --version-name "${version[0]}" --version-code "${version[1]}"
python3 "$repo/tools/package_android.py" "$stage"
"$tools/zipalign" -f -p 4 "$stage/unsigned.apk" "$output/rigelengine-proton-arm64-unsigned.apk"

# Development CI is deliberately isolated from the permanent release key.
key=${key:-"$build/debug.keystore"}
if [[ ! -f "$key" ]]; then
  "${java_bin}keytool" -genkeypair -keystore "$key" -storepass android \
    -keypass android -alias rigel -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Rigel Debug" -noprompt
fi
export ANDROID_KEYSTORE_PASSWORD=${ANDROID_KEYSTORE_PASSWORD:-android}
export ANDROID_KEY_PASSWORD=${ANDROID_KEY_PASSWORD:-$ANDROID_KEYSTORE_PASSWORD}
signed=rigelengine-proton-arm64-debug.apk
if [[ "$release" == 1 ]]; then
  signed=rigelengine-proton-arm64.apk
fi
"${java_bin}java" -jar "$tools/lib/apksigner.jar" sign --ks "$key" \
  --ks-key-alias "${ANDROID_KEY_ALIAS:-rigel}" \
  --ks-pass env:ANDROID_KEYSTORE_PASSWORD --key-pass env:ANDROID_KEY_PASSWORD \
  --out "$output/$signed" "$output/rigelengine-proton-arm64-unsigned.apk"
"${java_bin}java" -jar "$tools/lib/apksigner.jar" verify --verbose --print-certs \
  "$output/$signed"
"$tools/zipalign" -c -p 4 "$output/$signed"
python3 "$repo/tools/package_android.py" --verify "$output/$signed"
"$tools/aapt2" dump badging "$output/$signed" |
  grep -F "versionCode='${version[1]}' versionName='${version[0]}'"
(
  cd "$output"
  sha256sum "./$signed" ./rigelengine-proton-arm64-unsigned.apk > SHA256SUMS
  git -C "$repo" rev-parse HEAD > COMMIT
  if git -C "$repo" diff --quiet HEAD; then
    printf 'clean tracked source tree\n' > SOURCE_STATE
  else
    printf 'modified tracked source tree\n' > SOURCE_STATE
  fi
)
printf 'Android APKs: %s\n' "$output"
