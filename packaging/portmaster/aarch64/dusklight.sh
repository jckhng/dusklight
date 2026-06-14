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

if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
fi
[ -n "${CFW_NAME:-}" ] && [ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
if command -v get_controls >/dev/null 2>&1; then
  get_controls
fi

directory=${directory:-roms}
if [ -z "${DEVICE_ARCH:-}" ]; then
  case "$(uname -m 2>/dev/null)" in
    aarch64|arm64)
      DEVICE_ARCH=aarch64
      ;;
    *)
      DEVICE_ARCH=aarch64
      ;;
  esac
fi

GAMEDIR=/$directory/ports/dusklight/
RUNTIME_DIR="$GAMEDIR/runtime"
ASSETS_DIR="$GAMEDIR/assets"
BIN="$GAMEDIR/dusklight.${DEVICE_ARCH}"

mkdir -p "$RUNTIME_DIR" "$RUNTIME_DIR/home" "$ASSETS_DIR"

cd "$GAMEDIR"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export HOME="$RUNTIME_DIR/home"
export XDG_DATA_HOME="$RUNTIME_DIR"
export XDG_CACHE_HOME="$RUNTIME_DIR/cache"
export XDG_CONFIG_HOME="$RUNTIME_DIR/config"

export DUSKLIGHT_PM_GRAPHICS_MODE="${DUSKLIGHT_PM_GRAPHICS_MODE:-dawn-sdl2shim}"
export DUSKLIGHT_PORTMASTER_LOW_SPEC=1
export DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
export DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
export DUSKLIGHT_PORTMASTER_GX_STATS="${DUSKLIGHT_PORTMASTER_GX_STATS:-0}"
export DUSKLIGHT_PORTMASTER_NOINDEX_TRIANGLES=1
export DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY="${DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY:-1}"
export DUSKLIGHT_PORTMASTER_BATCH_STRIPS="${DUSKLIGHT_PORTMASTER_BATCH_STRIPS:-1}"
export DUSKLIGHT_PORTMASTER_BATCH_QUADS="${DUSKLIGHT_PORTMASTER_BATCH_QUADS:-1}"
export DUSKLIGHT_PORTMASTER_BATCH_REUSE_CACHE="${DUSKLIGHT_PORTMASTER_BATCH_REUSE_CACHE:-1}"
export DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS="${DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS:-30}"
export DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS="${DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS:-4}"
export DUSKLIGHT_PORTMASTER_DISABLE_DEPTH_PEEK=1
export DUSKLIGHT_PORTMASTER_DRAW_SKIP="${DUSKLIGHT_PORTMASTER_DRAW_SKIP:-0}"
export DUSKLIGHT_PORTMASTER_SKIP_PIPELINE_CACHE_LOAD="${DUSKLIGHT_PORTMASTER_SKIP_PIPELINE_CACHE_LOAD:-1}"
export DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW="${DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW:-1}"
export DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW="${DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW:-1}"
export DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW="${DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW:-1}"
export DUSKLIGHT_PORTMASTER_INPUT_DIAG="${DUSKLIGHT_PORTMASTER_INPUT_DIAG:-0}"
export DUSKLIGHT_PORTMASTER_IGNORE_CONTROLLER_MAPPINGS="${DUSKLIGHT_PORTMASTER_IGNORE_CONTROLLER_MAPPINGS:-1}"

