#!/usr/bin/env python3
"""Generate TigerSheets' Ubuntu launcher icons offline from its canonical logo."""
from __future__ import annotations

import argparse
import io
from pathlib import Path
import sys

from PIL import Image


SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "packaging" / "icons" / "tiger-sheets-master.png"


def output_path(icon_root: Path, size: int) -> Path:
    return icon_root / "hicolor" / f"{size}x{size}" / "apps" / "tiger-sheets.png"


def render_png(master: Image.Image, size: int) -> bytes:
    # Premultiplication avoids dark fringes on translucent downsampled edges.
    resized = master.convert("RGBa").resize(
        (size, size), getattr(Image, "Resampling", Image).LANCZOS, reducing_gap=3.0
    ).convert("RGBA")
    encoded = io.BytesIO()
    resized.save(encoded, format="PNG", optimize=True, compress_level=9)
    return encoded.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="regenerate the launcher PNGs")
    mode.add_argument("--check", action="store_true", help="verify without changing any files")
    parser.add_argument("--icon-root", type=Path, default=ROOT / "packaging" / "icons")
    args = parser.parse_args()
    try:
        with Image.open(MASTER) as source:
            if source.mode != "RGBA" or source.width != source.height:
                raise ValueError("canonical logo must be a square RGBA PNG")
            master = source.copy()
        matches = True
        for size in SIZES:
            path = output_path(args.icon_root, size)
            expected = render_png(master, size)
            if args.check:
                if not path.is_file() or path.read_bytes() != expected:
                    print(f"Missing or changed launcher icon: {path}", file=sys.stderr)
                    matches = False
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(expected)
        if not matches:
            return 1
        print("All eight TigerSheets launcher icons " + ("match." if args.check else "generated."))
    except (OSError, ValueError) as error:
        print(f"Icon generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
