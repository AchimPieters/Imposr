# Imposr Pro – Gap Closure Plan (Execution)

Dit plan vertaalt de DoD-matrix naar een uitvoerbare werkvolgorde met harde outputs.

## Werkafspraken

- Elke taak heeft: code, tests, docs, en acceptatiecheck.
- Geen placeholders; alleen productiegeschikte implementatie.
- Elke fase sluit af met een expliciete quality gate.

---

## Fase 1 closure (afgerond)

### Open gap
- Geen open blockers binnen de huidige TypeScript PDF-kernscope.

### Te leveren
1. Volledige TS module-dekking voor ontbrekende core submodules (`PDFLoader`, `PDFExporter`, `PDFValidator`).
2. Unit tests + integration tests voor PDF edge cases (corrupt/large/encrypted).
3. Coverage gate hard valideren (`npm run test:coverage`) en rapport vastleggen.

### Exit checks
- `npm run typecheck`
- `npm run lint`
- `npm run test:coverage`
- Coverage threshold global >=80% en fase artifacts gedocumenteerd.

---

## Fase 2 closure (afgerond)

### Open gap
- Geen open blockers binnen huidige licensing security scope (key rotation + replay protection).

### Te leveren
1. Secret rotation strategy en key-versioning voor license/webhook signing.
2. Replay-protection voor webhook events (idempotency store).
3. Security testcases (tampering/replay/time-skew).

### Exit checks
- Security test suite groen.
- Threat model sectie in docs toegevoegd.

---

## Fase 3 closure (afgerond)

### Open gap
- Geen open blockers binnen huidige contract/documentatie-scope.

### Te leveren
1. OpenAPI specificatie voor `/api/license/*` en `/api/webhooks/*`.
2. Consistente error-schema’s en request validation schemas.
3. API compatibility tests.

### Exit checks
- OpenAPI gevalideerd.
- Contract tests groen.

---

## Fase 4 closure (afgerond)

### Open gap
- Geen open blockers binnen operationele storage/migratie-scope.

### Te leveren
1. Locking strategy voor file stores (concurrent writer safety) geïmplementeerd.
2. Optional encrypted-at-rest storage voor activation/audit toegevoegd.
3. Backward-compatible migratie van legacy state files toegevoegd.

### Exit checks
- Concurrency tests groen.
- Migration tests groen.

---

## Fase 5 closure (afgerond)

### Open gap
- Geen open blockers binnen provider-integratie scope.

### Te leveren
1. Provider adapters (`StripeAdapter`, `PaddleAdapter`) toegevoegd.
2. Webhook signature parser per provider + timestamp tolerance geïmplementeerd.
3. Sandbox end-to-end tests met replay fixtures toegevoegd.

### Exit checks
- Provider integration tests groen.
- Incident runbook voor webhook failure handling toegevoegd.

---

## Fase 6 closure (in uitvoering)

### Open gap
- Host-runtime evidence matrix met non-mock runs ontbreekt nog.

### Te leveren
1. Support SLA + escalation matrix + audit retention policy gefinaliseerd.
2. Release governance docs en checklist-baseline gefinaliseerd.
3. Host evidence matrix (Windows/macOS) nog open tot non-mock runtime evidence beschikbaar is.

### Exit checks
- Release gate document op PASS (na host non-mock evidence).
- Alle global exit criteria in DoD matrix op PASS (na host non-mock evidence).

---

## Directe uitvoering (nu starten)

1. Sluit Fase 1 open punten (PDF submodules + coveragebewijs).
2. Daarna Fase 2 security hardening.
3. Daarna Fase 3 API contracts.
4. Fase 6 afronden met echte host-runtime evidence (Windows/macOS) en finale gate-run.

Dit document is de leidraad; bij elke voltooiing wordt status geüpdatet in `docs/COMMERCIAL_DOD_MATRIX.md`.
