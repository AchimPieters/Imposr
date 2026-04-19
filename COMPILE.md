# Compile guide (Windows, macOS, Linux)

Deze handleiding legt **stap voor stap** uit hoe je Imposr bouwt.
Ik ga uit van een beginner (“noob”), dus je krijgt:
- wat je nodig hebt,
- welke commando’s je precies kopieert,
- en wat je ongeveer als resultaat mag verwachten.

> Belangrijk: de Acrobat plug-in kun je alleen bouwen als je de Adobe Acrobat SDK lokaal hebt en `ACROBAT_SDK_DIR` instelt.

---

## 0) Alles automatisch laten doen (aanrader)

Wil je zo min mogelijk handmatig doen? Gebruik dan de platformscripts:

- **Windows (PowerShell):** `.\scripts\build_installer_windows.ps1`
- **macOS (Terminal):** `./scripts/build_installer_macos.sh`
- **Linux (Terminal):** `./scripts/build_installer_linux.sh`

Wat deze scripts doen:
1. Controleren of basissoftware/tools aanwezig zijn.
2. Missende tools proberen te installeren (via de package manager van het platform).
3. Daarna automatisch een packaging build uitvoeren.
4. Tot slot installers/packages genereren met CPack.

### 0.1 Snel starten (copy/paste)

#### Windows (PowerShell)

```powershell
.\scripts\build_installer_windows.ps1
```

#### macOS (Terminal)

```bash
chmod +x ./scripts/build_installer_macos.sh
./scripts/build_installer_macos.sh
```

#### Linux (Terminal)

```bash
chmod +x ./scripts/build_installer_linux.sh
./scripts/build_installer_linux.sh
```

### 0.2 Optionele parameters per script

#### Windows (`build_installer_windows.ps1`)

```powershell
.\scripts\build_installer_windows.ps1 `
  -BuildDir build-package `
  -BuildType Release `
  -Generator "Visual Studio 17 2022" `
  -Arch x64
```

Plugin build:

```powershell
.\scripts\build_installer_windows.ps1 -WithPlugin -AcrobatSdkDir "C:\path\to\AcrobatSDK"
```

#### macOS (`build_installer_macos.sh`)

```bash
./scripts/build_installer_macos.sh --build-dir build-package --jobs 8
```

Plugin build:

```bash
./scripts/build_installer_macos.sh --with-plugin --acrobat-sdk-dir "/path/to/AcrobatSDK"
```

#### Linux (`build_installer_linux.sh`)

```bash
./scripts/build_installer_linux.sh --build-dir build-package --jobs 8
```

> Linux plug-in flow wordt niet ondersteund in deze repository; `--with-plugin` wordt genegeerd met waarschuwing.

### 0.3 Wat krijg je als output?

Na een succesvolle run vind je artifacts in de gekozen buildmap (standaard `build-package/`), bijvoorbeeld:

- `.zip` / `.exe` (Windows, afhankelijk van CPack generators)
- `.dmg` / `.tar.gz` (macOS, afhankelijk van CPack generators)
- `.tar.gz` / `.deb` (Linux, afhankelijk van beschikbare tooling)

> Let op: installaties kunnen adminrechten vragen (bijv. `sudo` op Linux/macOS of elevated PowerShell op Windows).
> Let op 2: sommige installers (bijv. NSIS op Windows of DEB op Linux) worden alleen gemaakt als de benodigde packaging tools beschikbaar zijn.

---

## 1) Eerst: wat moet je geïnstalleerd hebben?

Installeer deze onderdelen eerst:

1. **CMake 3.21 of nieuwer**
2. **Een C++17 compiler**
3. **Python 3** (voor tooling en checks)

### Platform-specifiek

- **Windows**: Visual Studio 2022 (MSVC) *(aanrader)* of Ninja + MSVC toolchain
- **macOS**: Xcode Command Line Tools (clang)
- **Linux**: GCC of Clang + make/ninja

### Voor de Acrobat plug-in (alleen Windows/macOS)

- Download en plaats de **Adobe Acrobat SDK** lokaal.
- Onthoud het pad, want dat zet je straks in `ACROBAT_SDK_DIR`.

---

## 2) Snelle build (zonder plug-in) — werkt op alle platformen

Dit is de beste eerste test om te checken of je omgeving goed staat.

### Stap 1: open een terminal in de projectmap

Ga naar de map waar deze repository staat (de map met `CMakeLists.txt`).

### Stap 2: configureer de build

```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF
```

### Stap 3: bouw het project

```bash
cmake --build build -j
```

### Stap 4: run de tests

```bash
ctest --test-dir build --output-on-failure
```

### Wat wordt gebouwd?

- `aimp_core`
- `imposr_cli`
- tests (`aimp_planner_tests`)

---

## 3) Windows (PowerShell)

## 3.1 Core/CLI/tests bouwen (zonder plug-in)

