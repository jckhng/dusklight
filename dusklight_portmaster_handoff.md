# Dusklight PortMaster / RG35XX H Feasibility Handoff

## Goal

Investigate whether `TwilitRealm/dusklight` can be ported to PortMaster, specifically for RG35XX H / muOS-class Linux handhelds.

Target device class:

```text
RG35XX H
Allwinner H700
Mali-G31 MP2
1 GB RAM
muOS / PortMaster Linux environment
Likely graphics target: SDL + OpenGL ES / EGL, not Vulkan
```

Primary question:

```text
Can Dusklight run on Linux ARM64 using the OpenGL ES backend instead of Vulkan?
```

## Current conclusion

This is **not obviously impossible**. It is worth a spike.

The project is open source and the Android port is not a Java/Kotlin game. It is a minimal SDLActivity wrapper around native C/C++ code. That means the Android port is useful evidence that the native code can build for ARM64.

However, this is **not a simple SDL/OpenGL ES game**. Dusklight uses Aurora, and Aurora routes rendering through WebGPU/Dawn. The `opengles` backend appears to mean:

```text
Dusklight
  -> Aurora GX
    -> WebGPU API
      -> Dawn backend
        -> OpenGLES
```

So the hard question is not “can SDL do GLES?” The hard question is whether **Dawn OpenGLES can be built and run on Linux ARM64 / muOS**.

## Repository evidence

### Android wrapper is SDLActivity-based

`platforms/android/README.md` says the Android directory contains a minimal SDLActivity-based Android wrapper. It builds native libraries and stages `libmain.so` into the APK.

Relevant repo path:

```text
platforms/android/README.md
```

Key implications:

```text
Good: Native core already exists.
Good: Android is not the main game implementation.
Good: Android ARM64 build path exists.
Bad: Android build path does not directly prove Linux ARM64/muOS compatibility.
```

### Dusklight accepts `opengles` as a backend string

In `src/dusk/ui/settings.cpp`, Dusklight parses backend strings including:

```text
auto
d3d11
d3d12
metal
vulkan
opengl
opengles
webgpu
null
```

Important lines/concepts:

```cpp
if (backend == "opengles") {
    outBackend = BACKEND_OPENGLES;
    return true;
}
```

It also maps `BACKEND_OPENGLES` back to display name `OpenGL ES` and ID `opengles`.

### Actual backend availability comes from Aurora

Dusklight calls Aurora to get available backends:

```cpp
const AuroraBackend* raw = aurora_get_available_backends(&backendCount);
```

So Dusklight may parse `opengles`, but whether it is shown/usable depends on Aurora/Dawn compile flags.

### Aurora has `BACKEND_OPENGLES`

In `include/aurora/aurora.h`, Aurora defines:

```cpp
typedef enum {
  BACKEND_AUTO,
  BACKEND_D3D11,
  BACKEND_D3D12,
  BACKEND_METAL,
  BACKEND_VULKAN,
  BACKEND_OPENGL,
  BACKEND_OPENGLES,
  BACKEND_WEBGPU,
  BACKEND_NULL,
} AuroraBackend;
```

### Aurora’s backend list includes OpenGLES when Dawn enables it

In `lib/aurora.cpp`, Aurora’s preferred backend list includes:

```cpp
#ifdef DAWN_ENABLE_BACKEND_OPENGLES
    BACKEND_OPENGLES,
#endif
```

This is compile-time gated.

### The CMake gate is not obviously Android-only

In Aurora CMake logic, OpenGL/OpenGLES backend definitions are driven by CMake variables:

```cmake
if (DAWN_ENABLE_DESKTOP_GL OR DAWN_ENABLE_OPENGLES)
    target_compile_definitions(aurora_core PRIVATE DAWN_ENABLE_BACKEND_OPENGL)
    if (DAWN_ENABLE_DESKTOP_GL)
        target_compile_definitions(aurora_core PRIVATE DAWN_ENABLE_BACKEND_DESKTOP_GL)
    endif ()
    if (DAWN_ENABLE_OPENGLES)
        target_compile_definitions(aurora_core PRIVATE DAWN_ENABLE_BACKEND_OPENGLES)
    endif ()
endif ()
```

This suggests `opengles` is not inherently Android-only at the CMake/Aurora level.

### Window creation is not Android-only for OpenGLES

In `lib/window.cpp`, Aurora uses SDL window flags for both OpenGL and OpenGLES:

```cpp
case BACKEND_OPENGL:
case BACKEND_OPENGLES:
    flags |= SDL_WINDOW_OPENGL;
    break;
```

No obvious Android-only GLES branch was found there.

### Aurora maps OpenGLES to Dawn/WebGPU OpenGLES

In `lib/webgpu/gpu.cpp`, Aurora maps:

```cpp
case BACKEND_OPENGLES:
    return wgpu::BackendType::OpenGLES;
```

This confirms it is using Dawn’s OpenGLES backend, not a direct custom GLES renderer.

## Main risks

### 1. Dawn OpenGLES on Linux ARM64 / muOS

This is the main blocker.

Need to verify:

```text
Can Dawn build with OpenGLES for Linux ARM64?
Can it create a WebGPU surface from an SDL window on muOS?
Can it request an OpenGLES adapter successfully?
Can it present reliably on Mali-G31 / Panfrost?
```

Likely failure phases:

```text
CMake configure failure
Dawn build failure
link failure against EGL/GLES
runtime surface creation failure
runtime adapter request failure
black screen / present failure
shader translation/runtime errors
```

### 2. SDL3 availability

Dusklight uses SDL3. PortMaster environments often assume SDL2 or bundled libraries.

Likely approach:

```text
Use AURORA_SDL3_PROVIDER=vendor
Bundle SDL3 with the port if needed
Avoid assuming system SDL3 exists on muOS
```

### 3. RAM pressure

RG35XX H has 1 GB total RAM, not 1 GB free.

Practical RSS target:

```text
500–650 MB steady RSS: plausible
650–800 MB steady RSS: fragile
800 MB+ steady RSS: likely OOM/kill/swap pain
```

An 800 MB peak may be salvageable if it only occurs during loading. An 800 MB steady-state runtime is much harder.

### 4. GPU/performance

Mali-G31 MP2 is weak. Even if it boots, performance may be poor.

Expected risk:

```text
shader compile stalls
low FPS
GPU driver quirks
texture/cache pressure
thermal throttling
stutter from swap or IO
```

### 5. Renderer abstraction overhead

Compared with Ship of Harkinian, Dusklight is heavier:

```text
Ship of Harkinian:
N64-era game, direct native renderer, lighter assets

Dusklight:
GameCube-era game, Aurora, WebGPU/Dawn, heavier assets/scenes
```

SoH proves the category is plausible. It does not prove Dusklight will fit RG35XX H.

## Low-end settings to force

Create or patch config before booting, avoiding prelaunch UI where possible.

Suggested low-end preset:

```ini
backend.graphicsBackend=opengles
backend.skipPreLaunchUI=true
backend.showPipelineCompilation=false

video.maxFrameRate=30
video.enableVsync=false

 game.bloomMode=0
 game.depthOfFieldMode=0
 game.disableWaterRefraction=true
 game.enableTextureReplacements=false
 game.enableFrameInterpolation=0
 game.internalResolutionScale=1
 game.shadowResolutionMultiplier=0
 game.resampler=0
 game.enableMapBackground=false
 game.enableAchievementToasts=false
 game.enableControllerToasts=false
 game.enableDiscordPresence=false
```

Remove leading spaces if writing directly into the actual config format.

High-impact items:

```text
Force 30 FPS
Disable bloom
Disable depth of field
Disable texture replacements
Disable frame interpolation
Force 1x internal resolution
Disable water refraction
Disable map background
Disable Discord/update/toast overhead
```

## Potential memory reduction tactics

### Easy

```text
Use opengles, not vulkan
Skip prelaunch UI
30 FPS cap
1x internal resolution
MSAA = 1, if exposed
Disable post-processing
Disable texture replacements
Disable frame interpolation
Use plain ISO/GCM for first test instead of RVZ if decompression causes CPU/RAM pressure
```

### Medium

```text
Add swapfile for loading spikes
Limit cache sizes
Disable debug/tool overlays
Disable Sentry/crash reporting
Disable update checks
Disable Discord presence
Disable LiveSplit/recording/input viewer systems
Bundle only required libraries
```

