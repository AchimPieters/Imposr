#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-package"
BUILD_TYPE="Release"
WITH_PLUGIN="true"
ACROBAT_SDK_DIR_VALUE="${ACROBAT_SDK_DIR:-}"
ACROBAT_PLUGIN_INSTALL_DIR=""
JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"
PLUGIN_VERSION="0.1.0"
REQUIRE_PLUGIN="false"

usage() {
  cat <<USAGE
Gebruik:
  ./scripts/build_installer_macos.sh [--without-plugin] [--with-plugin] [--require-plugin] [--acrobat-sdk-dir PAD] [--acrobat-plugin-install-dir PAD] [--build-dir MAP] [--jobs N]

Standaard:
  - Bouwt installer/packages via CPack (DragNDrop/TGZ)
  - Probeert standaard MET Acrobat plug-in te bouwen
  - Valt automatisch terug naar CLI-only build als SDK ontbreekt (geen GUI prompts)
  - Genereert extra .pkg installer voor automatische plugin-installatie in Acrobat
USAGE
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

detect_acrobat_plugin_install_dir() {
  if [[ -n "$ACROBAT_PLUGIN_INSTALL_DIR" ]]; then
    return
  fi

  local candidates=(
    "/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plug-ins"
    "/Applications/Adobe Acrobat/Adobe Acrobat.app/Contents/Plug-ins"
  )

  local candidate
  local app_bundle
  for candidate in "${candidates[@]}"; do
    app_bundle="${candidate%/Contents/Plug-ins}"
    if [[ -d "$app_bundle" ]]; then
      ACROBAT_PLUGIN_INSTALL_DIR="$candidate"
      return
    fi
  done

  ACROBAT_PLUGIN_INSTALL_DIR="${candidates[0]}"
}

auto_extract_acrobat_sdk_archive() {
  local archives=()
  while IFS= read -r archive; do
    archives+=("$archive")
  done < <(find "$HOME/Downloads" -maxdepth 2 -type f \( -iname "*acrobat*sdk*.zip" -o -iname "*adobe*acrobat*.zip" \) 2>/dev/null)

  if [[ ${#archives[@]} -eq 0 ]]; then
    return
  fi

  local extract_root="$HOME/Adobe/AcrobatSDK-auto"
  mkdir -p "$extract_root"

  local archive
  for archive in "${archives[@]}"; do
    echo "[INFO] Probeer SDK archive uit te pakken: $archive"
    unzip -qo "$archive" -d "$extract_root" || true
  done

  local candidate
  while IFS= read -r candidate; do
    if [[ -d "$candidate/API" ]]; then
      ACROBAT_SDK_DIR_VALUE="$candidate"
      echo "[OK] Acrobat SDK automatisch gevonden na extract: $candidate"
      return
    fi
  done < <(find "$extract_root" -maxdepth 6 -type d \( -iname "*acrobat*sdk*" -o -iname "*adobe*acrobat*sdk*" -o -iname "*sdk*" \) 2>/dev/null)
}

auto_detect_acrobat_sdk_dir() {
  if [[ -n "$ACROBAT_SDK_DIR_VALUE" ]]; then
    return
  fi

  local candidates=(
    "$HOME/Adobe/AcrobatSDK"
    "$HOME/Downloads/AcrobatSDK"
    "$HOME/Documents/AcrobatSDK"
    "$HOME/Desktop/AcrobatSDK"
    "/Applications/Adobe Acrobat SDK"
    "/opt/acrobat-sdk"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -d "$candidate" && -d "$candidate/API" ]]; then
      ACROBAT_SDK_DIR_VALUE="$candidate"
      return
    fi
  done

  while IFS= read -r candidate; do
    if [[ -d "$candidate/API" ]]; then
      ACROBAT_SDK_DIR_VALUE="$candidate"
      return
    fi
  done < <(find "$HOME" -maxdepth 5 -type d \( -iname "*acrobat*sdk*" -o -iname "*adobe*acrobat*sdk*" \) 2>/dev/null)

  auto_extract_acrobat_sdk_archive
}


ensure_xcode_cli() {
  if xcode-select -p >/dev/null 2>&1; then
    echo "[OK] Xcode Command Line Tools gevonden"
    return
  fi
  echo "[INFO] Xcode Command Line Tools ontbreken; installatie wordt gestart"
  xcode-select --install || true
  echo "[ERROR] Rond de GUI-installatie van Xcode Command Line Tools af en run dit script opnieuw." >&2
  exit 1
}

ensure_brew() {
  if command_exists brew; then
    echo "[OK] Homebrew gevonden"
    return
  fi

  echo "[INFO] Homebrew ontbreekt; installatie wordt gestart"
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

  if [[ -x /opt/homebrew/bin/brew ]]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
  elif [[ -x /usr/local/bin/brew ]]; then
    eval "$(/usr/local/bin/brew shellenv)"
  fi

  if ! command_exists brew; then
    echo "[ERROR] Homebrew installatie lijkt mislukt." >&2
    exit 1
  fi
}

ensure_brew_tool() {
  local tool="$1"
  local formula="$2"

  if command_exists "$tool"; then
    echo "[OK] $tool gevonden"
    return
  fi

  echo "[INFO] $tool ontbreekt; installeer $formula via Homebrew"
  brew install "$formula"

  if ! command_exists "$tool"; then
    echo "[ERROR] Installatie van $tool lijkt mislukt." >&2
    exit 1
  fi
}

create_auto_plugin_pkg() {
  local plugin_src="$1"
  local pkg_root="$BUILD_DIR/pkgroot"
  local pkg_scripts="$BUILD_DIR/pkgscripts"
  local pkg_out="$BUILD_DIR/Imposr-Acrobat-Plugin-${PLUGIN_VERSION}.pkg"

  rm -rf "$pkg_root" "$pkg_scripts"
  mkdir -p "$pkg_root/usr/local/imposr/lib" "$pkg_root/usr/local/imposr/bin" "$pkg_scripts"
  cp -f "$plugin_src" "$pkg_root/usr/local/imposr/lib/libAcrobatImpositionPlugin.dylib"
  cp -f "scripts/uninstall_acrobat_plugin_macos.sh" "$pkg_root/usr/local/imposr/bin/uninstall_acrobat_plugin_macos.sh"
  chmod +x "$pkg_root/usr/local/imposr/bin/uninstall_acrobat_plugin_macos.sh"

  cat > "$pkg_scripts/postinstall" <<'POSTINSTALL'
#!/bin/bash
set -euo pipefail

PLUGIN_SRC="/usr/local/imposr/lib/libAcrobatImpositionPlugin.dylib"
TARGETS=(
  "/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plug-ins/AcrobatImpositionPlugin.dylib"
  "/Applications/Adobe Acrobat/Adobe Acrobat.app/Contents/Plug-ins/AcrobatImpositionPlugin.dylib"
)

if [[ ! -f "$PLUGIN_SRC" ]]; then
  echo "[ERROR] Plugin source missing: $PLUGIN_SRC" >&2
  exit 1
fi

installed="false"
for target in "${TARGETS[@]}"; do
  plugin_dir="$(dirname "$target")"
  app_bundle="${plugin_dir%/Contents/Plug-ins}"
  if [[ -d "$app_bundle" ]]; then
    mkdir -p "$plugin_dir"
    cp -f "$PLUGIN_SRC" "$target"
    xattr -dr com.apple.quarantine "$target" || true
    codesign --force --sign - "$target" || true
    echo "[OK] Plugin deployed to: $target"
    installed="true"
  fi
done

if [[ "$installed" != "true" ]]; then
  echo "[WARN] No Acrobat app bundle found; plugin copied to /usr/local/imposr/lib only."
fi

exit 0
POSTINSTALL

  chmod +x "$pkg_scripts/postinstall"

  pkgbuild \
    --root "$pkg_root" \
    --scripts "$pkg_scripts" \
    --identifier "com.imposr.acrobat.plugin" \
    --version "$PLUGIN_VERSION" \
    "$pkg_out"

  echo "[OK] Automatische plugin-installer gemaakt: $pkg_out"
  echo "[OK] Uninstaller in pkg payload: /usr/local/imposr/bin/uninstall_acrobat_plugin_macos.sh"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-plugin)
      WITH_PLUGIN="true"
      shift
      ;;
    --without-plugin)
      WITH_PLUGIN="false"
      shift
      ;;
    --require-plugin)
      REQUIRE_PLUGIN="true"
      shift
      ;;
    --acrobat-sdk-dir)
      ACROBAT_SDK_DIR_VALUE="${2:-}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="${2:-build-package}"
      shift 2
      ;;
    --acrobat-plugin-install-dir)
      ACROBAT_PLUGIN_INSTALL_DIR="${2:-}"
      shift 2
      ;;
    --jobs)
      JOBS="${2:-4}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Onbekende optie: $1" >&2
      usage
      exit 1
      ;;
  esac
