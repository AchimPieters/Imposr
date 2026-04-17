# Ultra-Deep Code Audit (Current Repository State)

Date: 2026-04-16  
Audit type: static + dynamic + workflow-level verification

## 1) Scope and depth

Audited areas:

- Build system and warning policy:
  - `CMakeLists.txt`
  - `.github/workflows/planner-ci.yml`
- Core planning and serialization:
  - `include/aimp/ImpositionPlan.h`
  - `src/ImpositionPlan.cpp`
- Output and workflow components:
  - `include/aimp/PdfComposer.h` / `src/PdfComposer.cpp`
  - `include/aimp/Preset.h` / `src/Preset.cpp`
  - `include/aimp/ArtifactBundle.h` / `src/ArtifactBundle.cpp`
  - `src/CliMain.cpp`
  - `src/ImposePlugin.cpp`
- Test suite and cross-platform behavior:
  - `tests/ImpositionPlanTests.cpp`
  - planner CI matrix behavior
- Product-level status docs:
  - `README.md`
  - `docs/MVP_STATUS.md`
  - `docs/ROADMAP.md`

Audit depth:

- conflict-marker scan
- smell scan (`TODO`/`FIXME`/`XXX`/`HACK`)
- Debug and Release planner-only build+test runs
- CLI artifact-bundle runtime check
- manual security/robustness read-through (I/O, path handling, parse strictness)

## 2) Commands executed

1. Conflict/smell scan + Debug configure/build/test:
   - `rg -n "TODO|FIXME|XXX|HACK|<<<<<<<|>>>>>>>|=======" CMakeLists.txt README.md docs src include tests .github || true`
   - `cmake -S . -B build-audit-debug -DAIMP_BUILD_PLUGIN=OFF -DAIMP_BUILD_TESTS=ON -DAIMP_BUILD_CLI=ON -DAIMP_ENABLE_STRICT_WARNINGS=ON -DCMAKE_BUILD_TYPE=Debug`
   - `cmake --build build-audit-debug`
   - `ctest --test-dir build-audit-debug --output-on-failure`
2. Release configure/build/test:
   - `cmake -S . -B build-audit-release -DAIMP_BUILD_PLUGIN=OFF -DAIMP_BUILD_TESTS=ON -DAIMP_BUILD_CLI=ON -DAIMP_ENABLE_STRICT_WARNINGS=ON -DCMAKE_BUILD_TYPE=Release`
   - `cmake --build build-audit-release`
   - `ctest --test-dir build-audit-release --output-on-failure`
3. CLI end-to-end artifact-bundle check:
   - `./build-audit-release/imposr_cli two-up --pages 8 --sheet-width 1190.55 --sheet-height 841.89 --bundle-dir /tmp/aimp_audit_bundle --bundle-base audit --bundle-include-pdf 1`
   - `ls /tmp/aimp_audit_bundle`

## 3) Findings

### Finding A — Cross-platform temp-path fragility in tests (previously observed)

Risk category: Cross-platform reliability  
Severity: High (CI-breaking on Windows)

Details:

- Tests previously wrote to hardcoded `/tmp/...` paths.
- This can fail on Windows runners and produce false negatives.

Status:

- Addressed in test harness via temp-directory based path creation (`std::filesystem::temp_directory_path` with fallback).
- Verified by successful Debug + Release test runs after fix.

### Finding B — CI warning-flag portability

Risk category: Build portability  
Severity: Medium

Details:

- Compiler flags must remain compiler-specific (`/W4` vs `-Wall -Wextra -Wpedantic`).
- Global hardcoded `CMAKE_CXX_FLAGS` in CI can regress Windows quickly.

Status:

- Warning policy is centralized in CMake (`AIMP_ENABLE_STRICT_WARNINGS`) and applied per compiler/target.
- CI config now uses CMake option rather than hardcoding GNU flags.

### Finding C — Serialization + presets robustness

Risk category: Data integrity / user workflow correctness  
Severity: Medium

Details:

- Preset loader must fail loudly on malformed data to prevent silent behavioral drift.
- JSON/XML export must remain deterministic and parse-safe.

Status:

- Preset parsing remains strict with explicit key validation and parse checks.
- JSON/XML paths are covered by integration tests and pass in Debug + Release.

### Finding D — Artifact bundle workflow stability

Risk category: Operator UX / reproducibility  
Severity: Medium

Details:

- Bundle output must atomically produce predictable artifact set and paths.

Status:

- CLI bundle generation succeeds and produces `.json`, `-audit.xml`, and `-report.pdf`.
- Runtime check confirms expected files are emitted.

## 4) Security and correctness notes

- No unresolved merge conflict markers detected in audited files.
- No obvious path traversal issue in current bundle API usage (paths are caller-provided and created explicitly).
- Plugin code paths now use `temp_directory_path(std::error_code&)` style checks where relevant.
- Current test strategy is still “behavioral smoke + integration checks”; no snapshot/golden PDF diff yet.

## 5) MVP risk register (remaining blockers)

Blocking for full product-MVP:

1. Real Acrobat-side page composition into a newly generated imposed output PDF.
2. Deterministic marks/bleed/crop + creep/trim-shift implementation in composition stage.
3. Acrobat-level integration tests (golden-output quality gates).

## 6) Overall status

- Debug planner-only build/test: PASS
- Release planner-only build/test: PASS
- CLI artifact bundle runtime check: PASS
- Cross-platform risk posture: improved, but full Acrobat composition still pending for product-MVP completion.
