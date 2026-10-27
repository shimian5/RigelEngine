#!/usr/bin/env python3
"""Validate the shared release version without loading any game files."""
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read_version(root=ROOT):
    info = json.loads((root / "version.json").read_text())
    version, code = info["version"], info["android_version_code"]
    match = re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z.-]+)?", version)
    if not match or type(code) is not int or not 1 <= code <= 2100000000:
        raise ValueError("Invalid release version or Android version code")
    cmake_version = ".".join(match.groups())
    if f"project(RigelEngineProton VERSION {cmake_version})" not in (root / "CMakeLists.txt").read_text():
        raise ValueError("CMake version disagrees with version.json")
    manifest = ET.parse(root / "android/app/AndroidManifest.xml").getroot()
    ns = "{http://schemas.android.com/apk/res/android}"
    if manifest.get(ns + "versionName") != version or manifest.get(ns + "versionCode") != str(code):
        raise ValueError("Android manifest disagrees with version.json")
    return version, code


if __name__ == "__main__":
    version, code = read_version()
    if len(sys.argv) == 3 and sys.argv[1] == "--tag":
        if sys.argv[2] != "v" + version:
            raise SystemExit("Tag must match version.json: v" + version)
    elif len(sys.argv) != 1:
        raise SystemExit("usage: release_version.py [--tag TAG]")
    print(version)
    print(code)
