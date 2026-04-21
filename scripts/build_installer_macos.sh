#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-package"
BUILD_TYPE="Release"
WITH_PLUGIN="true"
ACROBAT_SDK_DIR_VALUE="${ACROBAT_SDK_DIR:-}"
ACROBAT_PLUGIN_INSTALL_DIR=""
JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

usage() {
  cat <<USAGE
Gebruik:
  ./scripts/build_installer_macos.sh [--without-plugin] [--with-plugin] [--acrobat-sdk-dir PAD] [--acrobat-plugin-install-dir PAD] [--build-dir MAP] [--jobs N]

Standaard:
  - Bouwt installer/packages via CPack (DragNDrop/TGZ)
  - Bouwt MET Acrobat plug-in
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

auto_detect_acrobat_sdk_dir() {
  if [[ -n "$ACROBAT_SDK_DIR_VALUE" ]]; then
    return
  fi

  local candidates=(
    "$HOME/Adobe/AcrobatSDK"
    "$HOME/Downloads/AcrobatSDK"
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

if [[ "$WITH_PLUGIN" == "true" ]]; then
  auto_detect_acrobat_sdk_dir
  if [[ -z "$ACROBAT_SDK_DIR_VALUE" ]]; then
    echo "[ERROR] Acrobat plugin build vereist Adobe Acrobat SDK." >&2
    echo "[ERROR] Geef --acrobat-sdk-dir op of zet ACROBAT_SDK_DIR. Voorbeeld:" >&2
    echo "        ./scripts/build_installer_macos.sh --acrobat-sdk-dir \"$HOME/Adobe/AcrobatSDK\"" >&2
    exit 1
  fi
  export ACROBAT_SDK_DIR="$ACROBAT_SDK_DIR_VALUE"
  echo "[OK] Gebruik Acrobat SDK: $ACROBAT_SDK_DIR"
  PLUGIN_FLAG="ON"
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

  UNINSTALL_SCRIPT="$BUILD_DIR/uninstall-plugin.sh"
  cat > "$UNINSTALL_SCRIPT" <<UNINSTALL
#!/usr/bin/env bash
set -euo pipefail
sudo rm -f "$PLUGIN_DST"
echo "Plugin verwijderd: $PLUGIN_DST"
UNINSTALL
  chmod +x "$UNINSTALL_SCRIPT"
  echo "[OK] Deinstaller script gemaakt: $UNINSTALL_SCRIPT"
fi

echo "Klaar. Check artifacts in $BUILD_DIR/"
