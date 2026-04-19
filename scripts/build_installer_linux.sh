#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build-package"
BUILD_TYPE="Release"
WITH_PLUGIN="false"
ACROBAT_SDK_DIR_VALUE="${ACROBAT_SDK_DIR:-}"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

usage() {
  cat <<USAGE
Gebruik:
  ./scripts/build_installer_linux.sh [--with-plugin] [--acrobat-sdk-dir PAD] [--build-dir MAP] [--jobs N]

Let op:
  - Plug-in builds zijn in deze repository niet bedoeld voor Linux.
  - Dit script bouwt standaard packaging/installers via CPack (TGZ/DEB afhankelijk van tools).
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

if [[ "$WITH_PLUGIN" == "true" ]]; then
  echo "[WARN] Linux plug-in flow wordt niet ondersteund in deze repository. Ga door met --with-plugin=OFF."
  WITH_PLUGIN="false"
fi

if [[ $EUID -eq 0 ]]; then
  SUDO=""
else
  SUDO="sudo"
fi

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

install_apt() {
  $SUDO apt-get update
  $SUDO apt-get install -y "$@"
}

install_dnf() {
  $SUDO dnf install -y "$@"
}

install_pacman() {
  $SUDO pacman -Sy --noconfirm "$@"
}

install_zypper() {
  $SUDO zypper --non-interactive install "$@"
}

ensure_tool() {
  local tool="$1"
  shift
  local pkgs=("$@")

  if command_exists "$tool"; then
    echo "[OK] $tool gevonden"
    return
  fi

  echo "[INFO] $tool ontbreekt; probeer te installeren (${pkgs[*]})"

  if command_exists apt-get; then
    install_apt "${pkgs[@]}"
  elif command_exists dnf; then
    install_dnf "${pkgs[@]}"
  elif command_exists pacman; then
    install_pacman "${pkgs[@]}"
  elif command_exists zypper; then
    install_zypper "${pkgs[@]}"
  else
    echo "[ERROR] Geen ondersteunde package manager gevonden (apt/dnf/pacman/zypper)." >&2
    exit 1
  fi

  if ! command_exists "$tool"; then
    echo "[ERROR] Installatie van $tool lijkt mislukt." >&2
    exit 1
  fi
}

echo "[1/4] Controleer/verzamel vereiste tools"
ensure_tool cmake cmake
ensure_tool ctest cmake
ensure_tool cpack cmake
ensure_tool c++ g++
ensure_tool python3 python3

if ! command_exists dpkg; then
  echo "[INFO] dpkg niet gevonden; DEB packaging wordt waarschijnlijk overgeslagen, TGZ blijft mogelijk."
fi

echo "[2/4] Configureer packaging build"
cmake -S . -B "$BUILD_DIR" \
  -DAIMP_BUILD_PLUGIN=OFF \
  -DAIMP_BUILD_CLI=ON \
  -DAIMP_BUILD_TESTS=OFF \
  -DAIMP_ENABLE_PACKAGING=ON \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "[3/4] Bouw project"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "[4/4] Genereer installers/packages"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -C "$BUILD_TYPE"

echo "Klaar. Check artifacts in $BUILD_DIR/"
