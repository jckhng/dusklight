#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh [--image dusklight-portmaster-aarch64-focal-sdl2shim] [--assets-dir /path/to/assets] [--out-dir artifacts/portmaster-focal-sdl2shim] [--incremental]

Description:
  Builds an experimental PortMaster package with bmdhacks SDL sdl2-backend.
  This is for compatibility testing on firmware whose working display stack is
  exposed through a patched SDL2 library.

  Use --incremental during local iteration to reuse build/portmaster-aarch64-focal-sdl2shim
  instead of deleting and reconfiguring the whole Dawn/SDL tree.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="dusklight-portmaster-aarch64-focal-sdl2shim"
ASSETS_DIR=""
OUT_DIR="$ROOT_DIR/artifacts/portmaster-focal-sdl2shim"
INCREMENTAL=0

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
        --incremental)
            INCREMENTAL=1
            shift
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

docker build -f "$ROOT_DIR/packaging/portmaster/cmake-aarch64-focal-sdl2shim.Dockerfile" -t "$IMAGE" "$ROOT_DIR"

BUILD_DIR="$ROOT_DIR/build/portmaster-aarch64-focal-sdl2shim"
if [[ "$INCREMENTAL" == "1" && -f "$BUILD_DIR/build.ninja" ]]; then
    docker run --rm -v "$ROOT_DIR:/work" "$IMAGE" bash -lc '
set -euo pipefail
cmake --build /work/build/portmaster-aarch64-focal-sdl2shim --target dusklight -j$(nproc)
cmake --install /work/build/portmaster-aarch64-focal-sdl2shim
file /work/build/portmaster-aarch64-focal-sdl2shim/install/dusklight
'
else
    docker run --rm -v "$ROOT_DIR:/work" "$IMAGE"
fi

docker run --rm -v "$ROOT_DIR:/work" "$IMAGE" bash -lc '
set -euo pipefail
for file in \
    /work/build/portmaster-aarch64-focal-sdl2shim/install/dusklight \
    /work/build/portmaster-aarch64-focal-sdl2shim/_deps/aurora_nod-build/libnod.so \
    /work/build/portmaster-aarch64-focal-sdl2shim/_deps/sdl-build/libSDL3.so* \
    /work/build/portmaster-aarch64-focal-sdl2shim/_deps/dawn-build/src/dawn/native/libwebgpu_dawn.so
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

ARGS=(
    --binary "$BIN_PATH"
    --lib-dir "$BUILD_DIR/install"
    --lib-dir "$BUILD_DIR/_deps/aurora_nod-build"
    --lib-dir "$BUILD_DIR/_deps/sdl-build"
    --lib-dir "$BUILD_DIR/_deps/dawn-build/src/dawn/native"
    --out-dir "$OUT_DIR"
)
if [[ -n "$ASSETS_DIR" ]]; then
    ARGS+=(--assets-dir "$ASSETS_DIR")
fi

"$ROOT_DIR/scripts/portmaster/build_aarch64_bundle.sh" "${ARGS[@]}"
