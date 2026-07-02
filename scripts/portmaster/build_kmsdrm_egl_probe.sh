#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_kmsdrm_egl_probe.sh [--image dusklight-portmaster-aarch64-focal] [--out-dir artifacts/kmsdrm-egl-probe]

Description:
  Builds the small aarch64 KMSDRM/EGL probe against the already-configured bmd
  SDL3 tree in build/portmaster-aarch64-focal. This avoids rebuilding Dusklight.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="dusklight-portmaster-aarch64-focal"
OUT_DIR="$ROOT_DIR/artifacts/kmsdrm-egl-probe"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)
            IMAGE="$2"
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

BUILD_DIR="$ROOT_DIR/build/portmaster-aarch64-focal"
SDL_SRC="$BUILD_DIR/_deps/sdl-src"
SDL_BUILD="$BUILD_DIR/_deps/sdl-build"

if [[ ! -f "$SDL_SRC/include/SDL3/SDL.h" || ! -f "$SDL_BUILD/libSDL3.so.0" ]]; then
    cat >&2 <<EOF
Missing bmd SDL build artifacts under:
  $SDL_SRC
  $SDL_BUILD

Run the focal PortMaster build first:
  scripts/portmaster/build_docker_aarch64_focal_bundle.sh --out-dir artifacts/bmd-sdl-test
EOF
    exit 1
fi

mkdir -p "$OUT_DIR"

docker run --rm -v "$ROOT_DIR:/work" "$IMAGE" bash -lc '
set -euo pipefail
mkdir -p /work/artifacts/kmsdrm-egl-probe
aarch64-linux-gnu-gcc-10 \
  -O2 -g0 \
  -I/work/build/portmaster-aarch64-focal/_deps/sdl-build/include-config-release \
  -I/work/build/portmaster-aarch64-focal/_deps/sdl-build/include-revision \
  -I/work/build/portmaster-aarch64-focal/_deps/sdl-src/include \
  /work/packaging/portmaster/probes/kmsdrm_egl_probe.c \
  -L/work/build/portmaster-aarch64-focal/_deps/sdl-build \
  -Wl,-rpath,'\''$ORIGIN'\'' \
  -lSDL3 -lEGL \
  -o /work/artifacts/kmsdrm-egl-probe/kmsdrm-egl-probe.aarch64
aarch64-linux-gnu-strip --strip-unneeded /work/artifacts/kmsdrm-egl-probe/kmsdrm-egl-probe.aarch64
chown -R '"$(id -u):$(id -g)"' /work/artifacts/kmsdrm-egl-probe
'

cp -a "$SDL_BUILD"/libSDL3.so* "$OUT_DIR/"
cp -f "$ROOT_DIR/packaging/portmaster/probes/kmsdrm_egl_probe_README.md" "$OUT_DIR/README.md"

cat > "$OUT_DIR/run-kmsdrm-egl-probe.sh" <<'EOF'
#!/bin/sh
cd "$(dirname "$0")" || exit 1
export LD_LIBRARY_PATH="$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER=kmsdrm
./kmsdrm-egl-probe.aarch64 2>&1 | tee kmsdrm-egl-probe.log
EOF
chmod +x "$OUT_DIR/run-kmsdrm-egl-probe.sh" "$OUT_DIR/kmsdrm-egl-probe.aarch64"

tar -C "$(dirname "$OUT_DIR")" -czf "$OUT_DIR.tar.gz" "$(basename "$OUT_DIR")"

echo "Built: $OUT_DIR"
echo "Archive: $OUT_DIR.tar.gz"
