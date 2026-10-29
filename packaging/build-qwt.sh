#!/usr/bin/env bash
# Download, verify and build Qwt against the Qt 6 found via $QMAKE (or qmake6
# / qmake on PATH), installing it into the given prefix.
#
# usage: packaging/build-qwt.sh <install-prefix> [qwt-version]
#        QWT_TARBALL=/path/qwt-6.3.0.tar.bz2 packaging/build-qwt.sh ...  (offline)
#
# Only the plotting library is built (no designer plugin, examples or docs).
set -euo pipefail

PREFIX=${1:?usage: build-qwt.sh <install-prefix> [qwt-version]}
QWT_VERSION=${2:-6.3.0}

# SHA-256 of the official SourceForge tarballs (same values as Homebrew's).
case "$QWT_VERSION" in
  6.3.0) QWT_SHA256=dcb085896c28aaec5518cbc08c0ee2b4e60ada7ac929d82639f6189851a6129a ;;
  *) echo "no known checksum for Qwt $QWT_VERSION; add it to $0" >&2; exit 1 ;;
esac

QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake || true)}
if [ -z "$QMAKE" ]; then
  echo "qmake for Qt 6 not found; set QMAKE" >&2
  exit 1
fi
echo "Building Qwt $QWT_VERSION with $QMAKE ($("$QMAKE" -query QT_VERSION))"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

TARBALL="$WORK/qwt-$QWT_VERSION.tar.bz2"
if [ -n "${QWT_TARBALL:-}" ]; then
  cp "$QWT_TARBALL" "$TARBALL"          # pre-downloaded copy (offline builds)
else
  curl -fsSL --retry 5 --retry-delay 5 -o "$TARBALL" \
    "https://downloads.sourceforge.net/project/qwt/qwt/$QWT_VERSION/qwt-$QWT_VERSION.tar.bz2"
fi
echo "$QWT_SHA256  $TARBALL" | sha256sum -c -

tar -xjf "$TARBALL" -C "$WORK"
cd "$WORK/qwt-$QWT_VERSION"

# install prefix + only what QSapecNG needs
sed -i \
  -e "s|^\(\s*QWT_INSTALL_PREFIX\s*=\).*/usr/local/qwt-.*|\1 $PREFIX|" \
  -e 's/^QWT_CONFIG\s*+=\s*QwtDesigner/#&/' \
  -e 's/^QWT_CONFIG\s*+=\s*QwtExamples/#&/' \
  -e 's/^QWT_CONFIG\s*+=\s*QwtPlayground/#&/' \
  -e 's/^QWT_CONFIG\s*+=\s*QwtTests/#&/' \
  qwtconfig.pri

"$QMAKE" qwt.pro
make -j"$(nproc)"
make install

echo "Qwt installed to $PREFIX"
