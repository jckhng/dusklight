#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR=/$directory/ports/dusklight/
if [ ! -x "$GAMEDIR/dusklight.${DEVICE_ARCH}" ] && [ -x "/$directory/dusklight/dusklight.${DEVICE_ARCH}" ]; then
  GAMEDIR="/$directory/dusklight/"
elif [ ! -x "$GAMEDIR/dusklight.${DEVICE_ARCH}" ] && [ -x "/roms/ports/dusklight/dusklight.${DEVICE_ARCH}" ]; then
  GAMEDIR="/roms/ports/dusklight/"
fi
RUNTIME_DIR="$GAMEDIR/runtime"
ASSETS_DIR="$GAMEDIR/assets"
BIN="$GAMEDIR/dusklight.${DEVICE_ARCH}"
DISPLAY_W="${DISPLAY_WIDTH:-640}"
DISPLAY_H="${DISPLAY_HEIGHT:-480}"

mkdir -p "$RUNTIME_DIR" "$RUNTIME_DIR/home" "$ASSETS_DIR"

cd "$GAMEDIR"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export HOME="$RUNTIME_DIR/home"
export XDG_DATA_HOME="$RUNTIME_DIR"
export XDG_CACHE_HOME="$RUNTIME_DIR/cache"
export XDG_CONFIG_HOME="$RUNTIME_DIR/config"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$GAMEDIR/lib.${DEVICE_ARCH}:$GAMEDIR/lib:$GAMEDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

DVD_PATH=""
for candidate in "$ASSETS_DIR"/*.iso "$ASSETS_DIR"/*.ISO "$ASSETS_DIR"/*.gcm "$ASSETS_DIR"/*.GCM "$ASSETS_DIR"/*.rvz "$ASSETS_DIR"/*.RVZ; do
  if [ -f "$candidate" ]; then
    DVD_PATH="$candidate"
    break
  fi
done

if [ ! -x "$BIN" ]; then
  echo "Missing executable: $BIN"
  command -v pm_finish >/dev/null 2>&1 && pm_finish
  exit 1
fi

if [ -z "$DVD_PATH" ]; then
  echo "No disc image found in $ASSETS_DIR; Dusklight may open the prelaunch UI."
fi

pm_platform_helper "$BIN"

DUSKLIGHT_ARGS=(
  --backend opengles
  --cvar backend.graphicsBackend=opengles
  --cvar backend.skipPreLaunchUI=true
  --cvar backend.showPipelineCompilation=false
  --cvar backend.checkForUpdates=false
  --cvar video.enableFullscreen=true
  --cvar video.enableVsync=false
  --cvar video.maxFrameRate=5
  --cvar game.bloomMode=0
  --cvar game.bloomMultiplier=0
  --cvar game.depthOfFieldMode=0
  --cvar game.disableWaterRefraction=true
  --cvar game.disableCutscenePillarboxing=true
  --cvar game.enableTextureReplacements=false
  --cvar game.enableFrameInterpolation=0
  --cvar game.internalResolutionScale=1
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

RUN_SCRIPT="$RUNTIME_DIR/run-dusklight.sh"
{
  printf '#!/bin/bash\n'
  printf 'exec %q' "$BIN"
  for arg in "${DUSKLIGHT_ARGS[@]}"; do
    printf ' %q' "$arg"
  done
  printf '\n'
} > "$RUN_SCRIPT"
chmod +x "$RUN_SCRIPT"

$ESUDO env \
  DUSKLIGHT_PORTMASTER_X11_DAWN=1 \
  DUSKLIGHT_PORTMASTER_NO_SURFACE=1 \
  DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1 \
  DUSKLIGHT_PORTMASTER_FBDEV_PRESENT=1 \
  DUSKLIGHT_PORTMASTER_LOW_SPEC=1 \
  DUSKLIGHT_PORTMASTER_GX_STATS=1 \
  SDL_VIDEODRIVER=offscreen \
  "$RUN_SCRIPT"

command -v pm_finish >/dev/null 2>&1 && pm_finish
