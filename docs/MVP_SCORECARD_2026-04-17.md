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
