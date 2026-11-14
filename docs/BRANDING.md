# TigerSheets branding

TigerSheets uses the same warm neutral and forest Qt interface as OwlDocs, with a sibling tiger mascot. The mascot is a frontal, symmetric faceted tiger head in orange, coral, peach, magenta and plum with cream highlights and a transparent background.

## Source and provenance

The built-in ImageGen tool generated the logo on 2026-11-11 using the inspected OwlDocs `packaging/icons/owl-docs-master.png` as a style reference. No API key, external image service, runtime download or image-generation CLI is required to build or run TigerSheets.

Canonical asset: `packaging/icons/tiger-sheets-master.png`.

- Dimensions: 1254 × 1254 pixels.
- Format: 8-bit RGBA PNG with actual alpha transparency.
- SHA-256: `ce5b5f3a0841c24d37aea46a105dc1f9523c5b5cd4edfddf85e119ec9cd25ee6`.
- The original generated file was copied into the project without modification.
- Final prompt:

> Use case: logo-brand. Asset type: final native Ubuntu application icon for TigerSheets, sibling of OwlDocs. Input image 1 is a style reference only: the approved OwlDocs app mascot. Create a brand-new TIGER head in exactly this same suite visual language: centered symmetrical straight-on animal head, crisp faceted low-poly triangular geometry, saturated orange/coral/peach highlights with magenta, aubergine and deep purple shadow facets, warm cream highlights, rich dark plum eyes with tiny cream catchlights. The subject must read unmistakably as a tiger, with small rounded triangular ears, broad strong feline cheeks, clear dark geometric tiger stripes across forehead and cheeks, almond feline eyes, angular cream muzzle and small dark nose. Keep the premium friendly intelligent mascot feel of OwlDocs and its balanced silhouette and visual weight. Head only, no body. Square canvas with modest transparent margin all around and actual alpha transparency outside the head. Clean precise edges and clear simple silhouette that remains recognizable at 16 pixels. No text, no letters, no spreadsheet/grid badge, no border, no enclosing app tile, no background, no drop shadow, no mockup, no extra objects. Produce a single polished logo asset.

## Ubuntu and Qt assets

`resources/tiger-sheets.qrc` embeds the exact hicolor PNGs used by the Ubuntu launcher, under aliases `:/icons/tiger-sheets-<size>.png`. Assets cover 16, 24, 32, 48, 64, 128, 256 and 512 pixels. The same artwork therefore appears in the title bar, application header and desktop launcher.

Regenerate the checked-in derivatives offline:

```sh
python3 tools/generate_app_icons.py --write
```

Verify them without modifying files:

```sh
python3 tools/generate_app_icons.py --check
```

The generator uses premultiplied RGBA Lanczos resizing with reducing gap 3.0, converts back to straight RGBA and writes compressed PNGs without timestamps or metadata. Premultiplication preserves clean translucent edges. Regeneration is byte-for-byte reproducible with the same Pillow/zlib runtime. The checked-in assets were generated using the already-installed Pillow 9.0.1 and zlib 1.2.11. Different encoder versions can change PNG bytes; they must be reviewed before intentionally regenerating committed assets. Pillow is an artwork-maintenance/test dependency, not an application runtime dependency.

`--icon-root PATH` generates or checks derivatives in an isolated directory; the canonical master remains the project asset. The tests use temporary output directories and never rewrite checked-in artwork.

## TDD evidence

The command for each focused test was `python3 tests/branding_tests.py BrandingTests.<test_name>`.

1. **RED:** `test_write_generates_all_launcher_sizes_with_transparent_edges` failed with “TigerSheets needs an offline icon-generation command.” **GREEN:** implemented the real offline command; it generated all eight sizes, preserved transparent corners and opaque center pixels, and generated identical bytes on repetition. Initial use of Pillow's newer `Resampling` API failed on the installed 9.0.1 runtime; the implementation was corrected to support that runtime without installation.
2. **RED:** `test_check_detects_missing_and_changed_icons_without_modifying_them` failed because `--check` did not exist (CLI exit 2 requiring `--write`). **GREEN:** added read-only comparison, returning 1 for a missing/corrupted asset while retaining corrupted bytes unchanged.
3. **RED:** `test_qt_resource_manifest_embeds_all_packaged_launcher_sizes` failed with “native Qt windows need the packaged TigerSheets icon resource.” **GREEN:** generated the checked-in hicolor assets and added the Qt manifest; every alias resolves to its correctly-sized package asset and the real default `--check` succeeds.

**Verification:** `python3 tests/branding_tests.py` passed all 3 tests; `python3 tools/generate_app_icons.py --check` passed; `python3 -m py_compile tools/generate_app_icons.py tests/branding_tests.py` passed. The generated master and the 128px derivative were visually inspected. No behavior-preserving refactor was necessary.