### Hard

```text
Patch renderer/resource cache eviction
Free CPU-side texture copies after GPU upload
Reduce texture residency
Lazy-load UI/menu assets
Free previous area assets sooner
Patch or fork Aurora/Dawn behavior for low-end GLES
Add a true lean SDL/GLES backend, bypassing WebGPU/Dawn
```

## Swapfile note

Swap may help if memory spikes only during loading. It will not help if the game constantly touches swapped memory during gameplay.

Example testing swapfile:

```bash
fallocate -l 1G /roms/ports/dusklight/swapfile
chmod 600 /roms/ports/dusklight/swapfile
mkswap /roms/ports/dusklight/swapfile
swapon /roms/ports/dusklight/swapfile
```

Bad sign:

```text
constant paging during gameplay
massive stutter
audio crackle
input latency
```

Good sign:

```text
loading spike survives
runtime RSS settles lower
no constant paging
```

## First build experiment

Try to build Linux ARM64 with Dawn OpenGLES enabled and Vulkan disabled.

Pseudo-command:

```bash
cmake -S . -B build-rg35xxh \
  -DCMAKE_TOOLCHAIN_FILE=<your-aarch64-toolchain.cmake> \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=OFF \
  -DAURORA_SDL3_PROVIDER=vendor \
  -DDAWN_ENABLE_OPENGLES=ON \
  -DDAWN_ENABLE_VULKAN=OFF \
  -DDAWN_ENABLE_DESKTOP_GL=OFF

cmake --build build-rg35xxh -j$(nproc)
```

If this fails, inspect whether Dawn supports Linux ARM64 OpenGLES in the current configuration.

## First runtime experiment

On target device:

```bash
./dusklight --backend opengles --dvd /path/to/game.iso
```

If command-line backend flag does not work, pre-seed config with:

```ini
backend.graphicsBackend=opengles
```

## Measurements to collect

Use `/usr/bin/time` if available:

```bash
/usr/bin/time -v ./dusklight --backend opengles --dvd /path/to/game.iso
```

Collect:

```text
Maximum resident set size
startup time
first menu FPS
first playable scene FPS
GPU/driver errors
surface creation errors
adapter creation errors
shader/pipeline errors
kernel OOM logs
```

Also measure during runtime:

```bash
cat /proc/$(pidof dusklight)/status | grep -E "VmRSS|VmHWM|VmSize"
```

Take readings at:

```text
startup
prelaunch menu
after selecting ISO
loading screen
first playable scene
after 5 minutes
after area transition
```

Interpretation:

```text
High at startup -> engine/library overhead
Spike during loading then drops -> swap/cache tuning may help
Rises every area -> leak or cache not evicting
High only during gameplay -> texture/render/cache issue
Flat but too high -> architectural memory floor
```

## Go / no-go criteria

### Green

```text
Build succeeds for Linux ARM64
OpenGLES backend is listed/accepted
Surface and adapter creation succeed
Boots to menu
Loads first playable scene
Steady RSS <= 650 MB
FPS near 25–30 after low-end settings
```

### Yellow

```text
Steady RSS 650–800 MB
Boots but stutters
Loading spikes trigger OOM without swap
Shader compilation stalls but gameplay later stabilizes
Some scenes playable, some not
```

### Red

```text
Dawn OpenGLES cannot build for Linux ARM64
SDL/EGL surface creation fails on muOS
OpenGLES adapter request fails
Steady RSS > 800 MB
GPU crashes or black screen after scene load
FPS consistently below 15
```

## Most likely first blocker

```text
Dawn OpenGLES backend support on Linux ARM64 / muOS
```

Not:

```text
Android-specific GLES implementation
```

The repo scan did not show an obvious Android-only gate around OpenGLES in Aurora. The OpenGLES path appears compile-flag gated through Dawn.

## Strategic recommendation

Do a spike, not a full port first.

Milestone order:

```text
1. Cross-build Linux ARM64 with DAWN_ENABLE_OPENGLES=ON
2. Run on RG35XX H with --backend opengles
3. Boot to menu
4. Load first playable scene
5. Measure RSS/FPS
6. Apply low-end preset
7. Decide whether to package for PortMaster
```

Do not start with PortMaster packaging. First prove the binary can render and stay inside memory limits.

---

# Execution Notes - 2026-06-03

This section records the first real PortMaster/device spike so future work does not repeat the same dead ends.

## Device Tested

```text
IP: 192.168.10.136
OS: muOS 2502.0 PIXIE
PortMaster: 2026.04.01-1426
Device: RG35XX-H
CPU: Allwinner H700
GPU: Mali-G31
Display: 640x480
RAM: 1 GB class
```

The ROM used for private testing was placed in:

```text
/mnt/mmc/ports/dusklight/assets/Legend of Zelda, The - Twilight Princess (USA).rvz
```

Do not include ROM data in distributable packages.

## Packaging / Build Work Completed

PortMaster packaging scaffold was added under:

```text
packaging/portmaster/aarch64/
scripts/portmaster/
```

The build path uses Docker to produce a Linux aarch64 binary and stage a PortMaster-shaped bundle. The relevant scripts are:

```text
scripts/portmaster/build_docker_aarch64_bundle.sh
scripts/portmaster/build_docker_aarch64_prebuilt_target_runtime_bundle.sh
scripts/portmaster/build_aarch64_bundle.sh
```

The target-runtime build expects a small runtime sysroot from the device at:

```text
/tmp/dusklight-target-sysroot
```

At minimum the current configured build needed:

```text
/tmp/dusklight-target-sysroot/lib/libc.so.6
/tmp/dusklight-target-sysroot/lib/libm.so.6
/tmp/dusklight-target-sysroot/usr/lib/libstdc++.so
```

The current build tree has a working aarch64 Dusklight binary at:

```text
build/portmaster-aarch64/dusklight.stripped
```

## Probes Added

Several small probes were added under:

```text
packaging/portmaster/probes/
```

Important probes:

```text
sdl2_gles_probe.c
sdl3_gl_probe.c
sdl3_window_probe.c
egl_fbdev_probe.c
egl_gles_version_probe.c
```

These are useful because Dusklight itself is heavy to rebuild and launch. Keep using probes to isolate SDL/EGL/Dawn failures before changing engine code.

## What Worked

### SDL2 Native Mali GLES Works

Direct SDL2 on the device, without Weston, can create a GLES context through the native Mali driver:

```text
video_driver_count=1
video_driver[0]=mali
current_video_driver=mali
SDL2 GLES context created
drawable=640x480
status=0
```

This proves the device has a usable hardware GLES path.

### Raw EGL fbdev Works

The direct EGL framebuffer probe succeeded:

```text
fb=640x480 bpp=32
egl=1.4 vendor=ARM
surface=... error=0x3000
bind_gles=1 error=0x3000
context=... error=0x3000
make_current=1 error=0x3000
status=0
```

The newer GLES version probe confirmed the actual graphics stack:

```text
egl=1.4 vendor=ARM client_apis=OpenGL_ES
gl_version_es2.0=OpenGL ES 3.2 v1.r20p0-01rel0...
gl_vendor_es2.0=ARM
gl_renderer_es2.0=Mali-G31
gl_version_es3.1=OpenGL ES 3.2 v1.r20p0-01rel0...
gl_version_es3.2=OpenGL ES 3.2 v1.r20p0-01rel0...
```

So the device is not limited to GLES2. It can create ES 3.1/3.2 contexts directly through EGL/fbdev.

### SDL3 Plain X11 Window Works Under WestonPack

PortMaster WestonPack with `crusty_x11egl` can create a plain SDL3 X11 window if SDL is not asked to create an OpenGL context:

```text
SDL_VIDEODRIVER=x11
current_video_driver=x11
SDL_CreateWindow ok
status=0
```

This was tested with `sdl3_window_probe.c`.

