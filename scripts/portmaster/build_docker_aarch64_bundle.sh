#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_docker_aarch64_bundle.sh [--image dusklight-portmaster-aarch64] [--assets-dir /path/to/assets] [--out-dir artifacts/portmaster]

Description:
  Builds Dusklight for Linux aarch64 in Docker, then stages a PortMaster zip.
  The Docker build uses Aurora's prebuilt Linux aarch64 Dawn package, vendored
  SDL3, and source-built nod for DVD support.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="dusklight-portmaster-aarch64"
ASSETS_DIR=""
OUT_DIR="$ROOT_DIR/artifacts/portmaster"

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

docker build -f "$ROOT_DIR/packaging/portmaster/cmake-aarch64.Dockerfile" -t "$IMAGE" "$ROOT_DIR"
docker run --rm -v "$ROOT_DIR:/work" "$IMAGE"

BUILD_DIR="$ROOT_DIR/build/portmaster-aarch64"
BIN_PATH="$BUILD_DIR/Binaries/dusklight"
LIB_DIR="$BUILD_DIR/install"

ARGS=(--binary "$BIN_PATH" --lib-dir "$LIB_DIR" --out-dir "$OUT_DIR")
if [[ -n "$ASSETS_DIR" ]]; then
    ARGS+=(--assets-dir "$ASSETS_DIR")
fi

"$ROOT_DIR/scripts/portmaster/build_aarch64_bundle.sh" "${ARGS[@]}"

