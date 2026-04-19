# Compile guide (Windows, macOS, Linux)

Dit document beschrijft hoe je Imposr bouwt op elk platform, inclusief de Acrobat plug-in build op Windows/macOS.

> Belangrijk: de Acrobat plug-in target kan alleen gebouwd worden als `ACROBAT_SDK_DIR` is gezet.

---

## 1) Vereisten

- CMake 3.21+
- C++17 compiler
- Python 3 (voor tooling en sommige gates)

### Platform toolchains

- **Windows**: Visual Studio 2022 (MSVC) of Ninja + MSVC toolchain
- **macOS**: Xcode Command Line Tools (clang)
- **Linux**: GCC/Clang + make/ninja

### Acrobat plug-in specifiek

- Alleen zinvol op **Windows/macOS** met lokale Adobe Acrobat SDK.
- Zet `ACROBAT_SDK_DIR` naar je SDK-pad.

---

## 2) Snelle build zonder plug-in (alle platformen)

```bash
cmake -S . -B build -DAIMP_BUILD_PLUGIN=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Dit bouwt:
- `aimp_core`
- `imposr_cli`
- tests (`aimp_planner_tests`)

---

## 3) Windows build

### 3.1 Core/CLI/tests

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DAIMP_BUILD_PLUGIN=OFF
cmake --build build-win --config Release
ctest --test-dir build-win --build-config Release --output-on-failure
```

### 3.2 Acrobat plug-in (Windows)

```powershell
$env:ACROBAT_SDK_DIR="C:\path\to\AcrobatSDK"
cmake -S . -B build-win-plugin -G "Visual Studio 17 2022" -A x64 -DAIMP_BUILD_PLUGIN=ON -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON
cmake --build build-win-plugin --config Release
```

Output (typisch):
- `build-win-plugin/Release/AcrobatImpositionPlugin.dll`

---

## 4) macOS build

### 4.1 Core/CLI/tests

```bash
cmake -S . -B build-mac -DAIMP_BUILD_PLUGIN=OFF
cmake --build build-mac -j
ctest --test-dir build-mac --output-on-failure
```

### 4.2 Acrobat plug-in (macOS)

```bash
export ACROBAT_SDK_DIR="/path/to/AcrobatSDK"
cmake -S . -B build-mac-plugin -DAIMP_BUILD_PLUGIN=ON -DAIMP_ENABLE_EXPERIMENTAL_SDK_COMPOSER=ON
cmake --build build-mac-plugin -j
```

Output (typisch):
- `build-mac-plugin/libAcrobatImpositionPlugin.dylib`

> Voor distributie op macOS zijn signing/notarization stappen aanvullend nodig.

---

## 5) Linux build

### 5.1 Core/CLI/tests

```bash
cmake -S . -B build-linux -DAIMP_BUILD_PLUGIN=OFF
cmake --build build-linux -j
ctest --test-dir build-linux --output-on-failure
```

### 5.2 Acrobat plug-in op Linux?

- Acrobat plug-in host-flow is niet bedoeld voor Linux in deze repository.
- Linux wordt gebruikt voor core/CLI/testing/CI en packaging tooling.

---

## 6) Strict build (aanbevolen voor release branches)

```bash
cmake -S . -B build-strict \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_ENABLE_WARNINGS=ON \
  -DAIMP_WARNINGS_AS_ERRORS=ON \
  -DAIMP_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-strict -j
ctest --test-dir build-strict --output-on-failure
```

---

## 7) Packaging/installers

```bash
cmake -S . -B build-package \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_BUILD_CLI=ON \
  -DAIMP_BUILD_TESTS=OFF \
  -DAIMP_ENABLE_PACKAGING=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-package -j
cpack --config build-package/CPackConfig.cmake -C Release
```

CI workflows:
- `.github/workflows/installers.yml`
- `.github/workflows/release-readiness.yml`
- `.github/workflows/host-runtime-gate.yml`