This is why a temporary Aurora change was made to skip `SDL_WINDOW_OPENGL` when:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
```

With that change, Dusklight can launch far enough under Weston/X11 to reach Aurora WebGPU initialization.

## What Failed / Dead Ends

### SDL3 + Weston + SDL-Owned GL Context

Earlier attempts asking SDL3 to create an OpenGL/OpenGLES context under Weston failed:

```text
SDL_CreateWindow failed: Could not initialize OpenGL / GLES library
SDL_CreateWindow failed: Could not get EGL display
OpenGL context already created
segfault
```

Conclusion: avoid the path where SDL owns the GL context under Weston. It is not a good direction for Dusklight/Aurora because Dawn wants to own backend setup anyway.

### Temporary SDL3 `mali-fbdev` Backend

An experimental SDL3 video backend named `mali` was added in the build tree and made SDL3 GLES probes work directly on fbdev.

It could create an SDL3 OpenGLES context, but Dusklight/Aurora still failed because Dawn could not create a supported WebGPU surface for that custom SDL window.

Known result:

```text
[INFO | aurora::gpu] Attempting to initialize OpenGLES
[ERROR | aurora::gpu] Failed to create surface descriptor for current window
```

Then a temporary attempt mapped the custom `mali` SDL window to:

```text
wgpu::SurfaceSourceAndroidNativeWindow
```

That failed:

```text
Error: Unsupported sType (SType::SurfaceSourceAndroidNativeWindow)
Adapter request failed: No supported adapters
```

Conclusion: do not continue the Android surface-source trick. Dawn's Linux prebuilt rejects that surface source.

### Weston/X11 Gets Past SDL But Dawn Finds No Adapter

After skipping `SDL_WINDOW_OPENGL`, Dusklight under WestonPack `crusty_x11egl` reached:

```text
[INFO | aurora::gpu] Creating WebGPU instance
[INFO | aurora::gpu] Attempting to initialize OpenGLES
```

But Dawn failed:

```text
[WARNING | aurora::gpu] Adapter request failed: No supported adapters
[ERROR | aurora::gpu] Failed to create adapter
```

This happened even when the adapter request was changed to skip compatible-surface filtering:

```text
DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1
```

Conclusion: the failure is not just X11 surface compatibility. Dawn is not enumerating a usable GLES adapter through the Weston/Xwayland path.

### Forcing Library Order Did Not Fix X11

The game process was rerun with `crusty_x11egl` and real GLES libraries forced ahead of PortMaster's Mesa X11 stubs:

```text
LD_LIBRARY_PATH=/tmp/weston/lib_aarch64/graphics/crusty_x11egl:...:/usr/lib64:/usr/lib
LD_PRELOAD=/tmp/weston/lib_aarch64/graphics/crusty_x11egl/libcrusty.so
SDL_EGL_LIBRARY=/tmp/weston/lib_aarch64/graphics/crusty_x11egl/libEGL.so
SDL_OPENGL_LIBRARY=/usr/lib64/libGLESv2.so
```

Dawn still returned:

```text
No supported adapters
```

Conclusion: simple library path ordering is not enough.

### Direct Mali + No Surface Still Finds No Dawn Adapter

A diagnostic Aurora change allowed skipping surface creation entirely:

```text
DUSKLIGHT_PORTMASTER_NO_SURFACE=1
DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1
SDL_VIDEODRIVER=mali
```

Even then, Dawn returned:

```text
[INFO | aurora::gpu] Attempting to initialize OpenGLES
[WARNING | aurora::gpu] Adapter request failed: No supported adapters
[ERROR | aurora::gpu] Failed to create adapter
```

This is the most important result from the spike.

Conclusion: the current Dawn OpenGLES backend/prebuilt does not enumerate the device's direct Mali EGL/fbdev stack, even though raw EGL proves the stack is usable.

## Current Root Cause Hypothesis

The problem is not:

```text
No binary
No ROM
SDL cannot create any window
Device GLES is too old
EGL is inherently slow
Mali-G31 cannot create ES 3.1/3.2 contexts
```

The current blocker is:

```text
Dawn's Linux OpenGLES adapter creation does not support, or does not discover,
the direct fbdev Mali EGL runtime used on this PortMaster device.
```

PortMaster's Weston/Xwayland path also does not expose the hardware GLES stack in a way Dawn accepts. The Weston log includes:

```text
xwayland glamor: GBM backend (default) is not available
Missing Wayland requirements for glamor GBM backend
Failed to initialize glamor, falling back to sw
```

So Weston/X11 is not the promising performance path anyway.

## Current Code Changes To Be Careful With

Some changes are diagnostic and should not be treated as final product code.

Diagnostic Aurora gates:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN
DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE
DUSKLIGHT_PORTMASTER_NO_SURFACE
```

The first one may be useful for future X11 experiments. The latter two are only diagnostics and should not ship.

The experimental SDL3 `mali-fbdev` backend exists in the generated build tree, not as a durable source patch. If the build directory is recreated, that work disappears. It is diagnostic only unless promoted to a real patch.

## Recommended Next Step

Do not keep iterating on the PortMaster launcher first. The launcher is not the active blocker.

The next efficient task is a minimal Dawn-only probe:

```text
Build a tiny Linux aarch64 program using the same Dawn package/source.
Request an OpenGLES adapter with no surface.
Run it on the device with direct Mali EGL libraries.
Instrument why adapter enumeration rejects the device.
```

If the Dawn-only probe fails the same way, patch Dawn in isolation before returning to Dusklight.

Likely Dawn investigation points:

```text
OpenGLES adapter discovery
EGL display/platform selection
Assumptions about X11/Wayland/GBM versus fbdev
Dynamic loading of libEGL/libGLESv2
Required EGL extensions
Required GLES version/extensions
```

The raw EGL probe gives the known-good baseline Dawn should eventually match:

```text
eglGetDisplay(EGL_DEFAULT_DISPLAY)
eglInitialize -> ARM EGL 1.4
eglCreateWindowSurface with fbdev native window
eglCreateContext ES 3.1/3.2
glGetString -> Mali-G31 / OpenGL ES 3.2
```

If Dawn can be taught to create an adapter through that path, then the next major problem will be presentation/surface integration. If Dawn still needs a standard WebGPU surface type, a custom fbdev surface path or offscreen-render-plus-GLES-present bridge may be required.

## Go / No-Go Update

Updated status after device spike:

```text
Build succeeds for Linux ARM64: yes
PortMaster package scaffold exists: yes
ROM loads far enough for app startup: yes
Raw hardware EGL/GLES works: yes
SDL2 native GLES works: yes
SDL3 plain X11 under Weston works: yes
Dusklight reaches Aurora WebGPU init: yes
Dawn OpenGLES adapter creation works: no
Boots to menu: no
Performance/RSS measurable: no
```

The project has moved from packaging uncertainty to a focused renderer-backend blocker.

---

# Follow-up Execution Notes - 2026-06-03 Evening

This section supersedes part of the earlier `Execution Notes - 2026-06-03`.
The earlier conclusion that "Dawn OpenGLES adapter creation works: no" was true
for the prebuilt/initial Dawn path, but is no longer true after building Dawn
from source with OpenGLES enabled and requesting WebGPU compatibility mode.

## Source-built Dawn OpenGLES Result

A source-built Dawn configuration was created in:

```text
build/portmaster-aarch64-vendor-dawn
```

Important CMake characteristics:

```text
DAWN_ENABLE_OPENGLES=ON
DAWN_ENABLE_DESKTOP_GL=OFF
DAWN_ENABLE_VULKAN=OFF
DAWN_ENABLE_NULL=ON
DAWN_USE_WAYLAND=ON
DAWN_USE_X11=OFF initially, then ON for the X11 experiment
AURORA_DAWN_PROVIDER=vendor
AURORA_DAWN_LINKAGE=shared
AURORA_SDL3_PROVIDER=vendor
AURORA_SDL3_LINKAGE=shared
```

The source-built Dawn adapter probe only succeeded after setting:

```c
options.featureLevel = WGPUFeatureLevel_Compatibility;
```

Aurora was patched similarly for OpenGL/OpenGLES adapter requests:

```cpp
if (backend == wgpu::BackendType::OpenGLES || backend == wgpu::BackendType::OpenGL) {
  featureLevel = wgpu::FeatureLevel::Compatibility;
}
```

With that change, the full Dusklight binary can initialize Dawn on the device and
select the hardware adapter:

```text
[INFO | aurora::gpu] Graphics adapter information
  API: OpenGLES
  Device: Mali-G31 (Unknown)
  Driver: OpenGL version OpenGL ES 3.2 v1.r20p0-01rel0...
```