### Stap 1: configureer Visual Studio build

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DAIMP_BUILD_PLUGIN=OFF
```

### Stap 2: bouw Release

```powershell
cmake --build build-win --config Release
```

### Stap 3: run tests

```powershell
ctest --test-dir build-win --build-config Release --output-on-failure
```

## 3.2 Acrobat plug-in bouwen op Windows

> Alleen doen als je de Acrobat SDK lokaal hebt.

### Stap 1: zet je SDK-pad

```powershell
$env:ACROBAT_SDK_DIR="C:\path\to\AcrobatSDK"
```

### Stap 2: configureer plug-in build

```powershell
cmake -S . -B build-win-plugin -G "Visual Studio 17 2022" -A x64 -DAIMP_BUILD_PLUGIN=ON -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON
```

### Stap 3: bouw Release

```powershell
cmake --build build-win-plugin --config Release
```

### Verwachte output

- `build-win-plugin/Release/AcrobatImpositionPlugin.dll`

---

## 4) macOS

## 4.1 Core/CLI/tests bouwen (zonder plug-in)

### Stap 1: configureer build

```bash
cmake -S . -B build-mac -DAIMP_BUILD_PLUGIN=OFF
```

### Stap 2: bouw

```bash
cmake --build build-mac -j
```

### Stap 3: run tests

```bash
ctest --test-dir build-mac --output-on-failure
```

## 4.2 Acrobat plug-in bouwen op macOS

> Alleen doen als je de Acrobat SDK lokaal hebt.

### Stap 1: zet je SDK-pad

```bash
export ACROBAT_SDK_DIR="/path/to/AcrobatSDK"
```

### Stap 2: configureer plug-in build

```bash
cmake -S . -B build-mac-plugin -DAIMP_BUILD_PLUGIN=ON -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON
```

### Stap 3: bouw

```bash
cmake --build build-mac-plugin -j
```

### Verwachte output

- `build-mac-plugin/libAcrobatImpositionPlugin.dylib`

> Let op: voor distributie op macOS heb je daarna meestal nog signing/notarization nodig.

---

## 5) Linux

## 5.1 Core/CLI/tests bouwen (zonder plug-in)

### Stap 1: configureer build

```bash
cmake -S . -B build-linux -DAIMP_BUILD_PLUGIN=OFF
```

### Stap 2: bouw

```bash
cmake --build build-linux -j
```

### Stap 3: run tests

```bash
ctest --test-dir build-linux --output-on-failure
```

## 5.2 Acrobat plug-in op Linux?

Kort antwoord: **nee, niet in deze repo-flow**.

- Linux is hier bedoeld voor core/CLI/tests/CI/packaging tooling.
- De Acrobat plug-in host-flow is bedoeld voor Windows/macOS.

---

## 6) Strict build (aanbevolen voor release branches)

Gebruik dit als je extra streng wilt controleren (warnings, sanitizers, debug checks).

### Stap 1: configureer strict build

```bash
cmake -S . -B build-strict \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_ENABLE_WARNINGS=ON \
  -DAIMP_WARNINGS_AS_ERRORS=ON \
  -DAIMP_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
```

### Stap 2: bouw

```bash
cmake --build build-strict -j
```

### Stap 3: run tests

```bash
ctest --test-dir build-strict --output-on-failure
```

---

## 7) Packaging/installers

Gebruik deze stappen als je installer/packages wilt maken.

### Stap 1: configureer packaging build

```bash
cmake -S . -B build-package \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_BUILD_CLI=ON \
  -DAIMP_BUILD_TESTS=OFF \
  -DAIMP_ENABLE_PACKAGING=ON \
  -DCMAKE_BUILD_TYPE=Release
```

### Stap 2: bouw

```bash
cmake --build build-package -j
```

### Stap 3: maak package(s)

```bash
cpack --config build-package/CPackConfig.cmake -C Release
```

Relevante CI-workflows:
- `.github/workflows/installers.yml`
- `.github/workflows/release-readiness.yml`
- `.github/workflows/host-runtime-gate.yml`

> Je kunt deze stap ook in één keer doen met de scripts uit sectie **0**.

---

## 8) Troubleshooting (als iets fout gaat)

- **CMake command not found**
  - CMake is niet geïnstalleerd of staat niet in je PATH.
- **Compiler errors direct bij configure/build**
  - Controleer of je platform-toolchain correct is geïnstalleerd.
- **Plug-in build faalt op `ACROBAT_SDK_DIR`**
  - Variabele niet gezet of pad klopt niet.
- **Tests falen**
  - Gebruik `ctest --output-on-failure` (zoals hierboven) om exacte fout te zien.

Tip voor beginners: begin altijd met sectie **2** (snelle build zonder plug-in). Als dat werkt, pas daarna plugin/packaging proberen.
