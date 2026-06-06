#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PKG_SRC="${ROOT_DIR}/package/bootstrap"
DIST_DIR="${ROOT_DIR}/dist/plumos-bootstrap"
ARCHIVE="${ROOT_DIR}/dist/plumos-bootstrap.tar.gz"

if [ ! -d "$PKG_SRC" ]; then
  echo "error: missing package source: $PKG_SRC" >&2
  exit 1
fi

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR" "${ROOT_DIR}/dist"
cp -R "${PKG_SRC}/." "$DIST_DIR/"

chmod 0755 "${DIST_DIR}/plumos/bootstrap/MainUI.wrapper"

rm -f "$ARCHIVE"
tar -C "${ROOT_DIR}/dist" -czf "$ARCHIVE" "$(basename "$DIST_DIR")"

echo "Built ${DIST_DIR}"
echo "Built ${ARCHIVE}"
find "$DIST_DIR" -type f | sed "s#${DIST_DIR}/##" | sort