This is a major milestone. The current blocker is no longer adapter discovery.
It is presentation/surface/swapchain integration.

## Packaging / Deployment Updates

The live device uses:

```text
/roms/ports/dusklight.sh
/roms/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/lib.aarch64/libwebgpu_dawn.so
/roms/ports/dusklight/lib.aarch64/libSDL3.so*
/roms/ports/dusklight/lib.aarch64/libnod.so
/roms/ports/dusklight/assets/Legend of Zelda, The - Twilight Princess (USA).rvz
```

Important packaging gotchas:

```text
/roms/ports/PortMaster is only the visible/fallback path on this muOS device.
The real PortMaster runtime files are under /mnt/mmc/MUOS/PortMaster.
WestonPack runtime is /mnt/mmc/MUOS/PortMaster/libs/weston_pkg_0.2.squashfs.
The active log is /roms/ports/dusklight/log.txt.
An old wrong-path log may exist under /roms/ports/ports/dusklight/log.txt.
```

The original launcher path logic produced this wrong path on muOS:

```text
/roms/ports/ports/dusklight
```

because `control.txt` sets:

```text
directory=roms/ports
```

The launcher now needs a fallback that prefers:

```text
/roms/ports/dusklight
```

when that executable exists.

Also ensure `libnod.so` is packaged. Dusklight failed without it:

```text
error while loading shared libraries: libnod.so: cannot open shared object file
```

## Confirmed Runtime Milestone

With diagnostic no-surface mode and SDL3 offscreen windowing, the full app reaches
the game loop:

```text
[INFO | aurora::gpu] Graphics adapter information
  API: OpenGLES
  Device: Mali-G31 (Unknown)
[WARNING | aurora::gpu] DUSKLIGHT_PORTMASTER_NO_SURFACE set; using diagnostic offscreen render targets
[INFO | dusk] Loaded game disc is GZ2E01
[INFO | dusk::osReport] Starting main01 (Game Loop)...
[DEBUG | dusk] cDyl_InitCallback: fpcNm_LOGO_SCENE_e created, DONE
```

This proves:

```text
Dusklight binary runs on the device.
Source-built Dawn can create an OpenGLES device on Mali-G31.
The RVZ loads.
The game loop starts.
```

It does not prove presentation, because:

```text
DUSKLIGHT_PORTMASTER_NO_SURFACE=1
SDL_VIDEODRIVER=offscreen
```

cause the app to skip real display output.

## WestonPack / Crusty Findings

The useful WestonPack wrapper is:

```text
/mnt/mmc/MUOS/PortMaster/libs/weston_pkg_0.2.squashfs
/tmp/weston*/westonwrap.sh
```

`westonwrap.sh` supports these relevant graphics options:

```text
crusty_gbm
crusty_x11egl
crusty_glx
gl4es
gl4es_x11
system
llvmpipe
virgl
zink
```

The system SDL2 library is not a normal desktop SDL2. It is a Mali fbdev build:

```text
/usr/lib64/libSDL2.so -> /usr/lib/libSDL2-2.0.so.0.2800.5
strings show:
  SDL_malivideo.c
  SDL_maliopengles.c
  mali-fbdev
  video driver: mali
```

Important consequence:

```text
Do not set outer SDL_VIDEODRIVER=x11 for Crusty on this device.
```

`crusty_x11egl` failed because Crusty's SDL2 backend tried to use X11:

```text
Could not init SDL! SDL_Error: x11 not available
Could not create SDL Window: x11 not available
```

The viable wrapper shape was:

```bash
export SDL_VIDEODRIVER=mali
westonwrap.sh headless noop kiosk crusty_gbm <run-script>
```

Then inside the Dusklight run script, SDL3 can be set separately:

```bash
export SDL_VIDEODRIVER=wayland
# or
export SDL_VIDEODRIVER=x11
```

## Wayland Surface Dead End

Using:

```text
westonwrap.sh headless noop kiosk crusty_gbm
outer SDL_VIDEODRIVER=mali
inner SDL_VIDEODRIVER=wayland
DUSKLIGHT_PORTMASTER_X11_DAWN=1
real surface enabled
```

the app gets far enough to create a Wayland window, select Mali-G31 through Dawn,
load the ROM, and start the game loop.

But Dawn's OpenGL/EGL swapchain does not implement Wayland surface support:

```text
[INFO | aurora::gpu] Using surface format RGBA8Unorm, present mode Mailbox
[WARNING | aurora::gpu] Device lost: [Surface "Surface"] cannot be supported on EGL.
  at .../dawn/native/opengl/SwapChainEGL.cpp:233
```

The relevant Dawn source says:

```cpp
// TODO: Add support for creating surfaces using EGL_KHR_platform_base and friends.
case Surface::Type::WaylandSurface:
default:
    return DAWN_FORMAT_INTERNAL_ERROR("%s cannot be supported on EGL.", surface);
```

Conclusion:

```text
Do not keep trying the Wayland surface path with current Dawn.
It is explicitly unsupported by Dawn's OpenGL EGL swapchain.
```

Wayland may become viable only if Dawn is patched to create EGL Wayland surfaces
through `EGL_KHR_platform_wayland` / `EGL_EXT_platform_wayland`.

## X11 Surface Experiment

Dawn was rebuilt with:

```text
DAWN_USE_X11=ON
```

The cross image lacked `X11/Xlib-xcb.h`, so a minimal compatibility header was
added in the generated Dawn source tree:

```text
build/portmaster-aarch64-vendor-dawn/_deps/dawn-src/src/X11/Xlib-xcb.h
```

This is a build-tree patch, not a durable source patch. If the build directory is
deleted, it must be recreated or handled properly in Docker/sysroot.

With X11 enabled:

```text
westonwrap.sh headless noop kiosk crusty_gbm
outer SDL_VIDEODRIVER=mali
inner SDL_VIDEODRIVER=x11
DUSKLIGHT_PORTMASTER_X11_DAWN=1
real surface enabled
```

the app reaches:

```text
Xwayland starts on :0
SDL3 creates the X11 window
Dawn selects Mali-G31 OpenGLES
Dawn accepts the Xlib surface
```

But it crashes in the Mali EGL/GLES driver during Dawn swapchain initialization:

```text
[INFO | aurora::gpu] Using surface format RGBA8Unorm, present mode Fifo
Reason: SIGSEGV
Crash PC: /usr/lib64/libEGL.so or /usr/lib64/libGLESv2.so
Backtrace:
  libwebgpu_dawn.so ... dawn::native::opengl::SwapChainEGL::Initialize
  libwebgpu_dawn.so ... dawn::native::Surface::Configure
```

Forcing `video.enableVsync=true` changed present mode from `Mailbox` to `Fifo`
but did not change the crash. Therefore present mode is not the primary trigger.

Running X11 without Crusty preloaded into the app still crashed in the same Dawn
swapchain path, so this is not solely Crusty's interposition.

Conclusion:

```text
X11 is closer than Wayland because Dawn supports Xlib surfaces.
But current Mali EGL + Xwayland + Dawn EGL swapchain crashes during configure.
Do not spend more time toggling present mode or simple launcher env vars.
```

## Diagnostic Environment Flags

Current local diagnostics:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
  Makes Aurora avoid SDL_WINDOW_OPENGL for OpenGL/OpenGLES windows.
  This is useful when Dawn should own the GL/EGL setup.

DUSKLIGHT_PORTMASTER_NO_SURFACE=1
  Skips real WebGPU surface usage and creates diagnostic offscreen render targets.
  Useful only to prove adapter/device/game-loop startup.
  Must not ship.

DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1
  Requests adapter without compatibleSurface.
  Useful only for probing.
  Must not ship.
