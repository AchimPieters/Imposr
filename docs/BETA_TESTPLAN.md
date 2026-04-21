# Imposr Pro Beta testplan

Deze beta is test-klaar via een reproduceerbare TypeScript workflow met machineleesbaar rapport.

## Snelle start (lokaal/CI)

```bash
npm run typecheck
npm run test -- --runInBand tests/unit/commercial/BetaReadinessEvaluator.test.ts tests/unit/cli/beta.test.ts tests/unit/cli/beta-ready.test.ts
npm run beta:ready
```

Dit genereert `docs/BETA_READINESS_REPORT.json`.

## Handmatige run met custom paden

```bash
npm run build:main
node dist/cli/beta-ready.js \
  --release-root . \
  --evidence docs/sdk_smoke_evidence.json \
  --output docs/BETA_READINESS_REPORT.json
```

## Uitleg van de 6 fases

1. Evidence JSON is geldig en leesbaar.
2. Vereiste host-rijen (Windows/macOS) bestaan.
3. Runtime checks per host staan op PASS.
4. Commerciële governance-documenten bestaan en zijn niet leeg.
5. Checklists bevatten geen open `- [ ]` items.
6. Finale compliance/go-no-go status op basis van fase 1–5.
