# Imposr CLI Reference

## Synopsis

```
imposr_cli <mode> [options]
```

---

## Modes

### Imposition Planners

| Mode | Description |
|------|-------------|
| `two-up` | Two pages side-by-side per sheet |
| `n-up` | N pages per sheet in a grid |
| `booklet` | Saddle-stitch booklet signature planning |
| `step-repeat` | Step-and-repeat die cut layout |
| `tile` | Tile a large page across multiple output sheets |
| `manual` | Custom page sequence |

### New Module Modes

| Mode | Description |
|------|-------------|
| `trim-shift` | Build booklet plan then apply creep shift via TrimShift module |
| `adjust-pages` | Apply scale/crop/extend/scale-to-fit/scale-to-fill to all placements |
| `insert-blank` | Insert blank pages at a given position in the sequence |
| `insert-file` | Insert pages from another document at a given position |
| `insert-conditional` | Insert blank pages at even or odd positions |
| `stick-text` | Stamp a text string on every page |
| `stick-fields` | Stamp CSV variable fields on pages |
| `stick-bates` | Apply Bates numbering to pages |
| `stick-pdf` | Stamp a page from another PDF |
| `stick-tape` | Overlay a masking tape rectangle |
| `peel-off` | Overlay a peel-off label zone |
| `variable-data` | Merge CSV variable data with an overlay template |

### Compound Commands

| Mode | Sub-command | Description |
|------|-------------|-------------|
| `batch` | — | Run multiple jobs from a CSV file |
| `preset` | `list` | List preset JSON files in a directory |
| `preset` | `apply` | Apply a preset (use with `--load-preset`) |
| `sequence` | `list` | List sequence files in a directory |
| `sequence` | `run` | Run a sequence CSV file |

---

## Options

### Sheet / Layout

| Option | Type | Description |
|--------|------|-------------|
| `--pages <N>` | uint | Number of source pages |
| `--sheet-width <pt>` | double | Sheet width in PDF points |
| `--sheet-height <pt>` | double | Sheet height in PDF points |
| `--columns <N>` | uint | Grid columns (n-up, manual) |
| `--rows <N>` | uint | Grid rows (n-up, manual) |
| `--signature-size <N>` | uint | Pages per booklet signature |
| `--creep <pt>` | double | Booklet creep per sheet (points) |
| `--tile-overlap <pt>` | double | Tile overlap in points |

### Step-and-Repeat

| Option | Type | Description |
|--------|------|-------------|
| `--repeat-x <N>` | uint | Repetitions in X |
| `--repeat-y <N>` | uint | Repetitions in Y |
| `--step-x <pt>` | double | Step offset in X |
| `--step-y <pt>` | double | Step offset in Y |
| `--slot-width <pt>` | double | Slot width |
| `--slot-height <pt>` | double | Slot height |

### Page Sequence

| Option | Type | Description |
|--------|------|-------------|
| `--page-sequence <csv>` | string | Explicit 1-based page order (0 = blank) |
| `--manual-sequence <csv>` | string | Manual slot assignment (manual mode) |
| `--reverse 0\|1` | bool | Reverse page order |
| `--filter all\|even\|odd` | enum | Page filter |
| `--pad-multiple <N>` | uint | Pad page count to a multiple of N |
| `--source-page-width <pt>` | double | Override source page width |
| `--source-page-height <pt>` | double | Override source page height |
| `--fit-to-slot 0\|1` | bool | Scale pages to fit their slot |
| `--rotate-to-fit 0\|1` | bool | Auto-rotate pages to fit |

### Adjust Pages

| Option | Type | Description |
|--------|------|-------------|
| `--adjust-mode <mode>` | enum | `scale`, `crop`, `extend`, `scale-to-fit`, `scale-to-fill` |
| `--scale-x <f>` | double | X scale factor (default 1.0) |
| `--scale-y <f>` | double | Y scale factor (default 1.0) |
| `--target-width <pt>` | double | Target width for scale-to-fit/fill |
| `--target-height <pt>` | double | Target height for scale-to-fit/fill |
| `--crop-x <pt>` | double | Crop rect X origin |
| `--crop-y <pt>` | double | Crop rect Y origin |
| `--crop-w <pt>` | double | Crop rect width |
| `--crop-h <pt>` | double | Crop rect height |
| `--extend-top <pt>` | double | Extension at top |
| `--extend-bottom <pt>` | double | Extension at bottom |
| `--extend-left <pt>` | double | Extension at left |
| `--extend-right <pt>` | double | Extension at right |

### Insert

| Option | Type | Description |
|--------|------|-------------|
| `--insert-at <N>` | uint | 0-based position to insert at |
| `--insert-count <N>` | uint | Number of pages to insert (default 1) |
| `--insert-doc <id>` | string | Source document ID for insert-file |

### Stick-On Elements

| Option | Type | Description |
|--------|------|-------------|
| `--text <str>` | string | Text content for stick-text |
| `--anchor <anchor>` | enum | `topleft`, `topcenter`, `topright`, `middleleft`, `middlecenter`, `middleright`, `bottomleft`, `bottomcenter`, `bottomright` |
| `--font-size <pt>` | double | Font size in points (default 12) |
| `--opacity <0-1>` | double | Opacity (default 1.0) |
| `--color-r <0-1>` | double | Red channel |
| `--color-g <0-1>` | double | Green channel |
| `--color-b <0-1>` | double | Blue channel |
| `--stick-rect-x <pt>` | double | Stick-on rect X (default 0) |
| `--stick-rect-y <pt>` | double | Stick-on rect Y (default 0) |
| `--stick-rect-w <pt>` | double | Stick-on rect width (default 200) |
| `--stick-rect-h <pt>` | double | Stick-on rect height (default 20) |
| `--apply-to-pages <spec>` | string | `all` or comma-separated 0-based indices |
| `--variable-csv <file>` | path | CSV for stick-fields / variable-data |
| `--overlay-template <str>` | string | Template string with `{{key}}` tokens |

