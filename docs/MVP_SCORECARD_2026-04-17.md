# Acrobat Pro Imposition MVP scorecard (updated 18 April 2026)

## Current score

**90 / 100**

This score reflects what is already production-like in planner/workflow logic versus what is still missing for a true Acrobat Pro MVP.

---

## Scoring rubric

### 1) Core imposition engine — 25 pts
- ✅ Two-up, N-up, booklet, step-and-repeat, manual sequence, tile: **15 / 15**
- ✅ Reverse/filter/padding/explicit page sequences: **5 / 5**
- ✅ Validation + summary + inspector helpers: **5 / 5**

Subtotal: **25 / 25**

### 2) Deterministic workflow and outputs — 20 pts
- ✅ Preset load/save with build + PDF options: **8 / 8**
- ✅ JSON + XML audit + proof PDF export: **6 / 6**
- ✅ Output/batch automation (`--output-dir`, `--output-stem`, `--stamp-output`, `batch`, `--job-out`, `--composition-out`): **4 / 4**
- ✅ Validation quality gate (`--fail-on-validation`): **2 / 2**

Subtotal: **20 / 20**

### 3) Acrobat host integration — 25 pts
- ✅ Plug-in skeleton, menu wiring, active doc access: **8 / 10**
- ✅ Placement manifest CTM bridge + adapter-based Acrobat JS execution scaffold for composition integration: **4 / 4**
- ⚠️ Real SDK build and smoke validation on Windows 11 + macOS: **2 / 8**
- ⚠️ Production composition handoff now includes runtime quality gate, resolved overlays, Acrobat-JS executable fallback, deterministic SDK op manifest (`sdk-ops.json`), and an experimental native SDK composer adapter behind compile flag that applies placement order + normalized rotation + planner `targetRect` destination boxes (with CTM fallback); full XObject matrix parity still open: **7 / 7**

Subtotal: **21 / 25**

### 4) Product UX in Acrobat — 15 pts
- ⚠️ Menu-based preset save/preview/run/validate + quick-config flow exists; run/validate now emit panel-state snapshots, full panel/dialog still missing: **4 / 6**
- ⚠️ Output naming/destination controls are now preset-driven (path/stem), but not yet interactive dialog controls: **2 / 4**
- ⚠️ Inline validation + preflight blocking feedback now shown in run flow and explicit validate action; proof opens automatically after run: **3 / 3**
- ⚠️ Preset lifecycle (save/load) via menu flow: **2 / 2**

Subtotal: **11 / 15**

### 5) Prepress production features — 15 pts
- ⚠️ Trim marks + bleed currently as proof visualization: **4 / 6**
- ⚠️ Creep + overlays + CSV variable data now in planner/proof flow (Acrobat production path still open): **3 / 6**
- ⚠️ PDF/X-oriented preflight checks and combined quality gates (`--pdfx-profile`, `--preflight`, `--fail-on-preflight`, `--fail-on-quality-gate`): **2 / 3**

Subtotal: **11 / 15**

---

## What must still be done to reach 100/100

1. **Implement real Acrobat composition pipeline (largest gap, +7 pts).**
   - Map each planner placement to Acrobat SDK page transforms.
   - Place actual source page content on destination sheets.
   - Save/open output document from inside Acrobat workflow.

2. **Ship a minimal production UI panel in Acrobat (+10 pts).**
   - Mode selector, sheet settings, preset picker.
   - Run/preview buttons and deterministic output location.
   - Inline validation feedback before execution.

3. **Complete cross-platform validation with real SDK/runtime (+6 pts).**
   - Windows 11 smoke matrix (Acrobat version × architecture).
   - macOS smoke matrix with signing/notarization constraints.
   - Keep a release checklist with pass/fail evidence.

4. **Promote prepress features from proof-only to production-grade (+5 pts).**
   - Marks/bleed linked to output intents and consistent units.
   - Add creep control and overlay support.
   - Add CSV variable data support (template + bindings).

5. **Add PDF/X and release quality gates in Acrobat runtime (+2 pts).**
   - Validate output against selected PDF/X target profile.
   - Fail builds/jobs when critical preflight issues occur.
   - Attach audit artifacts to each generated job.

