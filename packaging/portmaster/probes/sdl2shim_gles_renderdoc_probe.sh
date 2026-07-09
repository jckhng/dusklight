#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
elif [ -d "/mnt/mmc/MUOS/PortMaster/" ]; then
  controlfolder="/mnt/mmc/MUOS/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR=/$directory/ports/sdl2shim-gles-renderdoc-probe/
RUNTIME_DIR="$GAMEDIR/runtime"
BIN="$GAMEDIR/sdl2shim-gles-present-probe.aarch64"

mkdir -p "$RUNTIME_DIR" "$RUNTIME_DIR/home" "$GAMEDIR/captures"

cd "$GAMEDIR"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export HOME="$RUNTIME_DIR/home"
export XDG_DATA_HOME="$RUNTIME_DIR"
export XDG_CACHE_HOME="$RUNTIME_DIR/cache"
export XDG_CONFIG_HOME="$RUNTIME_DIR/config"

export SDL_VIDEODRIVER=sdl2
export SDL3SHIM_SDL2_LIB="${SDL3SHIM_SDL2_LIB:-libSDL2-2.0.so.0}"
unset SDL_RENDER_DRIVER

export PROBE_RENDERDOC_CAPTURE_AFTER="${PROBE_RENDERDOC_CAPTURE_AFTER:-60}"
export PROBE_RENDERDOC_MODE="${PROBE_RENDERDOC_MODE:-trigger}"
export PROBE_RENDERDOC_LIB="${PROBE_RENDERDOC_LIB:-$GAMEDIR/renderdoc/lib/librenderdoc.so}"
export PROBE_RENDERDOC_CAPTURE_PATH="${PROBE_RENDERDOC_CAPTURE_PATH:-$GAMEDIR/captures/sdl2shim-gles-present-probe}"
export PROBE_IGNORE_INPUT="${PROBE_IGNORE_INPUT:-1}"

if [ -d "$GAMEDIR/renderdoc/lib" ]; then
  export LD_LIBRARY_PATH="$GAMEDIR/renderdoc/lib:$GAMEDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="$GAMEDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

echo "--- SDL2 Shim GLES RenderDoc PortMaster Probe ---"
echo "CFW_NAME=${CFW_NAME:-}"
echo "DEVICE_ARCH=${DEVICE_ARCH:-}"
echo "directory=${directory:-}"
echo "controlfolder=$controlfolder"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}"
echo "SDL3SHIM_SDL2_LIB=${SDL3SHIM_SDL2_LIB:-}"
echo "PROBE_RENDERDOC_CAPTURE_AFTER=${PROBE_RENDERDOC_CAPTURE_AFTER:-}"
echo "PROBE_RENDERDOC_MODE=${PROBE_RENDERDOC_MODE:-}"
echo "PROBE_RENDERDOC_LIB=${PROBE_RENDERDOC_LIB:-}"
echo "PROBE_RENDERDOC_CAPTURE_PATH=${PROBE_RENDERDOC_CAPTURE_PATH:-}"
echo "PROBE_IGNORE_INPUT=${PROBE_IGNORE_INPUT:-}"
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}"
echo "--- devices ---"
ls -l /dev/fb* /dev/dri/* /dev/mali* /dev/galcore 2>/dev/null || true
echo "--- renderdoc version ---"
if [ -x "$GAMEDIR/renderdoc/bin/renderdoccmd" ]; then
  "$GAMEDIR/renderdoc/bin/renderdoccmd" version 2>&1 || true
else
  echo "renderdoccmd not bundled"
fi
echo "--- probe start ---"

pm_platform_helper "$BIN"

"$BIN" "${PROBE_FRAMES:-180}"
RESULT=$?

echo "--- captures ---"
find "$GAMEDIR/captures" -maxdepth 1 -type f -ls 2>/dev/null || true
echo "--- probe end result=$RESULT ---"

pm_finish
exit "$RESULT"