### Bates Numbering

| Option | Type | Description |
|--------|------|-------------|
| `--bates-prefix <str>` | string | Prefix before counter |
| `--bates-suffix <str>` | string | Suffix after counter |
| `--bates-start <N>` | uint | Starting counter value (default 1) |
| `--bates-pad <N>` | uint | Zero-pad width (default 4) |

### PDF Stamp from File

| Option | Type | Description |
|--------|------|-------------|
| `--source-pdf <path>` | path | PDF to stamp |
| `--source-pdf-page <N>` | uint | 0-based page index in source PDF |

### PDF Composition

| Option | Type | Description |
|--------|------|-------------|
| `--pdf-header <text>` | string | Header text on proof PDF |
| `--pdf-footer <text>` | string | Footer text on proof PDF |
| `--pdf-sheet-number 0\|1` | bool | Include sheet number in proof PDF |
| `--pdf-trim-marks 0\|1` | bool | Draw trim marks in proof PDF |
| `--pdf-trim-length <pt>` | double | Trim mark length in points |
| `--pdf-trim-offset <pt>` | double | Trim mark offset from trim box |
| `--pdf-bleed-box 0\|1` | bool | Draw bleed box |
| `--pdf-bleed <pt>` | double | Bleed amount in points |
| `--pdf-bates-enable 0\|1` | bool | Include Bates number in proof PDF |
| `--pdf-bates-prefix <str>` | string | Bates prefix in proof PDF |
| `--pdf-bates-start <N>` | uint | Bates start counter in proof PDF |
| `--pdf-overlay-template <str>` | string | Overlay template for proof PDF |
| `--pdf-variable-csv <file>` | path | Variable data CSV for proof PDF |

### Quality Gate & Preflight

| Option | Type | Description |
|--------|------|-------------|
| `--pdfx-profile none\|pdfx-1a\|pdfx-4` | enum | PDF/X compliance target |
| `--preflight 0\|1` | bool | Run prepress preflight |
| `--preflight-out <file>` | path | Write preflight JSON |
| `--fail-on-preflight 0\|1` | bool | Exit 3 if preflight has errors |
| `--validate 0\|1` | bool | Run plan validation |
| `--fail-on-validation 0\|1` | bool | Exit 2 if validation has issues |
| `--fail-on-quality-gate 0\|1` | bool | Exit 4 if either gate fails |

### Presets

| Option | Type | Description |
|--------|------|-------------|
| `--load-preset <file>` | path | Load settings from JSON preset |
| `--save-preset <file>` | path | Save current settings to JSON preset |
| `--preset-dir <dir>` | path | Directory to scan for `preset list` |

### Sequence

| Option | Type | Description |
|--------|------|-------------|
| `--sequence-file <file>` | path | CSV sequence file for `sequence run` |
| `--sequence-dir <dir>` | path | Directory to scan for `sequence list` |

### Batch

| Option | Type | Description |
|--------|------|-------------|
| `--batch-csv <file>` | path | CSV job list for batch mode |
| `--batch-report-out <file>` | path | Write batch report JSON |
| `--batch-stop-on-error 0\|1` | bool | Stop batch on first failure |

### Output Paths

| Option | Type | Description |
|--------|------|-------------|
| `--output-dir <dir>` | path | Base directory; auto-generates all output paths |
| `--output-stem <name>` | string | Filename stem for auto-generated paths |
| `--stamp-output 0\|1` | bool | Append UTC timestamp to stem |
| `--out <file>` | path | Plan JSON output |
| `--audit-out <file>` | path | Audit XML output |
| `--manifest-out <file>` | path | Placement manifest JSON |
| `--acrobat-js-out <file>` | path | Acrobat placement JavaScript |
| `--sdk-ops-out <file>` | path | Acrobat SDK ops JSON |
| `--composition-out <file>` | path | Production composition JSON |
| `--pdf-out <file>` | path | Proof PDF output |
| `--preflight-out <file>` | path | Preflight JSON output |
| `--job-out <file>` | path | Job report JSON |
| `--module-out <file>` | path | Module-specific JSON output |

### Inspection

| Option | Type | Description |
|--------|------|-------------|
| `--inspect-source-page <N>` | uint | Show which sheet/slot a source page lands on |
| `--inspect-sheet <N>` | uint | Show source page for sheet+slot |
| `--inspect-slot <N>` | uint | Slot index for inspection |
| `--summary 0\|1` | bool | Print human-readable plan summary |

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Usage or I/O error |
| 2 | Validation failed (`--fail-on-validation`) |
| 3 | Preflight failed (`--fail-on-preflight`) |
| 4 | Combined quality gate failed (`--fail-on-quality-gate`) |
| 5 | One or more batch/sequence jobs failed |

---

## PDF Points Reference

1 inch = 72 pt. Common sizes:

| Format | Width × Height (pt) |
|--------|---------------------|
| A4 portrait | 595.28 × 841.89 |
| A4 landscape | 841.89 × 595.28 |
| A3 portrait | 841.89 × 1190.55 |
| US Letter portrait | 612 × 792 |
| US Letter landscape | 792 × 612 |
