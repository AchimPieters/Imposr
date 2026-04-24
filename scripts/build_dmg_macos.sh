#!/usr/bin/env bash
# build_dmg_macos.sh
#
# Builds a QI+-style installer DMG for macOS.
#
# What it produces (in dist/):
#   imposr-<version>.dmg
#     └── Install Imposr.app   ← double-click to install or uninstall
#
# The .app finds the .api in its own Contents/Resources/, asks for the Acrobat
# location (auto-detected or via Browse), and installs with a native GUI dialog.
# The same app handles uninstallation — no Terminal required.
#
# Usage:
#   ./scripts/build_dmg_macos.sh [options]
#
# Options:
#   --plugin-path <path>    Path to AcrobatImpositionPlugin.api to bundle
#   --version <x.y.z>       Override version string (default: 0.1.0)
#   --dist-dir <dir>        Output directory            (default: dist)
#   --work-dir <dir>        Temporary working directory (default: /tmp/imposr-dmg-work)
#   --volume-name <name>    DMG volume name             (default: "Install Imposr")
#   --no-background         Skip DMG background image
#   -h|--help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PLUGIN_PATH=""
VERSION="0.1.0"
DIST_DIR="$REPO_ROOT/dist"
WORK_DIR="/tmp/imposr-dmg-work"
VOLUME_NAME="Install Imposr"
DMG_WINDOW_W=540
DMG_WINDOW_H=380
USE_BACKGROUND=true

usage() {
  cat <<USAGE
Usage:
  ./scripts/build_dmg_macos.sh [--plugin-path <api>] [--version <x.y.z>]
                                [--dist-dir <dir>] [--work-dir <dir>]
                                [--volume-name <name>] [--no-background]
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --plugin-path)   PLUGIN_PATH="${2:-}"; shift 2 ;;
    --version)       VERSION="${2:-}";     shift 2 ;;
    --dist-dir)      DIST_DIR="${2:-}";    shift 2 ;;
    --work-dir)      WORK_DIR="${2:-}";    shift 2 ;;
    --volume-name)   VOLUME_NAME="${2:-}"; shift 2 ;;
    --no-background) USE_BACKGROUND=false; shift ;;
    -h|--help)       usage; exit 0 ;;
    *) echo "[ERROR] Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

# ── Resolve plugin path ───────────────────────────────────────────────────────
if [[ -z "$PLUGIN_PATH" ]]; then
  # Try common build locations.
  for candidate in \
      "$REPO_ROOT/build/AcrobatImpositionPlugin.api" \
      "$REPO_ROOT/build-package/AcrobatImpositionPlugin.api" \
      "$REPO_ROOT/dist/AcrobatImpositionPlugin.api"; do
    if [[ -f "$candidate" ]]; then
      PLUGIN_PATH="$candidate"
      break
    fi
  done
fi

DMG_NAME="imposr-${VERSION}.dmg"
DMG_STAGING="$WORK_DIR/staging"
APP_NAME="Install Imposr.app"
APP_BUNDLE="$DMG_STAGING/$APP_NAME"

echo "[1/7] Prepare work directory"
rm -rf "$WORK_DIR"
mkdir -p "$DMG_STAGING"
mkdir -p "$DIST_DIR"

# ── Compile the AppleScript app ───────────────────────────────────────────────
echo "[2/7] Compile installer app"
APPLESCRIPT_SRC="$REPO_ROOT/installer/Install Imposr.applescript"

if [[ ! -f "$APPLESCRIPT_SRC" ]]; then
  echo "[ERROR] AppleScript source not found: $APPLESCRIPT_SRC" >&2
  exit 1
fi

osacompile -o "$APP_BUNDLE" "$APPLESCRIPT_SRC"
echo "[OK]  Compiled: $APP_BUNDLE"

# ── Bundle .api into the app ──────────────────────────────────────────────────
echo "[3/7] Bundle plug-in into app"
RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"
mkdir -p "$RESOURCES_DIR"

if [[ -n "$PLUGIN_PATH" && -f "$PLUGIN_PATH" ]]; then
  cp -f "$PLUGIN_PATH" "$RESOURCES_DIR/AcrobatImpositionPlugin.api"
  echo "[OK]  Plug-in bundled from: $PLUGIN_PATH"
else
  # Create a placeholder so the installer app exists and is functional for demo.
  echo "IMPOSR_PLUGIN_PLACEHOLDER_${VERSION}" > "$RESOURCES_DIR/AcrobatImpositionPlugin.api"
  echo "[WARN] No .api found — placeholder inserted."
  echo "[WARN] Build with AIMP_BUILD_PLUGIN=ON and pass --plugin-path to embed the real plug-in."
fi

# ── Set a custom app icon ─────────────────────────────────────────────────────
echo "[4/7] Set app icon"
ICON_SRC="$REPO_ROOT/images/imposr.png"
ICONSET_DIR="$WORK_DIR/Imposr.iconset"

if [[ -f "$ICON_SRC" ]] && command -v sips >/dev/null 2>&1; then
  mkdir -p "$ICONSET_DIR"
  for size in 16 32 64 128 256 512; do
    sips -z $size $size "$ICON_SRC" --out "$ICONSET_DIR/icon_${size}x${size}.png" >/dev/null 2>&1 || true
    sips -z $((size*2)) $((size*2)) "$ICON_SRC" --out "$ICONSET_DIR/icon_${size}x${size}@2x.png" >/dev/null 2>&1 || true
  done
  if iconutil -c icns "$ICONSET_DIR" -o "$WORK_DIR/Imposr.icns" 2>/dev/null; then
    cp -f "$WORK_DIR/Imposr.icns" "$RESOURCES_DIR/applet.icns"
    # Tag bundle to use custom icon.
    /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string applet" \
        "$APP_BUNDLE/Contents/Info.plist" 2>/dev/null || \
    /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile applet" \
        "$APP_BUNDLE/Contents/Info.plist" 2>/dev/null || true
    echo "[OK]  App icon set"
  fi
