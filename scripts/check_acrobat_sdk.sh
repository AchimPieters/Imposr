#!/usr/bin/env bash
# check_acrobat_sdk.sh
#
# Verifies that the Adobe Acrobat SDK is present and correctly structured,
# then optionally kicks off the full installer build.
#
# Usage:
#   ./scripts/check_acrobat_sdk.sh [--sdk-dir <path>] [--build]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SDK_DIR="${ACROBAT_SDK_DIR:-$HOME/Adobe/AcrobatSDK}"
DO_BUILD=false
PASS=true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdk-dir) SDK_DIR="${2:-}"; shift 2 ;;
    --build)   DO_BUILD=true; shift ;;
    -h|--help)
      echo "Usage: ./scripts/check_acrobat_sdk.sh [--sdk-dir <path>] [--build]"
      exit 0 ;;
    *) echo "Onbekende optie: $1" >&2; exit 1 ;;
  esac
done

sdk_has_required_files() {
  local dir="$1"
  [[ -f "$dir/PluginSupport/Headers/SDK/PIHeaders.h" && \
     -f "$dir/PluginSupport/Headers/API/PIMain.c" ]]
}

# Auto-extract SDK from a DMG in the repo root if not yet present.
if ! sdk_has_required_files "$SDK_DIR"; then
  DMG=""
  while IFS= read -r f; do DMG="$f"; break; done \
    < <(find "$REPO_ROOT" -maxdepth 1 -type f -iname "Acrobat_DC_SDK_Mac*.dmg" 2>/dev/null)
  if [[ -n "$DMG" ]]; then
    echo "[INFO] SDK niet gevonden; DMG gevonden in repo: $DMG"
    echo "[INFO] Uitpakken naar: $SDK_DIR"
    MOUNT_OUT="$(hdiutil attach "$DMG" -nobrowse -readonly 2>&1)"
    MOUNT_POINT="$(echo "$MOUNT_OUT" | grep '/Volumes/' | awk -F'\t' '{print $NF}' | tr -d '\n')"
    if [[ -n "$MOUNT_POINT" ]]; then
      mkdir -p "$SDK_DIR"
      if command -v rsync >/dev/null 2>&1; then
        rsync -a --exclude='.DS_Store' "$MOUNT_POINT/" "$SDK_DIR/"
      else
        cp -rf "$MOUNT_POINT/." "$SDK_DIR/"
      fi
      hdiutil detach "$MOUNT_POINT" -quiet 2>/dev/null || true
      echo "[OK] SDK uitgepakt naar: $SDK_DIR"
    else
      echo "[WARN] Kon DMG niet mounten."
    fi
  fi
fi

echo ""
echo "══════════════════════════════════════════════════"
echo "  Imposr — Adobe Acrobat SDK verificatie"
echo "══════════════════════════════════════════════════"
echo ""
echo "  SDK-pad: $SDK_DIR"
echo ""

# ── Acrobat installatie ───────────────────────────────
echo "── Acrobat installatie ──────────────────────────"
ACROBAT_FOUND=false
for bundle in \
    "/Applications/Adobe Acrobat DC/Adobe Acrobat.app" \
    "/Applications/Adobe Acrobat/Adobe Acrobat.app" \
    "/Applications/Adobe Acrobat 2020/Adobe Acrobat.app" \
    "/Applications/Adobe Acrobat 2017/Adobe Acrobat.app"; do
  if [[ -d "$bundle" ]]; then
    echo "  [OK] Acrobat gevonden: $bundle"
    ACROBAT_FOUND=true
    break
  fi
done
if [[ "$ACROBAT_FOUND" == false ]]; then
  echo "  [WARN] Adobe Acrobat niet gevonden in /Applications"
  echo "         Zonder Acrobat kan de plug-in niet worden getest."
fi

# ── SDK aanwezigheid ──────────────────────────────────
echo ""
echo "── SDK structuur ────────────────────────────────"

check_path() {
  local label="$1"
  local path="$2"
  if [[ -e "$path" ]]; then
    echo "  [OK] $label"
  else
    echo "  [FOUT] $label"
    echo "         Verwacht: $path"
    PASS=false
  fi
}

