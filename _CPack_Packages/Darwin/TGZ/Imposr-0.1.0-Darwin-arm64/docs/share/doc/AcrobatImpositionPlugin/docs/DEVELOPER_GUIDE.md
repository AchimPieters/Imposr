# Imposr Developer Guide

## Architecture Overview

```
include/aimp/          Public C++ headers (all modules)
src/                   Module implementations
tests/                 Unit tests (no external test framework)
tools/                 Python host-gate tooling
scripts/               macOS/Windows installer scripts
docs/                  Documentation
```

### Core Library: `aimp_core`

All business logic lives in the `aimp_core` static library. It has zero external dependencies — no PDF library, no Adobe SDK. The library produces planning data structures and JSON/XML/PDF-content-stream representations. Actual PDF content composition is handled by:

- **CLI / proof composer**: `src/PdfComposer.cpp` — produces geometric proof PDFs for review.
- **Acrobat plug-in** (`AcrobatImpositionPlugin`): compiled only when `ACROBAT_SDK_DIR` is set; all SDK calls are guarded by `#ifdef AIMP_BUILD_PLUGIN`.

### Module Map

| Header | Source | Purpose |
|--------|--------|---------|
| `ImpositionPlan.h` | `ImpositionPlan.cpp` | Core plan types, all planner implementations |
| `PanelState.h` | `PanelState.cpp` | Acrobat plug-in UI state serialization |
| `PdfComposer.h` | `PdfComposer.cpp` | Proof PDF output |
| `Preset.h` | `Preset.cpp` | JSON preset load/save |
| `TrimShift.h` | `TrimShift.cpp` | Booklet creep correction |
| `VariableData.h` | `VariableData.cpp` | CSV variable data merge |
| `Shuffle.h` | `Shuffle.cpp` | Signature shuffle, even/odd split, interleave |
| `SplitMerge.h` | `SplitMerge.cpp` | Range-based plan split and merge |
| `PageTools.h` | `PageTools.cpp` | Duplicate, delete, move, rotate, insert blank pages |
| `StickOn.h` | `StickOn.cpp` | Text, Bates, page number, PDF stamp, tape, peel-off |
| `PrinterMarks.h` | `PrinterMarks.cpp` | Registration marks, crop marks, bleed marks, color bar |
| `AdjustPages.h` | `AdjustPages.cpp` | Scale, crop, extend, scale-to-fit/fill placements |
| `TilePages.h` | `TilePages.cpp` | Tile-grid geometry computation |
| `Bleed.h` | `Bleed.cpp` | Bleed box computation and content generation |
| `PdfX.h` | `PdfX.cpp` | PDF/X metadata detection and compliance validation |
| `Annotations.h` | `Annotations.cpp` | Annotation processing rules (preserve/discard/flatten) |

---

## Building

### Minimal (no plugin)
```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### With Acrobat plugin
```bash
export ACROBAT_SDK_DIR=/path/to/AcrobatSDK
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `AIMP_BUILD_PLUGIN` | ON | Build Acrobat plug-in (requires SDK) |
| `AIMP_BUILD_CLI` | ON | Build `imposr_cli` |
| `AIMP_BUILD_TESTS` | ON | Build test executables |
| `AIMP_ENABLE_WARNINGS` | ON | Enable `-Wall -Wextra -Wpedantic` |
| `AIMP_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |
| `AIMP_ENABLE_SANITIZERS` | OFF | ASan + UBSan |
| `AIMP_ENABLE_PACKAGING` | ON | Enable CPack targets |
| `AIMP_ACROBAT_PLUGIN_INSTALL_DIR` | (platform default) | Where to install the `.api` |

---

## Running Tests

```bash
ctest --test-dir build --output-on-failure
```

Five test targets:

1. `aimp_planner_tests` — core planner unit tests (`tests/ImpositionPlanTests.cpp`)
2. `aimp_new_modules_tests` — all 12 new module unit tests (`tests/NewModulesTests.cpp`)
3. `imposr_cli_smoke` — CLI basic smoke test
4. `imposr_cli_quality_gate_smoke` — CLI quality gate flow
5. `imposr_cli_batch_smoke` — CLI batch orchestration smoke test

---

## Adding a New Module

1. Create `include/aimp/MyModule.h` — define config structs, result structs, and free functions.
2. Create `src/MyModule.cpp` — implement all functions declared in the header.
3. Add `src/MyModule.cpp` to `aimp_core` in `CMakeLists.txt`.
4. Add test functions to `tests/NewModulesTests.cpp` and register them in `main()`.
5. Add CLI support in `src/CliMain.cpp` — new mode name, new flags, new handler.
6. Document in `docs/USER_GUIDE.md` and `docs/CLI_REFERENCE.md`.

### Module conventions

- All types and functions live in the `aimp` namespace.
- No external dependencies — represent PDF operations as content-stream strings or structured data.
- Return result structs rather than mutating input structs where possible.
- Provide a `*ToJson()` serialization function for every result struct.
- Guard SDK-specific code with `#ifdef AIMP_BUILD_PLUGIN`.

