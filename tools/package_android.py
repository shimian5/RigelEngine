#!/usr/bin/env python3
"""Package only the explicit native libraries and generated DEX."""
import re
import shutil
import sys
import zipfile
from pathlib import Path

LIBRARIES = {"libmain.so", "libSDL2.so", "libSDL2_mixer.so", "libc++_shared.so"}
PREFIX = "lib/arm64-v8a/"
CONTENTS = {"AndroidManifest.xml", "resources.arsc", "classes.dex"} | {
    PREFIX + name for name in LIBRARIES
}


def verify(path):
    with zipfile.ZipFile(path) as apk:
        files = [name for name in apk.namelist() if not name.endswith("/")]
        signatures = {
            name for name in files
            if name == "META-INF/MANIFEST.MF"
            or re.fullmatch(r"META-INF/[A-Z0-9_-]{1,8}\.(SF|RSA|DSA|EC)", name)
        }
        payload = set(files) - signatures
        if payload != CONTENTS or len(files) != len(set(files)):
            raise RuntimeError("Unexpected or missing APK payload: " + repr(files))
        for name in LIBRARIES:
            elf = apk.read(PREFIX + name)
            if elf[:6] != b"\x7fELF\x02\x01" or int.from_bytes(elf[18:20], "little") != 183:
                raise RuntimeError("Native library is not ARM64: " + name)
        if not apk.read("classes.dex").startswith(b"dex\n"):
            raise RuntimeError("Missing compiled Java bytecode")
    print("Verified ARM64 APK payload; no assets or game files")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--verify":
        verify(Path(sys.argv[2]))
    elif len(sys.argv) == 2:
        stage = Path(sys.argv[1])
        shutil.copyfile(stage / "base.apk", stage / "unsigned.apk")
        with zipfile.ZipFile(stage / "unsigned.apk", "a", zipfile.ZIP_DEFLATED) as apk:
            apk.write(stage / "classes.dex", "classes.dex")
            for name in sorted(LIBRARIES):
                apk.write(stage / PREFIX / name, PREFIX + name)
        verify(stage / "unsigned.apk")
    else:
        raise SystemExit("usage: package_android.py STAGING | --verify APK")
