# Imposr Pro Beta testplan

Deze beta is test-klaar met een volledig geautomatiseerde 6-fase pipeline.

## 1) End-to-end beta build + gate

```bash
npm run beta:prepare
```

Deze command voert systematisch alle fases uit:

1. Preflight evidence check.
2. Type safety (`npm run typecheck`).
3. Test quality gate (`npm run test -- --runInBand tests/unit/commercial tests/unit/cli`).
4. Build (`npm run build:main`).
5. Beta readiness gate (`node dist/cli/beta-ready.js ...`).
6. Artifact verificatie (`docs/BETA_READINESS_REPORT.json`).

## 2) Snelle handmatige route

```bash
npm run typecheck
npm run test -- --runInBand tests/unit/commercial/BetaReadinessEvaluator.test.ts tests/unit/commercial/BetaReleaseOrchestrator.test.ts tests/unit/cli/beta.test.ts tests/unit/cli/beta-ready.test.ts tests/unit/cli/beta-prepare.test.ts
npm run beta:ready
```

## Output

- Beta readiness rapport: `docs/BETA_READINESS_REPORT.json`
- Orchestration details: JSON output op stdout van `beta:prepare`


Optioneel strengere coverage-run:

```bash
node dist/cli/beta-prepare.js --release-root . --evidence docs/sdk_smoke_evidence.json --report docs/BETA_READINESS_REPORT.json --coverage
```