log_platform_info() {
  echo "--- Dusklight PortMaster platform diagnostic ---"
  echo "DUSKLIGHT_PM_GRAPHICS_MODE=$DUSKLIGHT_PM_GRAPHICS_MODE"
  echo "CFW_NAME=${CFW_NAME:-}"
  echo "DEVICE_ARCH=${DEVICE_ARCH:-}"
  echo "directory=${directory:-}"
  echo "controlfolder=$controlfolder"
  echo "uname=$(uname -a 2>/dev/null)"
  echo "--- os-release ---"
  cat /etc/os-release 2>/dev/null || true
  echo "--- display env ---"
  echo "DISPLAY=${DISPLAY:-}"
  echo "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
  echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-}"
  echo "SDL3SHIM_SDL2_LIB=${SDL3SHIM_SDL2_LIB:-}"
  echo "SDL3SHIM_SDL2_VIDEODRIVER=${SDL3SHIM_SDL2_VIDEODRIVER:-}"
  echo "SDL3SHIM_SDL2_AUDIODRIVER=${SDL3SHIM_SDL2_AUDIODRIVER:-}"
  echo "DUSKLIGHT_PORTMASTER_X11_DAWN=${DUSKLIGHT_PORTMASTER_X11_DAWN:-}"
  echo "DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=${DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE:-}"
  echo "DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=${DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE:-}"
  echo "DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=${DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE:-}"
  echo "DUSKLIGHT_PORTMASTER_IGNORE_CONTROLLER_MAPPINGS=${DUSKLIGHT_PORTMASTER_IGNORE_CONTROLLER_MAPPINGS:-}"
  echo "--- devices ---"
  ls -l /dev/fb* /dev/dri/* /dev/mali* /dev/galcore 2>/dev/null || true
  echo "--- graphics libraries ---"
  find /usr /lib /opt -name 'libEGL*' -o -name 'libGLES*' -o -name 'libgbm*' -o -name 'libdrm*' 2>/dev/null | head -100 || true
  echo "--- binary deps ---"
  ldd "$BIN" 2>&1 | grep -Ei 'not found|egl|gles|gbm|drm|sdl|wayland|x11|libc|libstdc' || true
  echo "--- input devices ---"
  grep -E '^(N:|H:)' /proc/bus/input/devices 2>/dev/null || true
  echo "--- end diagnostic ---"
}

resolve_graphics_mode() {
  case "$DUSKLIGHT_PM_GRAPHICS_MODE" in
    auto)
      if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ -e /dev/fb0 ]; then
        echo "dawn-fbdev-sentinel"
      elif ls /dev/dri/card* >/dev/null 2>&1; then
        echo "dawn-kmsdrm"
      else
        echo "dawn-sdl"
      fi
      ;;
    dawn-sdl|dawn-wayland|dawn-x11|dawn-kmsdrm|dawn-fbdev-sentinel|dawn-sdl2shim|diag)
      echo "$DUSKLIGHT_PM_GRAPHICS_MODE"
      ;;
    *)
      echo "Unknown DUSKLIGHT_PM_GRAPHICS_MODE=$DUSKLIGHT_PM_GRAPHICS_MODE, falling back to dawn-sdl" >&2
      DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl
      echo "dawn-sdl"
      ;;
  esac
}

apply_graphics_mode() {
  case "$1" in
    dawn-sdl)
      unset SDL_VIDEODRIVER
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      unset DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE
      unset DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE
      ;;
    dawn-wayland)
      export SDL_VIDEODRIVER=wayland
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      unset DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE
      unset DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE
      ;;
    dawn-x11)
      export SDL_VIDEODRIVER=x11
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      unset DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE
      unset DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE
      ;;
    dawn-kmsdrm)
      export SDL_VIDEODRIVER=kmsdrm
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      unset DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE
      unset DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE
      ;;
    dawn-fbdev-sentinel)
      export SDL_VIDEODRIVER=offscreen
      export DUSKLIGHT_PORTMASTER_X11_DAWN=1
      export DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
      unset DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE
      unset DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE
      ;;
    dawn-sdl2shim)
      export SDL_VIDEODRIVER=sdl2
      export SDL3SHIM_SDL2_LIB="${SDL3SHIM_SDL2_LIB:-libSDL2-2.0.so.0}"
      export DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=1
      export DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      ;;
    diag)
      ;;
    *)
      echo "Unsupported graphics mode: $1"
      return 1
      ;;
  esac
}

if grep -q 'Name="muOS-Keys"' /proc/bus/input/devices 2>/dev/null; then
  sdl_controllerconfig="$(grep -m1 ',muOS-Keys,' "$GAMEDIR/res/gamecontrollerdb.txt")"
fi
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export LD_LIBRARY_PATH="$GAMEDIR/lib.${DEVICE_ARCH}:$GAMEDIR/libs.${DEVICE_ARCH}:$GAMEDIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

GPTOKEYB_PID=""
finish_portmaster() {
  if [ -n "$GPTOKEYB_PID" ]; then
    kill "$GPTOKEYB_PID" 2>/dev/null || true
    wait "$GPTOKEYB_PID" 2>/dev/null || true
    GPTOKEYB_PID=""
  fi

  if command -v pm_finish >/dev/null 2>&1; then
    pm_finish
  fi
}
trap finish_portmaster EXIT

RESOLVED_GRAPHICS_MODE="$(resolve_graphics_mode)"
apply_graphics_mode "$RESOLVED_GRAPHICS_MODE"
echo "Dusklight PortMaster graphics mode: requested=$DUSKLIGHT_PM_GRAPHICS_MODE resolved=$RESOLVED_GRAPHICS_MODE SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-default}"
log_platform_info

if [ "$RESOLVED_GRAPHICS_MODE" = "diag" ]; then
  exit 0
fi

DVD_PATH=""
for candidate in "$ASSETS_DIR"/*.iso "$ASSETS_DIR"/*.ISO "$ASSETS_DIR"/*.gcm "$ASSETS_DIR"/*.GCM "$ASSETS_DIR"/*.rvz "$ASSETS_DIR"/*.RVZ; do
  if [ -f "$candidate" ]; then
    DVD_PATH="$candidate"
    break
  fi
done

if [ ! -x "$BIN" ]; then
  echo "Fatal: expected binary not found or not executable: $BIN"
  echo "DEVICE_ARCH=$DEVICE_ARCH"
  echo "Package should contain: $GAMEDIR/dusklight.aarch64"
  exit 1
fi

if [ -n "${GPTOKEYB:-}" ] && [ -x "$GPTOKEYB" ]; then
  "$GPTOKEYB" "dusklight.${DEVICE_ARCH}" -c "$GAMEDIR/dusklight.gptk" &
  GPTOKEYB_PID=$!
  echo "Started gptokeyb pid=$GPTOKEYB_PID for PortMaster quit combo"
else
  echo "Warning: GPTOKEYB is not available; PortMaster quit combo will not work"
fi

if command -v pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$BIN"
fi

DUSKLIGHT_ARGS=(
  --log-level 1
  --backend opengles
  --cvar backend.graphicsBackend=opengles
  --cvar backend.skipPreLaunchUI=true
  --cvar backend.showPipelineCompilation=false
  --cvar backend.checkForUpdates=false
  --cvar video.enableFullscreen=true
  --cvar video.enableVsync=false
  --cvar video.maxFrameRate=30
  --cvar game.bloomMode=0
  --cvar game.bloomMultiplier=0
  --cvar game.depthOfFieldMode=0
  --cvar game.disableWaterRefraction=true
  --cvar game.disableCutscenePillarboxing=true
  --cvar game.disableRupeeCutscenes=true
  --cvar game.enableTextureReplacements=false
  --cvar game.enableFrameInterpolation=0
  --cvar game.fastTears=true
  --cvar game.instantSaves=true
  --cvar game.instantText=true
  --cvar game.internalResolutionScale=0
  --cvar game.shadowResolutionMultiplier=0
  --cvar game.resampler=0
  --cvar game.enableMapBackground=false
  --cvar game.enableAchievementToasts=false
  --cvar game.enableControllerToasts=false
  --cvar game.enableDiscordPresence=false
  --cvar audio.enableReverb=false
  --cvar audio.enableHrtf=false
)

if [ -n "$DVD_PATH" ]; then
  DUSKLIGHT_ARGS+=("$DVD_PATH")
fi

"$BIN" "${DUSKLIGHT_ARGS[@]}"
