#!/usr/bin/env bash
set -euo pipefail

PLUGIN_SOURCE=""
PLUGIN_INSTALL_DIR=""
ACTION="install"
ACROBAT_APP=""
DRY_RUN="false"
JSON_OUTPUT="false"
REQUIRE_INSTALLED="false"
REQUIRE_NOT_INSTALLED="false"

CURRENT_USER="${SUDO_USER:-$USER}"

open_path() {
  local target="$1"
  if [[ "$DRY_RUN" == "true" ]]; then
    echo "[DRY-RUN] zou map openen: $target"
    return
  fi
  if command -v open >/dev/null 2>&1; then
    open "$target"
    return
  fi
  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$target"
    return
  fi
  echo "[ERROR] Geen 'open' of 'xdg-open' gevonden om map te openen: $target" >&2
  exit 1
}

collect_uninstall_targets() {
  local install_target="$1"
  cat <<EOF
$install_target
$HOME/Library/Application Support/Adobe/Acrobat/DC/Plug-ins/AcrobatImpositionPlugin.api
/Users/$CURRENT_USER/Library/Application Support/Adobe/Acrobat/DC/Plug-ins/AcrobatImpositionPlugin.api
/Library/Application Support/Adobe/Acrobat/DC/Plug-ins/AcrobatImpositionPlugin.api
/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plug-ins/AcrobatImpositionPlugin.api
/Applications/Adobe Acrobat/Adobe Acrobat.app/Contents/Plug-ins/AcrobatImpositionPlugin.api
EOF
}

usage() {
  cat <<USAGE
Gebruik:
  ./install_acrobat_plugin_macos.sh [--action install|uninstall|open-folder|list-targets|status] [--acrobat-app PAD] [--plugin-source PAD] [--install-dir PAD] [--dry-run] [--json] [--require-installed] [--require-not-installed]

Standaard:
  - Zoekt plugin in ./lib/AcrobatImpositionPlugin.api (naast dit script)
  - Deployt naar standaard Acrobat Plug-ins map (DC of nieuwe Acrobat)
  - --acrobat-app overschrijft detectie en gebruikt <Acrobat.app>/Contents/Plug-ins
  - --dry-run toont acties zonder bestanden te wijzigen
  - --json geeft machine-readable output voor list-targets/status
  - --require-installed / --require-not-installed: status-gate voor --action status
  - Default actie: install
USAGE
}

json_escape() {
  local value="$1"
  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  value="${value//$'\n'/\\n}"
  printf '%s' "$value"
}

detect_install_dir() {
  if [[ -n "$ACROBAT_APP" ]]; then
    if [[ ! -d "$ACROBAT_APP" ]]; then
      echo "[ERROR] --acrobat-app bestaat niet of is geen map: $ACROBAT_APP" >&2
      exit 1
    fi
    PLUGIN_INSTALL_DIR="$ACROBAT_APP/Contents/Plug-ins"
    return
  fi

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
    --action)
      ACTION="${2:-}"
      shift 2
      ;;
    --plugin-source)
      PLUGIN_SOURCE="${2:-}"
      shift 2
      ;;
    --acrobat-app)
      ACROBAT_APP="${2:-}"
      shift 2
      ;;
    --install-dir)
      PLUGIN_INSTALL_DIR="${2:-}"
      shift 2
      ;;
    --dry-run)
      DRY_RUN="true"
      shift
      ;;
    --json)
      JSON_OUTPUT="true"
      shift
      ;;
    --require-installed)
      REQUIRE_INSTALLED="true"
      shift
      ;;
    --require-not-installed)
      REQUIRE_NOT_INSTALLED="true"
      shift
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

case "$ACTION" in
  install|uninstall|open-folder|list-targets|status)
    ;;
  *)
    echo "[ERROR] Ongeldige --action: $ACTION (gebruik: install, uninstall, open-folder, list-targets, status)" >&2
    usage
    exit 1
    ;;
esac

if [[ "$REQUIRE_INSTALLED" == "true" && "$REQUIRE_NOT_INSTALLED" == "true" ]]; then
  echo "[ERROR] --require-installed en --require-not-installed kunnen niet tegelijk gebruikt worden." >&2
  exit 1
fi

detect_install_dir

if [[ "$ACTION" == "list-targets" ]]; then
  mapfile -t TARGETS < <(collect_uninstall_targets "$PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.api")
  if [[ "$JSON_OUTPUT" == "true" ]]; then
    printf '{"action":"list-targets","install_dir":"%s","uninstall_targets":[' "$(json_escape "$PLUGIN_INSTALL_DIR")"
    FIRST="true"
    for target in "${TARGETS[@]}"; do
      [[ -z "$target" ]] && continue
      if [[ "$FIRST" == "true" ]]; then
        FIRST="false"
      else
        printf ','
      fi
      printf '"%s"' "$(json_escape "$target")"
    done
    printf ']}\n'
  else
    echo "action=list-targets"
    echo "install_dir=$PLUGIN_INSTALL_DIR"
    echo "uninstall_targets:"
    for target in "${TARGETS[@]}"; do
      [[ -z "$target" ]] && continue
      echo "  - $target"
    done
  fi
  exit 0
