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

export DUSKLIGHT_PORTMASTER_X11_DAWN=1
export DUSKLIGHT_PORTMASTER_LOW_SPEC=1
export DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
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
export SDL_VIDEODRIVER=offscreen

if grep -q 'Name="muOS-Keys"' /proc/bus/input/devices 2>/dev/null; then
  sdl_controllerconfig="$(grep -m1 ',muOS-Keys,' "$GAMEDIR/res/gamecontrollerdb.txt")"
fi
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export LD_LIBRARY_PATH="$GAMEDIR/lib.${DEVICE_ARCH}:$GAMEDIR/libs.${DEVICE_ARCH}:$GAMEDIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

DVD_PATH=""
for candidate in "$ASSETS_DIR"/*.iso "$ASSETS_DIR"/*.ISO "$ASSETS_DIR"/*.gcm "$ASSETS_DIR"/*.GCM "$ASSETS_DIR"/*.rvz "$ASSETS_DIR"/*.RVZ; do
  if [ -f "$candidate" ]; then
    DVD_PATH="$candidate"
    break
  fi
done

pm_platform_helper "$BIN"

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

pm_finish