```

## Current Go / No-Go Update

```text
Build succeeds for Linux ARM64: yes
Source-built Dawn OpenGLES adapter/device works: yes
PortMaster package scaffold exists: yes
RVZ loads: yes
Game loop starts: yes
Wayland surface presentation works: no, unsupported in Dawn EGL swapchain
X11 surface presentation works: no, Mali EGL/GLES segfaults in Dawn swapchain init
Boots visibly to menu/game: no
Performance/RSS measurable during visible gameplay: no
```

## Best Path Ahead

The better path is **not** more launcher iteration. The launcher has now done its
job: it can get Dusklight to the point where the renderer backend fails.

The best next technical path is:

```text
Bypass Dawn's platform swapchain on PortMaster.
Keep Dawn/WebGPU for internal rendering if possible.
Present the final rendered texture through a known-good device-native path.
```

The known-good presentation path on this device is:

```text
SDL2 mali/fbdev + EGL + GLES
```

Why this is better than patching Dawn Wayland first:

```text
Dawn's Wayland EGL surface support is currently unimplemented.
Adding it may still only render into Weston headless/noop, not the physical screen.
PortMaster's device has no real GBM path, and Xwayland falls back to software.
The native SDL2 Mali path already reaches the physical display and creates GLES.
```

Why this is better than X11:

```text
X11 requires Xwayland.
Xwayland has no GBM/glamor path here and falls back to software.
Dawn's Xlib EGL swapchain reaches the Mali driver but segfaults.
The segfault happens below Dusklight/Aurora in libEGL/libGLESv2.
```

Two concrete implementation options:

```text
Option A: Patch Aurora/Dawn integration for a PortMaster "external present" mode.
  Render Dusklight internally with Dawn to offscreen textures.
  Read/copy/blit the final frame to a native GLES presenter controlled by us.
  This is awkward but keeps most of Aurora's renderer intact.

Option B: Add a lean PortMaster presentation backend beside Dawn's swapchain.
  Use SDL2 Mali or raw EGL fbdev to own the native display.
  Avoid WebGPU Surface/SwapChain entirely on this platform.
  This is likely the most direct route to first pixels, but requires engine work.
```

Do **not** start with a full SDL2 renderer rewrite. That is probably too large.
Start with the smallest "first pixels" bridge:

```text
1. Keep source-built Dawn OpenGLES compatibility mode.
2. Keep `DUSKLIGHT_PORTMASTER_NO_SURFACE`-style offscreen rendering.
3. Identify Aurora's final present source texture.
4. Add a PortMaster-only native GLES presenter.
5. First attempt can be crude: copy/readback/blit only enough to prove pixels.
6. Optimize only after visible output exists.
```

Expected challenge:

```text
Moving pixels from Dawn's internal GL texture to our native GLES presenter may
require access to Dawn's underlying GL texture, an explicit readback path, or a
small Dawn patch. Readback will be slow, but it is acceptable for a fail-fast
first-pixels proof.
```

If that bridge proves impossible or too slow, the second-best path is a focused
Dawn patch for EGL Wayland or fbdev-native surfaces, using the existing raw EGL
probe as the reference. That is deeper engine/platform work and should come after
the simpler external-present experiment.

## 2026-06-03 Update: First Pixels And muOS Launcher Layout

The external-present experiment succeeded enough to prove the direction:

```text
Dawn/OpenGLES compatibility device: works on Mali-G31
Dawn platform surface/swapchain: bypassed
Aurora render target: offscreen RGBA8
Presenter: crude CPU readback to /dev/fb0
SDL video driver: offscreen
Visible output: yes, Dusklight prelaunch UI rendered on the device framebuffer
```

The framebuffer dump from `/dev/fb0` showed the Dusklight prelaunch screen with
the logo and menu items. This means the no-surface Dawn path plus fbdev presenter
is a valid first-pixels path. Do not go back to Weston/X11 for the current
iteration unless there is a specific reason; that path was fighting muOS UX and
had already hit Dawn/EGL surface problems.

The currently useful launch environment is:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_NO_SURFACE=1
DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1
DUSKLIGHT_PORTMASTER_FBDEV_PRESENT=1
SDL_VIDEODRIVER=offscreen
--backend opengles
--cvar backend.graphicsBackend=opengles
--cvar backend.skipPreLaunchUI=true
```

The muOS/PortMaster launcher layout matters:

```text
Launcher script:
  /mnt/mmc/roms/ports/dusklight.sh

Canonical payload directory derived by PortMaster control.txt:
  /mnt/mmc/ports/dusklight

Important files currently synced there:
  /mnt/mmc/ports/dusklight/dusklight.aarch64
  /mnt/mmc/ports/dusklight/lib.aarch64/libwebgpu_dawn.so
  /mnt/mmc/ports/dusklight/assets/Legend of Zelda, The - Twilight Princess (USA).rvz
```

Do not rely on `/roms/ports/dusklight.sh` for muOS discovery. During SSH testing
we also used `/roms/ports/dusklight`, but the menu launcher belongs under
`/mnt/mmc/roms/ports`, and PortMaster resolves the game payload under
`/$directory/ports/dusklight`, which is `/mnt/mmc/ports/dusklight` for this
launcher path.

## 2026-06-03 Update: Current Renderer Blocker

After first pixels, the next blocker moved to GX shader compatibility with the
Mali GLES driver:

```text
Program link failed:
The number of vertex shader storage blocks (1) is greater than the maximum
number allowed (0).
```

Aurora's GX path normally stores raw GameCube vertex streams in WebGPU storage
buffers and manually fetches vertex attributes from WGSL vertex shaders:

```text
extern/aurora/lib/gx/shader.cpp:
  var<storage, read> vbuf: array<u32>;
  var<storage, read> abuf: array<u32>;
```

That is the right design for Vulkan/desktop WebGPU, but this Mali GLES stack
reports/supports zero vertex shader storage blocks. A narrow native vertex input
experiment was added for the simplest direct-attribute GX pipeline:

```text
pos f32x3 at offset 0
optional color rgba8 at offset 12
stride 16
no indexed GX arrays
```

That experiment got past the first `JUTFader`/startup pipeline and advanced much
further into game resource and scene loading. The next crash is another simple
storage-backed GX pipeline:

```text
pos f32x3 at offset 0
stride 12
no color
```

So the better path ahead is to generalize the native vertex input fallback for
direct GX attributes that GLES can express as vertex buffer layouts. Indexed GX
array attributes may still need a different fallback, such as CPU-expanded
vertices or another GLES-compatible data path.

The stride-12 pos-only case has now been added to the native vertex input
fallback and deployed to the device payload. The next menu-run test should show
whether the Classic path reaches visible gameplay or exposes the next pipeline
shape that still uses vertex-stage storage buffers.

The next exposed shape was an indexed POS/CLR0 pipeline:

```text
vbuf stream stride 4:
  u16 position index
  u16 color index

abuf arrays:
  POS f32x3, stride 12
  CLR0 rgba8, stride 4
```

This has now been handled with a targeted CPU expansion path in
`extern/aurora/lib/gx/command_processor.cpp`: the indexed stream is expanded into
direct `pos f32x3 + color rgba8` vertices before upload, then rendered through
the native vertex input fallback. The updated executable was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

The next exposed shape added indexed `TEX0`:

```text
vbuf stream stride 6:
  u16 position index
  u16 color index
  u16 tex0 index

abuf arrays:
  POS f32x3, stride 12
  CLR0 rgba8, stride 4
  TEX0 s16x2, frac 10, stride 4
```

This was extended in the same CPU expansion path by converting TEX0 to direct
`f32x2` and using native vertex location 2. The expanded direct vertex layout is
now:

```text
offset 0:  POS f32x3
offset 12: CLR0 rgba8
offset 16: TEX0 f32x2
stride 24
```

The next exposed shape was indexed `POS + TEX0` without `CLR0`:

```text
vbuf stream stride 4:
  u16 position index
  u16 tex0 index

abuf arrays:
  POS f32x3, stride 12
  TEX0 s16x2, frac 14, stride 4
```

This is now also CPU-expanded through the same native layout, with a dummy white
`CLR0` slot inserted at offset 12 and TEX0 converted using the source fractional
scale. If the Classic/Dusklight choice no longer appears, that is because
`runtime/TwilitRealm/Dusklight/config.json` has `backend.wasPresetChosen: true`;
remove/reset that runtime config to force the choice again.

Follow-up fix: the generalized matcher initially forgot to count the mandatory
2-byte POS index when comparing the FIFO vertex stride, so the `POS+TEX0` case
did not activate. That has been corrected:

```text
2 bytes POS + optional 2 bytes CLR0 + optional 2 bytes TEX0 == FIFO stride
```

The corrected executable was deployed after this fix.

The next exposed shape was direct FIFO `POS + TEX0`:

