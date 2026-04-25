# Imposr User Guide

Imposr is a PDF imposition engine that brings parity with Quite Imposing Plus. It provides a CLI tool (`imposr_cli`), an Adobe Acrobat plug-in, and a C++ planning library.

---

## Installation

### macOS

Download `dist/Imposr-0.1.0.pkg` and double-click to install. The CLI is placed at `/usr/local/bin/imposr_cli`.

To also install the Acrobat plug-in, run the separate `Imposr-Acrobat-Plugin-0.1.0.pkg`. It requires Adobe Acrobat DC or Acrobat (2020+).

To uninstall the plug-in:
```bash
/usr/local/imposr/bin/uninstall_acrobat_plugin_macos.sh
```

### Build from source
```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

---

## Imposition Modes

### 2-Up
Places two source pages side by side on each sheet.
```bash
imposr_cli two-up --pages 8 --sheet-width 841.89 --sheet-height 595.28
```

### N-Up
Places pages in a grid of columns × rows.
```bash
imposr_cli n-up --pages 12 --sheet-width 841.89 --sheet-height 595.28 --columns 3 --rows 2
```

### Booklet (Saddle-Stitch)
Plans signatures for saddle-stitched booklets with optional creep correction.
```bash
imposr_cli booklet --pages 32 --sheet-width 420 --sheet-height 595 --signature-size 16 --creep 0.5
```

### Step-and-Repeat
Repeats a page in a grid of slots with configurable step offsets.
```bash
imposr_cli step-repeat --pages 1 --sheet-width 841.89 --sheet-height 595.28 \
  --repeat-x 3 --repeat-y 2 --step-x 280 --step-y 297 --slot-width 270 --slot-height 285
```

### Tile
Tiles a large page across multiple output sheets with optional overlap.
```bash
imposr_cli tile --pages 1 --sheet-width 841.89 --sheet-height 595.28 \
  --columns 2 --rows 2 --tile-overlap 6
```

### Manual
Places pages in a user-defined sequence.
```bash
imposr_cli manual --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --columns 2 --rows 1 --manual-sequence 3,1,2,4
```

---

## Quite Imposing Plus Parity Features

### Trim/Creep Shift
Apply creep correction to a booklet plan independently of the planner.
```bash
imposr_cli trim-shift --pages 16 --sheet-width 420 --sheet-height 595 --creep 1.5
```

### Adjust Pages
Scale, crop, extend, or scale-to-fit all placements.
```bash
# Uniform scale
imposr_cli adjust-pages --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --adjust-mode scale --scale-x 0.95 --scale-y 0.95

# Scale to fit a target size
imposr_cli adjust-pages --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --adjust-mode scale-to-fit --target-width 280 --target-height 190

# Crop region
imposr_cli adjust-pages --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --adjust-mode crop --crop-x 10 --crop-y 10 --crop-w 260 --crop-h 180

# Extend (add margin)
imposr_cli adjust-pages --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --adjust-mode extend --extend-top 6 --extend-bottom 6 --extend-left 6 --extend-right 6
```

### Insert Blank Pages
```bash
imposr_cli insert-blank --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --insert-at 2 --insert-count 1
```

### Insert from File
Marks insertion slots from a named document (content resolved at Acrobat render time).
```bash
imposr_cli insert-file --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --insert-at 2 --insert-count 2 --insert-doc "cover.pdf"
```

### Insert Conditional
Insert blank pages at even- or odd-indexed positions.
```bash
imposr_cli insert-conditional --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --insert-at 0 --filter even
```

---

## Stick-On Elements

### Text Stamp
```bash
imposr_cli stick-text --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --text "DRAFT" --anchor topleft --font-size 24 --opacity 0.5 \
  --color-r 0.8 --color-g 0 --color-b 0
```

### Variable-Field Stamp
Merges fields from a CSV file onto pages.
```bash
imposr_cli stick-fields --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --variable-csv data.csv --overlay-template "{{name}}"
```

### Bates Numbering
```bash
imposr_cli stick-bates --pages 8 --sheet-width 841.89 --sheet-height 595.28 \
  --bates-prefix "CASE-" --bates-start 1000 --bates-pad 6 --anchor bottomright
