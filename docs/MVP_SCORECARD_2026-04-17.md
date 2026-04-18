# Acrobat Pro Imposition MVP scorecard (17 April 2026)

## Current score

**71 / 100**

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
- ✅ Placement manifest CTM bridge + Acrobat JS handoff skeleton for composition integration: **4 / 4**
- ⚠️ Real SDK build and smoke validation on Windows 11 + macOS: **2 / 8**
- ❌ Real page-content placement pipeline into imposed output PDF: **0 / 7**

Subtotal: **14 / 25**

### 4) Product UX in Acrobat — 15 pts
- ⚠️ Menu-based preset save/preview/run flow exists; full panel/dialog still missing: **1 / 6**
- ❌ Output naming/destination controls in UI: **0 / 4**
- ⚠️ Validation feedback in UI: **1 / 3**
- ⚠️ Preset lifecycle (save/load) via menu flow: **2 / 2**

Subtotal: **4 / 15**

### 5) Prepress production features — 15 pts
- ⚠️ Trim marks + bleed currently as proof visualization: **4 / 6**
- ⚠️ Creep + overlays + CSV variable data now in planner/proof flow (Acrobat production path still open): **3 / 6**
- ⚠️ PDF/X-oriented preflight checks and combined quality gates (`--pdfx-profile`, `--preflight`, `--fail-on-preflight`, `--fail-on-quality-gate`): **2 / 3**

Subtotal: **8 / 15**

---

## What must be done to reach 100/100

1. **Implement real Acrobat composition pipeline (largest gap).**
   - Map each planner placement to Acrobat SDK page transforms.
   - Place actual source page content on destination sheets.
   - Save/open output document from inside Acrobat workflow.

2. **Ship a minimal production UI panel in Acrobat.**
   - Mode selector, sheet settings, preset picker.
   - Run/preview buttons and deterministic output location.
   - Inline validation feedback before execution.

3. **Complete cross-platform validation with real SDK/runtime.**
   - Windows 11 smoke matrix (Acrobat version × architecture).
   - macOS smoke matrix with signing/notarization constraints.
   - Keep a release checklist with pass/fail evidence.

4. **Promote prepress features from proof-only to production-grade.**
   - Marks/bleed linked to output intents and consistent units.
   - Add creep control and overlay support.
   - Add CSV variable data support (template + bindings).

5. **Add PDF/X and release quality gates.**
   - Validate output against selected PDF/X target profile.
   - Fail builds/jobs when critical preflight issues occur.
   - Attach audit artifacts to each generated job.

---

## Fastest path from 67 → 85

If only one milestone is prioritized next, deliver **real Acrobat page-content placement** first. That single milestone unlocks most of the blocked score in host integration and makes the UI work immediately more valuable.
