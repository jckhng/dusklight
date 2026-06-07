#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_docker_aarch64_focal_bundle.sh [--image dusklight-portmaster-aarch64-focal] [--assets-dir /path/to/assets] [--out-dir artifacts/portmaster-focal]

Description:
  Builds Dusklight for Linux aarch64 in an Ubuntu 20.04 based Docker image, then
  stages a PortMaster zip. This targets older PortMaster firmwares whose glibc is
  too old for the default Debian bookworm build.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="dusklight-portmaster-aarch64-focal"
ASSETS_DIR=""
OUT_DIR="$ROOT_DIR/artifacts/portmaster-focal"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)
            IMAGE="$2"
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

docker build -f "$ROOT_DIR/packaging/portmaster/cmake-aarch64-focal.Dockerfile" -t "$IMAGE" "$ROOT_DIR"
docker run --rm -v "$ROOT_DIR:/work" "$IMAGE"

BUILD_DIR="$ROOT_DIR/build/portmaster-aarch64-focal"
docker run --rm -v "$ROOT_DIR:/work" "$IMAGE" bash -lc '
set -euo pipefail
for file in \
    /work/build/portmaster-aarch64-focal/install/dusklight \
    /work/build/portmaster-aarch64-focal/_deps/aurora_nod-build/libnod.so \
    /work/build/portmaster-aarch64-focal/_deps/sdl-build/libSDL3.so* \
    /work/build/portmaster-aarch64-focal/_deps/dawn-build/src/dawn/native/libwebgpu_dawn.so
do
    if [ -f "$file" ]; then
        aarch64-linux-gnu-strip --strip-unneeded "$file"
    fi
done
'

BIN_PATH="$BUILD_DIR/install/dusklight"
if [[ ! -f "$BIN_PATH" ]]; then
    BIN_PATH="$BUILD_DIR/dusklight"
fi
LIB_DIR="$BUILD_DIR/install"

ARGS=(
    --binary "$BIN_PATH"
    --lib-dir "$LIB_DIR"
    --lib-dir "$BUILD_DIR/_deps/aurora_nod-build"
    --lib-dir "$BUILD_DIR/_deps/sdl-build"
    --lib-dir "$BUILD_DIR/_deps/dawn-build/src/dawn/native"
    --out-dir "$OUT_DIR"
)
if [[ -n "$ASSETS_DIR" ]]; then
    ARGS+=(--assets-dir "$ASSETS_DIR")
fi

"$ROOT_DIR/scripts/portmaster/build_aarch64_bundle.sh" "${ARGS[@]}"
