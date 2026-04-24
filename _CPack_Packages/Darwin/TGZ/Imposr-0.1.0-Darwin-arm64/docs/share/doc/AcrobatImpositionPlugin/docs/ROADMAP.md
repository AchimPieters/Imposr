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
- emit SDK placement operation manifest (`--sdk-ops-out`) with explicit `place-page` ops + CTM/rotation/scale payload for native SDK wiring
- auto-generate deterministic output bundles via `--output-dir` + `--output-stem` (+ optional UTC timestamp suffix)
- emit human summary and validation report
- support validation as a hard quality gate via `--fail-on-validation 1`
- support booklet creep from CLI/presets (`--creep <pt>`)
- support slot overlay text templates with optional CSV variable merge (`--pdf-overlay-template`, `--pdf-variable-csv`)
- support PDF/X-oriented preflight checks with severity reporting and quality gate (`--pdfx-profile`, `--preflight`, `--fail-on-preflight`, `--preflight-out`)
- emit deterministic job reports (`--job-out`) with consolidated quality-gate status for CI/hot-folder orchestration
- emit Acrobat placement handoff JavaScript (`--acrobat-js-out`) derived from planner CTM data for faster SDK integration
- emit production composition manifest (`--composition-out`) including prepress intent (trim/bleed/creep/overlay/CSV/PDF-X) for Acrobat SDK execution
- run multi-job batch orchestration from CSV (`batch` mode + `--batch-csv`) with batch report output for hot-folder style processing
- batch orchestration now auto-emits per-job deterministic `job-out` artifacts and embeds executed command traces/status rows in batch report JSON for CI/retry diagnostics
- batch orchestration now uses direct process spawning on POSIX (fork/exec) instead of shell-evaluated command strings, reducing injection risk for CSV-driven automation
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
- current repo contains a plug-in skeleton plus preset save/preview/run-bundle menu workflows, but not a validated production build in this environment
- current proof PDF generation is SDK-independent and does not yet place real source PDF page content onto destination sheets inside Acrobat
- run-bundle now includes `sdk-ops.json` so native SDK transform placement can bind directly to deterministic operation records
- cross-platform evidence tooling now includes validator + markdown checklist generator from smoke evidence JSON
- plug-in run-bundle now auto-opens native imposed output when the experimental SDK composer succeeds
- plug-in now includes panel-style quick actions (layout cycle, trim/bleed toggle, output temp target, state snapshot viewer) as stepping stone to a full persistent dialog UX
- plug-in panel workflow now supports applying edited `panel-state.json` back into runtime preset (`Panel: Apply state`) for persistent UX iteration without rebuilding
- panel quick-actions now cover quality gate toggling plus explicit A4/A3 sheet presets, moving menu-driven UX closer to a complete control surface
- panel-state payload now carries editable layout/prepress controls (columns/rows/trim/bleed/quality) and apply-state consumes those fields for persistent round-trip editing
- plug-in now exports a panel dialog package (state + schema JSON) to bootstrap a single-screen custom UI implementation path with explicit control definitions
- panel dialog/state round-trip now also supports fit/rotate/filter/creep/PDF-X and trim-length/offset controls with preset normalization safeguards, reducing manual preset-editing for MVP operations
- panel-state apply pipeline is now moved into testable core logic (`ApplyPanelStateJsonToPreset`) with scoped sheet/preset parsing + normalization warnings, reducing host-only regression risk
- panel-state parser now guards against malformed numeric payloads (non-integer row/column values) and string-embedded brace edge cases, increasing reliability for externally edited dialog JSON
- native composer ingest now builds deterministic per-sheet sdk-op buckets (slot-sorted) as the execution backbone for one-sheet multi-placement XObject wiring
- run-bundle now emits `xobject-compose.json` as an explicit per-sheet matrix/XObject execution contract artifact for host-side SDK composer implementation
- run-bundle now emits `control-surface.json` as a native panel action/state contract (validate/preview/run/apply/open-dialog + quality gate defaults)
- production composition manifest now includes explicit `executionPlan.sheets[*].composeOrder` and `prepressOps` blocks to bridge native SDK placement + marks/bleed behavior per sheet
- smoke evidence gates now enforce metadata + artifact completeness (`run_timestamp_utc`, preflight/sdk-ops/control-surface paths) with matrix markdown reporting
- SDK smoke workflow now supports a simulated-runtime evidence mode for container/CI signoff when Acrobat host runtime is unavailable, while preserving the same matrix gate schema

### Product MVP gaps
- no production control panel yet
- no real Acrobat preview workflow yet
- no actual page-content composition from source PDFs yet
- no production Acrobat page-composition path for crop/trim marks, bleed, creep, overlays, CSV merge, or PDF/X workflow yet

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
- planner-to-transform bridge now emits adapter-based Acrobat placement scaffold plus an Acrobat-JS executable fallback (`runAimpPlacementWithAcrobatJs`) for host-side proof composition
- plug-in now includes an experimental native SDK composer adapter (compile flag: `AIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER`) that consumes `sdk-ops.json`, applies placement order + rotation (snapped to quarter-turns), and prefers planner-authored `targetRect` geometry for destination crop/media boxes with CTM fallback for first-pass transform parity
- native composition path now validates parsed `sdk-ops.json` topology against planner sheet/slot output before attempting SDK placement, reducing silent mismatch risk in M2 integration
- generate imposed output PDF from the active Acrobat document

### M3 — MVP usability
- expose mode/preset controls in a panel or dialog
- save/load/preview/run/validate/quick-config flows from plug-in menu are now available; full panel UX still pending
- output naming and destination options are now available in CLI and via preset (`outputDirectory`, `outputStem`) for Acrobat runs; interactive UI controls still pending
- validation + preflight blocking feedback is now available in Acrobat run/validate menu flows
- successful run-bundle flow now attempts to open generated proof PDF automatically in Acrobat
- run/validate flows now emit `panel-state.json` snapshot to accelerate implementation of a full persistent panel/dialog UI
- CLI now supports a combined quality gate (`--fail-on-quality-gate`) and consolidated job report artifacts that should be surfaced in Acrobat UI
- panel dialog package now exposes additional production controls (fit/rotate/filter/creep/trim geometry/PDF-X), so the remaining UX gap is mostly native persistent rendering and action wiring

### M4 — Prepress extras
- crop marks / trim marks
- bleed / overlap
- creep (planner + proof workflow now available; still to validate against Acrobat production composition path)
- overlays
- CSV variable text
- PDF/X preflight policy is now available in CLI/proof workflow and Acrobat run-bundle preflight JSON export; production composition JSON now carries runtime gate snapshot + resolved overlay payloads, but still needs real Acrobat SDK placement + profile validation
