# Endproduct release runbook

Gebruik dit runbook om van development naar een echte endproduct release te gaan.

## 1) Vul echte host-evidence in

Werk `docs/sdk_smoke_evidence.json` bij met **echte** Windows/macOS host-runtime resultaten.
Mock-data is niet toegestaan voor een echte release.

## 2) Draai de release voorbereiding (hard gate + packaging)

```bash
python3 tools/prepare_endproduct_release.py \
  --evidence docs/sdk_smoke_evidence.json \
  --build-dir build-package \
  --build-type Release \
  --jobs 8
```

Deze command doet:
1. Hard endproduct gate (`tools/release_endproduct_gate.py`)
2. CMake configure voor packaging
3. Build
4. CPack package generatie
5. SHA256 checksum generatie
6. Report schrijven naar `docs/ENDPRODUCT_RELEASE_PREP_REPORT.md`

## 3) Check het report

Open:
- `docs/ENDPRODUCT_RELEASE_PREP_REPORT.md`

Als de gate faalt, stop direct en fix evidence/artefacten.

## 4) Publiceer pas bij volledige PASS

Publiceer pas wanneer:
- Hard gate PASS
- Packaging PASS
- Checksums PASS
- Host matrix volledig PASS met echte runtime evidence

## Dry-run (alleen development)

```bash
python3 tools/prepare_endproduct_release.py \
  --allow-mock \
  --allow-simulated-runtime
```

> Nooit gebruiken voor echte productrelease.
