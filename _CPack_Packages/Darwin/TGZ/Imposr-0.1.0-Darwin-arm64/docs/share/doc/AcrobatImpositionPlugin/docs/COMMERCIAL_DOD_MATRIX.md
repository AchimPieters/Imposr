# Imposr Pro – Definition of Done (DoD) Matrix

Dit document definieert de **harde acceptatiecriteria** per fase voor de transformatie van Imposr naar een commercieel product (Quite Imposing-klasse).

## Scoring model

- **PASS**: criterium is aantoonbaar gehaald (code + test + documentatie + operationeel bewijs).
- **PARTIAL**: gedeeltelijk aanwezig, nog blockers.
- **FAIL**: niet aanwezig.

Bewijsvormen:
1. Broncode in `src/**`
2. Testevidence (`tests/**` + CI)
3. Runbook/documentatie (`docs/**`)
4. Operationele output (release artifacts, host validation)

---

## Fase 1 — Core Foundation

| ID | Criterium | Meetbaar bewijs | Huidige status |
|---|---|---|---|
| F1-1 | TS/Electron project-config production-proof | `package.json`, `tsconfig`, `jest`, lint/format | PASS |
| F1-2 | Error model + logging met recoverability | `src/utils/errors.ts`, `src/utils/logger.ts` | PASS |
| F1-3 | PDF core processor stabiel + tests | `src/core/pdf/*`, unit tests, coverage >=80% | PASS |

**Definition of done Fase 1:** alle 3 criteria op PASS.

---

## Fase 2 — Licensing Core

| ID | Criterium | Meetbaar bewijs | Huidige status |
|---|---|---|---|
| F2-1 | Signed license verify/sign/assert | `LicenseManager`, tests | PASS |
| F2-2 | Machine binding + activation lifecycle | `MachineId`, `ActivationService`, tests | PASS |
| F2-3 | Offline token validatie | `OfflineValidator`, tests | PASS |

**Definition of done Fase 2:** alle 3 criteria op PASS.

---

## Fase 3 — API Enforcement

| ID | Criterium | Meetbaar bewijs | Huidige status |
|---|---|---|---|
| F3-1 | License guards op commerciële API routes | middleware + server wiring + tests | PASS |
| F3-2 | License lifecycle endpoints | `/api/license/*` + tests | PASS |
| F3-3 | Optionele API bearer auth | auth middleware + tests | PASS |

**Definition of done Fase 3:** alle 3 criteria op PASS.

---

## Fase 4 — Hardening & Persistence

| ID | Criterium | Meetbaar bewijs | Huidige status |
|---|---|---|---|
| F4-1 | Persistente activation storage | `FileActivationStore` + tests | PASS |
| F4-2 | Runtime factory met production secret enforcement | `LicensingFactory` + tests | PASS |
| F4-3 | Entitlement check = tier + explicit feature | `FeatureGate` + tests | PASS |

**Definition of done Fase 4:** alle 3 criteria op PASS.

---

## Fase 5 — Billing Integration

| ID | Criterium | Meetbaar bewijs | Huidige status |
|---|---|---|---|
| F5-1 | Signed webhook verify/process | `PaymentHandler` + tests | PASS |
| F5-2 | Trial/Paid issuance flows | payment APIs + tests | PASS |
| F5-3 | Webhook API surface operationeel | `/api/webhooks/*` + tests | PASS |

**Definition of done Fase 5:** alle 3 criteria op PASS.

---

## Fase 6 — Compliance & Ops

| ID | Criterium | Meetbaar bewijs | Huidige status |
|---|---|---|---|
| F6-1 | Append-only licensing audit trail | `LicenseAuditLogger` + tests | PASS |
| F6-2 | Audit query endpoint | `/api/license/audit` + tests | PASS |
| F6-3 | Supportability runbook + release gate | docs + release evidence | PARTIAL |

**Definition of done Fase 6:** alle 3 criteria op PASS.

---

## Global exit criteria (project-level)

1. **Coverage >=80% global** op actieve TS productmodules.
2. CI gate: test/lint/typecheck verplicht groen.
3. Host-validatie evidence op Windows 11 + macOS volledig ingevuld.
4. Commerciële docs compleet (pricing/licensing/support/ops/security).
5. Release artifacts en signing/notarization aantoonbaar.

**Projectstatus nu:** **PARTIAL** (governance docs op orde; host non-mock runtime evidence nog vereist voor finale PASS).
