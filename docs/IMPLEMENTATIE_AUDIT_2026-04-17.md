# Uitgebreide implementatie-audit (status op 17 april 2026)

## Doel van deze audit

Deze audit vergelijkt de eerder voorgestelde Acrobat-imposition roadmap met wat in deze repository **daadwerkelijk** is geïmplementeerd. Focus: wat is al gedaan, wat mist nog, en wat is nodig om expliciet te landen op **Windows 11 + macOS**.

---

## 1) Wat is aantoonbaar al gedaan

### 1.1 Planner-kern (MVP kernlogica)
Geïmplementeerd en testbaar buiten Acrobat:
- 2-up planning (`TwoUpPlanner`).
- N-up planning (`NUpPlanner`).
- Booklet planning inclusief signature-normalisatie (`BookletPlanner`).
- Step-and-repeat planning (`StepAndRepeatPlanner`).
- Page filter (all/even/odd), reverse order en blank padding.
- Fit-to-slot + auto-rotate metadata in placement geometrie.
- Inspector helpers (source→placement en placement→source).
- Plan statistieken + validatie helper + human summary.
- JSON export en XML audit export.

### 1.2 PDF compositie (proof/debug)
Geïmplementeerd zonder Acrobat SDK-afhankelijkheid:
- Genereren van geldige output-PDF met sheet/slot-visualisatie.
- Header/footer labels, sheetnummering, Bates-prefix/startwaarde.
- Trim-marks en bleed-box visualisatie voor prepress-proofing.

Belangrijk: dit is nu nog een **proof composer** en plaatst geen echte bronpagina-content in output vellen.

### 1.3 Workflow/presets + CLI
Geïmplementeerd:
- CLI om plannen te bouwen voor 2-up, N-up, booklet en step-repeat.
- Presets laden/opslaan.
- JSON / XML audit / PDF proof output.
- Validatie-output, inspectie-opties en smoke testpad.

### 1.4 Acrobat plug-in skeleton
Geïmplementeerd als host-integratie basis:
- Plug-in target in CMake (conditioneel op `ACROBAT_SDK_DIR`).
- Menu-registratie met demo-actie en report-actie.
- Actieve documenttoegang (`AVAppGetActiveDoc`, `AVDocGetPDDoc`) en page-count uitlezen.
- Export van proof-PDF naar temp map vanuit plug-in menuactie.

Belangrijk: dit is nog **geen productieklare content-imposition in Acrobat**.

### 1.5 Kwaliteit/controle
Aantoonbaar uitgevoerd in deze omgeving:
- CMake configure/build voor core+CLI+tests (plug-in uit).
- Unit/integratietests groen.
- CLI smoke test groen.

---

## 2) Gap-analyse t.o.v. het oorspronkelijke productdoel

## 2.1 Featurestatus (samenvatting)

### Reeds aanwezig (volledig of functioneel MVP-niveau)
- Booklet / N-up / Step & Repeat planning.
- Reverse/even-odd/padding logica.
- JSON/XML audit trail basis.
- Nummering/tekstlabels in proof-PDF (debug/doelcontrole).

### Gedeeltelijk aanwezig
- Acrobat integratie: skeleton + menu’s aanwezig, maar nog geen volwaardige UI/panel.
- Output-PDF genereren: ja, maar nog proof-mode i.p.v. echte bronpagina-plaatsing.
- Inspectie/reverse mapping: aanwezig op planniveau; nog niet gekoppeld aan volledige Acrobat-job lifecycle.

### Nog niet aanwezig (belangrijkste open punten)
- Echte compositie van bronpagina’s naar destination sheets in Acrobat SDK APIs.
- Split/merge/insert als productflow in plug-in UI.
- Manual imposition editor (drag/drop/shuffle assistent).
- Crop/trim marks als production-grade output (nu debugvisualisatie).
- Creep/trim shift, overlays, CSV variable data, PDF/X validatie-flow.
- Preset/workflow UX in plug-in panel.
- Server/hot-folder/CLI-companion buiten huidige lokale CLI.

---

## 3) Specifiek voor jouw eis: Windows 11 + macOS

## 3.1 Huidige stand
- CMake bevat platformdefines voor Windows (`WIN_PLATFORM`) en macOS (`MAC_PLATFORM`).
- Buildpad voor plug-in is aanwezig, maar **alleen actief met lokale Acrobat SDK-installatie** (`ACROBAT_SDK_DIR`).
- Er is in deze omgeving geen echte Acrobat SDK + host runtime smoke test uitgevoerd voor beide OS’en.

Conclusie: codebasis is voorbereid op cross-platform, maar **platformvalidatie is nog niet afgerond**.

## 3.2 Wat nog moet gebeuren voor “werkt op Windows 11 en macOS”

### Must-do (release blocker)
1. Windows 11: plug-in build met doel-SDK, laden in Acrobat Pro, menu-acties smoke testen.
2. macOS: zelfde smoke test met juiste toolchain/signing/notarization-vereisten.
3. Compatibiliteitsmatrix bijhouden (Acrobat versie × OS × CPU architectuur × SDK versie).
4. 64-bit-only release policy afdwingen in build/release scripts.

### Sterk aanbevolen direct daarna
5. CI voor pure core (Linux/macOS/Windows) en handmatige Acrobat host smoke checklist per release.
6. Platformabstractie voor pad-/temp-/host glue consistent houden.

---

## 4) Concreet restantwerk in fases

### Fase A — Host bring-up (kortste kritieke pad)
- Productiebuild van plug-in op Windows 11 + macOS.
- Gestandaardiseerde smoke testprocedure vastleggen.
- Foutmeldingen en logging in hostlaag aanscherpen.

### Fase B — Echte compositie
- Bronpagina-content importeren/plaatsen via Acrobat SDK transformaties.
- Planner-output 1-op-1 koppelen aan SDK placement pipeline.
- Outputdocument openen/saven binnen Acrobat workflow.

### Fase C — MVP UX
- Basis control panel (modus, sheet, preset, run).
- Output naming + bestemming + validatiefeedback.
- Preset lifecycle (save/load/update/delete) vanuit UI.

### Fase D — Prepress en uitbreiding
- Productiewaardige marks/bleed/creep.
- Overlay en CSV-variabele data.
- PDF/X validatieprofielen + rapportage.

---

## 5) Eindconclusie

Er is **substantieel meer gedaan dan alleen een idee of design**: de planner-core, proof composer, CLI-workflow, presets, audit-export en plug-in skeleton zijn aanwezig en geverifieerd via tests/build.

Tegelijk is het belangrijkste open gat voor jouw einddoel nog steeds:
- echte Acrobat-contentcompositie,
- volwaardige plug-in UX,
- én harde platformvalidatie op **Windows 11 + macOS** met de echte SDK/runtime.

Kort gezegd: de fundering is goed, maar het product is nog in de overgang van “technische prototypekern” naar “productierijpe Acrobat plug-in”.