---

## ImpositionPlan Type Reference

```cpp
struct PageRef {
    std::string sourceDocumentId;
    std::uint32_t pageIndex;  // 0-based; kBlankPageIndex for blank
};

struct Rect {
    double x, y, width, height;
};

struct Placement {
    std::uint32_t sheetIndex;   // 0-based output sheet
    std::uint32_t slotIndex;    // 0-based slot within sheet
    PageRef sourcePage;
    Rect targetRect;            // position on output sheet (PDF points)
    double rotationDegrees;     // 0, 90, 180, 270
    double scaleX, scaleY;      // applied scale
};

struct ImpositionPlan {
    std::string sourceDocumentId;
    std::uint32_t sourcePageCount;
    std::uint32_t paddedPageCount;
    SheetSize outputSheet;
    std::vector<Placement> placements;
};
```

The `kBlankPageIndex` sentinel (`UINT32_MAX - 1`) marks slots that should be left blank.

---

## Planner Implementations

All planners are static factory classes:

```cpp
aimp::TwoUpPlanner::Build(documentId, pageCount, sheetSize, buildOptions)
aimp::NUpPlanner::Build(documentId, pageCount, sheetSize, columns, rows, buildOptions)
aimp::BookletPlanner::Build(documentId, pageCount, sheetSize, buildOptions)
aimp::StepAndRepeatPlanner::Build(documentId, pageCount, sheetSize, config, buildOptions)
aimp::TilePlanner::Build(documentId, pageCount, sheetSize, config, buildOptions)
aimp::ManualPlanner::Build(documentId, pageCount, sheetSize, columns, rows, sequence, buildOptions)
```

`BuildOptions` controls: `reverseOrder`, `padToMultiple`, `bookletSignatureSize`, `bookletCreepPerSheetPoints`, `scaleToFit`, `autoRotateToFit`, `sourcePageWidthPoints`, `sourcePageHeightPoints`, `filter`, `explicitPageSequence`.

---

## PDF Content Stream Conventions

Modules that produce content streams (`StickOn`, `PrinterMarks`, `Bleed`) use plain PDF content stream operator syntax:

- `q` / `Q` — save/restore graphics state
- `<r> <g> <b> RG` — stroke color (RGB)
- `<r> <g> <b> rg` — fill color (RGB)
- `<x> <y> <w> <h> re S` — rect stroke
- `<x> <y> <w> <h> re f` — rect fill
- `<x1> <y1> m <x2> <y2> l S` — line
- `BT ... Tj ET` — text block
- `% comment` — PDF comment (ignored by renderers)

These strings are passed to the Acrobat SDK's `PDPageAddCosContents` or equivalent to add the mark to the actual PDF output.

---

## Preset JSON Format

```json
{
  "kind": "imposr-preset",
  "version": 1,
  "sheetWidth": 841.89,
  "sheetHeight": 595.28,
  "columns": 2,
  "rows": 1,
  "buildOptions": {
    "reverseOrder": false,
    "padToMultiple": 0,
    "bookletSignatureSize": 0,
    "bookletCreepPerSheetPoints": 0.0,
    "scaleToFit": false,
    "autoRotateToFit": false
  },
  "pdfOptions": {
    "drawTrimMarks": false,
    "bleedPoints": 0.0,
    "targetPdfxProfile": "none"
  }
}
```

---

## Packaging

```bash
# macOS: produces dist/Imposr-0.1.0.pkg + dist/Imposr-0.1.0-Darwin-arm64.dmg
bash scripts/build_installer_macos.sh --without-plugin

# With plugin (requires ACROBAT_SDK_DIR):
bash scripts/build_installer_macos.sh
```

Artifacts land in `dist/`. The `.pkg` installs `imposr_cli` to `/usr/local/bin`. The plugin `.pkg` installs `AcrobatImpositionPlugin.api` into the Acrobat Plug-ins folder via a postinstall script.