done

detect_acrobat_plugin_install_dir

if [[ ! -d "${ACROBAT_PLUGIN_INSTALL_DIR%/Contents/Plug-ins}" ]]; then
  echo "[WARN] Acrobat app bundle niet gevonden op verwachte locatie."
  echo "[WARN] Gebruik eventueel --acrobat-plugin-install-dir om expliciet pad te zetten."
fi

echo "[1/4] Controleer/verzamel vereiste tools"
ensure_xcode_cli
ensure_brew
ensure_brew_tool cmake cmake
ensure_brew_tool python3 python
if ! command_exists pkgbuild; then
  echo "[ERROR] pkgbuild niet gevonden. Installeer Xcode Command Line Tools volledig." >&2
  exit 1
fi

if [[ "$WITH_PLUGIN" == "true" ]]; then
  auto_detect_acrobat_sdk_dir

  if [[ -z "$ACROBAT_SDK_DIR_VALUE" ]]; then
    if [[ "$REQUIRE_PLUGIN" == "true" ]]; then
      echo "[ERROR] Acrobat plugin build vereist Adobe Acrobat SDK." >&2
      echo "[ERROR] Geef --acrobat-sdk-dir op of zet ACROBAT_SDK_DIR. Voorbeeld:" >&2
      echo "        ./scripts/build_installer_macos.sh --acrobat-sdk-dir \"$HOME/Downloads/Adobe Acrobat SDK\"" >&2
      echo "[ERROR] Tip: script scant automatisch HOME + Downloads zip zonder GUI prompts." >&2
      exit 1
    fi

    echo "[WARN] Geen Acrobat SDK gevonden; val terug naar CLI-only build." >&2
    echo "[WARN] Gebruik --require-plugin om hard te falen als plugin verplicht is." >&2
    WITH_PLUGIN="false"
    PLUGIN_FLAG="OFF"
  else
    export ACROBAT_SDK_DIR="$ACROBAT_SDK_DIR_VALUE"
    echo "[OK] Gebruik Acrobat SDK: $ACROBAT_SDK_DIR"
    PLUGIN_FLAG="ON"
  fi

