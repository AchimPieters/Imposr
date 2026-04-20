# Imposr Pro REST API Reference

Deze referentie beschrijft de commerciële API-endpoints voor licensing en billing.

## Endpoints

- `GET /health`
- `GET /api/license/verify`
- `GET /api/license/audit?limit=<n>`
- `POST /api/license/offline/validate`
- `POST /api/webhooks/licensing`
- `POST /api/webhooks/trial`
- `POST /api/webhooks/paid`

## Contractbron

De formele contractdefinitie staat in `docs/api-reference/openapi.yaml`.

## Validaties

Request-body validatie wordt in runtime afgedwongen met Zod-schema's in:
`src/api/middleware/schemas.ts`.
