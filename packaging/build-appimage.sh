#!/usr/bin/env bash
# Build QSapecNG and package it as an AppImage with linuxdeploy.
#
# usage: packaging/build-appimage.sh <qwt-prefix> [output-dir]
#
# Environment:
#   QMAKE       qmake of the Qt 6 to bundle (default: qmake6 / qmake on PATH)
#   BUILD_DIR   CMake build directory (default: build-appimage)
#   TOOLS_DIR   where linuxdeploy is downloaded/cached (default: $BUILD_DIR/tools)
#   CMAKE_ARGS  extra CMake options, e.g. -DQSAPECNG_BUILD_TESTS=ON
set -euo pipefail

QWT_PREFIX=${1:?usage: build-appimage.sh <qwt-prefix> [output-dir]}
OUT_DIR=$(realpath -m "${2:-.}")
SRC_DIR=$(realpath "$(dirname "$0")/..")
BUILD_DIR=$(realpath -m "${BUILD_DIR:-build-appimage}")
TOOLS_DIR=$(realpath -m "${TOOLS_DIR:-$BUILD_DIR/tools}")
APPDIR="$BUILD_DIR/AppDir"

# pinned linuxdeploy releases
LINUXDEPLOY_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
PLUGIN_QT_URL=https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage

QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake || true)}
if [ -z "$QMAKE" ]; then
  echo "qmake for Qt 6 not found; set QMAKE" >&2
  exit 1
fi
export QMAKE
QT_PREFIX=$("$QMAKE" -query QT_INSTALL_PREFIX)

# --- build and install into the AppDir --------------------------------------
read -r -a EXTRA_CMAKE_ARGS <<< "${CMAKE_ARGS:-}"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
  -DQWT_ROOT_DIR="$QWT_PREFIX" \
  "${EXTRA_CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

VERSION=$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\(.*\)")$/\1/p' "$BUILD_DIR/CPackConfig.cmake")
[ -n "$VERSION" ] || VERSION=unknown

# --- linuxdeploy --------------------------------------------------------------
mkdir -p "$TOOLS_DIR"
[ -x "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" ] || \
  curl -fsSL --retry 5 -o "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" "$LINUXDEPLOY_URL"
[ -x "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" ] || \
  curl -fsSL --retry 5 -o "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" "$PLUGIN_QT_URL"
chmod +x "$TOOLS_DIR"/*.AppImage

# no FUSE needed (CI runners, containers)
export APPIMAGE_EXTRACT_AND_RUN=1
# let linuxdeploy find Qwt and the bundled Qt
QT_LIBS=$("$QMAKE" -query QT_INSTALL_LIBS)
export LD_LIBRARY_PATH="$QWT_PREFIX/lib:$QT_LIBS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
export PATH="$TOOLS_DIR:$PATH"

mkdir -p "$OUT_DIR"
cd "$OUT_DIR"
"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
  --appdir "$APPDIR" \
  --desktop-file "$APPDIR/usr/share/applications/qsapecng.desktop" \
  --icon-file "$SRC_DIR/src/gui/images/qsapecng-256x256.png" \
  --icon-filename qsapecng \
  --plugin qt \
  --output appimage

ls -l "$OUT_DIR"/*.AppImage
