# Roadmap and status

## Latest deep audit
- See `docs/IMPLEMENTATIE_AUDIT_2026-04-17.md` for a detailed done-vs-open analysis, including Windows 11 + macOS readiness.


## What is complete in this repository

### Core planner
- 2-up planning
- N-up planning
- booklet planning with signature normalization
- step-and-repeat planning
- manual sequence planning (explicit page shuffle order)
- tile planning with configurable overlap grid
- page filtering (all/even/odd)
- reverse order
- blank padding
- explicit page-sequence overrides (duplicates/blanks/custom order)
- fit-to-slot metadata
- auto-rotate metadata
- plan statistics and validation helpers
- JSON export
- XML audit export
- source-to-placement and placement-to-source inspector helpers

### CLI / prototype
- build plans from the command line
- load and save presets
- presets now store tile overlap + manual/explicit page sequences
- emit JSON, XML audit, and visual proof PDF output
- emit placement manifest JSON (`--manifest-out`) with per-slot CTM data for Acrobat composition handoff
- auto-generate deterministic output bundles via `--output-dir` + `--output-stem` (+ optional UTC timestamp suffix)
- emit human summary and validation report
- support validation as a hard quality gate via `--fail-on-validation 1`
- support booklet creep from CLI/presets (`--creep <pt>`)
- support slot overlay text templates with optional CSV variable merge (`--pdf-overlay-template`, `--pdf-variable-csv`)
- inspect source page usage and placement reverse lookup
- smoke-tested in CI-style local builds

### PDF proof composer
- produces a valid PDF proof document
- draws sheet border
- draws slot outlines
- marks blank slots
- prints placement labels, geometry, and optional Bates numbering
- supports optional trim-mark and bleed-box visualization for prepress proofing
- suitable for validating planner geometry without Acrobat SDK dependencies

## What is not complete yet

### Acrobat plug-in integration
- requires the real Adobe Acrobat SDK on the build machine
- requires platform-specific include/link settings for Windows 11 and macOS
- current repo contains a plug-in skeleton and menu registration code, but not a validated production build in this environment
- current proof PDF generation is SDK-independent and does not yet place real source PDF page content onto destination sheets inside Acrobat

### Product MVP gaps
- no production control panel yet
- no real Acrobat preview workflow yet
- no actual page-content composition from source PDFs yet
- no crop/trim marks, bleed, creep, overlays, CSV merge, or PDF/X workflow yet

## Recommended next milestones

### M1 — Acrobat host build bring-up
- compile and load the plug-in with the target Acrobat SDK on Windows 11
- compile and load the plug-in with the target Acrobat SDK on macOS
- verify menu actions, temp-file export, and document lifecycle handling
- add a shared compatibility matrix (Acrobat version x OS x CPU architecture) and keep it updated per release

### Cross-platform guardrails (Windows 11 + macOS)
- enforce a thin platform abstraction for filesystem paths, temp directories, and UI wiring
- keep planner/composer logic free of OS-specific code; only host glue may be platform-specific
- validate on both platforms in CI (planner/composer/tests) and in manual Acrobat smoke tests (plugin load + menu action)
- maintain separate build presets/toolchains for MSVC (Windows 11) and Clang/Xcode (macOS)
- ship and test 64-bit targets only for modern Acrobat builds

### M2 — Real composition
- import source page content into destination sheets
- planner-to-transform bridge is now available via manifest CTM export; still needs Acrobat SDK execution path
- generate imposed output PDF from the active Acrobat document

### M3 — MVP usability
- expose mode/preset controls in a panel or dialog
- save presets from the plug-in UI
- output naming and destination options are now available in CLI (`--output-dir`, `--output-stem`, `--stamp-output`) and still need to be surfaced in Acrobat UI
- add validation feedback to the UI

### M4 — Prepress extras
- crop marks / trim marks
- bleed / overlap
- creep (planner + proof workflow now available; still to validate against Acrobat production composition path)
- overlays
- CSV variable text
