# Roadmap and status

## What is complete in this repository

### Core planner
- 2-up planning
- N-up planning
- booklet planning with signature normalization
- step-and-repeat planning
- page filtering (all/even/odd)
- reverse order
- blank padding
- fit-to-slot metadata
- auto-rotate metadata
- plan statistics and validation helpers
- JSON export
- XML audit export
- source-to-placement and placement-to-source inspector helpers

### CLI / prototype
- build plans from the command line
- load and save presets
- emit JSON, XML audit, and visual proof PDF output
- emit human summary and validation report
- inspect source page usage and placement reverse lookup
- smoke-tested in CI-style local builds

### PDF proof composer
- produces a valid PDF proof document
- draws sheet border
- draws slot outlines
- marks blank slots
- prints placement labels, geometry, and optional Bates numbering
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

### M2 — Real composition
- import source page content into destination sheets
- map planner placements to Acrobat page transforms
- generate imposed output PDF from the active Acrobat document

### M3 — MVP usability
- expose mode/preset controls in a panel or dialog
- save presets from the plug-in UI
- add output naming and destination options
- add validation feedback to the UI

### M4 — Prepress extras
- crop marks / trim marks
- bleed / overlap
- creep
- overlays
- CSV variable text
