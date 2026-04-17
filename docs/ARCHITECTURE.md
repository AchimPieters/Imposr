# Architecture

## Layers

### 1. Acrobat host integration
Responsibilities:
- handshake callbacks
- menu/panel integration
- active document access
- notifications
- bridge between Acrobat document objects and pure-C++ planning/composition logic

### 2. Imposition engine
Responsibilities:
- booklet signatures
- N-up slot calculation
- step & repeat
- blank-page padding
- reverse order and page filtering
- fit-to-slot / auto-rotate metadata
- cross-platform pure-C++ core that can be tested outside Acrobat

### 3. PDF composition layer
Responsibilities:
- create destination PDF proof output without Acrobat SDK
- visualize target sheets and slot geometry
- draw labels / numbering / marks needed for debugging
- later: place real source pages on target sheets through Acrobat SDK APIs

### 4. Workflow engine
Responsibilities:
- presets
- reusable sequences
- XML audit trail
- variables
- deterministic, repeatable job setup

### 5. Inspector layer
Responsibilities:
- trace source page -> output sheet
- reverse mapping
- validation helpers
- human-readable summaries for CLI and future UI

## Guiding rules

- Keep planning separate from Acrobat UI.
- Make composition as deterministic as possible.
- Store metadata for traceability.
- Design features to be non-destructive and reproducible.
- Keep the planner fully usable without Acrobat so geometry can be tested on macOS and Windows CI.

## Current implementation split

### Pure C++ and testable today
- planners
- preset loading/saving
- inspector helpers
- plan statistics and validation
- JSON/XML export
- proof PDF generation
- CLI

### Requires Adobe SDK / Acrobat runtime
- actual plug-in loading
- active document access in Acrobat
- menu and panel lifecycle
- real placement of source PDF page content into imposed destination pages
