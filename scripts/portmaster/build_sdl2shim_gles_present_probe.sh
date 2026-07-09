#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  scripts/portmaster/build_sdl2shim_gles_present_probe.sh [--image dusklight-portmaster-aarch64-focal-sdl2shim] [--out-dir artifacts/sdl2shim-gles-present-probe] [--portmaster-out-dir artifacts/sdl2shim-gles-renderdoc-portmaster]

Description:
  Builds a small aarch64 probe against the bmdhacks SDL3 sdl2-backend build
  already produced by the focal-sdl2shim Dusklight build. The probe creates an
  SDL OpenGLES window, clears changing colors, swaps, then exits.
EOF
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="dusklight-portmaster-aarch64-focal-sdl2shim"
OUT_DIR="$ROOT_DIR/artifacts/sdl2shim-gles-present-probe"
PORTMASTER_OUT_DIR="$ROOT_DIR/artifacts/sdl2shim-gles-renderdoc-portmaster"

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
        --portmaster-out-dir)
            PORTMASTER_OUT_DIR="$2"
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

BUILD_DIR="$ROOT_DIR/build/portmaster-aarch64-focal-sdl2shim"
SDL_SRC="$BUILD_DIR/_deps/sdl-src"
SDL_BUILD="$BUILD_DIR/_deps/sdl-build"

if [[ ! -f "$SDL_SRC/include/SDL3/SDL.h" || ! -f "$SDL_BUILD/libSDL3.so.0" ]]; then
    cat >&2 <<EOF
Missing bmd SDL build artifacts under:
  $SDL_SRC
  $SDL_BUILD

Run the sdl2shim PortMaster build first:
  scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh --out-dir artifacts/portmaster-focal-sdl2shim
EOF
    exit 1
fi

mkdir -p "$OUT_DIR"

docker run --rm -v "$ROOT_DIR:/work" "$IMAGE" bash -lc '
set -euo pipefail
mkdir -p /work/artifacts/sdl2shim-gles-present-probe
aarch64-linux-gnu-gcc-10 \
  -O2 -g0 \
  -I/work/build/portmaster-aarch64-focal-sdl2shim/_deps/sdl-build/include-config-release \
  -I/work/build/portmaster-aarch64-focal-sdl2shim/_deps/sdl-build/include-revision \
  -I/work/build/portmaster-aarch64-focal-sdl2shim/_deps/sdl-src/include \
  /work/packaging/portmaster/probes/sdl2shim_gles_present_probe.c \
  -L/work/build/portmaster-aarch64-focal-sdl2shim/_deps/sdl-build \
  -Wl,-rpath,'\''$ORIGIN'\'' \
  -lSDL3 -lGLESv2 -ldl \
  -o /work/artifacts/sdl2shim-gles-present-probe/sdl2shim-gles-present-probe.aarch64
aarch64-linux-gnu-strip --strip-unneeded /work/artifacts/sdl2shim-gles-present-probe/sdl2shim-gles-present-probe.aarch64
chown -R '"$(id -u):$(id -g)"' /work/artifacts/sdl2shim-gles-present-probe
'

cp -a "$SDL_BUILD"/libSDL3.so* "$OUT_DIR/"
cp -f "$ROOT_DIR/packaging/portmaster/probes/sdl2shim_gles_present_probe_README.md" "$OUT_DIR/README.md"
if [[ -d "$ROOT_DIR/artifacts/renderdoc-arm64-bullseye-stage" ]]; then
    mkdir -p "$OUT_DIR/renderdoc"
    cp -a "$ROOT_DIR/artifacts/renderdoc-arm64-bullseye-stage"/bin "$OUT_DIR/renderdoc/"
    cp -a "$ROOT_DIR/artifacts/renderdoc-arm64-bullseye-stage"/lib "$OUT_DIR/renderdoc/"
fi

cat > "$OUT_DIR/run-sdl2shim-gles-present-probe.sh" <<'EOF'
#!/bin/sh
cd "$(dirname "$0")" || exit 1
if [ -d "$PWD/renderdoc/lib" ]; then
  export LD_LIBRARY_PATH="$PWD/renderdoc/lib:$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export SDL_VIDEODRIVER=sdl2
export SDL3SHIM_SDL2_LIB="${SDL3SHIM_SDL2_LIB:-libSDL2-2.0.so.0}"
export PROBE_RENDERDOC_CAPTURE_AFTER="${PROBE_RENDERDOC_CAPTURE_AFTER:-0}"
export PROBE_RENDERDOC_MODE="${PROBE_RENDERDOC_MODE:-trigger}"
export PROBE_RENDERDOC_LIB="${PROBE_RENDERDOC_LIB:-./renderdoc/lib/librenderdoc.so}"
export PROBE_RENDERDOC_CAPTURE_PATH="${PROBE_RENDERDOC_CAPTURE_PATH:-./captures/sdl2shim-gles-present-probe}"
unset SDL_RENDER_DRIVER
./sdl2shim-gles-present-probe.aarch64 "$@" 2>&1 | tee sdl2shim-gles-present-probe.log
EOF
chmod +x "$OUT_DIR/run-sdl2shim-gles-present-probe.sh" "$OUT_DIR/sdl2shim-gles-present-probe.aarch64"

tar -C "$(dirname "$OUT_DIR")" -czf "$OUT_DIR.tar.gz" "$(basename "$OUT_DIR")"

PM_STAGE_DIR="$PORTMASTER_OUT_DIR/Sdl2ShimGlesRenderdocProbe"
PM_GAME_DIR="$PM_STAGE_DIR/sdl2shim-gles-renderdoc-probe"
rm -rf "$PM_STAGE_DIR"
mkdir -p "$PM_GAME_DIR"
cp -a "$OUT_DIR"/. "$PM_GAME_DIR/"
cp -f "$ROOT_DIR/packaging/portmaster/probes/sdl2shim_gles_renderdoc_probe.sh" \
    "$PM_STAGE_DIR/sdl2shim-gles-renderdoc-probe.sh"
cp -f "$ROOT_DIR/packaging/portmaster/probes/sdl2shim_gles_renderdoc_probe_port.json" \
    "$PM_GAME_DIR/port.json"
chmod +x "$PM_STAGE_DIR/sdl2shim-gles-renderdoc-probe.sh" \
    "$PM_GAME_DIR/run-sdl2shim-gles-present-probe.sh" \
    "$PM_GAME_DIR/sdl2shim-gles-present-probe.aarch64"

(
    cd "$PM_STAGE_DIR"
    ZIP_PATH="$PORTMASTER_OUT_DIR/Sdl2ShimGlesRenderdocProbe-portmaster.zip"
    rm -f "$ZIP_PATH"
    if command -v zip >/dev/null 2>&1; then
        zip -r "$ZIP_PATH" sdl2shim-gles-renderdoc-probe.sh sdl2shim-gles-renderdoc-probe >/dev/null
    elif command -v bsdtar >/dev/null 2>&1; then
        bsdtar -a -cf "$ZIP_PATH" sdl2shim-gles-renderdoc-probe.sh sdl2shim-gles-renderdoc-probe
    else
        echo "Need zip or bsdtar to create PortMaster archive" >&2
        exit 1
    fi
)

echo "Built: $OUT_DIR"
echo "Archive: $OUT_DIR.tar.gz"
echo "PortMaster package: $PORTMASTER_OUT_DIR/Sdl2ShimGlesRenderdocProbe-portmaster.zip"
