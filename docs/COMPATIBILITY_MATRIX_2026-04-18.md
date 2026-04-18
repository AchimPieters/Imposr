# Acrobat Imposition – compatibiliteitsmatrix (18 april 2026)

## Doel
Deze matrix is bedoeld als release-evidence voor de echte Acrobat host-validatie op Windows 11 en macOS.

## Matrix

| OS | CPU | Acrobat versie | SDK versie | Plugin load | Menu actions | Preset preview | Preset run bundle | Status |
|---|---|---|---|---|---|---|---|---|
| Windows 11 23H2 | x64 | _TBD_ | _TBD_ | ⏳ | ⏳ | ⏳ | ⏳ | Pending |
| macOS 14 Sonoma | arm64 | _TBD_ | _TBD_ | ⏳ | ⏳ | ⏳ | ⏳ | Pending |
| macOS 14 Sonoma | x64 (Rosetta/Intel) | _TBD_ | _TBD_ | ⏳ | ⏳ | ⏳ | ⏳ | Pending |

## Benodigde evidence per rij
1. Screenshot of log dat plugin succesvol laadt.
2. Screenshot van menu-items zichtbaar en klikbaar.
3. Pad naar gegenereerde preview PDF.
4. Pad naar run-bundle directory (plan/manifest/audit/placement JS/preflight/proof).
5. Eventuele crashlogs of SDK-waarschuwingen.

## Release-gate
Een release is pas **GO** wanneer alle doelplatform-rijen minimaal `Plugin load + Menu actions + Preset preview + Preset run bundle` op **Pass** staan.