fi

# ── Update Info.plist for a cleaner app name / bundle id ─────────────────────
PL="$APP_BUNDLE/Contents/Info.plist"
if [[ -f "$PL" ]]; then
  /usr/libexec/PlistBuddy -c "Set :CFBundleName 'Install Imposr'"         "$PL" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName 'Install Imposr'"  "$PL" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier 'com.imposr.installer'" "$PL" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString '$VERSION'"  "$PL" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Set :CFBundleVersion '$VERSION'"             "$PL" 2>/dev/null || true
fi

# ── Optional DMG background image ────────────────────────────────────────────
BACKGROUND_PNG=""
if [[ "$USE_BACKGROUND" == true ]]; then
  echo "[5/7] Generate DMG background"
  BG_OUT="$WORK_DIR/dmg_background.png"
  bash "$REPO_ROOT/installer/make_dmg_background.sh" "$BG_OUT" 2>/dev/null || true
  if [[ -f "$BG_OUT" ]]; then
    BACKGROUND_PNG="$BG_OUT"
    echo "[OK]  Background: $BG_OUT"
  else
    echo "[INFO] No background image generated; DMG will use default white."
  fi
else
  echo "[5/7] Background skipped (--no-background)"
fi

# ── Build the DMG ─────────────────────────────────────────────────────────────
echo "[6/7] Create DMG"

# If create-dmg (Homebrew) is available use it for polished window layout.
if command -v create-dmg >/dev/null 2>&1; then
  CREATEDMG_ARGS=(
    --volname  "$VOLUME_NAME"
    --window-size "$DMG_WINDOW_W" "$DMG_WINDOW_H"
    --icon-size 100
    --icon "$APP_NAME" 270 190
    --hide-extension "$APP_NAME"
  )
  if [[ -n "$BACKGROUND_PNG" ]]; then
    CREATEDMG_ARGS+=(--background "$BACKGROUND_PNG")
    CREATEDMG_ARGS+=(--text-size 13)
  fi

  create-dmg \
    "${CREATEDMG_ARGS[@]}" \
    "$DIST_DIR/$DMG_NAME" \
    "$DMG_STAGING"

else
  # Fallback: plain hdiutil DMG with Finder window positioning via AppleScript.
  TEMP_RW_DMG="$WORK_DIR/rw.dmg"

  hdiutil create \
    -srcfolder "$DMG_STAGING" \
    -volname   "$VOLUME_NAME" \
    -fs        HFS+ \
    -fsargs    "-c c=16,b=4096" \
    -format    UDRW \
    -size      20m \
    "$TEMP_RW_DMG"

  # Detach any leftover volume with the same name from a previous failed run.
  hdiutil detach "/Volumes/${VOLUME_NAME}" -quiet 2>/dev/null || true

  # Mount the writable DMG.
  # hdiutil output is tab-separated; use -F'\t' so spaces in the volume name survive.
  MOUNT_POINT="$(hdiutil attach -readwrite -noverify -noautoopen \
      "$TEMP_RW_DMG" | grep '/Volumes/' | awk -F'\t' '{print $NF}' | tr -d '\n')"
  echo "[OK]  Mounted at: $MOUNT_POINT"

  # Copy background if available.
  if [[ -n "$BACKGROUND_PNG" ]]; then
    mkdir -p "$MOUNT_POINT/.background"
    cp "$BACKGROUND_PNG" "$MOUNT_POINT/.background/background.png"
  fi

  # Set window appearance with AppleScript (Finder scripting).
  # Wrapped in try/end try so a Finder hiccup doesn't abort the DMG build.
  BG_APPLESCRIPT=""
  if [[ -n "$BACKGROUND_PNG" ]]; then
    BG_APPLESCRIPT="set background picture of theWindow to file \".background:background.png\""
  fi

  osascript 2>/dev/null <<FINDER_SCRIPT || echo "[WARN] Finder window positioning skipped (non-fatal)"
tell application "Finder"
  activate
  -- open the mounted volume first so we can get its window
  tell disk "${VOLUME_NAME}"
    open
  end tell
  delay 1
  try
    tell disk "${VOLUME_NAME}"
      set theWindow to window 1
      set current view of theWindow to icon view
      set toolbar visible of theWindow to false
      set statusbar visible of theWindow to false
      set bounds of theWindow to {100, 100, $((100 + DMG_WINDOW_W)), $((100 + DMG_WINDOW_H))}
      set theViewOptions to icon view options of theWindow
      set arrangement of theViewOptions to not arranged
      set icon size of theViewOptions to 100
      ${BG_APPLESCRIPT}
      set position of item "${APP_NAME}" of theWindow to {270, 190}
      update without registering applications
      delay 1
      close
    end tell
  end try
end tell
FINDER_SCRIPT

  # Bless the volume (sets the icon for the DMG itself in Finder).
  bless --folder "$MOUNT_POINT" --openfolder "$MOUNT_POINT" 2>/dev/null || true

  # Hide background dir.
  SetFile -a V "$MOUNT_POINT/.background" 2>/dev/null || true

  hdiutil detach "$MOUNT_POINT" -quiet

  # Convert to compressed read-only DMG.
  hdiutil convert "$TEMP_RW_DMG" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$DIST_DIR/$DMG_NAME"
fi

echo "[7/7] Done"
echo ""
echo "  DMG: $DIST_DIR/$DMG_NAME"
echo ""
echo "  Users open the DMG and double-click 'Install Imposr' to install or"
echo "  uninstall the Imposr plug-in — no Terminal required."
