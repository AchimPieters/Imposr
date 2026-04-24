# Acrobat Imposition – compatibiliteitsmatrix (18 april 2026)

## Doel
Deze matrix is bedoeld als release-evidence voor de echte Acrobat host-validatie op Windows 11 en macOS.

## Matrix

| OS | CPU | Acrobat versie | SDK versie | Plugin load | Menu actions | Preset validate | Preset preview | Preset run bundle | Runtime quality gate | Status |
|---|---|---|---|---|---|---|---|---|---|---|
| Windows 11 23H2 | x64 | _TBD_ | _TBD_ | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | Pending |
| macOS 14 Sonoma | arm64 | _TBD_ | _TBD_ | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | Pending |
| macOS 14 Sonoma | x64 (Rosetta/Intel) | _TBD_ | _TBD_ | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | Pending |

## Benodigde evidence per rij
1. Screenshot of log dat plugin succesvol laadt.
2. Screenshot van menu-items zichtbaar en klikbaar.
3. Resultaat van `Preset: Validate active job` inclusief issue-aantallen.
4. Pad naar gegenereerde preview PDF.
5. Pad naar run-bundle directory (plan/manifest/sdk-ops/audit/placement JS/preflight/proof/imposed-output/composition/panel-state).
6. Bewijs dat runtime quality gate blokkeert bij kritieke preflight fouten.
7. Bewijs dat native `imposed-output.pdf` automatisch opent na succesvolle run-bundle composer.
8. Bewijs van panel quick-actions (`Cycle layout`, `Toggle trim+bleed`, `Toggle quality gate`, `Sheet A4`, `Sheet A3`, `Set output temp`, `Show state`, `Apply state`, `Open unified dialog`).
9. Bundle/proof/imposed output artifact-paden ingevuld in smoke evidence JSON.
10. Bewijs dat `xobject-compose.json` is gegenereerd in run-bundle.
11. Eventuele crashlogs of SDK-waarschuwingen.

## Release-gate
Een release is pas **GO** wanneer alle doelplatform-rijen minimaal `Plugin load + Menu actions + Preset validate + Preset preview + Preset run bundle + Runtime quality gate` op **Pass** staan.

Automatische controle:
- Vul `docs/sdk_smoke_evidence.template.json` in als `docs/sdk_smoke_evidence.json`.
- Run `python3 tools/validate_sdk_smoke.py --evidence docs/sdk_smoke_evidence.json`.
- Genereer release-checklist: `python3 tools/generate_release_checklist.py --evidence docs/sdk_smoke_evidence.json --out docs/RELEASE_CHECKLIST.md`.
- Werk individuele matrix-rijen bij: `python3 tools/upsert_sdk_smoke_row.py ...`.
- Auto-collect bundle artifacts naar evidence-rijen: `python3 tools/collect_host_smoke_evidence.py ...`.
- De release gate faalt zolang niet alle vereiste checks op `pass` staan en artifact-paden ontbreken.
- Zie ook `docs/SDK_SMOKE_RUNBOOK.md` voor stap-voor-stap testuitvoering per platform.
