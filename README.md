# Imposr

Imposr is een Adobe Acrobat plug-in voor professionele boekopmaak (imposition). Nadat je hem hebt geïnstalleerd verschijnt er een **Imposr**-menu in Acrobat waarmee je 2-up, N-up, boekjes, stap-en-herhaal en veel meer kunt uitvoeren — zonder de terminal.

> **Huidige status:** planner, proof-composer, presets, audits en batch-flow zijn bruikbaar. De echte Acrobat-pagina-compositie (bronpagina's daadwerkelijk plaatsen op output-sheets) is nog in ontwikkeling en vereist de Adobe Acrobat SDK.

---

## Installer bouwen — stap voor stap

> **Heb je nog nooit iets gecompileerd? Begin hier.** Het script regelt alles: Homebrew, CMake, create-dmg, Pillow en de rest. Jij hoeft alleen de Acrobat SDK te downloaden.

### Wat heb je nodig?

| Vereiste | Details |
|---|---|
| Mac | macOS 13 (Ventura) of nieuwer |
| Adobe Acrobat | DC of nieuwer, geïnstalleerd in `/Applications` |
| Adobe Acrobat SDK | Gratis download via Adobe Developer Portal (zie stap 1) |
| Internetverbinding | Het script installeert ontbrekende tools automatisch |

---

### Stap 1 — Download de Adobe Acrobat SDK

1. Ga naar **[developer.adobe.com/console/servicesandapis](https://developer.adobe.com/console/servicesandapis)** en log in met je Adobe-account (gratis registratie).
2. Kies bij **Acrobat SDK** de versie voor **Macintosh** en download het ZIP-bestand (naam lijkt op `AcrobatSDK_DC.zip`).
3. Pak het ZIP-bestand uit. Je krijgt een map met daarin o.a. een `API/`-submap. Zet die map neer op een logische plek, bijvoorbeeld:
   ```
   ~/Downloads/AcrobatSDK/
   ~/Adobe/AcrobatSDK/
   ~/Desktop/AcrobatSDK/
   ```
   Het script herkent deze mappen automatisch.

---

### Stap 2 — Open Terminal en ga naar de projectmap

```bash
cd ~/Desktop/Imposr
```

---

### Stap 3 — Voer het build-script uit

**Variant A — SDK automatisch zoeken** (script zoekt zelf in je HOME en Downloads):

```bash
./scripts/build_installer_macos.sh
```

**Variant B — SDK-pad zelf opgeven** (aanbevolen als je weet waar de SDK staat):

```bash
./scripts/build_installer_macos.sh --acrobat-sdk-dir ~/Downloads/AcrobatSDK
```

Het script doet nu automatisch het volgende:

1. Controleert Xcode Command Line Tools en installeert ze als ze ontbreken
2. Installeert Homebrew als het er niet is
3. Installeert CMake, create-dmg en Python Pillow via Homebrew/pip
4. Compileert de Acrobat plug-in (`AcrobatImpositionPlugin.api`)
5. Bouwt de CLI-tool
6. Maakt de installers aan in `dist/`

Het hele proces duurt de eerste keer 5–15 minuten (afhankelijk van je verbinding en machine).

---

### Stap 4 — Installeer de plug-in

Wanneer het script klaar is zie je in `dist/`:

```
dist/
  imposr-0.1.0.dmg                       ← installeer via dubbelklik (aanbevolen)
  Imposr-Acrobat-Plugin-0.1.0.pkg        ← automatische installatie via macOS-installer
  Imposr-0.1.0.pkg                        ← installeert alleen de CLI-tool
```

**Methode 1 — DMG (aanbevolen, zelfde als QI+):**

```bash
open dist/imposr-0.1.0.dmg
```

Er opent een Finder-venster met **Install Imposr**. Dubbelklik erop. De installer biedt nu drie opties:

- **Install a Imposr plug-in** — installeert de plug-in in de gekozen Acrobat-installatie.
- **Uninstall a Imposr plug-in** — verwijdert een bestaande installatie.
- **Open an Acrobat plug-in folder in the Finder** — opent direct de plug-inmap voor handmatige controle.

Kies **Install** en klik **Continue**. In de volgende stap kun je Acrobat desnoods via **Browse…** handmatig kiezen.

**Methode 2 — pkg (volledig automatisch, geen klikken):**

```bash
sudo installer -pkg dist/Imposr-Acrobat-Plugin-0.1.0.pkg -target /
```

**Terminal alternatief (zelfde drie acties als GUI-installer):**

```bash
# Installeren
./scripts/install_acrobat_plugin_macos.sh --action install

# Deïnstalleren
./scripts/install_acrobat_plugin_macos.sh --action uninstall

# Plug-in map openen in Finder
./scripts/install_acrobat_plugin_macos.sh --action open-folder

# Richt expliciet op een andere Acrobat.app
./scripts/install_acrobat_plugin_macos.sh --action install --acrobat-app "/Applications/Adobe Acrobat/Adobe Acrobat.app"

# Alleen tonen wat er zou gebeuren (geen wijzigingen)
./scripts/install_acrobat_plugin_macos.sh --action install --dry-run

# Toon alle resolved install/uninstall paden
./scripts/install_acrobat_plugin_macos.sh --action list-targets

# Toon actuele installatiestatus op bekende paden
./scripts/install_acrobat_plugin_macos.sh --action status

# JSON output voor automation
./scripts/install_acrobat_plugin_macos.sh --action status --json

# CI gate: faal als plug-in niet geïnstalleerd is
./scripts/install_acrobat_plugin_macos.sh --action status --require-installed
```

`--action uninstall` controleert meerdere bekende Acrobat plug-in locaties (user + system) en verwijdert gevonden `AcrobatImpositionPlugin.api` bestanden.

---

### Stap 5 — Controleer in Acrobat

1. Sluit Acrobat volledig af en start het opnieuw.
2. Kijk in het **Plug-ins**-menu → je ziet **Imposr → Imposition control panel**.

---

### Problemen?

| Fout | Oplossing |
|---|---|
| `"Install Imposr" kan niet worden geopend` | Rechtermuisklik → Openen → Toch openen, of: `xattr -dr com.apple.quarantine "/Volumes/Install Imposr/Install Imposr.app"` |
| SDK niet gevonden | Geef `--acrobat-sdk-dir` mee met het exacte pad naar de uitgepakte SDK-map |
| Acrobat niet gevonden bij installatie | Controleer of Acrobat in `/Applications` staat; kies anders **Browse** in het installatie-dialoog |
| Plugin laadt niet in Acrobat | Controleer of je Acrobat volledig hebt afgesloten en opnieuw hebt gestart |

---

## Wat dit project kan

- 2-up, N-up, booklet, step-repeat, tile en manual sequence planning
- Reverse/even/odd filtering, blank padding, page sequence overrides
- **TrimShift** — creep-correctie voor saddle-stitch boekjes
- **VariableData** — CSV-based variable data merge met `{{key}}` templates
- **Shuffle** — signature shuffle, even/odd split, interleave
- **SplitMerge** — range-based plan split en merge
- **PageTools** — duplicate, delete, move, rotate, insert blank pages
- **StickOn** — text, Bates-nummering, paginanummer, PDF-stempel, maskeertape, peel-off
- **PrinterMarks** — registratiemerken, snijmerken, bleedmerken, kleurenbalk, job-infostrip
- **AdjustPages** — scale, crop, extend, scale-to-fit, scale-to-fill
- **TilePages** — tilegrootte-berekening voor grootformaat tiling
- **Bleed** — bleed zone generatie (mirror/scale/extend/solidcolor)
- **PdfX** — PDF/X metadata detectie en compliance validatie (X-1a, X-3, X-4, X-5)
- **Annotations** — annotatie verwerking (preserve/discard/flatten)
- JSON plan output, XML audit output en preflight/quality-gate rapportage
- Proof PDF (geometrie/labels/trim/bleed visualisatie) zonder SDK afhankelijkheid
- Preset load/save en batch-orchestratie via CSV
- Sequence automation (sequence run/list)
- Acrobat plug-in menu met validate/preview/run-bundle acties

---

## Documentatie

- [User Guide](docs/USER_GUIDE.md) — alle CLI-modes met voorbeelden
- [CLI Reference](docs/CLI_REFERENCE.md) — compleet overzicht van alle opties
- [Developer Guide](docs/DEVELOPER_GUIDE.md) — module-architectuur, modules toevoegen
- [Roadmap](docs/ROADMAP.md) — release planning

---

## Snel bouwen zonder Acrobat SDK (alleen CLI + tests)

```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### CLI voorbeeld

```bash
./build/imposr_cli two-up \
  --pages 8 \
  --sheet-width 1190.55 \
  --sheet-height 841.89 \
  --output-dir ./out \
  --output-stem demo \
  --summary 1 \
  --validate 1 \
  --fail-on-quality-gate 1
```

---

## Wat nog niet productie-klaar is

- Volledige native Acrobat page-content compositie (bronpagina's echt plaatsen op output sheets)
- Volwaardige persistente plug-in UI/panel
- Volledige host-validatie matrix (Windows 11 + macOS) met ingevulde pass-evidence
- Definitieve code signing, notarization en release-flow

Zie voor detailstatus: `docs/ROADMAP.md`, `docs/IMPLEMENTATIE_AUDIT_2026-04-17.md`, `docs/COMPATIBILITY_MATRIX_2026-04-18.md`.

---

## Licentie

Zie `LICENSE`.
