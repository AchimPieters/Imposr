# Architecture

## Layers

### 1. Acrobat host integration
Responsibilities:
- handshake callbacks
- menu/panel integration
- active document access
- notifications

### 2. Imposition engine
Responsibilities:
- booklet signatures
- N-up slot calculation
- step & repeat
- blank-page padding
- shuffle rules
- tile planning

### 3. PDF composition layer
Responsibilities:
- create destination PDF
- place source pages on target sheets
- apply transforms
- draw marks / overlays / numbering

### 4. Workflow engine
Responsibilities:
- presets
- reusable sequences
- XML audit trail
- variables

### 5. Inspector layer
Responsibilities:
- trace source page -> output sheet
- reverse mapping
- debug and validation helpers

## Guiding rules

- Keep planning separate from Acrobat UI.
- Make composition as deterministic as possible.
- Store metadata for traceability.
- Design features to be non-destructive and reproducible.