fi

if [[ "$ACTION" == "status" ]]; then
  mapfile -t TARGETS < <(collect_uninstall_targets "$PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.api")
  FOUND=0
  DETECTED_PATHS=()
  for target in "${TARGETS[@]}"; do
    [[ -z "$target" ]] && continue
    if [[ -f "$target" ]]; then
      DETECTED_PATHS+=("$target")
      FOUND=$((FOUND + 1))
    fi
  done
  STATUS_VALUE="not-installed"
  if [[ "$FOUND" -gt 0 ]]; then
    STATUS_VALUE="installed"
  fi
  if [[ "$JSON_OUTPUT" == "true" ]]; then
    printf '{"action":"status","install_dir":"%s","status":"%s","installed_count":%s,"detected_plugin_paths":[' \
      "$(json_escape "$PLUGIN_INSTALL_DIR")" "$(json_escape "$STATUS_VALUE")" "$FOUND"
    FIRST="true"
    for target in "${DETECTED_PATHS[@]}"; do
      if [[ "$FIRST" == "true" ]]; then
        FIRST="false"
      else
        printf ','
      fi
      printf '"%s"' "$(json_escape "$target")"
    done
    printf ']}\n'
  else
    echo "action=status"
    echo "install_dir=$PLUGIN_INSTALL_DIR"
    echo "detected_plugin_paths:"
    for target in "${DETECTED_PATHS[@]}"; do
      echo "  - $target"
    done
    if [[ "$STATUS_VALUE" == "installed" ]]; then
      echo "status=installed"
      echo "installed_count=$FOUND"
    else
      echo "status=not-installed"
      echo "installed_count=0"
    fi
  fi
  if [[ "$REQUIRE_INSTALLED" == "true" && "$STATUS_VALUE" != "installed" ]]; then
    echo "[ERROR] status-gate failed: plug-in is not installed." >&2
    exit 2
  fi
  if [[ "$REQUIRE_NOT_INSTALLED" == "true" && "$STATUS_VALUE" != "not-installed" ]]; then
    echo "[ERROR] status-gate failed: plug-in is installed." >&2
    exit 2
  fi
  exit 0
fi

if [[ "$ACTION" == "open-folder" ]]; then
  echo "[INFO] Open Acrobat plug-in map: $PLUGIN_INSTALL_DIR"
  if [[ "$DRY_RUN" == "true" ]]; then
    echo "[DRY-RUN] zou map aanmaken indien nodig: $PLUGIN_INSTALL_DIR"
  else
    mkdir -p "$PLUGIN_INSTALL_DIR"
  fi
  open_path "$PLUGIN_INSTALL_DIR"
  echo "[OK] Finder geopend."
  exit 0
fi

if [[ "$ACTION" == "uninstall" ]]; then
  mapfile -t TARGETS < <(collect_uninstall_targets "$PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.api")
  REMOVED_COUNT=0
  for target in "${TARGETS[@]}"; do
    [[ -z "$target" ]] && continue
    if [[ -f "$target" ]]; then
      echo "[INFO] Verwijder plugin: $target"
      if [[ "$DRY_RUN" == "true" ]]; then
        echo "[DRY-RUN] zou verwijderen: $target"
      else
        rm -f "$target" 2>/dev/null || sudo rm -f "$target"
      fi
      REMOVED_COUNT=$((REMOVED_COUNT + 1))
    fi
  done
  if [[ "$REMOVED_COUNT" -eq 0 ]]; then
    echo "[INFO] Geen plugin gevonden op bekende locaties."
  else
    echo "[OK] $REMOVED_COUNT plugin-bestand(en) verwijderd."
  fi
  exit 0
fi

detect_plugin_source

if [[ ! -f "$PLUGIN_SOURCE" ]]; then
  echo "[ERROR] Plugin bron bestaat niet: $PLUGIN_SOURCE" >&2
  exit 1
fi

PLUGIN_TARGET="$PLUGIN_INSTALL_DIR/AcrobatImpositionPlugin.api"

echo "[INFO] Installeer plugin naar: $PLUGIN_TARGET"
if [[ "$DRY_RUN" == "true" ]]; then
  echo "[DRY-RUN] zou uitvoeren: mkdir -p '$PLUGIN_INSTALL_DIR'"
  echo "[DRY-RUN] zou uitvoeren: cp -f '$PLUGIN_SOURCE' '$PLUGIN_TARGET'"
  echo "[DRY-RUN] zou uitvoeren: xattr -dr com.apple.quarantine '$PLUGIN_TARGET'"
  echo "[DRY-RUN] zou uitvoeren: codesign --force --sign - '$PLUGIN_TARGET'"
else
  sudo mkdir -p "$PLUGIN_INSTALL_DIR"
  sudo cp -f "$PLUGIN_SOURCE" "$PLUGIN_TARGET"
  sudo xattr -dr com.apple.quarantine "$PLUGIN_TARGET" || true
  sudo codesign --force --sign - "$PLUGIN_TARGET" || true
fi

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
