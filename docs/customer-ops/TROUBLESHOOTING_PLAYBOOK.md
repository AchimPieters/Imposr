# Troubleshooting Playbook

## Scope

Runbook voor L1/L2 support bij commerciële deploys.

## Verplichte troubleshooting domeinen

1. Installatieproblemen (prerequisites, signer trust, installer logs)
2. Plugin load-fouten (SDK/host compatibility, load traces)
3. Preflight/runtime quality gate failures
4. Loglocaties, support bundle verzameling en escalatiecriteria

## Werkwijze

- Diagnose volgens vaste triage-sequentie (environment -> licensing -> runtime -> output).
- Reproduceerbaarheid en artifact capture verplicht vóór escalatie naar engineering.
- Link incidenten aan bekende errors en regressies voor snellere TTR.

## Governance status

**Status (2026-04-20):** playbook baseline vastgesteld voor support-operaties.
