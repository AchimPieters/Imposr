# Incident SLA

## Severity model

- **Sev1:** productie down / data-integriteitrisico
- **Sev2:** kritieke functionaliteit ernstig degraded
- **Sev3:** beperkte impact, workaround beschikbaar

## Response SLO targets

- **Sev1:** eerste respons binnen 1 uur, updates elke 60 min
- **Sev2:** eerste respons binnen 4 uur, updates elke 4 uur
- **Sev3:** eerste respons binnen 1 werkdag, updates per werkdag

## Escalation matrix

1. On-call support engineer
2. Product engineer owner
3. Incident commander
4. Engineering manager + customer success lead

## Communicatiebeleid

- Incident ticket + tijdlijn verplicht vanaf Sev2.
- Externe statusupdate verplicht voor customer-facing Sev1/Sev2 incidents.
- Postmortem verplicht voor Sev1 en terugkerende Sev2 incidents.

## Audit-retentiebeleid

- Incident logs en decision timeline: **minimaal 24 maanden**.
- Licensing audit records: **minimaal 24 maanden**.
- Postmortems en mitigatie-acties: **minimaal 36 maanden**.

## Governance status

**Status (2026-04-20):** SLA + escalation + retention policy vastgesteld voor release-operaties.
