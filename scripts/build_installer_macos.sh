#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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

  # Prefer external plugin directories (outside the app bundle) so we don't
  # break Acrobat's bundle code signature. Acrobat DC loads from these paths
  # without requiring the bundle seal to be intact.
  local external_candidates=(
    "$HOME/Library/Application Support/Adobe/Acrobat/DC/Plug-ins"
    "/Library/Application Support/Adobe/Acrobat/DC/Plug-ins"
  )
  for candidate in "${external_candidates[@]}"; do
    if [[ -d "$candidate" ]]; then
      ACROBAT_PLUGIN_INSTALL_DIR="$candidate"
      echo "[OK] Externe plugin-map gevonden: $candidate"
      return
    fi
  done

  # Fall back: check if Acrobat is installed and create the user-level external dir.
  local app_candidates=(
    "/Applications/Adobe Acrobat DC/Adobe Acrobat.app"
    "/Applications/Adobe Acrobat/Adobe Acrobat.app"
  )
  for app in "${app_candidates[@]}"; do
    if [[ -d "$app" ]]; then
      local ext_dir="$HOME/Library/Application Support/Adobe/Acrobat/DC/Plug-ins"
      mkdir -p "$ext_dir"
      ACROBAT_PLUGIN_INSTALL_DIR="$ext_dir"
      echo "[OK] Externe plugin-map aangemaakt: $ext_dir"
      return
    fi
  done

  ACROBAT_PLUGIN_INSTALL_DIR="$HOME/Library/Application Support/Adobe/Acrobat/DC/Plug-ins"
}

sdk_has_required_files() {
  # Actual SDK layout: PluginSupport/Headers/SDK/PIHeaders.h
  #                    PluginSupport/Headers/API/PIMain.c
  local dir="$1"
  [[ -f "$dir/PluginSupport/Headers/SDK/PIHeaders.h" && \
     -f "$dir/PluginSupport/Headers/API/PIMain.c" ]]
}

extract_acrobat_sdk_from_dmg() {
  # Mount an Acrobat SDK .dmg and copy its contents to $HOME/Adobe/AcrobatSDK.
  local dmg="$1"
  local target="${ACROBAT_SDK_DIR_VALUE:-$HOME/Adobe/AcrobatSDK}"
  echo "[INFO] SDK DMG gevonden: $dmg"
  echo "[INFO] Uitpakken naar: $target"

  # Detach any leftover volume with the same name first.
  local vol_name
  vol_name="$(hdiutil imageinfo "$dmg" 2>/dev/null | grep 'Volume Name' | awk -F': ' '{print $2}' | tr -d '\n')" || true

  local mount_out
  mount_out="$(hdiutil attach "$dmg" -nobrowse -readonly 2>&1)"
  local mount_point
  mount_point="$(echo "$mount_out" | grep '/Volumes/' | awk -F'\t' '{print $NF}' | tr -d '\n')"

  if [[ -z "$mount_point" ]]; then
    echo "[WARN] Kon SDK DMG niet mounten: $dmg"
    return
  fi
  echo "[INFO] DMG gemount op: $mount_point"

  mkdir -p "$target"
  # rsync preserves structure; fall back to cp -rf when rsync unavailable.
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --exclude='.DS_Store' "$mount_point/" "$target/"
  else
    cp -rf "$mount_point/." "$target/"
  fi

  hdiutil detach "$mount_point" -quiet 2>/dev/null || true

  if sdk_has_required_files "$target"; then
    ACROBAT_SDK_DIR_VALUE="$target"
    echo "[OK] SDK uitgepakt naar: $target"
  else
    echo "[WARN] SDK gekopieerd maar PIHeaders.h niet gevonden op verwachte locatie."
    echo "       Controleer: $target/PluginSupport/Headers/SDK/PIHeaders.h"
  fi
}

