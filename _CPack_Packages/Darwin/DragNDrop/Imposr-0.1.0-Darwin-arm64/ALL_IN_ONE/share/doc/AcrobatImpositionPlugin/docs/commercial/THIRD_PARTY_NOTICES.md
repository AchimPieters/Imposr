# Third-Party Notices

Dit document beschrijft het minimale releasebeleid voor third-party notices in commerciële builds.

## Scope

Voor **elke** dependency in runtime/build/distributie moet onderstaande metadata aanwezig zijn in de release bundle:

1. Package naam
2. Exacte versie
3. Licentietype (SPDX waar mogelijk)
4. Copyright notice
5. Bron-URL
6. Eventuele attribution-tekst zoals door licentie vereist

## Release procedure

- Voor elke release wordt een dependency-inventaris getrokken uit lockfiles en build manifests.
- Notices worden geüpdatet en opgeslagen als artifact in de release map.
- Legal/compliance voert finale sign-off uit vóór GA publicatie.

## Governance status

**Status (2026-04-20):** policy gedefinieerd; releaseblokker blijft actief als notice-set ontbreekt of verouderd is.
