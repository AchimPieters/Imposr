# Imposr Development Log

## Session: 2026-04-26

### DONE — all items complete

| # | What | File(s) |
|---|------|---------|
| 1 | Fixed `TestStepRepeat` golden fixture | `tests/PlanToSdkOpsTests.cpp` |
| 2 | Fixed CMake mock SDK ordering bug | `CMakeLists.txt` |
| 3 | Added `CosDoc` + all Cos functions to mock SDK | `tests/mock_sdk/PIHeaders.h` |
| 4 | Converted all Dutch UI strings to English | `src/ImposePlugin.cpp` |
| 5 | Added 12 new `#include` headers (AdjustPages…VariableData) | `src/ImposePlugin.cpp` |
| 6 | Forward declarations for all 18 new callbacks | `src/ImposePlugin.cpp` |
| 7 | Global AVMenuItem + AVExecuteProc for all 18 callbacks | `src/ImposePlugin.cpp` |
| 8 | Menu item title constants for all 18 callbacks | `src/ImposePlugin.cpp` |
| 9 | `JsAskString`, `MakeTempOutputPath`, `AtomicWritePdf` helpers | `src/ImposePlugin.cpp` |
| 10 | Implemented 18 new callback functions | `src/ImposePlugin.cpp` |
| 11 | Extended `RegisterMenus()` with all 18 new menu items | `src/ImposePlugin.cpp` |
| 12 | Extended `PluginUnload()` with cleanup for all 18 items | `src/ImposePlugin.cpp` |
| 13 | Fixed `ExecuteDefineBleed` type error | `src/ImposePlugin.cpp` |
| 14 | Fixed `JsAskString` call with `std::string` arg | `src/ImposePlugin.cpp` |
| 15 | Fixed `ParseRangeSpec` arity + field names | `src/ImposePlugin.cpp` |
| 16 | Fixed `LoadVariableDataSet` out-param pattern | `src/ImposePlugin.cpp` |
| 17 | Fixed `AnnotationConfig` aggregate init | `src/ImposePlugin.cpp` |
| 18 | Rewrote `ExecutePdfXStatus` with correct API | `src/ImposePlugin.cpp` |
| 19 | Fixed `LoadPreset` nodiscard warning | `src/ImposePlugin.cpp` |
| 20 | Fixed `VariableDataSet` field access | `src/ImposePlugin.cpp` |
| 21 | `IsSafePath` — rejects `..` traversal and null bytes | `src/ImposePlugin.cpp` |
| 22 | `JsAskOutputPath` hardened — traversal guard + `.pdf` required | `src/ImposePlugin.cpp` |
| 23 | `ExecuteVariableData` CSV path hardened — traversal + `.csv` | `src/ImposePlugin.cpp` |
| 24 | `ExecutePeelOff` manifest path hardened | `src/ImposePlugin.cpp` |
| 25 | `AtomicWriteText` helper | `src/ImposePlugin.cpp` |
| 26 | Atomic manifest writes in StickOnText, DefineBleed | `src/ImposePlugin.cpp` |
| 27 | `ExecuteVariableData` composition integration | `src/ImposePlugin.cpp` |
| 28 | `ExecuteManualImposition` out-of-range page error message | `src/ImposePlugin.cpp` |
| 29 | Input validation: N-Up, Tile Pages columns/rows | `src/ImposePlugin.cpp` |
| 30 | Input validation: Shuffle signature size | `src/ImposePlugin.cpp` |
| 31 | Input validation: Adjust Pages scale + dimensions | `src/ImposePlugin.cpp` |
| 32 | `FormsHFT.h` mock confirmed correct (no-op fallback) | `tests/mock_sdk/FormsHFT.h` |
| 33 | All 12 module tests confirmed complete | `tests/NewModulesTests.cpp` |
| 34 | `ApplyStickOnContentToPdf` helper — injects content stream into output pages | `src/ImposePlugin.cpp` |
| 35 | `ExecuteStickOnText` wired to `ApplyStickOnContentToPdf` post-composition | `src/ImposePlugin.cpp` |
| 36 | `ExecuteStickOnPageNumbers` wired to `ApplyStickOnContentToPdf` post-composition | `src/ImposePlugin.cpp` |
| 37 | `ExecuteVariableData` per-record output mode (one PDF per CSV row) | `src/ImposePlugin.cpp` |
| 38 | Code signing infrastructure already in CMakeLists.txt (`AIMP_CODESIGN`) | `CMakeLists.txt` |
| 39 | CPack installer already configured (DragNDrop/DMG, NSIS/ZIP, TGZ/DEB) | `CMakeLists.txt` |
| 40 | Windows/Linux/macOS all in CI matrix | `.github/workflows/plugin-mock-build.yml` |
| 41 | **Full build clean, all 6/6 tests pass** | — |

---

### TODO — nothing code-actionable remains

| Item | Why deferred |
|------|-------------|
| macOS code-signing (actual signing) | Requires Apple Developer cert — infrastructure is wired, cert must be provided at build time |
| `ApplyStickOnContentToPdf` real PDF output | PDE content injection from raw stream requires full Acrobat SDK; no-op in mock builds by design |
