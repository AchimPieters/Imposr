#!/usr/bin/env bash
set -euo pipefail

PLUGIN_SOURCE=""
PLUGIN_INSTALL_DIR=""

usage() {
  cat <<USAGE
Gebruik:
  ./install_acrobat_plugin_macos.sh [--plugin-source PAD] [--install-dir PAD]

Standaard:
  - Zoekt plugin in ./lib/AcrobatImpositionPlugin.api (naast dit script)
  - Deployt naar standaard Acrobat Plug-ins map (DC of nieuwe Acrobat)
USAGE
}

detect_install_dir() {
  if [[ -n "$PLUGIN_INSTALL_DIR" ]]; then
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
      PLUGIN_INSTALL_DIR="$candidate"
      return
    fi
  done

  PLUGIN_INSTALL_DIR="${candidates[0]}"
}

detect_plugin_source() {
  if [[ -n "$PLUGIN_SOURCE" ]]; then
    return
  fi

  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local default_path="$script_dir/lib/AcrobatImpositionPlugin.api"
  if [[ -f "$default_path" ]]; then
    PLUGIN_SOURCE="$default_path"
    return
  fi

  echo "[ERROR] Plugin bronbestand niet gevonden: $default_path" >&2
  echo "[ERROR] Geef --plugin-source op met een geldig pad naar AcrobatImpositionPlugin.api" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --plugin-source)
      PLUGIN_SOURCE="${2:-}"
      shift 2
      ;;
    --install-dir)
      PLUGIN_INSTALL_DIR="${2:-}"
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

detect_install_dir
detect_plugin_source

if [[ ! -f "$PLUGIN_SOURCE" ]]; then
  echo "[ERROR] Plugin bron bestaat niet: $PLUGIN_SOURCE" >&2
  exit 1
fi

PLUGIN_TARGET="$PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.api"

echo "[INFO] Installeer plugin naar: $PLUGIN_TARGET"
sudo mkdir -p "$PLUGIN_INSTALL_DIR"
sudo cp -f "$PLUGIN_SOURCE" "$PLUGIN_TARGET"
sudo xattr -dr com.apple.quarantine "$PLUGIN_TARGET" || true
sudo codesign --force --sign - "$PLUGIN_TARGET" || true

echo "[OK] Plugin geïnstalleerd: $PLUGIN_TARGET"
echo "[INFO] Herstart Adobe Acrobat volledig om de plugin te laden."


prompt_cleanup() {
  if [[ ! -t 0 ]]; then
    return
  fi

  local reply
  printf "Wil je installatiebestanden opruimen (dmg/volume)? [y/N]: "
  read -r reply || true
  if [[ ! "$reply" =~ ^[Yy]$ ]]; then
    return
  fi

  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

  if [[ "$script_dir" == /Volumes/* ]]; then
    local volume="/Volumes/${script_dir#/Volumes/}"
    volume="${volume%%/*}"
    volume="/Volumes/$volume"

    local dmg_path
    dmg_path="$(hdiutil info | awk -v vol="$volume" '$0 ~ vol {found=1} found && $1=="image-path" {print $2; exit}')"

    hdiutil detach "$volume" >/dev/null 2>&1 || true

    if [[ -n "$dmg_path" && -f "$dmg_path" ]]; then
      osascript -e 'on run argv
set p to item 1 of argv
tell application "Finder" to delete POSIX file p
end run' "$dmg_path" >/dev/null 2>&1 || true
      echo "[OK] Installatiebestand verplaatst naar prullenmand: $dmg_path"
    fi
  fi
}

prompt_cleanup