# SDK layout (Acrobat DC SDK Mac 2023 / Windows 2021):
#   PluginSupport/Headers/SDK/PIHeaders.h   ← master include
#   PluginSupport/Headers/API/PIMain.c      ← plugin bootstrap + alle API headers
check_path "SDK map aanwezig"                         "$SDK_DIR"
check_path "PluginSupport/ map"                       "$SDK_DIR/PluginSupport"
check_path "PluginSupport/Headers/SDK/ map"           "$SDK_DIR/PluginSupport/Headers/SDK"
check_path "PIHeaders.h (master include)"             "$SDK_DIR/PluginSupport/Headers/SDK/PIHeaders.h"
check_path "PluginSupport/Headers/API/ map"           "$SDK_DIR/PluginSupport/Headers/API"
check_path "PIMain.c (plugin bootstrap)"              "$SDK_DIR/PluginSupport/Headers/API/PIMain.c"

# Controleer minimaal aantal headers
HEADER_COUNT=$(find "$SDK_DIR/PluginSupport/Headers/API" -name "*.h" 2>/dev/null | wc -l | tr -d ' ')
if [[ "$HEADER_COUNT" -gt 50 ]]; then
  echo "  [OK] $HEADER_COUNT API-headers gevonden"
elif [[ "$HEADER_COUNT" -gt 0 ]]; then
  echo "  [WARN] Slechts $HEADER_COUNT API-headers gevonden (verwacht >50)"
else
  echo "  [FOUT] Geen headers gevonden in $SDK_DIR/PluginSupport/Headers/API"
  PASS=false
fi

# ── Omgevingsvariabele ────────────────────────────────
echo ""
echo "── Omgevingsvariabele ───────────────────────────"
if [[ -n "${ACROBAT_SDK_DIR:-}" ]]; then
  echo "  [OK] ACROBAT_SDK_DIR is ingesteld: $ACROBAT_SDK_DIR"
else
  echo "  [INFO] ACROBAT_SDK_DIR is niet ingesteld in deze shell."
  echo "         Voeg toe aan ~/.zshrc:"
  echo "         export ACROBAT_SDK_DIR=\"$SDK_DIR\""
fi

# ── Resultaat ─────────────────────────────────────────
echo ""
echo "══════════════════════════════════════════════════"
if [[ "$PASS" == true ]]; then
  echo "  RESULTAAT: SDK correct geconfigureerd"
  echo "══════════════════════════════════════════════════"
  echo ""
  echo "  Volgende stap — bouw de installer:"
  echo ""
  echo "    ./scripts/build_installer_macos.sh --acrobat-sdk-dir \"$SDK_DIR\""
  echo ""

  if [[ "$DO_BUILD" == true ]]; then
    echo "  --build opgegeven, start installer build..."
    echo ""
    exec "$SCRIPT_DIR/build_installer_macos.sh" --acrobat-sdk-dir "$SDK_DIR"
  fi
else
  echo "  RESULTAAT: SDK niet of niet correct gevonden"
  echo "══════════════════════════════════════════════════"
  echo ""
  echo "  Wat je moet doen:"
  echo ""
  echo "  1. Ga naar https://developer.adobe.com/console/servicesandapis"
  echo "     (log in met je Adobe-account — gratis registratie)"
  echo ""
  echo "  2. Kies bij 'Acrobat SDK' de versie voor Macintosh en download het ZIP"
  echo "     (bestandsnaam lijkt op: AcrobatSDK_DC.zip)"
  echo ""
  echo "  3. Pak het ZIP-bestand uit naar:"
  echo "     $SDK_DIR"
  echo ""
  echo "     Controleer daarna of dit bestand bestaat:"
  echo "     $SDK_DIR/API/PIHeaders.h"
  echo ""
  echo "  4. Voer dit script opnieuw uit:"
  echo "     ./scripts/check_acrobat_sdk.sh"
  echo ""
  echo "  5. Als alles groen is, bouw de installer:"
  echo "     ./scripts/check_acrobat_sdk.sh --build"
  echo ""
  exit 1
fi
