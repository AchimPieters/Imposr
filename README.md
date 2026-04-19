# Imposr

Imposr is een C++ imposition-engine met een Adobe Acrobat plug-in skeleton, CLI-workflow en testbare planner-kern.

> **Huidige status:** de planner, proof-composer, presets, audits en batch-flow zijn bruikbaar. De echte productiecompositie van bronpagina-content in Acrobat is nog in ontwikkeling.

Zie `COMPILE.md` voor platform-specifieke compile-instructies (Windows/macOS/Linux, inclusief plug-in buildpad).

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

### Strict production-gate build (aanbevolen voor release branches)

```bash
cmake -S . -B build-strict \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_ENABLE_WARNINGS=ON \
  -DAIMP_WARNINGS_AS_ERRORS=ON \
  -DAIMP_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-strict -j
ctest --test-dir build-strict --output-on-failure
```

### Installers/packages compileren (Windows, macOS, Linux)

Het project ondersteunt CPack packaging via `AIMP_ENABLE_PACKAGING=ON`.

```bash
cmake -S . -B build-package \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_BUILD_CLI=ON \
  -DAIMP_BUILD_TESTS=OFF \
  -DAIMP_ENABLE_PACKAGING=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-package -j
cpack --config build-package/CPackConfig.cmake -C Release
python3 tools/generate_package_checksums.py --dir build-package --out build-package/SHA256SUMS.txt
```

Verwachte package-uitvoer per platform:
- **Windows**: `ZIP` (en `NSIS` als `makensis` beschikbaar is).
- **macOS**: `DragNDrop (.dmg)` en `TGZ`.
- **Linux**: `TGZ` (en `DEB` als `dpkg` beschikbaar is).

Packaging gebruikt componenten (`runtime`, `devel`, `docs`) zodat release-artifacts duidelijk gescheiden blijven.
Controleer checksums met `SHA256SUMS.txt` voor distributie/uitrol.

CI-builds voor installers staan in `.github/workflows/installers.yml`.

## 100/100 production gate (definitie)

`100/100` betekent in dit project:
1. Alle matrix-rijen (Windows 11 x64, macOS arm64, macOS x64) hebben **host-runtime** evidence.
2. Alle vereiste checks staan op `pass`.
3. Artefactpaden en metadata zijn volledig ingevuld.

Valideer lokaal:

```bash
python3 tools/validate_sdk_smoke.py \
  --evidence docs/sdk_smoke_evidence.json \
  --require-host-runtime \
  --forbid-mock
python3 tools/score_sdk_readiness.py \
  --evidence docs/sdk_smoke_evidence.json \
  --require-100 \
  --forbid-mock
```

Één command voor end-to-end GO/NO-GO (finalize + validate + score + rapport):

```bash
python3 tools/run_host_go_no_go.py \
  --evidence docs/sdk_smoke_evidence.json \
  --fill docs/host_release_fill.json \
  --report-out docs/HOST_GO_NO_GO_REPORT.md
```

Tooling-integratietest (bewijst dat complete host-evidence 100/100 kan halen):

```bash
python3 tools/test_host_gate_tooling.py
```

Maak snel een host-runtime scaffold (met timestamp) vanuit template:

```bash
python3 tools/init_host_runtime_evidence.py \
  --timestamp-now \
  --acrobat-version "vul-hier-versie-in" \
  --sdk-version "vul-hier-sdk-in"
```

Markeer één platformrij direct als `pass` na een geslaagde host-run:

```bash
python3 tools/mark_host_smoke_pass.py \
  --evidence docs/sdk_smoke_evidence.json \
  --os "Windows 11 23H2" \
  --cpu "x64" \
  --acrobat-version "2026.001.xxxxx" \
  --sdk-version "DC-2026" \
  --bundle-path "C:/evidence/run-001" \
  --proof-pdf-path "C:/evidence/run-001/proof.pdf" \
  --imposed-output-path "C:/evidence/run-001/imposed-output.pdf" \
  --preflight-json-path "C:/evidence/run-001/preflight.json" \
  --sdk-ops-path "C:/evidence/run-001/sdk-ops.json" \
  --control-surface-path "C:/evidence/run-001/control-surface.json"
```

Markeer alle platformrijen in één keer (bulk) via invulbestand:

```bash
cp docs/host_release_fill.template.json docs/host_release_fill.json
# vul docs/host_release_fill.json met echte artifact-paden + versies
python3 tools/finalize_host_release_evidence.py \
  --evidence docs/sdk_smoke_evidence.json \
  --input docs/host_release_fill.json
python3 tools/validate_sdk_smoke.py \
  --evidence docs/sdk_smoke_evidence.json \
  --require-host-runtime \
  --forbid-mock
python3 tools/score_sdk_readiness.py \
  --evidence docs/sdk_smoke_evidence.json \
  --require-100 \
  --forbid-mock
```

CI production gate:
- `.github/workflows/host-runtime-gate.yml` (draait op `main` en `workflow_dispatch`).

Hard endproduct GO/NO-GO gate:

```bash
python3 tools/release_endproduct_gate.py --evidence docs/sdk_smoke_evidence.json
```

Documentatie:
- `docs/ENDPRODUCT_RELEASE_GATE.md`

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

Simulated gate (CI/container zonder echte Acrobat host):
- `python3 tools/generate_simulated_sdk_smoke_evidence.py --out docs/sdk_smoke_evidence.simulated.json --artifact-root /tmp/aimp-simulated-smoke`
- `python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.simulated.json`
- `python3 tools/generate_release_checklist.py --evidence docs/sdk_smoke_evidence.simulated.json --out docs/RELEASE_CHECKLIST.simulated.md`
- `python3 tools/generate_sdk_matrix_report.py --evidence docs/sdk_smoke_evidence.simulated.json --out docs/SDK_SMOKE_MATRIX_REPORT.simulated.md`

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