```text
FIFO vertex stride 16:
  POS f32x3 at offset 0
  TEX0 s16x2, frac 8 at offset 12
```

This is also CPU-expanded into the native layout with a dummy white `CLR0` slot
and TEX0 converted to `f32x2`. The expanded layout remains:

```text
offset 0:  POS f32x3
offset 12: CLR0 rgba8
offset 16: TEX0 f32x2
stride 24
```

The next exposed indexed shape added normals and lighting:

```text
vbuf stream stride 8:
  u16 position index
  u16 normal index
  u16 color index
  u16 tex0 index

abuf arrays:
  POS f32x3, stride 12
  NRM s16x3, frac 14, stride 6
  CLR0 rgba8, stride 4
  TEX0 s16x2, frac 12, stride 4
```

This is now expanded to:

```text
offset 0:  POS f32x3
offset 12: NRM f32x3
offset 24: CLR0 rgba8
offset 28: TEX0 f32x2
stride 36
```

The native vertex pipeline now derives attribute offsets from the generated
`ShaderConfig` instead of hardcoding only the early layouts.

## 2026-06-03 GLES storage-block follow-up

The direct `POS + TEX0` matcher was simplified to trust the concrete FIFO shape:

```text
vtxSize == 16
POS descriptor == GX_DIRECT
TEX0 descriptor == GX_DIRECT
```

Descriptor-loop and VAT-type gates were removed because the runtime shader could
still slip through them on the logo draw.

A separate GLES-specific issue was then identified: even when a native vertex
pipeline omitted the actual `@group(0)` storage buffer bindings, the generated
WGSL still carried unused generic fetch helpers with `ptr<storage>` parameters.
On the Mali-G31 GLES stack this may still count as a vertex shader storage block,
and the device reports:

```text
MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0
```

Native vertex shaders now omit the whole storage-fetch prelude, not just the
storage buffer bindings. This is important for PortMaster devices using GLES
drivers with zero vertex shader storage block support.

Temporary debug logging was also added behind:

```text
DUSKLIGHT_PORTMASTER_GX_DEBUG=1
```

It logs when the CPU expansion path is selected and when a native vertex pipeline
is built. If another `vertex shader storage blocks` failure appears, check
whether the log contains `PortMaster GX expand ...` and whether the generated
shader still contains `ptr<storage>` or `var<storage>`.

On 2026-06-04 the muOS launcher at `/mnt/mmc/roms/ports/dusklight.sh` was also
updated to export `DUSKLIGHT_PORTMASTER_GX_DEBUG=1`, and the union path
`/mnt/union/ROMS/Ports/dusklight.sh` reflected the same file. Menu launches after
that point should include the `PortMaster GX ...` diagnostic lines in
`/mnt/mmc/ports/dusklight/log.txt`.

The rebuilt stripped binary was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Both deployed files were `22386712` bytes after stripping.

## 2026-06-04 reassessment: current blocker and viability

The current blocker is no longer launch plumbing, ROM discovery, muOS lifecycle,
Weston, or framebuffer presentation. Those layers were either solved or worked
around enough to get Dusklight into the GX draw path.

The blocker is now the Aurora GX renderer's vertex-fetch model versus the target
device's GLES limits.

Aurora's normal GX shader path fetches vertex attributes from WebGPU storage
buffers in the vertex shader:

```wgsl
@group(0) @binding(0)
var<storage, read> vbuf: array<u32>;
@group(0) @binding(1)
var<storage, read> abuf: array<u32>;

let in_pos = fetch_f32_3(&vbuf, ubuf.vtx_start + vidx * 16u + 0u, false);
let in_tex0_uv = vec2f(
  fetch_s16_1(&vbuf, ubuf.vtx_start + vidx * 16u + 12u + 0u, 8u, false),
  fetch_s16_1(&vbuf, ubuf.vtx_start + vidx * 16u + 12u + 2u, 8u, false)
);
```

Dawn's OpenGLES backend translates that to a GLES vertex shader using shader
storage blocks. The target Mali-G31 GLES driver reports:

```text
MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0
```

So any GX pipeline that reaches the original storage-buffer vertex-fetch path
will fail at `CreateRenderPipeline` with:

```text
The number of vertex shader storage blocks (1) is greater than the maximum number allowed (0).
```

This is why the latest crash still occurs even after the offscreen/fbdev path
works. It is a graphics backend capability mismatch, not a missing file or
launcher bug.

### What the recent patches attempted

Recent patches special-cased several observed GX vertex layouts. Those layouts
are decoded on the CPU and re-uploaded as ordinary native vertex attributes so
the generated shader can avoid `vbuf`, `abuf`, `var<storage>`, and
`ptr<storage>`.

That got past multiple previous storage-block crashes, but the latest pasted
shader proves another draw still reached the original path. It is the same
general pattern again:

```text
FIFO stride 16
POS f32x3 at offset 0
TEX0 s16x2 frac8 at offset 12
```

The important lesson is not this single layout. The important lesson is that
patching one shape at a time is turning into whack-a-mole. Dusklight/Twilight
Princess can expose many GX descriptor/VAT combinations as scenes advance.

### Better technical path

The proper compatibility path would be a general PortMaster/GLES fallback:

```text
For every GX draw:
  read the active GX vertex descriptors
  read the active VAT attribute formats
  decode direct/index8/index16 attributes on the CPU
  expand them into one interleaved native vertex buffer
  build the pipeline using normal vertex attributes
  never emit vertex-stage storage-buffer fetches on GLES
```

That would stop the storage-block crash class directly. It would also make the
renderer less dependent on the GLES driver supporting WebGPU's storage-buffer
translation in vertex shaders.

### Performance risk

The general CPU expansion fallback may make the game unplayable on this class of
device.

Reasons:

```text
- more CPU work per GX draw
- more memory copying per frame
- many small GameCube-style draw calls
- existing fbdev presentation already requires GPU readback/copy work
- target hardware is low-power ARM + Mali GLES
```

It may still be worth one bounded experiment:

```text
Implement the general CPU expansion fallback well enough to reach gameplay.
Measure whether it is near playable or obviously hopeless.
```

Decision rule:

```text
If it reaches gameplay but is around 2-5 FPS, stop pursuing this GLES path.
If it is surprisingly close to playable, then optimize expansion/caching/batching.
```

Possible optimizations if the first general fallback is not terrible:

```text
- cache expanded static/indexed vertex data when source arrays and descriptors do not change
- avoid re-expanding identical draw payloads
- batch compatible adjacent draws after expansion
- reduce debug logging and pipeline churn
- keep all effects disabled and frame cap low
```

### Viability summary

This is no longer a simple PortMaster packaging job. The port has crossed into
renderer compatibility work.

The project may still be technically feasible, but the next useful step is not
another one-off vertex-layout patch. The next useful step is either:

```text
1. build a general no-vertex-storage fallback and measure performance quickly
2. pause/stop this GLES path if the expected CPU expansion cost is not worth it
```

Weston runtimes, X11/Wayland surface changes, and launcher tweaks are unlikely
to fix the current crash because the failing limit comes from the GLES shader
capabilities exposed by the driver.

## 2026-06-04 focused fallback experiment

A first general no-vertex-storage fallback was implemented and deployed.

Build-ID:

```text
27786ad31129812580fc6b1f6b74812103db7f4a
```

Deployment paths:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Both stripped files were:

```text
22386712 bytes
```

Scope of the experiment:

```text
- Keep existing hand-patched CPU expansion cases as fast paths.
- Add a generic CPU vertex expansion path before the raw storage-buffer path.
- Enable it for non-line/non-point GX draws when the PortMaster no-surface/fbdev path is active.
- Decode GX_DIRECT, GX_INDEX8, and GX_INDEX16 attributes on the CPU.
- Produce canonical native vertex attributes:
  - matrix indices as u32
  - POS and NRM as f32x3
  - CLR0/CLR1 as RGBA8 normalized vertex attributes
  - TEX0-TEX7 as f32x2
- Generate dynamic WGSL native vertex inputs instead of only hardcoded POS/CLR0/TEX0/NRM.
- Generate dynamic WebGPU vertex-buffer layouts matching those inputs.
- Decline the generic fallback if more than 16 native vertex attributes are active.
```

Important limitations:

```text
- GX_LINES, GX_LINESTRIP, and GX_POINTS are not handled by the generic fallback yet.
  Their shader path uses instance expansion and should be treated separately.
- This is a correctness/progress experiment, not an optimized implementation.
- If it reaches gameplay but runs badly, the next question is whether caching expanded
  vertex data can recover enough performance.
```

Expected diagnostic lines when the fallback is active:

```text
PortMaster GX generic native expand draw: ...
PortMaster GX native vertex pipeline: ...
```

If a storage-block crash still appears with this build, check whether the failing
draw is a line/point draw, whether the generic fallback declined due to too many
native attributes, or whether a native shader still accidentally emitted
`var<storage>`/`ptr<storage>`.

### Follow-up: bitset 255 crash

The first generic fallback build reached a storage-free generated shader, proving
that the no-storage path was active, but then crashed with:

```text
terminate: uncaught exception: bitset::set: __position (which is 255) >= _Nb (which is 30)
```

This was not the old GLES storage-block failure. The generated shader shown in
the log had no `var<storage>` bindings.

Cause:

```text
The forced generic native ShaderConfig replaced the whole shader config instead
of overlaying only native vertex attrs/stride on top of the fully populated
PipelineConfig. Non-vertex GX state such as texture generation config was left at
default placeholder values. One placeholder enum value was 255, which later got
used as a bitset index in shader_info.
```

Fix:

```text
handle_draw_unmerged now calls populate_pipeline_config first, then overlays only:
  - shaderConfig.attrs
  - shaderConfig.vtxStride
  - shaderConfig.nativeVertexFetch
  - shaderConfig.lineMode = 0 for this non-line fallback

TEV, TCG, fog, alpha compare, color channel, indirect stage, and texture state
remain from the normal populated config.
```

Fixed Build-ID:

```text
094d20eb3a88786368864e7f80f727e366d02570
```

The fixed binary was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

### Follow-up: coherent graphics but extremely slow

After the native vertex fallback fixes, the game reached the opening scene and
presented coherent geometry through the fbdev presenter. A later correctness fix
changed `GX_VA_PNMTXIDX` handling to divide the raw matrix-index byte by 3
before passing it to the shader. Before that fix, the screen showed exploded
triangles and large flat shards; after it, captures showed recognizable castle,
terrain, sky, horse/rider, and lighting.

The successful coherent build was:

```text
a90562f06d8a02871d3e2dff9c1bae868ec47856
```

It was still not meaningfully playable on the test handheld. The scene rendered
very slowly and looked effectively frozen while the device continued working.
This is consistent with the current PortMaster path doing CPU-side GX vertex
expansion because the Mali GLES stack reports:

```text
GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0
```

The core workaround is correct enough to draw, but it moves work from the GPU
vertex shader path into CPU expansion and native vertex-buffer uploads. That is
the primary performance risk.

### Low-spec graphics test build

To fail quickly on whether "eye candy" was the main remaining problem, a
PortMaster-only low-spec switch was added:

```text
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
```

When this environment variable is set, `src/m_Do/m_Do_graphic.cpp` now skips:

```text
shadow texture pass
shadow draw pass
motion blur
depth-of-field/framebuffer depth capture
retry framebuffer captures
bloom
3D particle draw groups
2D particle overlays
invisible/filter/indirect screen effect passes
```

The normal desktop path is unchanged unless the environment variable is present.

The PortMaster launcher also now forces a more aggressive low-spec profile:

```text
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
```

Low-spec Build-ID:

```text
0262e1b7b98311982be0a536719006dea5ceaf9a
```

Deployment notes for this build:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64      size matched: 22386712 bytes
/roms/ports/dusklight/dusklight.aarch64         size matched: 22386712 bytes
/roms/ports/dusklight.sh                        verified with LOW_SPEC flag
```

While deploying the launcher, the device's `/mnt/mmc` FUSE mount started
returning:

```text
Transport endpoint is not connected
```

The `/roms` path remained accessible and was updated/verified. If testing from
muOS does not pick up the new launcher, reboot the device first so `/mnt/mmc`
and `/mnt/union/ROMS` are remounted cleanly, then confirm:

```text
grep -n 'LOW_SPEC\|maxFrameRate\|bloomMultiplier' /mnt/mmc/roms/ports/dusklight.sh
```

If this low-spec build is still too slow, the next worthwhile work is not more
visual-option disabling. It is optimizing the CPU vertex expansion path:

```text
cache/reuse expanded vertex buffers where possible
re-enable corrected fast paths for common GX layouts
avoid per-draw allocations/copies
measure draw-count and expansion-byte hot spots with rate-limited logging
```

### Redeploy after power-cycle/missing launcher

After a later power cycle, launching from muOS immediately returned to the menu
and no fresh Dusklight log appeared. On the device at `192.168.10.132`,
`/mnt/mmc` was writable, but the canonical muOS launcher path was missing:

```text
/mnt/mmc/roms/ports/dusklight.sh: No such file or directory
```

The same low-spec binary and launcher were redeployed cleanly:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64      22386712 bytes
/roms/ports/dusklight/dusklight.aarch64         22386712 bytes
/mnt/mmc/roms/ports/dusklight.sh                3542 bytes
/roms/ports/dusklight.sh                        3542 bytes
```

Both launcher copies were verified to include:

```text
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
--cvar video.maxFrameRate=5
--cvar game.bloomMultiplier=0
--cvar audio.enableReverb=false
--cvar audio.enableHrtf=false
```

If a future launch returns to menu with no new log, first check that
`/mnt/mmc/roms/ports/dusklight.sh` still exists and is executable before
debugging the binary.

### Next planned pass: path 4 specialized native fast paths

The current working renderer path is correct enough to run, but too slow. The
next pass is to push "path 4": specialized native fast paths for common GX
layouts, while keeping the generic CPU expansion as the correctness fallback.

Important current finding:

```text
handle_draw() currently tries expand_portmaster_generic_draw() before the older
specialized fast paths.
```

That means the older fast paths such as:

```text
can_expand_index16_pos_color()
can_expand_direct_pos_tex()
```

are effectively bypassed whenever generic expansion succeeds. For PortMaster
no-vertex-storage mode, the draw routing should be:

```text
1. Try known specialized native fast paths.
2. Fall back to generic CPU expansion.
3. Fall back to original storage-buffer path only when not in PortMaster mode.
```

The immediate offline work, before live hardware is available again:

```text
Add compact layout statistics keyed by primitive, vtx format, FIFO stride,
and active GX_VA descriptors.

Rate-limit logs so device testing can reveal:
  - top draw layouts by draw count
  - top layouts by expanded bytes
  - how often specialized fast paths fire
  - how often generic expansion remains necessary

Move existing specialized paths before generic expansion in PortMaster mode.

Reserve expanded ByteBuffer capacity before per-vertex appends to avoid repeated
growth/copy overhead.

Add one or two safe specializations for already observed layouts:
  - direct POS + CLR0, stride 16, used by early logo/menu quads
  - indexed POS/NRM/CLR0/TEX0 combinations already handled by the older helper
```

Why this comes before the texture-fetch experiment:

```text
The layout stats will tell us whether a small number of GX layouts dominate.
If they do, native fast paths are the fastest route to a useful result.
If many layouts dominate or CPU upload remains too expensive, the same stats
tell us what a vertex-texture-fetch prototype must support first.
```

Offline stopping point for this pass:

```text
Build succeeds in the PortMaster AArch64 Docker toolchain.
No live-device performance conclusion until the next hardware test.
```

### Follow-up: debug log flood / possible OOM reset

The `c87221...` build got past the `bitset(255)` crash and started running the
draw loop. The log showed repeated successful generic/native expansion lines:

```text
PortMaster GX generic native expand draw: ...
PortMaster GX native vertex pipeline: ...
```

The user reported that nothing appeared on screen, then the console hard-reset
back to menu and needed a reboot. Most likely cause:

```text
DUSKLIGHT_PORTMASTER_GX_DEBUG=1 was still enabled in the menu launcher.
The app was logging every expanded GX draw through tee into log.txt.
On this low-memory device, the resulting IO/memory pressure can starve rendering
or destabilize muOS before a useful visual/performance test.
```

Local fixes prepared after that reset:

```text
- Removed DUSKLIGHT_PORTMASTER_GX_DEBUG=1 from packaging/portmaster/aarch64/dusklight.sh.
- Added a hard cap of 200 draw-level GX debug log messages in command_processor.cpp
  so accidental future debug launches do not flood indefinitely.
```

Quiet/rate-limited local build prepared:

```text
Build-ID: d152d2471c021ede69f12a8e75759f08451920b5
Size:     22386712 bytes
Path:     /tmp/dusklight.aarch64
```

After the console reboots, deploy both:

```text
/tmp/dusklight.aarch64 -> /mnt/mmc/ports/dusklight/dusklight.aarch64
packaging/portmaster/aarch64/dusklight.sh -> /mnt/mmc/roms/ports/dusklight.sh
```

Do not relaunch the old on-device menu script before replacing it, because it may
still export `DUSKLIGHT_PORTMASTER_GX_DEBUG=1`.

Deployed after reboot at `192.168.10.130`:

```text
/mnt/mmc/roms/ports/dusklight.sh
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Verified deployed binary:

```text
Build-ID: d152d2471c021ede69f12a8e75759f08451920b5
Size:     22386712 bytes
```

Verified launcher exports only:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_NO_SURFACE=1
DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1
DUSKLIGHT_PORTMASTER_FBDEV_PRESENT=1
```

`DUSKLIGHT_PORTMASTER_GX_DEBUG=1` is no longer present in the menu launcher.
Pipeline cache was cleared again before the next test launch.

### Follow-up: first coherent framebuffer captures

After deploying `a90562...`, framebuffer captures from `/dev/fb0` showed
coherent graphics instead of exploded triangles. The captured scene showed the
castle gate, horse/rider, terrain, sky, and lighting rendered plausibly.

Two captures taken a few seconds apart differed, so fbdev presentation is
updating rather than showing a single stale frame.

Current state:

```text
- No vertex shader storage-block crash.
- No bitset(255) crash.
- No debug log flood.
- Geometry is now coherent in captured frames.
- Runtime remains very slow because every non-line/non-point GX draw is CPU
  expanded before upload.
- Launcher is still capped at video.maxFrameRate=5 for safety while debugging.
```

Next useful checks:

```text
- User-visible animation/input responsiveness at 5 FPS cap.
- Try a slightly higher cap only after confirming it no longer wedges muOS.
- Profile/estimate whether CPU expansion can be optimized enough to be playable.
```

### Follow-up: PN matrix index conversion

The `118d712...` correctness-first build still produced exploded/dark triangle
geometry. A framebuffer capture from `/dev/fb0` showed geometry corruption rather
than framebuffer channel-order corruption.

Likely issue:

```text
Aurora's original shader path treats GX_VA_PNMTXIDX specially:
  raw_fetch_u8_1(...) / 3u

The generic CPU decoder was uploading the raw PN matrix byte as a native u32.
That can index the wrong position matrix and produce exploded geometry.
```

Fix:

```text
For GX_VA_PNMTXIDX, append (*src / 3) as the native u32 input.
Texture matrix indices remain raw.
```

Deployed Build-ID:

```text
a90562f06d8a02871d3e2dff9c1bae868ec47856
```

Deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Pipeline cache was cleared again before the next test launch.

### Follow-up: prefer generic decoder for correctness

The `68f345...` build still showed visual corruption. To remove mismatch from
the older hand-specialized fast paths, draw routing was changed so the
PortMaster no-storage path tries the generic CPU decoder first for non-line,
non-point draws. The older `INDEX16` and direct `POS+TEX0` fast paths remain as
fallbacks only if the generic decoder declines.

Rationale:

```text
The generic decoder applies one consistent conversion path for direct/index8/
index16 attributes and native vertex layout generation. The hand fast paths were
built incrementally while chasing crashes and may still differ subtly.
```

Deployed Build-ID:

```text
118d712df29f45e9f46f9a716c4ce5da8f5807be
```

Deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Pipeline cache was cleared again before the next test launch.

### Follow-up: graphical mess / indexed position endian fix

The quiet `d152d...` build ran far enough to load graphics, but output was very
slow and visually corrupt. Memory was not exhausted at the time of inspection,
and the log was no longer flooded with GX debug lines.

Likely visual corruption source found:

```text
expand_index16_pos_color copied POS f32x3 bytes directly from GX attribute
arrays into the native vertex buffer.
```

That is wrong for native vertex attributes if the source array is big-endian.
The generic fallback already endian-converted numeric attributes, but this older
INDEX16 fast path still uploaded raw bytes.

Fix:

```text
Read POS with read_f32(posData + offset, !posArray.le) and append host-endian
float values.
```

The launcher was also changed from:

```text
--cvar video.maxFrameRate=30
```

to:

```text
--cvar video.maxFrameRate=5
```

This is only for visual sanity testing under the expensive CPU-expansion path.

Deployed Build-ID:

```text
68f345037a0b9a15afafc3500141d5bf7f493381
```

Deployed to:

```text
/mnt/mmc/roms/ports/dusklight.sh
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Pipeline cache was cleared again before the next test launch.

### Follow-up: shader_info post matrix guard

The `094d20...` build still hit the same bitset exception. The log confirmed it
was running the correct Build-ID, and the generated shader was still
storage-free. The exception came from `shader_info.cpp` when calculating
`usesPTTexMtx`:

```text
tcg.postMtx = 255
postMtxIdx = (255 - GX_PTTEXMTX0) / 3
usesPTTexMtx.set(postMtxIdx) -> bitset out_of_range
```

Valid post texture matrices are only:

```text
GX_PTTEXMTX0..GX_PTTEXMTX19 = 64..121
GX_PTIDENTITY = 125
```

The guard now only sets `usesPTTexMtx` when `tcg.postMtx` is inside the valid
post texture matrix range. Identity and invalid/sentinel values are ignored.

Guarded Build-ID:

```text
76cfce3a34ee70e2169a2be5c6230404cc42128d
```

The guarded binary was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

### Follow-up: stale pipeline cache replay

The `76cfce...` guarded build still crashed with the same bitset exception.
`addr2line` showed the crash came from:

```text
aurora::gx::create_pipeline
aurora::gfx::find_pipeline_impl
aurora::gfx::initialize_pipeline_cache
```

So the exception was happening while replaying persisted pipeline cache entries,
not necessarily while processing a fresh live draw. The GX pipeline cache version
was still `14`, so rows produced by earlier buggy native-fallback builds were
still considered valid.

Fix:

```text
GXPipelineConfigVersion bumped from 14 to 15.
```

The device runtime cache was also cleared manually:

```text
/mnt/mmc/ports/dusklight/runtime/TwilitRealm/Dusklight/pipeline_cache.db
/mnt/mmc/ports/dusklight/runtime/TwilitRealm/Dusklight/pipeline_cache.db-shm
/mnt/mmc/ports/dusklight/runtime/TwilitRealm/Dusklight/pipeline_cache.db-wal
```

Version-bumped Build-ID:

```text
0c341697678e70e1e785f639b6ce74b2a080485d
```

The version-bumped binary was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

### Follow-up: native vertex shaderLocation underflow

After the cache version bump, the same `bitset::set(255)` exception appeared
again, but the log showed a fresh live generic expansion:

```text
PortMaster GX generic native expand draw: prim=128 fmt=0 vtxCount=4 fifoStride=16 nativeStride=16 bytes=64
```

`addr2line` showed the crash was in fresh pipeline creation, not cache replay:

```text
handle_draw_unmerged
pipeline_ref
create_pipeline
```

The generated WGSL used valid input locations:

```wgsl
@location(0) in_pos: vec3f
@location(1) in_clr0: vec4f
```

The bug was in the C++ WebGPU vertex-buffer layout construction. The code used
`out.attrs[attributeCount++]` and `.shaderLocation = attributeCount - 1` in the
same assignment expression. Evaluation order can make `.shaderLocation` compute
before `attributeCount++`, causing the first native attribute to underflow to an
invalid location. Dawn reports that through a `bitset<30>` exception with
position `255`.

Fix:

```text
const uint32_t location = attributeCount++;
out.attrs[location] = {
  ...
  .shaderLocation = location,
};
```

Fixed Build-ID:

```text
c87221b53a7ff9d9c6194daa5e511b3ac5dcfece
```

The fixed binary was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```
