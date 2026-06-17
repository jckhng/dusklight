#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_aarch64_bundle.sh --binary /path/to/dusklight [--lib-dir /path/to/libs ...] [--assets-dir /path/to/assets] [--out-dir artifacts/portmaster]

Description:
  Stages a generic Linux aarch64 PortMaster package from packaging/portmaster/aarch64 and emits a zip.
  The binary should be a Linux aarch64 Dusklight executable built with OpenGLES
  available in Aurora/Dawn.

  --assets-dir is for private device testing only. It copies local *.iso, *.ISO,
  *.gcm, *.GCM, *.rvz, and *.RVZ files into dusklight/assets. Do not distribute packages
  containing game data.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEMPLATE_DIR="$ROOT_DIR/packaging/portmaster/aarch64"
OUT_DIR="$ROOT_DIR/artifacts/portmaster"
BIN_PATH=""
LIB_DIRS=()
ASSETS_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --binary)
            BIN_PATH="$2"
            shift 2
            ;;
        --lib-dir)
            LIB_DIRS+=("$2")
            shift 2
            ;;
        --assets-dir)
            ASSETS_DIR="$2"
            shift 2
            ;;
        --out-dir)
            OUT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ -z "$BIN_PATH" ]]; then
    echo "--binary is required" >&2
    usage
    exit 1
fi
if [[ ! -f "$BIN_PATH" ]]; then
    echo "Binary not found: $BIN_PATH" >&2
    exit 1
fi
if [[ "$OUT_DIR" != /* ]]; then
    OUT_DIR="$ROOT_DIR/$OUT_DIR"
fi

case "$(file -b "$BIN_PATH")" in
    *"ARM aarch64"*) ;;
    *)
        echo "Expected an aarch64 binary, got: $(file -b "$BIN_PATH")" >&2
        exit 1
        ;;
esac

STAGE_DIR="$OUT_DIR/Dusklight"
rm -rf "$STAGE_DIR"
mkdir -p "$OUT_DIR"
cp -a "$TEMPLATE_DIR" "$STAGE_DIR"

cp -a "$BIN_PATH" "$STAGE_DIR/dusklight/dusklight.aarch64"
chmod +x "$STAGE_DIR/dusklight.sh" "$STAGE_DIR/dusklight/dusklight.aarch64"

rm -rf "$STAGE_DIR/dusklight/res"
cp -a "$ROOT_DIR/res" "$STAGE_DIR/dusklight/res"

if [[ ${#LIB_DIRS[@]} -gt 0 ]]; then
    mkdir -p "$STAGE_DIR/dusklight/lib.aarch64"
    for LIB_DIR in "${LIB_DIRS[@]}"; do
        if [[ ! -d "$LIB_DIR" ]]; then
            echo "Library directory not found: $LIB_DIR" >&2
            exit 1
        fi
        find "$LIB_DIR" -maxdepth 1 \( -type f -o -type l \) -name '*.so*' -exec cp -aL {} "$STAGE_DIR/dusklight/lib.aarch64/" \;
    done
fi

if [[ -n "$ASSETS_DIR" ]]; then
    if [[ ! -d "$ASSETS_DIR" ]]; then
        echo "Asset directory not found: $ASSETS_DIR" >&2
        exit 1
    fi
    find "$ASSETS_DIR" -maxdepth 1 \( -type f -o -type l \) \( -name '*.iso' -o -name '*.ISO' -o -name '*.gcm' -o -name '*.GCM' -o -name '*.rvz' -o -name '*.RVZ' \) -exec cp -a {} "$STAGE_DIR/dusklight/assets/" \;
fi

(
    cd "$STAGE_DIR"
    ZIP_PATH="$OUT_DIR/dusklight.zip"
    rm -f "$ZIP_PATH"
    if command -v zip >/dev/null 2>&1; then
        zip -r "$ZIP_PATH" dusklight.sh dusklight >/dev/null
    elif command -v bsdtar >/dev/null 2>&1; then
        bsdtar -a -cf "$ZIP_PATH" dusklight.sh dusklight
    else
        echo "Need zip or bsdtar to create PortMaster archive" >&2
        exit 1
    fi
)

echo "Built: $OUT_DIR/dusklight.zip"
