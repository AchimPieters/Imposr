# Endproduct release gate (GO/NO-GO)

Dit document definieert wanneer Imposr als **eindproduct** mag worden aangemerkt.

## Definitie van GO

Een release is alleen **GO** als:

1. Alle doelplatform-rijen bestaan in `docs/sdk_smoke_evidence.json`.
2. Elke rij `execution_mode=host-runtime` gebruikt (geen alleen-simulated release).
3. Alle checks op `pass` staan:
   - `plugin_load`
   - `menu_actions`
   - `preset_validate`
   - `preset_preview`
   - `preset_run_bundle`
   - `runtime_quality_gate`
   - `imposed_output_open`
   - `panel_quick_actions`
4. Metadata niet leeg/TBD is (`acrobat_version`, `sdk_version`, `run_timestamp_utc`).
5. Vereiste artifactpaden bestaan op disk:
   - `bundle_path`
   - `proof_pdf_path`
   - `imposed_output_path`
   - `preflight_json_path`
   - `sdk_ops_path`
   - `control_surface_path`
6. Er geen mock-evidence aanwezig is.
7. Verplichte commerciële governance-documenten aanwezig zijn (licentie/compliance/support/security/customer-ops).

## Eén commando voor GO/NO-GO

```bash
python3 tools/release_endproduct_gate.py --evidence docs/sdk_smoke_evidence.json
```

- Exit code `0` = PASS (release mag doorgaan)
- Exit code `1` = FAIL (release blokkeren)

## Lokale dry-run (alleen voor development)

```bash
python3 tools/release_endproduct_gate.py \
  --evidence docs/sdk_smoke_evidence.json \
  --allow-mock \
  --allow-simulated-runtime
```

> Gebruik `--allow-mock` nooit voor echte releases.
> Gebruik `--skip-commercial-docs-check` alleen tijdelijk tijdens setup; niet voor echte releases.
> Gebruik `--allow-incomplete-commercial-docs` alleen tijdens opstart; commerciële release vereist volledig afgevinkte checklists/policies.

## Aanbevolen release-volgorde

1. Host smoke evidence invullen (Windows + macOS).
2. Gate draaien: `release_endproduct_gate.py`.
3. Packaging draaien (`COMPILE.md`, sectie scripts).
4. Checksums genereren (`tools/generate_package_checksums.py`).
5. Pas dan release publiceren.
