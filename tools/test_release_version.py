"""Release metadata must agree before any build or tag publication."""
import json
import tempfile
import unittest
from pathlib import Path

from release_version import read_version


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "android/app").mkdir(parents=True)
        (self.root / "version.json").write_text(json.dumps({
            "version": "0.10.0-beta.1", "android_version_code": 100001
        }))
        (self.root / "CMakeLists.txt").write_text("project(RigelEngineProton VERSION 0.10.0)")
        (self.root / "android/app/AndroidManifest.xml").write_text(
            '<manifest xmlns:android="http://schemas.android.com/apk/res/android" '
            'android:versionName="0.10.0-beta.1" android:versionCode="100001"/>'
        )

    def test_matching_metadata(self):
        self.assertEqual(read_version(self.root), ("0.10.0-beta.1", 100001))

    def test_stale_manifest_rejected(self):
        p = self.root / "android/app/AndroidManifest.xml"
        p.write_text(p.read_text().replace("100001", "1"))
        with self.assertRaises(ValueError):
            read_version(self.root)

    def test_stale_desktop_version_rejected(self):
        (self.root / "CMakeLists.txt").write_text("project(RigelEngineProton VERSION 0.9.1)")
        with self.assertRaises(ValueError):
            read_version(self.root)

    def test_invalid_version_code_rejected(self):
        (self.root / "version.json").write_text(json.dumps({
            "version": "0.10.0-beta.1", "android_version_code": True
        }))
        with self.assertRaises(ValueError):
            read_version(self.root)


if __name__ == "__main__":
    unittest.main()
