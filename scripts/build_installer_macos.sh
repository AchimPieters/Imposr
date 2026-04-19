#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-package"
BUILD_TYPE="Release"
WITH_PLUGIN="false"
ACROBAT_SDK_DIR_VALUE="${ACROBAT_SDK_DIR:-}"
ACROBAT_PLUGIN_INSTALL_DIR="/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plug-ins"
JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

usage() {
  cat <<USAGE
Gebruik:
  ./scripts/build_installer_macos.sh [--with-plugin] [--acrobat-sdk-dir PAD] [--build-dir MAP] [--jobs N]

Standaard:
  - Bouwt installer/packages via CPack (DragNDrop/TGZ)
  - Bouwt zonder Acrobat plug-in
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-plugin)
      WITH_PLUGIN="true"
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
      ACROBAT_PLUGIN_INSTALL_DIR="${2:-$ACROBAT_PLUGIN_INSTALL_DIR}"
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

command_exists() {
  command -v "$1" >/dev/null 2>&1
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

echo "[1/4] Controleer/verzamel vereiste tools"
ensure_xcode_cli
ensure_brew
ensure_brew_tool cmake cmake
ensure_brew_tool python3 python

if [[ "$WITH_PLUGIN" == "true" ]]; then
  if [[ -z "$ACROBAT_SDK_DIR_VALUE" ]]; then
    echo "[ERROR] --with-plugin gebruikt, maar geen --acrobat-sdk-dir opgegeven en ACROBAT_SDK_DIR is leeg." >&2
    exit 1
  fi
  export ACROBAT_SDK_DIR="$ACROBAT_SDK_DIR_VALUE"
  PLUGIN_FLAG="ON"
else
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
  if [[ -f "$PLUGIN_SRC" ]]; then
    sudo mkdir -p "$ACROBAT_PLUGIN_INSTALL_DIR"
    sudo cp -f "$PLUGIN_SRC" "$PLUGIN_DST"
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
  else
    echo "[WARN] Plugin artifact niet gevonden op $PLUGIN_SRC; deploy overgeslagen."
  fi
fi

echo "Klaar. Check artifacts in $BUILD_DIR/"
