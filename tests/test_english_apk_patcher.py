"""Regression tests for NFSU1's direct-APK translation patch."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "nfsu1_apk_patch", "scripts/patch_english_release_apk.py"
)
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)


class StageTransitionTests(unittest.TestCase):
    def test_native_marker_matches_android_overlay_handler(self):
        android = Path(
            "android/app/src/main/java/com/nfsu1/androidevolved/GameActivity.java"
        ).read_text(encoding="utf-8")
        self.assertIn('message.contains("' + patch.STARTUP_ENGLISH + '")', android)
        source = Path("android/app/src/main/cpp/guest_boot_worker.h").read_text(
            encoding="utf-8"
        )
        self.assertIn(patch.STARTUP_ENGLISH, source)

    def test_patched_elf_string_is_null_terminated_and_size_preserving(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            directory = root / "lib" / "arm64-v8a"
            directory.mkdir(parents=True)
            lib = directory / "libnfsu1_android.so"
            spanish = patch.STARTUP_SPANISH.encode("utf-8")
            english = patch.STARTUP_ENGLISH.encode("utf-8")
            original = b"prefix\x00" + spanish + b"\x00tail" + spanish + b" not-terminated"
            lib.write_bytes(original)
            patch.patch_native(root)
            expected = (
                b"prefix\x00" + english
                + b"\x00" * (len(spanish) - len(english) + 1)
                + b"tail" + spanish + b" not-terminated"
            )
            self.assertEqual(lib.read_bytes(), expected)
            self.assertEqual(len(lib.read_bytes()), len(original))

    def test_absent_native_status_fails_closed(self):
        with tempfile.TemporaryDirectory() as folder:
            lib = Path(folder) / "lib" / "arm64-v8a"
            lib.mkdir(parents=True)
            (lib / "libnfsu1_android.so").write_bytes(b"no startup literal")
            with self.assertRaisesRegex(RuntimeError, "Expected one native startup marker"):
                patch.patch_native(Path(folder))

    def test_release_version_is_bumped(self):
        with tempfile.TemporaryDirectory() as folder:
            metadata = Path(folder) / "apktool.yml"
            metadata.write_text(
                "versionInfo:\n  versionCode: 7\n  versionName: 0.2.5\n",
                encoding="utf-8"
            )
            patch.patch_version(Path(folder))
            out = metadata.read_text(encoding="utf-8")
            self.assertIn("versionCode: '9'", out)
            self.assertIn("versionName: 0.2.7", out)


if __name__ == "__main__":
    unittest.main()
