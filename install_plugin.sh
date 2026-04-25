#!/bin/bash
SRC="$HOME/Desktop/Imposr/build-package/AcrobatImpositionPlugin.acroplugin"
DST="/Applications/Adobe Acrobat DC/Adobe Acrobat.app/Contents/Plugins"

# Sign the bundle before installing
codesign --force --sign - --deep "$SRC" 2>/dev/null || true

sudo rm -rf "$DST/AcrobatImpositionPlugin.acroplugin"
sudo cp -R "$SRC" "$DST/"
sudo xattr -dr com.apple.quarantine "$DST/AcrobatImpositionPlugin.acroplugin" 2>/dev/null || true
echo "Geinstalleerd. Herstart Acrobat."
