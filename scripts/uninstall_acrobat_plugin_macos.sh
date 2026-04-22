#!/usr/bin/env bash
set -euo pipefail

FORGET_RECEIPT="true"

usage() {
  cat <<USAGE
Gebruik:
  ./uninstall_acrobat_plugin_macos.sh [--keep-receipt]

Standaard:
  - Verwijdert AcrobatImpositionPlugin.dylib uit bekende Acrobat installaties
  - Verwijdert /usr/local/imposr (plugin payload + installer scripts)
  - Vergeet pkg receipt com.imposr.acrobat.plugin
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep-receipt)
      FORGET_RECEIPT="false"
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

TARGETS=(
  "/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plug-ins/AcrobatImpositionPlugin.dylib"
  "/Applications/Adobe Acrobat/Adobe Acrobat.app/Contents/Plug-ins/AcrobatImpositionPlugin.dylib"
)

for target in "${TARGETS[@]}"; do
  if [[ -f "$target" ]]; then
    sudo rm -f "$target"
    echo "[OK] Verwijderd: $target"
  fi
done

if [[ -d "/usr/local/imposr" ]]; then
  sudo rm -rf "/usr/local/imposr"
  echo "[OK] Verwijderd: /usr/local/imposr"
fi

if [[ "$FORGET_RECEIPT" == "true" ]] && command -v pkgutil >/dev/null 2>&1; then
  if pkgutil --pkgs | grep -q '^com\.imposr\.acrobat\.plugin$'; then
    sudo pkgutil --forget "com.imposr.acrobat.plugin" >/dev/null
    echo "[OK] pkg receipt vergeten: com.imposr.acrobat.plugin"
  fi
fi

echo "[OK] Uninstall afgerond. Acrobat volledig herstarten."
