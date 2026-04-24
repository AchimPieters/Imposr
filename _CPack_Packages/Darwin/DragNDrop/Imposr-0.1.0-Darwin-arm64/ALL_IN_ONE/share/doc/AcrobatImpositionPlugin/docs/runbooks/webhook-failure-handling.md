# Webhook Failure Handling Runbook

## Scope

Incident handling voor provider-webhooks (`stripe`, `paddle`) op endpoint `/api/webhooks/licensing`.

## Detectie

- Monitor 4xx/5xx spikes op webhook endpoint.
- Alarm op replay-detectie en signature mismatch ratio > 5% in 5 minuten.

## Classificatie

1. **Signature failures** (`*_signature mismatch`, `timestamp outside tolerance`)
2. **Replay failures** (`Webhook replay detected`)
3. **Payload/schema failures** (`VALIDATION_ERROR`, event type invalid)
4. **Downstream failures** (licensing store write/deactivate issue)

## Eerste respons (0-15 min)

1. Bevestig actieve provider (`PAYMENT_PROVIDER`) en secret rollout status.
2. Controleer host clock drift (`NTP`) bij timestamp tolerance failures.
3. Valideer dat webhook raw-body ongewijzigd doorkomt (proxy/body-parser settings).
4. Controleer replay store footprint en event-id collision patterns.

## Mitigatie

- **Signature mismatch:** rotate `PAYMENT_WEBHOOK_SECRET`, verlaag verkeer via provider retry backoff.
- **Timestamp failures:** herstel clock drift, tijdelijk `PAYMENT_WEBHOOK_SIGNATURE_TOLERANCE_MS` verhogen.
- **Replay spikes:** inspecteer duplicate delivery door provider, behoud idempotency store.
- **Schema failures:** rollback recent API contract changes of patch mapper/adapters.

## Recovery & Verificatie

1. Replay gesamplede sandbox fixtures (Stripe/Paddle) tegen staging.
2. Verifieer dat `payment.webhook` audit events weer succesvol zijn.
3. Check activation-state mutaties voor revoked/canceled events.

## Post-incident

- Documenteer root-cause en tijdlijn.
- Voeg regressietest toe met fixture uit incident.
- Plan secret rotation als actiepunt indien signing issue betrokken was.
