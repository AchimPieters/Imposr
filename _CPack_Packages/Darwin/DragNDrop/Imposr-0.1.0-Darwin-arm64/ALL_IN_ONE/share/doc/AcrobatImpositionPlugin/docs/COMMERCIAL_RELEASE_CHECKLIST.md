# Commercial Endproduct Release Checklist

Dit document beschrijft de harde release gates voor commerciële vrijgave.

## 1) Core product must-haves

- Native Acrobat compositiepad volledig operationeel.
- Persistente production UI/panel beschikbaar.
- Prepress runtime parity (marks/bleed/creep/overlay/CSV/PDF-X) bewezen.

## 2) Release-readiness gates

- Host-runtime evidence volledig PASS (Win11 x64, macOS arm64, macOS x64).
- Hard gates zonder uitzonderingen:
  - `validate_sdk_smoke --require-host-runtime --forbid-mock`
  - `score_sdk_readiness --require-100 --forbid-mock`
  - `release_endproduct_gate`
  - `prepare_endproduct_release`
- Geen mock-data in release evidence.

## 3) Packaging en distributie

- Signing/notarization/distributiebeleid vastgesteld.
- Reproduceerbare build + checksums + installer artifacts per release.

## 4) Commerciële governance docs (verplicht)

- `docs/commercial/THIRD_PARTY_NOTICES.md`
- `docs/commercial/EULA.md`
- `docs/commercial/PRIVACY_STATEMENT.md`
- `docs/commercial/TELEMETRY_DISCLOSURE.md`
- `docs/release/VERSIONING_POLICY.md`
- `docs/release/ROLLBACK_POLICY.md`
- `docs/release/UPGRADE_COMPATIBILITY_MATRIX.md`
- `docs/support/INCIDENT_SLA.md`
- `docs/security/SECURITY_RELEASE_CHECKLIST.md`
- `docs/security/SBOM_POLICY.md`
- `docs/customer-ops/ENTERPRISE_DEPLOYMENT.md`
- `docs/customer-ops/TROUBLESHOOTING_PLAYBOOK.md`

## Current gate state (2026-04-20)

- Governance documentation baseline: **PASS**.
- Host-runtime evidence (non-mock): **BLOCKED** totdat echte host runs zijn aangeleverd.
