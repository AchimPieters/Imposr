# Stappen naar 100/100 (endproduct release)

Dit plan vertaalt de open punten naar concrete uitvoering.

## Doel

Van de huidige MVP-status naar **100/100 release-ready** met echte host-runtime evidence.

## Stap 1 — Real Acrobat composition pipeline afronden

**DoD**
- Bronpagina-content wordt daadwerkelijk geplaatst op output sheets in Acrobat runtime.
- Output PDF opent na succesvolle run in host-flow.
- Minstens 1 echte end-to-end run per platform met imposed output artefact.

**Bewijs**
- `imposed_output_path` ingevuld per matrix-rij.
- Geen fallback-only pad; native pipeline actief.

## Stap 2 — Persistente production UI/panel afronden

**DoD**
- Eén persistente panel/dialog met mode/sheet/prepress/preset/run controls.
- Interactieve output bestemming + bestandsnaam in UI.
- Inline validatie/preflight feedback voor run.

**Bewijs**
- `panel_quick_actions` en run/validate flow op pass per platform.
- Screenshots/logs per OS in evidence bundle.

## Stap 3 — Cross-platform host matrix volledig PASS

**DoD**
- Windows 11 x64, macOS arm64 en macOS x64 allemaal PASS.
- Echte Acrobat + SDK versies ingevuld (geen placeholder/TBD).

**Bewijs**
- `docs/sdk_smoke_evidence.json` volledig ingevuld met host-runtime data.
- `python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.json --require-host-runtime --forbid-mock` geeft PASS.

## Stap 4 — Prepress features production-grade maken

**DoD**
- Marks/bleed/creep/overlay/CSV/PDF-X werken in het **echte** compositiepad (niet alleen proof).
- Kritieke preflight fouten blokkeren runtime run.

**Bewijs**
- Preflight JSON en quality gate artefacten aanwezig in iedere platformrij.
- Runtime quality gate op pass voor alle matrix-rijen.

## Stap 5 — Release engineering hardening

**DoD**
- Packaging, checksums, signing/notarization procedures zijn vastgelegd en reproduceerbaar.
- GO/NO-GO gate en release-prep flow zijn onderdeel van releaseproces.

**Bewijs**
- `python3 tools/prepare_endproduct_release.py --evidence docs/sdk_smoke_evidence.json --build-dir build-package --build-type Release --jobs 8`
- `python3 tools/release_endproduct_gate.py --evidence docs/sdk_smoke_evidence.json`

## Stap 6 — Hard gate 100/100 afdwingen

**DoD**
- `score_sdk_readiness` op 100/100 zonder mock.
- Alle gates groen zonder uitzonderingen.

**Commands (moeten allemaal slagen)**

```bash
python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.json --require-host-runtime --forbid-mock
python3 tools/score_sdk_readiness.py --evidence docs/sdk_smoke_evidence.json --require-100 --forbid-mock
python3 tools/release_endproduct_gate.py --evidence docs/sdk_smoke_evidence.json
python3 tools/prepare_endproduct_release.py --evidence docs/sdk_smoke_evidence.json --build-dir build-package --build-type Release --jobs 8
```

## Uitvoering in volgorde (kort)

1. Native compositie afronden.
2. Persistente UI/panel afronden.
3. Echte host-smoke runs op alle doelplatforms uitvoeren.
4. Evidence + artefacten invullen.
5. Hard gate draaien.
6. Packaging + checksums draaien.
7. Pas dan release publiceren.
