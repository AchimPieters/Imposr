# Commercial Endproduct Release Checklist

Dit document dekt alle gevraagde punten om van MVP naar commercieel eindproduct te gaan.

## 1) Core product “af” maken (must-have)

- [ ] Echte native Acrobat compositie volledig afgerond (geen skeleton/fallback-only).
- [ ] Persistente production UI/panel opgeleverd.
- [ ] Prepress features in echte compositiepad (marks/bleed/creep/overlay/CSV/PDF-X runtime).

## 2) Release-readiness en quality gates

- [ ] Alle doelplatformen host-runtime PASS (Win11 x64, macOS arm64, macOS x64).
- [ ] Hard gate zonder uitzonderingen PASS:
  - `validate_sdk_smoke --require-host-runtime --forbid-mock`
  - `score_sdk_readiness --require-100 --forbid-mock`
  - `release_endproduct_gate`
  - `prepare_endproduct_release`
- [ ] Geen mock-data in release-evidence.

## 3) Packaging/distributie professioneel maken

- [ ] Signing/notarization/distributiebeleid definitief.
- [ ] Reproduceerbare build + checksums + installer artifacts per release.

## 4) Commercieel gebruik (governance)

- [ ] Third-party notices: `docs/commercial/THIRD_PARTY_NOTICES.md`
- [ ] EULA: `docs/commercial/EULA.md`
- [ ] Privacy statement: `docs/commercial/PRIVACY_STATEMENT.md`
- [ ] Telemetry disclosure: `docs/commercial/TELEMETRY_DISCLOSURE.md`
- [ ] Versioning policy: `docs/release/VERSIONING_POLICY.md`
- [ ] Rollback policy: `docs/release/ROLLBACK_POLICY.md`
- [ ] Upgrade compatibility matrix: `docs/release/UPGRADE_COMPATIBILITY_MATRIX.md`
- [ ] Incident SLA: `docs/support/INCIDENT_SLA.md`
- [ ] Security checklist: `docs/security/SECURITY_RELEASE_CHECKLIST.md`
- [ ] SBOM policy: `docs/security/SBOM_POLICY.md`
- [ ] Enterprise deployment guide: `docs/customer-ops/ENTERPRISE_DEPLOYMENT.md`
- [ ] Troubleshooting playbook: `docs/customer-ops/TROUBLESHOOTING_PLAYBOOK.md`

## 5) Prioriteit (snelste route)

1. Real composition pipeline (grootste blocker).
2. Persistente UI/panel voor operators.
3. Echte host-evidence matrix volledig PASS.
4. Prepress runtime parity + PDF/X runtime gate.
5. Signing/notarization/distributie hardening.
