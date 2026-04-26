# Changelog

All notable changes to Imposr are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Added
- Native Acrobat SDK PDF composition enabled by default (`AIMP_NATIVE_PDF_COMPOSER=ON`).
  Placed source pages using `PDEContentToCosObj` + `PDEFormCreateFromCosObj` + `PDEContentAddElem`.
- CropBox-aware CTM calculation (`BuildCropBoxCorrectCtm`) for correct placement at all
  rotation angles (0°/90°/180°/270°) when source pages have a non-zero CropBox origin.
- Create Booklet menu item: saddle-stitch imposition with automatic padding to multiple of 4.
- N-Up Pages menu item: 2×2 layout with scale-to-fit on active document.
- Step & Repeat menu item: 2×2 repeat at source page step on active document.
- Unique temp filename per NativeComposePlan call (chrono-based) — prevents file
  collision when multiple Acrobat instances run simultaneously.
- `IsSafeStem` guard in batch CLI: CSV-sourced `output_stem` values containing path
  separators or `..` are rejected before they reach the filesystem.

### Changed
- Plugin menu renamed from "Acrobat Imposition Plugin" to "Imposr".
- `AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER` retained as backward-compatible alias for
  `AIMP_NATIVE_PDF_COMPOSER`.
- SDK exclusions and CPack artifacts added to `.gitignore`.

### Removed
- "2-Up Demo" and "2-Up Report PDF" items removed from the production menu
  (implementations retained internally for development use).

---

## [0.1.0] — 2026-04-25

### Added
- Initial imposition planning engine: Booklet, N-Up, Step & Repeat, Manual, Tile planners.
- Shuffle / signature reordering (saddle-stitch, even/odd split, ShuffleAssistant).
- TrimShift / Creep adjustment.
- PageTools: duplicate, delete, move, rotate, insert blank pages.
- PrinterMarks: trim marks, bleed box, registration marks.
- StickOn: image/PDF overlay placement.
- VariableData: CSV-driven per-page text and image fields.
- Annotations module stub.
- AdjustPages module stub.
- TilePages module stub.
- PdfX conformance reporting.
- CLI (`imposr_cli`): single-job and batch-CSV modes, JSON plan output, quality gates.
- Preset save/load/validate.
- CMake build system with CPack packaging (DMG, TGZ, NSIS).
- GitHub Actions CI: ubuntu/macos/windows matrix.
- Release-readiness gate with ASAN/UBSAN and warnings-as-errors.