auto_extract_acrobat_sdk_archive() {
  # Handle .zip archives in Downloads
  local archives=()
  while IFS= read -r archive; do
    archives+=("$archive")
  done < <(find "$HOME/Downloads" -maxdepth 2 -type f \( -iname "*acrobat*sdk*.zip" -o -iname "*adobe*acrobat*.zip" \) 2>/dev/null)

  if [[ ${#archives[@]} -eq 0 ]]; then
    return
  fi

  local extract_root="$HOME/Adobe/AcrobatSDK"
  mkdir -p "$extract_root"

  local archive
  for archive in "${archives[@]}"; do
    echo "[INFO] Probeer SDK archive uit te pakken: $archive"
    unzip -qo "$archive" -d "$extract_root" || true
  done

  # Locate the root that contains the actual plugin headers.
  local candidate
  while IFS= read -r candidate; do
    if sdk_has_required_files "$candidate"; then
      ACROBAT_SDK_DIR_VALUE="$candidate"
      echo "[OK] Acrobat SDK automatisch gevonden na extract: $candidate"
      return
    fi
  done < <(find "$extract_root" -maxdepth 6 -type d 2>/dev/null)
}

auto_detect_acrobat_sdk_dir() {
  if [[ -n "$ACROBAT_SDK_DIR_VALUE" ]]; then
    return
  fi

  # 1. Check fixed well-known directories.
  local candidates=(
    "$HOME/Adobe/AcrobatSDK"
    "$HOME/Downloads/AcrobatSDK"
    "$HOME/Documents/AcrobatSDK"
    "$HOME/Desktop/AcrobatSDK"
    "/opt/acrobat-sdk"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if sdk_has_required_files "$candidate"; then
      ACROBAT_SDK_DIR_VALUE="$candidate"
      echo "[OK] Acrobat SDK gevonden: $candidate"
      return
    fi
  done

  # 2. Look for any matching directory anywhere in HOME.
  while IFS= read -r candidate; do
    if sdk_has_required_files "$candidate"; then
      ACROBAT_SDK_DIR_VALUE="$candidate"
      echo "[OK] Acrobat SDK gevonden: $candidate"
      return
    fi
  done < <(find "$HOME" -maxdepth 7 -type d \( -iname "*acrobat*sdk*" -o -iname "*adobe*acrobat*sdk*" \) 2>/dev/null)

  # 3. SDK DMG in the repository root (Acrobat_DC_SDK_Mac_*.dmg).
  local dmg
  while IFS= read -r dmg; do
    extract_acrobat_sdk_from_dmg "$dmg"
    [[ -n "$ACROBAT_SDK_DIR_VALUE" ]] && return
  done < <(find "$REPO_ROOT" -maxdepth 1 -type f -iname "Acrobat_DC_SDK_Mac*.dmg" 2>/dev/null)

  # 4. ZIP archives in Downloads.
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

ensure_python_package() {
  local package="$1"
  local import_name="${2:-$1}"

  if python3 -c "import $import_name" 2>/dev/null; then
    echo "[OK] Python package '$package' gevonden"
    return
  fi

  echo "[INFO] Python package '$package' ontbreekt; probeer te installeren"
  # Try in order: --break-system-packages (Homebrew Python on macOS ≥14),
  # plain pip3, system pip — all non-fatal since Pillow is only used for
  # the DMG background (fallback: no background).
  python3 -m pip install "$package" --quiet --break-system-packages 2>/dev/null || \
  python3 -m pip install "$package" --quiet 2>/dev/null || \
  pip3 install "$package" --quiet 2>/dev/null || true

  if python3 -c "import $import_name" 2>/dev/null; then
    echo "[OK] $package geïnstalleerd"
  else
    echo "[WARN] $package kon niet worden geïnstalleerd — DMG-achtergrond wordt overgeslagen (niet kritiek)."
  fi
}

create_auto_plugin_pkg() {
  local plugin_src="$1"
  local pkg_root="$BUILD_DIR/pkgroot"
  local pkg_scripts="$BUILD_DIR/pkgscripts"
  local pkg_out="$BUILD_DIR/Imposr-Acrobat-Plugin-${PLUGIN_VERSION}.pkg"

  rm -rf "$pkg_root" "$pkg_scripts"
  mkdir -p "$pkg_root/usr/local/imposr/lib" "$pkg_root/usr/local/imposr/bin" "$pkg_scripts"
  cp -f "$plugin_src" "$pkg_root/usr/local/imposr/lib/AcrobatImpositionPlugin.api"
  cp -f "scripts/uninstall_acrobat_plugin_macos.sh" "$pkg_root/usr/local/imposr/bin/uninstall_acrobat_plugin_macos.sh"
  chmod +x "$pkg_root/usr/local/imposr/bin/uninstall_acrobat_plugin_macos.sh"

  cat > "$pkg_scripts/postinstall" <<'POSTINSTALL'
#!/bin/bash
set -euo pipefail

PLUGIN_SRC="/usr/local/imposr/lib/AcrobatImpositionPlugin.api"

if [[ ! -f "$PLUGIN_SRC" ]]; then
  echo "[ERROR] Plugin source missing: $PLUGIN_SRC" >&2
  exit 1
fi

# Install into the EXTERNAL plugin directory so Acrobat's app-bundle
# code signature stays intact. Acrobat DC loads plugins from both
# ~/Library and /Library paths without breaking its own bundle seal.
ACROBAT_APPS=(
  "/Applications/Adobe Acrobat DC/Adobe Acrobat.app"
  "/Applications/Adobe Acrobat/Adobe Acrobat.app"
)

installed="false"
for app in "${ACROBAT_APPS[@]}"; do
  if [[ -d "$app" ]]; then
    # Determine the installing user's home directory.
    INSTALL_USER="${USER:-$(id -un)}"
    if [[ "$INSTALL_USER" == "root" ]]; then
      TARGET_DIR="/Library/Application Support/Adobe/Acrobat/DC/Plug-ins"
    else
      TARGET_DIR="/Users/$INSTALL_USER/Library/Application Support/Adobe/Acrobat/DC/Plug-ins"
    fi
    mkdir -p "$TARGET_DIR"
    cp -f "$PLUGIN_SRC" "$TARGET_DIR/AcrobatImpositionPlugin.api"
    xattr -dr com.apple.quarantine "$TARGET_DIR/AcrobatImpositionPlugin.api" || true
    echo "[OK] Plugin installed to: $TARGET_DIR/AcrobatImpositionPlugin.api"
    installed="true"
    break
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

echo "[1/5] Controleer/verzamel vereiste tools"
ensure_xcode_cli
ensure_brew
ensure_brew_tool cmake cmake
ensure_brew_tool python3 python
ensure_brew_tool create-dmg create-dmg   # polished DMG window layout
ensure_python_package Pillow PIL         # DMG background image generation (optional)
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

echo "[2/5] Configureer packaging build"
cmake -S . -B "$BUILD_DIR" \
  -DAIMP_BUILD_PLUGIN="$PLUGIN_FLAG" \
  -DAIMP_ACROBAT_PLUGIN_INSTALL_DIR="$ACROBAT_PLUGIN_INSTALL_DIR" \
  -DAIMP_BUILD_CLI=ON \
  -DAIMP_BUILD_TESTS=OFF \
  -DAIMP_ENABLE_PACKAGING=ON \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "[3/5] Bouw project"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "[4/5] Genereer installers/packages"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -C "$BUILD_TYPE"

DIST_DIR="dist"
mkdir -p "$DIST_DIR"

# Move all generated CPack artifacts (dmg, tar.gz, zip) into dist/.
for artifact in Imposr-*.dmg Imposr-*.tar.gz Imposr-*.zip; do
  if [[ -f "$artifact" ]]; then
    mv -f "$artifact" "$DIST_DIR/"
    echo "[OK] Artifact verplaatst naar $DIST_DIR/: $artifact"
  fi
done

# Always build a .pkg for the CLI installer so dist/ contains a working .pkg.
create_cli_pkg() {
  local cli_src="$BUILD_DIR/imposr_cli"
  if [[ ! -f "$cli_src" ]]; then
    echo "[WARN] imposr_cli binary niet gevonden; .pkg wordt overgeslagen." >&2
    return
  fi

  local pkg_root="$BUILD_DIR/cli-pkgroot"
  local pkg_scripts="$BUILD_DIR/cli-pkgscripts"
  local pkg_out="$DIST_DIR/Imposr-${PLUGIN_VERSION}.pkg"

  rm -rf "$pkg_root" "$pkg_scripts"
  mkdir -p "$pkg_root/usr/local/bin" "$pkg_scripts"
  cp -f "$cli_src" "$pkg_root/usr/local/bin/imposr_cli"
  chmod +x "$pkg_root/usr/local/bin/imposr_cli"

  cat > "$pkg_scripts/postinstall" <<'POSTINSTALL'
#!/bin/bash
echo "[Imposr] CLI installed at /usr/local/bin/imposr_cli"
exit 0
POSTINSTALL
  chmod +x "$pkg_scripts/postinstall"

  pkgbuild \
    --root "$pkg_root" \
    --scripts "$pkg_scripts" \
    --identifier "com.imposr.cli" \
    --version "$PLUGIN_VERSION" \
    "$pkg_out"

  echo "[OK] CLI installer aangemaakt: $pkg_out"
}

create_cli_pkg

if [[ "$WITH_PLUGIN" == "true" ]]; then
  PLUGIN_SRC="$BUILD_DIR/AcrobatImpositionPlugin.api"
  PLUGIN_DST="$ACROBAT_PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.api"
  if [[ ! -f "$PLUGIN_SRC" ]]; then
    echo "[ERROR] Verwachte plugin artifact ontbreekt: $PLUGIN_SRC" >&2
    exit 1
  fi

  # Use sudo only when installing to a system-wide path; prefer the user-level
  # external directory so the Acrobat app-bundle seal stays intact.
  if [[ "$ACROBAT_PLUGIN_INSTALL_DIR" == /Library/* ]]; then
    sudo mkdir -p "$ACROBAT_PLUGIN_INSTALL_DIR"
    sudo cp -f "$PLUGIN_SRC" "$PLUGIN_DST"
    sudo xattr -dr com.apple.quarantine "$PLUGIN_DST" || true
  else
    mkdir -p "$ACROBAT_PLUGIN_INSTALL_DIR"
    cp -f "$PLUGIN_SRC" "$PLUGIN_DST"
    xattr -dr com.apple.quarantine "$PLUGIN_DST" || true
  fi
  echo "[OK] Plugin gedeployed naar: $PLUGIN_DST"

  create_auto_plugin_pkg "$PLUGIN_SRC"

  # Move plugin pkg to dist/ as well.
  if [[ -f "$BUILD_DIR/Imposr-Acrobat-Plugin-${PLUGIN_VERSION}.pkg" ]]; then
    mv -f "$BUILD_DIR/Imposr-Acrobat-Plugin-${PLUGIN_VERSION}.pkg" "$DIST_DIR/"
    echo "[OK] Plugin pkg verplaatst naar $DIST_DIR/"
  fi

  UNINSTALL_SCRIPT="$BUILD_DIR/uninstall-plugin.sh"
  cp -f scripts/uninstall_acrobat_plugin_macos.sh "$UNINSTALL_SCRIPT"
  chmod +x "$UNINSTALL_SCRIPT"
  echo "[OK] Deinstaller script gemaakt: $UNINSTALL_SCRIPT"
fi

# ── Build the QI+-style installer DMG ────────────────────────────────────────
echo ""
echo "[5/5] Bouw QI+-stijl installer DMG (Install Imposr.app in DMG)"

DMG_PLUGIN_ARG=""
if [[ "$WITH_PLUGIN" == "true" ]]; then
  PLUGIN_SRC_FOR_DMG="$BUILD_DIR/AcrobatImpositionPlugin.api"
  if [[ -f "$PLUGIN_SRC_FOR_DMG" ]]; then
    DMG_PLUGIN_ARG="--plugin-path $PLUGIN_SRC_FOR_DMG"
  fi
fi

bash "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/build_dmg_macos.sh" \
  --version "$PLUGIN_VERSION" \
  --dist-dir "$DIST_DIR" \
  ${DMG_PLUGIN_ARG}

echo ""
echo "Klaar. Check artifacts in $DIST_DIR/"
ls -lh "$DIST_DIR/" 2>/dev/null || true