```

### PDF Page Stamp
Stamps a page from another PDF onto every sheet.
```bash
imposr_cli stick-pdf --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --source-pdf watermark.pdf --source-pdf-page 0
```

### Masking Tape (Cover/Redact)
```bash
imposr_cli stick-tape --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --stick-rect-x 50 --stick-rect-y 50 --stick-rect-w 200 --stick-rect-h 30
```

### Peel-Off Label
```bash
imposr_cli peel-off --pages 4 --sheet-width 841.89 --sheet-height 595.28 \
  --stick-rect-x 0 --stick-rect-y 0 --stick-rect-w 120 --stick-rect-h 40
```

---

## Variable Data Printing

The `variable-data` command merges a CSV file with a template string.

CSV format — first column must be named `page` (1-based):
```csv
page,name,company,logo
1,Alice Smith,Acme Corp,acme_logo.png
2,Bob Jones,Globex,globex_logo.png
```

```bash
imposr_cli variable-data --pages 2 --sheet-width 841.89 --sheet-height 595.28 \
  --variable-csv recipients.csv --overlay-template "Dear {{name}} from {{company}}"
```

Columns whose names contain `image`, `img`, `photo`, or `logo` are treated as image paths.

---

## Preset Management

Save a preset:
```bash
imposr_cli two-up --pages 8 --sheet-width 841.89 --sheet-height 595.28 \
  --save-preset mypreset.json
```

Apply a preset:
```bash
imposr_cli preset apply --load-preset mypreset.json --pages 16
```

List presets in a directory:
```bash
imposr_cli preset list --preset-dir ./presets
```

---

## Sequence Automation

A sequence file (CSV) contains one CLI invocation per line:
```csv
two-up,--pages,8,--sheet-width,841.89,--sheet-height,595.28,--output-stem,job1
booklet,--pages,16,--sheet-width,420,--sheet-height,595,--creep,0.5,--output-stem,job2
```

Run the sequence:
```bash
imposr_cli sequence run --sequence-file myjobs.csv --output-dir ./output
```

List sequences:
```bash
imposr_cli sequence list --sequence-dir ./sequences
```

---

## Batch Processing

```bash
imposr_cli batch --batch-csv jobs.csv --output-dir ./output --batch-report-out report.json
```

CSV columns: `mode`, `pages`, `sheet_width`, `sheet_height`, and optionally `columns`, `rows`, `signature_size`, `creep`, `pdfx_profile`, `trim_marks`, `bleed`, `output_stem`.

---

## PDF/X Quality Gate

```bash
imposr_cli two-up --pages 8 --sheet-width 841.89 --sheet-height 595.28 \
  --pdfx-profile pdfx-4 --pdf-trim-marks 1 --pdf-bleed-box 1 --pdf-bleed 6 \
  --fail-on-quality-gate 1 --output-dir ./output
```

Supported profiles: `none`, `pdfx-1a`, `pdfx-4`.

---

## Common Options

| Option | Description |
|--------|-------------|
| `--output-dir <dir>` | Directory for all output files |
| `--output-stem <name>` | Base filename stem |
| `--pdfx-profile none\|pdfx-1a\|pdfx-4` | PDF/X compliance profile |
| `--pdf-trim-marks 0\|1` | Draw crop/trim marks |
| `--pdf-bleed-box 0\|1` | Draw bleed box |
| `--pdf-bleed <pt>` | Bleed amount in points |
| `--preflight 0\|1` | Run prepress preflight |
| `--fail-on-quality-gate 0\|1` | Exit non-zero if preflight fails |
| `--summary 0\|1` | Print human-readable summary |
| `--validate 0\|1` | Print plan validation issues |
| `--load-preset <file>` | Load settings from JSON preset |
| `--save-preset <file>` | Save current settings as JSON preset |
| `--page-sequence <csv>` | Override page order (1-based, 0=blank) |
| `--filter all\|even\|odd` | Filter pages before planning |
| `--reverse 0\|1` | Reverse page order |
| `--creep <pt>` | Booklet creep per sheet in points |
| `--signature-size <N>` | Pages per booklet signature |
