# MVP Status (Acrobat Pro Imposition)

_Last updated: 2026-04-16_

## Implemented

- Planner core (Two-Up, N-up, Booklet, Step-and-Repeat).
- Planner filters/padding/signature sizing and placement metadata (fit/rotate/scale).
- JSON and XML serialization.
- PDF report composer for plan previews.
- Preset save/load with strict validation.
- Artifact bundle writer (JSON + XML + optional PDF).
- CLI workflows for planning, exporting, inspection, presets, and artifact bundles.
- Optional Acrobat plug-in build and menu commands for demo/report/bundle export.
- Cross-platform planner CI (Ubuntu/macOS/Windows) and automated planner tests.

## Not yet implemented (blocking a full product-MVP)

- Real Acrobat-side page composition into a newly generated imposed PDF.
- Full production print marks/bleed/crop implementation.
- Creep/trim-shift logic integrated in output composition.
- Overlay and variable-data production features.
- End-to-end Acrobat integration tests for output composition quality checks.

## MVP completion path

1. Implement Acrobat composition pipeline for Two-Up from active source document into a destination PDDoc.
2. Validate transforms (scale/rotation/centering) against planner metadata in Acrobat output.
3. Extend composition path to N-up and Booklet signatures.
4. Add marks/bleed/creep controls and persist them in presets.
5. Add regression tests (golden-output style) for deterministic output.

