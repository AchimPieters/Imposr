# Acrobat SDK smoke runbook (Windows 11 + macOS)

Doel: aantoonbaar platformbewijs leveren voor release-go/no-go van de native Acrobat-imposition hostflow.

## 1) Voorwaarden

- Acrobat Pro versie vastgelegd per platform.
- Juiste Acrobat SDK versie lokaal geïnstalleerd.
- Plug-in build met:
  - `-DAIMP_BUILD_PLUGIN=ON`
  - `-DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON`
- Testdocument met meerdere pagina’s (minimaal 8 pagina’s).

## 2) Teststappen per matrix-rij

Voer onderstaande op elke doelrij uit:
- Windows 11 23H2 / x64
- macOS 14 Sonoma / arm64
- macOS 14 Sonoma / x64 (Rosetta/Intel)

### A. Plugin load + menu zichtbaar
1. Start Acrobat Pro.
2. Bevestig submenu **Acrobat Imposition Plugin** zichtbaar.
3. Leg screenshot/log vast.

### B. Panel UX quick actions
1. Klik `Panel: Cycle layout` (controleer preset update).
2. Klik `Panel: Toggle trim+bleed`.
3. Klik `Panel: Toggle quality gate`.
4. Klik `Panel: Sheet A4` en daarna `Panel: Sheet A3` (controleer sheet/preset update).
5. Klik `Panel: Set output temp`.
6. Klik `Panel: Show state` (na validate/run).
7. Bewerk `panel-state.json` (bijv. outputStem/mode) en klik `Panel: Apply state`; verifieer preset-update.
8. Klik `Panel: Open unified dialog` en controleer dat dialog package/HTML zichtbaar is.
9. Bewaar screenshot/log output.

### C. Validate flow
1. Klik `Preset: Validate active job`.
2. Controleer READY/BLOCKED status + issue counts.
3. Controleer dat `acrobat-imposition-panel-state.json` is bijgewerkt.

### D. Preview flow
1. Klik `Preset: Preview proof`.
2. Controleer proof-pad en openbaarheid van output PDF.

### E. Run-bundle + native output
1. Klik `Preset: Run bundle`.
2. Controleer artifacts in bundle:
   - `plan.json`
   - `manifest.json`
   - `sdk-ops.json`
   - `xobject-compose.json`
   - `production-composition.json`
   - `preflight.json`
   - `proof.pdf`
   - `imposed-output.pdf` (bij succesvolle SDK composer)
   - `panel-state.json`
3. Controleer dat native imposed output automatisch opent als composer slaagt.

### F. Runtime quality gate
1. Forceer een kritieke preflight-fout (bijv. PDF/X + ontbrekende trim/bleed policy).
2. Verifieer dat run geblokkeerd wordt wanneer preset gate dit vereist.

## 3) Evidence invullen en gates draaien

1. Vul `docs/sdk_smoke_evidence.template.json` in naar `docs/sdk_smoke_evidence.json`.
   - Vul ook `bundle_path`, `proof_pdf_path` en `imposed_output_path` per matrix-rij in.
2. Valideer gate:
   - `python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.json`
3. Genereer checklist:
   - `python3 tools/generate_release_checklist.py --evidence docs/sdk_smoke_evidence.json --out docs/RELEASE_CHECKLIST.md`
4. (Optioneel) vul/actualiseer één matrix-rij geautomatiseerd:
   - `python3 tools/upsert_sdk_smoke_row.py --evidence docs/sdk_smoke_evidence.json --os \"Windows 11 23H2\" --cpu \"x64\" --set plugin_load=pass --set menu_actions=pass --set preset_validate=pass --set preset_preview=pass --set preset_run_bundle=pass --set runtime_quality_gate=pass --set imposed_output_open=pass --set panel_quick_actions=pass --set bundle_path=C:/evidence/run-001 --set proof_pdf_path=C:/evidence/run-001/proof.pdf --set imposed_output_path=C:/evidence/run-001/imposed-output.pdf`
5. (Optioneel) auto-collect vanuit bundle mappen:
   - `python3 tools/collect_host_smoke_evidence.py --evidence docs/sdk_smoke_evidence.json --row \"Windows 11 23H2|x64|C:/evidence/run-001\"`

Release is alleen **GO** als alle vereiste checks op pass staan voor alle matrix-rijen.
