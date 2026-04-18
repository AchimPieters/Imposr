# Imposr

Imposr is een C++ imposition-engine met een Adobe Acrobat plug-in skeleton, CLI-workflow en testbare planner-kern.

> **Huidige status:** de planner, proof-composer, presets, audits en batch-flow zijn bruikbaar. De echte productiecompositie van bronpagina-content in Acrobat is nog in ontwikkeling.

## Wat dit project vandaag kan

- 2-up, N-up, booklet, step-repeat, tile en manual sequence planning.
- Reverse/even/odd filtering, blank padding, page sequence overrides.
- JSON plan output, XML audit output en preflight/quality-gate rapportage.
- Proof PDF (geometrie/labels/trim/bleed visualisatie) zonder SDK afhankelijkheid.
- Preset load/save en batch-orchestratie via CSV.
- Acrobat plug-in menu skeleton met validate/preview/run-bundle acties.

## Wat nog niet productie-klaar is

- Volledige native Acrobat page-content compositie (bronpagina's echt plaatsen op output sheets).
- Volwaardige persistente plug-in UI/panel (huidig: menu + panel-state artefacten).
- Volledige host-validatie matrix (Windows 11 + macOS) met ingevulde pass-evidence.
- Definitieve packaging/signing/notarization releaseflow voor distributie.

Zie voor detailstatus:
- `docs/ROADMAP.md`
- `docs/IMPLEMENTATIE_AUDIT_2026-04-17.md`
- `docs/COMPATIBILITY_MATRIX_2026-04-18.md`

## Snel starten (zonder Acrobat SDK)

### Vereisten

- CMake 3.21+
- C++17 compiler (GCC/Clang/MSVC)

### Build + tests

```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### CLI voorbeeld

```bash
./build/imposr_cli two-up \
  --pages 8 \
  --sheet-width 1190.55 \
  --sheet-height 841.89 \
  --output-dir ./out \
  --output-stem demo \
  --summary 1 \
  --validate 1 \
  --fail-on-quality-gate 1
```

Dit genereert (afhankelijk van flags) onder andere:
- `plan.json`
- `audit.xml`
- `manifest.json`
- `sdk-ops.json`
- `production-composition.json`
- `proof.pdf`
- `preflight.json`
- `job report` JSON

## Acrobat plug-in gebruiken (host-omgeving)

### Build met Adobe SDK

Stel `ACROBAT_SDK_DIR` in en bouw met plugin aan:

```bash
cmake -S . -B build-plugin \
  -DAIMP_BUILD_PLUGIN=ON \
  -DACROBAT_SDK_DIR="/pad/naar/AcrobatSDK" \
  -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON
cmake --build build-plugin -j
```

Daarna plaats/laad je de plugin volgens de normale Acrobat SDK host-instructies.

### Smoke-test runbook

Volg stap-voor-stap:
- `docs/SDK_SMOKE_RUNBOOK.md`

Evidence + release-gates:
- `docs/sdk_smoke_evidence.template.json`
- `python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.json`
- `python3 tools/generate_release_checklist.py --evidence docs/sdk_smoke_evidence.json --out docs/RELEASE_CHECKLIST.md`

## Productie-readiness checklist (hoog niveau)

1. **Host bring-up afgerond** op alle doelplatforms (Win11 x64, macOS arm64, macOS x64).
2. **Compatibiliteitsmatrix gevuld** met echte Acrobat + SDK versies en evidence artefacten.
3. **Native compositie stabiel** (planner output -> SDK placement -> imposed output PDF).
4. **Quality gates hard afdwingbaar** in host-run (validation + preflight + runtime fail).
5. **UX afgerond** (één persistente dialog/panel met preset lifecycle).
6. **Release engineering** (signing/notarization/versioning/rollback + support policy).

## Kan ik nu al testen met de nieuwste Adobe Acrobat op macOS?

**Ja, technisch kun je host-smoke tests doen als je lokaal de juiste Acrobat SDK + toolchain hebt.**

**Maar: nee, het project is nog niet “productie-klaar” voor volledige output-compositie.**

Wat je nu al kunt valideren op macOS:
- Plugin load in Acrobat.
- Menu acties (validate/preview/run-bundle).
- Artefactgeneratie (`plan/manifest/sdk-ops/preflight/proof/panel-state`).
- Runtime quality gate gedrag.

Wat nog ontbreekt voor echte productie-uitrol:
- Volledige native compositie + robuuste regressietests op echte klant-PDF's.
- Afgeronde matrix-pass op alle doelplatforms/architecturen.
- Hardening van packaging/signing/notarization en operationele supportdocumentatie.

## Licentie

Zie `LICENSE`.