else
  echo "[WARN] Plugin build is uitgeschakeld (--without-plugin)."
  PLUGIN_FLAG="OFF"
fi

echo "[2/4] Configureer packaging build"
cmake -S . -B "$BUILD_DIR" \
  -DAIMP_BUILD_PLUGIN="$PLUGIN_FLAG" \
  -DAIMP_ACROBAT_PLUGIN_INSTALL_DIR="$ACROBAT_PLUGIN_INSTALL_DIR" \
  -DAIMP_BUILD_CLI=ON \
  -DAIMP_BUILD_TESTS=OFF \
  -DAIMP_ENABLE_PACKAGING=ON \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "[3/4] Bouw project"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "[4/4] Genereer installers/packages"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -C "$BUILD_TYPE"

if [[ "$WITH_PLUGIN" == "true" ]]; then
  PLUGIN_SRC="$BUILD_DIR/libAcrobatImpositionPlugin.dylib"
  PLUGIN_DST="$ACROBAT_PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.dylib"
  if [[ ! -f "$PLUGIN_SRC" ]]; then
    echo "[ERROR] Verwachte plugin artifact ontbreekt: $PLUGIN_SRC" >&2
    exit 1
  fi

  sudo mkdir -p "$ACROBAT_PLUGIN_INSTALL_DIR"
  sudo cp -f "$PLUGIN_SRC" "$PLUGIN_DST"
  sudo xattr -dr com.apple.quarantine "$PLUGIN_DST" || true
  sudo codesign --force --sign - "$PLUGIN_DST" || true
  echo "[OK] Plugin gedeployed naar: $PLUGIN_DST"

  create_auto_plugin_pkg "$PLUGIN_SRC"

  UNINSTALL_SCRIPT="$BUILD_DIR/uninstall-plugin.sh"
  cp -f scripts/uninstall_acrobat_plugin_macos.sh "$UNINSTALL_SCRIPT"
  chmod +x "$UNINSTALL_SCRIPT"
  echo "[OK] Deinstaller script gemaakt: $UNINSTALL_SCRIPT"
fi

echo "Klaar. Check artifacts in $BUILD_DIR/"
