"""Exercise the real, offline TigerSheets launcher-asset command."""
from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "generate_app_icons.py"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


class BrandingTests(unittest.TestCase):
    def test_write_generates_all_launcher_sizes_with_transparent_edges(self):
        self.assertTrue(GENERATOR.is_file(), "TigerSheets needs an offline icon-generation command")
        with tempfile.TemporaryDirectory(prefix="tiger-sheets-branding-") as directory:
            target = Path(directory)
            command = [sys.executable, str(GENERATOR), "--write", "--icon-root", str(target)]
            first = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(first.returncode, 0, first.stderr)
            first_bytes = {}
            for size in SIZES:
                path = target / "hicolor" / f"{size}x{size}" / "apps" / "tiger-sheets.png"
                self.assertTrue(path.is_file(), f"missing {size}px Ubuntu launcher icon")
                first_bytes[size] = path.read_bytes()
                with Image.open(path) as icon:
                    self.assertEqual(icon.size, (size, size))
                    self.assertEqual(icon.mode, "RGBA")
                    self.assertEqual(icon.getpixel((0, 0))[3], 0)
                    self.assertGreater(icon.getpixel((size // 2, size // 2))[3], 240)
                    self.assertGreater(sum(alpha == 0 for alpha in icon.getchannel("A").getdata()), size * size // 10)
            second = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(second.returncode, 0, second.stderr)
            for size in SIZES:
                path = target / "hicolor" / f"{size}x{size}" / "apps" / "tiger-sheets.png"
                self.assertEqual(path.read_bytes(), first_bytes[size], "repeating generation changed PNG bytes")

    def test_check_detects_missing_and_changed_icons_without_modifying_them(self):
        with tempfile.TemporaryDirectory(prefix="tiger-sheets-branding-") as directory:
            command = [sys.executable, str(GENERATOR), "--icon-root", directory]
            subprocess.run(command + ["--write"], capture_output=True, text=True, check=True)
            check = subprocess.run(command + ["--check"], capture_output=True, text=True, check=False)
            self.assertEqual(check.returncode, 0, check.stderr)
            changed = Path(directory) / "hicolor" / "32x32" / "apps" / "tiger-sheets.png"
            changed.write_bytes(b"invalid icon")
            check = subprocess.run(command + ["--check"], capture_output=True, text=True, check=False)
            self.assertEqual(check.returncode, 1)
            self.assertIn("32x32", check.stderr)
            self.assertEqual(changed.read_bytes(), b"invalid icon", "checking must never regenerate artwork")
            changed.unlink()
            check = subprocess.run(command + ["--check"], capture_output=True, text=True, check=False)
            self.assertEqual(check.returncode, 1)
            self.assertIn("32x32", check.stderr)

    def test_qt_resource_manifest_embeds_all_packaged_launcher_sizes(self):
        manifest = ROOT / "resources" / "tiger-sheets.qrc"
        self.assertTrue(manifest.is_file(), "native Qt windows need the packaged TigerSheets icon resource")
        tree = ET.parse(manifest)
        resources = tree.findall("./qresource[@prefix='/icons']")
        self.assertEqual(len(resources), 1, "launcher artwork must have one dedicated icon resource")
        entries = {entry.get("alias"): entry.text for entry in resources[0].findall("file")}
        self.assertEqual(set(entries), {f"tiger-sheets-{size}.png" for size in SIZES})
        for size in SIZES:
            embedded = (manifest.parent / entries[f"tiger-sheets-{size}.png"]).resolve()
            packaged = ROOT / "packaging" / "icons" / "hicolor" / f"{size}x{size}" / "apps" / "tiger-sheets.png"
            self.assertEqual(embedded, packaged)
            self.assertTrue(packaged.is_file())
            with Image.open(packaged) as icon:
                self.assertEqual(icon.size, (size, size))
        check = subprocess.run([sys.executable, str(GENERATOR), "--check"], capture_output=True, text=True, check=False)
        self.assertEqual(check.returncode, 0, check.stderr)


if __name__ == "__main__":
    unittest.main()