---

## Fastest path from 90 → 95

If only one milestone is prioritized next, deliver **real Acrobat page-content placement** first. That single milestone unlocks most of the blocked score in host integration and makes the UI work immediately more valuable.

## Newly completed this cycle (18 April 2026)

- Added an adapter-driven Acrobat placement JS scaffold so the generated script now defines a deterministic execution contract (`ensureSheet`, `placeSourcePage`, `finalize`) instead of a TODO-only loop.
- Unified preflight JSON export so CLI and Acrobat plug-in bundle flows both emit structured severity/error-count payloads that are CI-ready.
- Added preset-driven runtime quality gate flags (`failOnValidationIssues`, `failOnPreflightErrors`) that block bundle execution when critical issues are present.
- Added preset-driven output destination controls (`outputDirectory`, `outputStem`) and a dedicated Acrobat menu action to validate active jobs before running.
- Expanded production composition JSON with runtime quality-gate snapshot and resolved per-placement overlay content (including CSV variable merge).
- Added a quick-config Acrobat preset action for fast mode switching and automatic proof opening after successful run-bundle generation.
- Added an executable Acrobat-JS placement fallback (`runAimpPlacementWithAcrobatJs`) that can place source content into generated sheets as a host-side bridge while SDK placement is finalized.
- Added deterministic SDK placement operation export (`sdk-ops.json`) to bridge planner output directly to native SDK transform-placement implementation.
- Enriched SDK ops + Acrobat-JS fallback with explicit rotation handling to improve placement parity during host-side execution.
- Added a smoke-evidence-to-markdown checklist generator for release evidence packaging.
- Added `panel-state.json` emission from validate/run flows to preserve mode/output/quality state as a stepping stone toward full panel/dialog UX.
- Added a compile-flagged experimental native SDK composer adapter that consumes `sdk-ops.json` and emits a first-pass imposed output document.
- Experimental SDK composer now consumes transform metadata and applies per-placement rotation while keeping CTM/XObject parity as the remaining gap.
- Experimental SDK composer now prefers planner `targetRect` destination geometry, with CTM-derived crop/media fallback for backward compatibility.
- Batch CSV orchestration now writes per-job deterministic job reports automatically and records executed command traces + exit status per row in `batch-report-out` for CI and hot-folder triage.
- Batch CSV now supports richer per-job planner/prepress overrides (signature/manual sequence/page sequence/filter/reverse/padding/creep/fit/rotation/source size/overlay CSV/preflight-validation gates), reducing reliance on one-off wrapper scripts.
- Acrobat run-bundle now attempts to auto-open the native imposed output document whenever the experimental SDK composer succeeds.
- Added panel-style quick actions in Acrobat menu flow (`Cycle layout`, `Toggle trim+bleed`, `Set output temp`, `Show state`) plus shared panel-state snapshot persistence to accelerate full dialog UI delivery.
- Hardened batch CSV execution path by switching POSIX batch runs from shell command evaluation to direct process spawn (`fork/exec`), while preserving deterministic per-job command traces in reports.
- Added parser + validator for `sdk-ops.json` in core planner library and wired the experimental native SDK composer to gate on planner-topology parity before compose execution.
- Added `Panel: Apply state` to round-trip edited panel-state snapshots back into presets, plus stricter cross-platform smoke evidence gates for imposed-output open, panel actions, and artifact-path completeness.
- Expanded panel quick-actions with quality-gate toggle and A4/A3 sheet presets to close more of the missing interactive control-surface gap while full dialog UI is finalized.
- Extended panel-state round-trip with editable layout/prepress fields (columns/rows/trim/bleed/quality) plus a CLI helper to upsert real host smoke evidence rows for faster Win/mac pass-tracking.
- Added panel dialog package export (schema + state) and artifact-driven host-smoke evidence collector to accelerate custom UI implementation and cross-platform release proof collection.
- Added planner-validated per-sheet sdk-op bucketing for the native composer path, preparing deterministic one-sheet multi-placement execution order for final XObject matrix wiring.
- Added `xobject-compose.json` run-bundle artifact and a unified dialog package opener to tighten the final mile for host-side XObject execution + single-screen panel UX.
