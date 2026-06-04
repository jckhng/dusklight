#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_docker_aarch64_prebuilt_target_runtime_bundle.sh --target-runtime-sysroot /tmp/dusklight-target-sysroot [--image dusklight-portmaster-aarch64-prebuilt-target-runtime] [--assets-dir /path/to/assets] [--out-dir artifacts/portmaster]

Description:
  Builds Dusklight for Linux aarch64 using Aurora's prebuilt Dawn package, while
  resolving final link symbols against a runtime library snapshot from a target
  PortMaster-capable device. This is a fast local compatibility probe, not a
  requirement for the distributable port package.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="dusklight-portmaster-aarch64-prebuilt-target-runtime"
BASE_IMAGE="dusklight-portmaster-aarch64"
TARGET_RUNTIME_SYSROOT=""
ASSETS_DIR=""
OUT_DIR="$ROOT_DIR/artifacts/portmaster"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)
            IMAGE="$2"
            shift 2
            ;;
        --base-image)
            BASE_IMAGE="$2"
            shift 2
            ;;
        --target-runtime-sysroot)
            TARGET_RUNTIME_SYSROOT="$2"
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

if [[ -z "$TARGET_RUNTIME_SYSROOT" ]]; then
    echo "--target-runtime-sysroot is required" >&2
    usage
    exit 1
fi
if [[ ! -d "$TARGET_RUNTIME_SYSROOT" ]]; then
    echo "Target runtime sysroot not found: $TARGET_RUNTIME_SYSROOT" >&2
    exit 1
fi

docker build -f "$ROOT_DIR/packaging/portmaster/cmake-aarch64.Dockerfile" -t "$BASE_IMAGE" "$ROOT_DIR"
docker build -f "$ROOT_DIR/packaging/portmaster/cmake-aarch64-prebuilt-target-runtime.Dockerfile" -t "$IMAGE" "$ROOT_DIR"
docker run --rm \
    -v "$ROOT_DIR:/work" \
    -v "$TARGET_RUNTIME_SYSROOT:/target-runtime-sysroot:ro" \
    -e TARGET_RUNTIME_SYSROOT=/target-runtime-sysroot \
    "$IMAGE"

BUILD_DIR="$ROOT_DIR/build/portmaster-aarch64"
BIN_PATH="$BUILD_DIR/dusklight.stripped"
NOD_LIB_DIR="$BUILD_DIR/_deps/aurora_nod-build"
SDL_LIB_DIR="$BUILD_DIR/_deps/sdl-build"

ARGS=(--binary "$BIN_PATH" --lib-dir "$NOD_LIB_DIR" --lib-dir "$SDL_LIB_DIR" --out-dir "$OUT_DIR")
if [[ -n "$ASSETS_DIR" ]]; then
    ARGS+=(--assets-dir "$ASSETS_DIR")
fi

"$ROOT_DIR/scripts/portmaster/build_aarch64_bundle.sh" "${ARGS[@]}"
