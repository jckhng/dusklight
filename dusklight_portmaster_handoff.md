# Dusklight PortMaster / RG35XX H Feasibility Handoff

Current experiment stocktake:

```text
docs/portmaster-experiment-stocktake.md
```

Use that document first for the latest graphics-mode, compatibility, and
dead-end ledger. This handoff keeps the longer historical investigation trail.

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

This is **working as a rough PortMaster test port**, not just a feasibility
spike. Dusklight can build for Linux aarch64, launch through PortMaster, load
user-provided game data, enter gameplay, and run on the RG35XX H / muOS /
Mali-G31 stack.

It is still not smooth. Heavy village scenes remain around low-double-digit FPS
on the RG35XX H, while lighter/indoor scenes can be much closer to the 30 FPS
pacing cap. The current work is therefore performance and compatibility
hardening, not first-boot enablement.

## Current 2026-06-09 Handoff

This section merges the newer `dusklight_vertex_texture_handoff.md` notes into
this canonical PortMaster handoff.

Current graphics path:

```text
Dusklight
  -> Aurora GX
  -> WebGPU/Dawn OpenGLES
  -> Mali EGL/fbdev native-window surface
  -> SDL offscreen video driver for input/window bootstrap
```

Important current paths:

```text
Launcher:
  /mnt/mmc/ROMS/Ports/dusklight.sh

Payload:
  /mnt/mmc/ports/dusklight

Binary:
  /mnt/mmc/ports/dusklight/dusklight.aarch64

Runtime log:
  /mnt/mmc/ports/dusklight/log.txt
```

Do not use `/roms/ports` on the RG35XX H / muOS test setup. It has been stale
or wrong during testing.

Current launcher defaults:

```text
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
DUSKLIGHT_PORTMASTER_NOINDEX_TRIANGLES=1
DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY=1
DUSKLIGHT_PORTMASTER_BATCH_STRIPS=1
DUSKLIGHT_PORTMASTER_BATCH_QUADS=1
DUSKLIGHT_PORTMASTER_DISABLE_DEPTH_PEEK=1
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=30
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=4
SDL_VIDEODRIVER=offscreen
```

### What Actually Unblocked It

The original feasibility question was whether Dawn OpenGLES could build and run
on Linux aarch64 / muOS. That now works. The harder runtime blocker became
Aurora GX's use of vertex-stage storage buffers.

The Mali GLES stack reports:

```text
GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0
```

That made the original GX vertex-fetch shaders fail with:

```text
The number of vertex shader storage blocks is greater than the maximum number allowed (0)
```

The working compatibility path is texture-backed vertex fetch. Compact GX
vertex stream/array data is uploaded to texture-like storage and fetched with
vertex-stage texture loads instead of `var<storage>` buffers. This is slower
than true vertex buffers/storage buffers, but it runs on Mali GLES and avoids
generic CPU expansion for every draw.

### Biggest Performance Win So Far

Triangle-strip batching was the biggest practical playability gain.

The important GX changes are:

```text
DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY=1
DUSKLIGHT_PORTMASTER_BATCH_STRIPS=1
DUSKLIGHT_PORTMASTER_NOINDEX_TRIANGLES=1
primitive/index pattern reuse
```

Before strip topology/batching, the port could render but felt much worse
because many tiny GX draws were submitted separately. Preserving strip topology
and batching compatible consecutive strips reduced submission pressure enough to
make route testing viable.

Quad batching exists, but current gameplay profiles show strips are the more
important heavy-scene primitive. Do not forget this when comparing future
performance work; strip batching is the major known win.

### Dead Ends / Low-Return Paths

Weston/X11/Xwayland was useful to understand the stack, but it was not the
portable solution for this port. The direct Mali EGL/fbdev native-window surface
is the current working path.

The old CPU fbdev presenter proved first pixels, but it was too slow. At
640x480 it spent hundreds of milliseconds in presentation. Recent logs show
`fbdev_present_ms=0.0`, so the presenter is no longer the bottleneck.

Broad generic native/CPU vertex expansion produced graphical corruption and did
not become a reliable performance path. Keep native vertex work targeted to very
specific known layouts only.

### Recent TEV Deferral Result

BP profiling showed raw writes to TEV color/K-color registers `E2-E7` dominate
the BP traffic. A safe deferral was added so TEV/K-color writes only force a
draw split if the next shader actually reads the changed register.

Result:

```text
gx_bp reg merge-block counts dropped sharply
raw gx_bp_top E2-E7 writes stayed high
steady village FPS changed little
```

Interpretation: TEV register dirtying was real overhead, but not the next big
performance lever after strip batching.

### Current Bottleneck / Next Work

The current steady heavy-scene cost is still draw submission and state churn.
Recent village logs show `submit_ms` often around 24-30 ms while presentation is
effectively zero. After TEV deferral, `gx_dirty[xf]`, texture/state changes, and
remaining BP changes are more important than TEV register dirtying alone.

Next good measurement:

```text
Add XF dirty-source instrumentation similar to BP top-register instrumentation.
Find whether transform/matrix writes are redundant, deferable, or actually used
by the next draw.
```

Avoid spending more time on blind primitive culling, TEV-only tweaks, or
Weston/presenter experiments until the XF/state profile is clearer.

### Current Optimization Protocol

Do not work on these areas while chasing the next performance gain:

```text
launcher
presenter
X11 / Wayland / WestonPack
controls
global draw skip
```

Those paths have either already been stabilized enough for the current test
package or already consumed enough time without being the active heavy-scene
bottleneck. Keep the next pass focused on GX work reduction.

Baseline profiling run:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
current strip/quad batching enabled
one heavy village route
```

The run should report:

```text
top XF dirty sources by count
which XF dirties block merges
gx_merge[try/ok/dirty/tex/idx/...]
top GX layouts by primitive name, fifoStride, desc, draws, fifoBytes
gx_index cache hit/miss/bytes
```

Add XF dirty-source instrumentation comparable to the existing BP top-register
instrumentation before making another optimization decision. The goal is to
separate real transform/matrix state changes from redundant XF writes that only
break batching/merging.

Only choose one targeted optimization after that profile:

```text
1. Defer redundant XF writes if the next draw does not consume them, or if the
   write is provably unchanged.
2. Improve strip/quad batching if merge blockers are now understood.
3. Add a low-spec cull only if a draw-cull A/B profile shows a material gain.
```

Current optimization order:

```text
1. Stabilize current test package.
2. Profile XF/state merge blockers.
3. Improve strip/quad batching based on counters.
4. A/B grass/shadow/simple-model culls.
5. Keep guarded pacing as the playability layer.
6. Stop if worst scenes remain around 4-5 FPS after those.
```

Triangle-strip batching/merging reference:

```text
Launcher flags:
  packaging/portmaster/aarch64/dusklight.sh
    DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY=1
    DUSKLIGHT_PORTMASTER_BATCH_STRIPS=1

Implementation:
  extern/aurora/lib/gx/command_processor.cpp
    build_triangle_strip_topology_indices()
    build_triangle_strip_batch_topology_indices()
    handle_draw() strip batch scan for consecutive GX_TRIANGLESTRIP draws
    adjacent draw merge path, including primitive restart insertion

Pipeline:
  extern/aurora/lib/gx/gx.cpp
    TriangleStrip topology + Uint16 strip restart format

Stats:
  extern/aurora/lib/gx/gx.hpp
  extern/aurora/lib/gfx/common.cpp
```

The critical detail is that batching does not combine arbitrary draws. It only
combines immediately adjacent compatible strip draws with no intervening GX
state command. The `0xffff` primitive restart marker separates original strips
inside the combined index stream so triangles do not connect across old draw
boundaries.

### 2026-06-09 XF Scalar Duplicate Skip

Village profiling with `DUSKLIGHT_PORTMASTER_GX_STATS=1` showed that merge
blocks were heavily attributed to XF texgen scalar writes:

```text
gx_xf_block_top[03F,040,050,...]
0x03F = numTexGens
0x040 = TexGen config 0
0x050 = post-transform TexGen 0
```

A conservative duplicate-write skip was added for independent scalar XF
registers:

```text
0x000-0x019
0x03F-0x05F
```

Viewport/projection payloads (`0x01A-0x026`) are intentionally excluded because
they are multi-word state blocks; comparing only the first scalar word could
skip a real projection/viewport update.

Expected next test:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
heavy village route
compare gx_dirty[xf], gx_xf_block[texgen], gx_xf_block_top[03F/040/050],
gx_merge[try/ok/dirty], and FPS against the previous profile.
```

Follow-up run after deploying the scalar XF skip showed the skip is active:

```text
xf_skip often in the hundreds of thousands per 5-second timing window
gx_xf_block_top no longer dominated only by 03F/040/050
```

The remaining heavy-scene blockers are mostly real state changes:

```text
0x000 = position matrix 0
0x00C = material color 0
0x078 = texture matrix 0
0x018 = matrix index A
0x680 = CP matrix index A synthetic marker
0x03F = numTexGens, still present but no longer the only story
```

Representative village samples:

```text
fps=10.59 gx_merge[try=12575 ok=5512 dirty=5261 line=1802]
gx_xf_block_top[000:4512,00C:1554,03F:742,400:522,018:274,680:274]

fps=12.90 gx_merge[try=14268 ok=5950 dirty=6108 line=2210]
gx_xf_block_top[000:5198,00C:1828,03F:853,400:593,00E:268,010:268]
```

Interpretation:

```text
The easy redundant-XF path is mostly exhausted.
Position matrices and material colors are changing for real per object/draw.
Further large batching gains probably require carrying per-strip/per-vertex
state through one larger draw, or reducing/culling selected low-spec scene work.
```

Do not keep broadening scalar XF duplicate skipping unless a new profile shows a
specific false-dirty source. Projection writes were separately content-checked
after this run because they were still marked dirty unconditionally, but that is
expected to be a small cleanup rather than a major FPS gain.

## Original Feasibility Notes

The project is open source and the Android port is not a Java/Kotlin game. It is a minimal SDLActivity wrapper around native C/C++ code. That means the Android port is useful evidence that the native code can build for ARM64.

However, this is **not a simple SDL/OpenGL ES game**. Dusklight uses Aurora, and Aurora routes rendering through WebGPU/Dawn. The `opengles` backend appears to mean:

```text
Dusklight
  -> Aurora GX
    -> WebGPU API
      -> Dawn backend
        -> OpenGLES
```

The early hard question was not “can SDL do GLES?” The hard question was whether
**Dawn OpenGLES can be built and run on Linux ARM64 / muOS**. That has now been
answered yes for the current RG35XX H / muOS test device, with the PortMaster
EGL/fbdev surface patch and texture-backed GX vertex fetch.

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

### Path 4 offline implementation pass

Implemented offline while live hardware was unavailable:

```text
PortMaster draw routing now tries specialized native fast paths before the
generic CPU expansion path.

Existing fast paths were made reachable again:
  - indexed POS/NRM/CLR0/TEX0 style path
  - direct POS+TEX0 path

Added one new observed-layout fast path:
  - direct POS+CLR0, FIFO stride 16
```

The stride-16 direct POS+CLR0 layout was seen repeatedly in earlier logs:

```text
PortMaster GX direct expand skipped: fmt=0 vtxSize=16 posDesc=1 tex0Desc=0 nrmDesc=0 clr0Desc=1
PortMaster GX generic native expand draw: prim=128 fmt=0 vtxCount=4 fifoStride=16 nativeStride=16 bytes=64
```

That layout should now use the new `direct-pos-clr0` fast path instead of the
generic decoder.

Also added:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
```

to the launcher for the next diagnostic run. When enabled, Aurora records compact
layout summaries every 2048 expanded draws. Expected log marker:

```text
PortMaster GX layout stats after ... expanded draws:
```

Each summary line reports:

```text
prim fmt fifoStride desc draws vertices fifoBytes nativeBytes
paths[g=...,i16=...,pt=...,pc=...]
```

Path counters:

```text
g   generic fallback expansion
i16 indexed POS/NRM/CLR0/TEX0 fast path
pt  direct POS+TEX0 fast path
pc  direct POS+CLR0 fast path
```

The expansion buffers now reserve the expected output size before appending
vertices, which should reduce allocator growth/copy overhead even when generic
fallback remains necessary.

Offline diagnostic Build-ID:

```text
8eb007901cf35d2328f82d356fc567cd4c00b84e
```

Not yet deployed or tested on hardware. Next hardware test should check:

```text
grep -n 'PortMaster GX layout stats\\|direct-pos-clr0\\|generic native expand' /mnt/mmc/ports/dusklight/log.txt
```

Useful interpretation:

```text
If pc/i16/pt dominate and speed is still bad:
  CPU upload/draw count is likely the bottleneck, not only generic decode.

If g still dominates:
  add fast paths for the top desc/fifoStride layouts in the stats summary.

If no stats appear:
  confirm /mnt/mmc/roms/ports/dusklight.sh contains DUSKLIGHT_PORTMASTER_GX_STATS=1.
```

### Regression: old indexed fast path causes no visible video

The first path-4 diagnostic deployment produced sound but no visible video. The
process kept running and stats were emitted, so this was not a launch failure.

The log showed the old indexed fast path dominated immediately:

```text
prim=152 fmt=0 fifoStride=8 desc=0x0000cfc0000 ... paths[g=0,i16=...,pt=0,pc=0]
```

That path had been unreachable while generic expansion was tried first. Making
it reachable appears to be a visual correctness regression. The immediate fix is
to disable the `i16` fast path again and route indexed layouts back through the
known-good generic native expansion.

Keep this dead end in mind:

```text
Do not re-enable can_expand_index16_pos_color() ahead of generic expansion
unless its output layout is made semantically identical to the generic native
shader config, including active attributes and matrix-index handling.
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

### Follow-up: black screen despite rendered frames

After disabling the regressing old INDEX16 fast path and returning to the
generic native expansion path, Dusklight could run without the previous shader
storage-buffer crash. The log advanced normally and GX stats showed live draw
traffic, but the device screen showed no visible frames.

The key framebuffer facts on RG35XX H / muOS:

```text
/sys/class/graphics/fb0/virtual_size = 640,960
fbset geometry = 640 480 640 960 32
stride = 2560
```

That means `/dev/fb0` contains two 640x480 pages. A raw capture showed:

```text
page 0: coherent Dusklight frame
page 1: black
```

Copying page 0 into page 1 once while the game was running made the image show
on the LCD. So this was not a renderer failure. The fbdev presenter was writing
only the first visible-height page while the LCD scanout was using the other
virtual page.

Temporary confirmation command used on device:

```bash
dd if=/dev/fb0 of=/tmp/fb-page0.raw bs=1228800 count=1
dd if=/tmp/fb-page0.raw of=/dev/fb0 bs=1228800 seek=1 conv=notrunc
```

Code fix:

```text
Aurora fbdev presenter now maps the full virtual framebuffer height and mirrors
each presented frame to every full-height virtual page.
```

This is intentionally conservative for PortMaster devices. It avoids assuming
which y-offset/page the frontend left active. It costs extra framebuffer copy
bandwidth, but the current bottleneck is still CPU-side GX vertex expansion, not
the final fbdev copy.

Patched Build-ID:

```text
90c548d3505a78591aea0d5e829ce7f9f34e8ba2
```

The patched binary was deployed to:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
```

Pipeline cache was cleared again:

```text
/mnt/mmc/ports/dusklight/runtime/TwilitRealm/Dusklight/pipeline_cache.db*
```

## 2026-06-04: Direct Mali EGL/fbdev Surface Experiment

The old offscreen Dawn + CPU fbdev-present path was proven to work, but timing
showed the presenter was the main bottleneck:

```text
PortMaster fbdev timing: avg_write_ms ~= 191-383
PortMaster timing: steady heavy scene ~= 2.8-3 FPS
```

A small raw EGL probe was added:

```text
packaging/portmaster/probes/egl_fbdev_swap_probe.c
```

It creates an ARM EGL window surface directly from a simple fbdev native window
struct:

```c
struct fbdev_window {
    unsigned short width;
    unsigned short height;
};
```

On RG35XX H / muOS it reported:

```text
egl=1.4 vendor=ARM client_apis=OpenGL_ES
gl_renderer=Mali-G31
swap_probe fps ~= 41-42 at 640x480
avg_swap_ms ~= 23-24
```

This proves native Mali EGL presentation is far faster than the CPU framebuffer
write path.

### Experimental Dawn Surface Hook

Dawn has no public Linux fbdev surface source. Its OpenGL EGL swapchain can call
`eglCreateWindowSurface`, but only for supported Dawn surface types. For a
fail-fast test, we used a local sentinel hack:

- Aurora returns a `SurfaceSourceXlibWindow` when
  `DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1`.
- `display == nullptr` means the `window` value is actually a pointer to the
  fbdev native window struct.
- Dawn validation accepts that sentinel.
- Dawn `SwapChainEGL` passes that pointer to `eglCreateWindowSurface`.

Files involved:

```text
extern/aurora/lib/dawn/BackendBinding.cpp
build/portmaster-aarch64-vendor-dawn/_deps/dawn-src/src/dawn/native/Surface.cpp
build/portmaster-aarch64-vendor-dawn/_deps/dawn-src/src/dawn/native/opengl/SwapChainEGL.cpp
```

Important caveat: the Dawn edits are currently in the generated build tree, not
a durable source patch. If the build directory is recreated, this direct-surface
hack disappears unless promoted into a patching step.

The launcher was changed on-device to use:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
DUSKLIGHT_PORTMASTER_GX_STATS=1
SDL_VIDEODRIVER=offscreen
```

`DUSKLIGHT_PORTMASTER_X11_DAWN=1` is still needed despite the bad name because
Aurora currently uses it to avoid adding `SDL_WINDOW_OPENGL` to the dummy SDL
window. Without it, SDL offscreen window creation fails before Dawn can create
the raw EGL surface.

### Combined With Vertex-Texture Fallback

The direct surface initially regressed to the old Mali SSBO crash:

```text
The number of vertex shader storage blocks (...) is greater than the maximum number allowed (0)
```

Fix:

```text
extern/aurora/lib/gx/command_processor.cpp
```

`portmaster_no_vertex_storage_mode()` now also treats
`DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1` as a no-storage-buffer mode, so the
texture-backed vertex fetch path is used with the direct surface.

Timing logs were also enabled for this path:

```text
extern/aurora/lib/gfx/common.cpp
```

### Result

After deploying both the rebuilt executable and rebuilt Dawn shared library:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64
/mnt/mmc/ports/dusklight/lib.aarch64/libwebgpu_dawn.so
/roms/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/lib.aarch64/libwebgpu_dawn.so
```

Dusklight configured a real Dawn surface:

```text
[INFO | aurora::gpu] Using surface format RGBA8Unorm, present mode Mailbox
```

It passed the previous SSBO crash point and ran past frame 600 in testing.

Measured timing examples:

```text
PortMaster timing: fps=11.17 frames=56 avg_cpu_frame_ms=16.7 max_cpu_frame_ms=221.5 draws_last=1
PortMaster timing: fps=7.65 frames=42 avg_cpu_frame_ms=121.9 max_cpu_frame_ms=1472.5 draws_last=139
PortMaster timing: fps=11.77 frames=59 avg_cpu_frame_ms=62.4 max_cpu_frame_ms=457.9 draws_last=141
PortMaster timing: fps=11.46 frames=58 avg_cpu_frame_ms=56.2 max_cpu_frame_ms=77.7 draws_last=136
```

This is a major improvement over the CPU fbdev presenter path, but still not
full-speed. Remaining bottlenecks appear to be GX draw count / texture-backed
vertex fetch / shader compilation and upload churn, not framebuffer presentation.

Latest deployed executable Build-ID:

```text
6ad8371dca97a11e68dc3d6d46fe2d1b37e9103d
```

Latest deployed Dawn shared library Build-ID:

```text
2e953d29450a8ff6d04ca9e6b4a843f6668adec6
```

### 2026-06-04 Follow-Up: Restored Known-Good Direct EGL Binary

User timing after direct EGL showed scene-dependent improvement into the
roughly 6-17 FPS range:

```text
fps=17.02 draws_last=130 avg_cpu_frame_ms=47.3
fps=11.23 draws_last=141 avg_cpu_frame_ms=55.8
fps=12.76 draws_last=136 avg_cpu_frame_ms=55.7
fps=5.84  draws_last=195 avg_cpu_frame_ms=84.1
fps=11.96 draws_last=127 avg_cpu_frame_ms=60.6
```

Interpretation: the CPU fbdev presenter is no longer the main bottleneck.
Remaining cost tracks GX/rendering work, especially the texture-backed vertex
fetch fallback and raw FIFO draw/upload churn.

A quick attempt was made to force the dummy SDL offscreen window to 640x480 in
`extern/aurora/lib/window.cpp`. That regressed badly:

```text
Using framebuffer size still reported 1024x768
SIGSEGV shortly after Starting main01 (Game Loop)
```

That SDL window-size cap was removed. The rebuilt executable returned to the
known-good direct EGL Build-ID:

```text
6ad8371dca97a11e68dc3d6d46fe2d1b37e9103d
```

The on-device launcher was then changed from:

```text
--cvar game.internalResolutionScale=1
```

to:

```text
--cvar game.internalResolutionScale=0
```

This is a low-risk test because it uses Dusklight's existing setting instead of
lying to SDL during window creation. The first startup log may still print the
initial framebuffer size before `VISetFrameBufferScale()` runs, so compare the
steady `PortMaster timing` lines after gameplay starts.

### 2026-06-04 Follow-Up: Direct EGL Bottleneck Breakdown

Additional timing was added for the direct EGL path:

```text
extern/aurora/lib/aurora.cpp
```

The new line splits end-frame time into:

```text
avg_gfx_end_ms
avg_render_encode_ms
avg_finish_ms
avg_submit_ms
avg_after_submit_ms
avg_fbdev_present_ms
```

At the previous 1024x768 render target, steady-state samples showed the wall:

```text
avg_total_ms ~= 248-250
avg_submit_ms ~= 245-247
avg_finish_ms = 0
avg_after_submit_ms = 0
fps ~= 3.3
```

Conclusion: this was not CPU command encoding, not `encoder.Finish()`, not the
old CPU fbdev presenter, and not post-submit Aurora work. The Mali driver/GPU
was blocking inside `g_queue.Submit()` while executing the submitted frame.

### 2026-06-04 Follow-Up: Render Cap Test

The earlier attempt to force SDL window creation to 640x480 crashed. A safer
approach was added in:

```text
extern/aurora/lib/window.cpp
```

For `DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1`, Aurora now reads `/dev/fb0` for
the real native surface size, then caps only the internal render target through:

```text
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
```

This avoided the SDL-window crash route and reduced the GPU submit wall:

```text
1024x768: avg_submit_ms ~= 245-247, fps ~= 3.3
320x240:  avg_submit_ms ~= 15-21,  fps ~= 10-14.5 depending on scene
```

This is the best-performing direct EGL path so far. It is visibly lower
resolution, but it changes the bottleneck from GPU submit saturation to
CPU/game/render-prep time.

### 2026-06-04 Follow-Up: Depth Peek Spikes

Some scenes still showed:

```text
avg_after_submit_ms ~= 40
fps ~= 6.5
```

The source was `gfx::after_submit()`, specifically the depth-peek readback path
used by `GXPeekZ()`.

An env-gated fast-fail switch was added:

```text
extern/aurora/lib/dolphin/gx/GXCpu2Efb.cpp
DUSKLIGHT_PORTMASTER_DISABLE_DEPTH_PEEK=1
```

When set, `GXPeekZ()` returns the clear depth value and does not request a GPU
depth snapshot. This may cause some depth-dependent effects to layer
incorrectly, but it removed the post-submit spikes in the tested area:

```text
avg_after_submit_ms = 0
avg_submit_ms ~= 15-21
fps ~= 10-13.5 in the observed run
```

Latest deployed executable Build-ID with render cap + depth-peek switch:

```text
d703e7aae31b82d0180036934aa76ec50cbcb9b7
```

Current on-device launcher env block includes:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
DUSKLIGHT_PORTMASTER_DISABLE_DEPTH_PEEK=1
DUSKLIGHT_PORTMASTER_GX_STATS=1
SDL_VIDEODRIVER=offscreen
```

Remaining likely bottleneck:

```text
PortMaster timing avg_cpu_frame_ms ~= 55-72
gx_draws[tex] ~= 467k-535k per 5s interval
uploads ~= 300-670 KB/frame
```

Next meaningful optimization path is reducing CPU/GX texture-vertex work and
upload churn. Presentation and raw GPU submit are no longer the dominant wall at
320x240.

### 2026-06-04 Follow-Up: PortMaster Controls and Draw-Skip Probe

The PortMaster package should not start `gptokeyb` for normal gameplay.
Dusklight is already an SDL gamepad application, and the launcher exports:

```text
SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
```

Letting `gptokeyb` synthesize keyboard input on top of SDL gamepad input risks
double input and scrambled controls. Keep `.gptk` only as a reference/fallback,
not an always-on gameplay mapper.

Dusklight's stock `PAD_TYPE_STANDARD` mapping was also not ideal for handhelds:
it mapped `R1` to GameCube `Z` while GameCube `L/R` lived mainly on analog
trigger axes. Many PortMaster handhelds expose digital shoulders, so L/R were
effectively missing or unintuitive.

An env-gated handheld mapping was added:

```text
src/m_Do/m_Do_main.cpp
DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING=1
```

Current PortMaster handheld mapping:

```text
South/East/West/North -> GameCube A/B/X/Y
Start                 -> GameCube Start
Back/Select           -> GameCube Z
L1/R1                 -> GameCube L/R
D-pad                 -> GameCube D-pad
Left stick            -> GameCube main stick
Right stick           -> GameCube C-stick
L2/R2 axes            -> GameCube analog L/R, when exposed by the device
```

The source launcher now sets:

```text
DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING=1
```

An experimental draw-skip probe was also added:

```text
src/f_pc/f_pc_manager.cpp
DUSKLIGHT_PORTMASTER_DRAW_SKIP=N
```

Unset or `1` means no draw skipping. Values above `1` run the main game logic
every loop but suppress selected draw phases on skipped frames. This is only a
fast-fail performance probe because game logic, render state, fades, particles,
and audio were not designed around emulator-style frame skipping.

The launcher initially passed:

```text
DUSKLIGHT_PORTMASTER_DRAW_SKIP="${DUSKLIGHT_PORTMASTER_DRAW_SKIP:-1}"
```

So the first deployed default remained normal drawing unless the environment
overrode it before launching.

QoL cvars added to reduce repeated slow sequences:

```text
--cvar game.disableRupeeCutscenes=true
--cvar game.fastTears=true
--cvar game.instantSaves=true
--cvar game.instantText=true
```

Deployed executable Build-ID for the controls/draw-skip build:

```text
2aff27adf64b5af68fe3add238870f16593a87f0
```

### 2026-06-04 Follow-Up: Draw-Skip Default Raised to 3

Latest gameplay log, before changing the skip default, confirmed the launcher
was still using `DUSKLIGHT_PORTMASTER_DRAW_SKIP=1` and the remaining bottleneck
was not presentation:

```text
fps=10.27 avg_cpu_frame_ms=76.3 draws_last=141
uploads[v=345030 i=157950 s=403632]
gx_draws[tex=456361 cpu=0 native=0 storage=0]
avg_gfx_end_ms=7.8 avg_render_encode_ms=2.2 avg_submit_ms=17.6 avg_after_submit_ms=0.0 avg_fbdev_present_ms=0.0

fps=10.75 avg_cpu_frame_ms=71.6 draws_last=149
uploads[v=351254 i=158886 s=403632]
gx_draws[tex=477507 cpu=0 native=0 storage=0]
avg_gfx_end_ms=0.2 avg_render_encode_ms=2.3 avg_submit_ms=17.8 avg_after_submit_ms=0.0 avg_fbdev_present_ms=0.0
```

Interpretation: direct EGL/fbdev presentation is no longer a factor, and submit
is acceptable at 320x240. The heavy cost is still CPU/game/GX prep plus
texture-backed vertex draw/upload churn. Draw skipping is therefore a reasonable
fast-fail test even though it may look unnatural.

The source and on-device launchers were temporarily changed to default to
drawing every third logic frame:

```text
DUSKLIGHT_PORTMASTER_DRAW_SKIP="${DUSKLIGHT_PORTMASTER_DRAW_SKIP:-3}"
```

Changed on device:

```text
/mnt/mmc/roms/ports/dusklight.sh
/roms/ports/dusklight.sh
```

### 2026-06-04 Follow-Up: Draw-Skip Blanking Fix

The first draw-skip implementation was wrong for the direct EGL path: it
suppressed the internal game draw phases, but the main loop still called:

```text
aurora_begin_frame()
aurora_end_frame()
```

That let Aurora/Dawn present an empty or cleared frame, so the visible result was
blanking rather than holding the previous frame.

The implementation was changed so `fpcM_Management()` and the main loop share
one frame decision:

```text
include/f_pc/f_pc_manager.h
src/f_pc/f_pc_manager.cpp
src/m_Do/m_Do_main.cpp
```

On skipped frames:

```text
do not call aurora_begin_frame()
do not call aurora_end_frame()
still run VIWaitForRetrace(), input, fapGm_Execute(), and audio
fpcM_Management() suppresses cAPIGph_Painter(), fpcDw_Handler(), simple model draw, and depth peek
```

This should leave the previously presented EGL frame visible instead of
presenting a blank frame. It is still experimental and may break effects or game
state that expect draw callbacks every logic frame.

Deployed executable Build-ID for the non-presenting draw-skip fix:

```text
d98b5b2135113590273b207d002d40372146187b
```

### 2026-06-04 Follow-Up: Draw Skip Marked Unsafe

Testing the non-presenting draw-skip fix showed it can improve light scenes such
as the name-entry flow:

```text
PortMaster draw skip: interval=3
fps ~= 29.8-30.0
avg_cpu_frame_ms ~= 11-20
draws_last=98
uploads[v=52176 i=7848 s=0]
```

But it blanked after entering the horse name / leaving the name scene. The log
showed the transition from `fpcNm_NAME_SCENE_e` to `fpcNm_OVERLAP0_e`, then the
run ended with:

```text
[WARNING | aurora::gpu] Device lost: Device was destroyed.
```

No fatal renderer error was emitted. This means the draw-skip strategy is not
safe across scene transitions or UI-to-game handoff. Likely causes:

```text
some scene/overlap code depends on draw callbacks every logic frame
some render/surface state becomes stale when begin/end/present are skipped
the previous-frame hold approach does not satisfy Dawn/EGL lifecycle expectations during transitions
```

Action taken:

```text
/mnt/mmc/roms/ports/dusklight.sh
/roms/ports/dusklight.sh
packaging/portmaster/aarch64/dusklight.sh
```

were returned to:

```text
DUSKLIGHT_PORTMASTER_DRAW_SKIP="${DUSKLIGHT_PORTMASTER_DRAW_SKIP:-1}"
```

Conclusion: keep `DUSKLIGHT_PORTMASTER_DRAW_SKIP` as a diagnostic env var only.
It proves render/GX work is the bottleneck in simple scenes, but it should not
be shipped as the default path.

### 2026-06-04 Follow-Up: Native Vertex Expansion Regression

A fast-fail test switched the no-SSBO path from texture-backed vertex fetch to
CPU-expanded native vertex buffers:

```text
DUSKLIGHT_PORTMASTER_VERTEX_TEXTURE=0
```

This was a clear regression in the tested scene:

```text
PortMaster timing: fps=1.57 frames=8 avg_cpu_frame_ms=219.2 draws_last=8775
uploads[v=1598484 i=157914 s=403632]
gx_draws[tex=0 cpu=70080 native=120 storage=0]
gx_bytes[fifo=2758704 native=12787872]

PortMaster end_frame timing: avg_render_encode_ms=72.2 avg_submit_ms=346.3
```

Interpretation:

```text
Texture fallback uploaded compact FIFO vertices and was GPU-submit tolerable.
Native expansion inflated vertex traffic heavily and made submit block badly.
```

The top generic-expanded layouts were all `GX_TRIANGLES` (`prim=152`) with small
FIFO strides such as 8, 6, 7, and 10 bytes. Example:

```text
prim=152 fmt=0 fifoStride=8 desc=0x0000cfc0000 paths[g=...]
```

Action taken:

```text
DUSKLIGHT_PORTMASTER_VERTEX_TEXTURE=0
```

was removed again. Keep the texture-backed vertex fetch path as the default.

The same run caused `/mnt/mmc` FUSE operations to intermittently fail with:

```text
Transport endpoint is not connected
```

`/roms/ports/dusklight.sh` was successfully cleaned, but
`/mnt/mmc/roms/ports/dusklight.sh` could not be edited until the mount recovers
or the handheld is rebooted.

### 2026-06-04 Follow-Up: Logging/Stats Defaults Reduced

The launcher previously ran at the default `--log-level 0` (`LOG_DEBUG`) and
enabled:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
```

For normal testing this is too noisy and can add overhead. The package launcher
was changed to:

```text
--log-level 1
```

and `DUSKLIGHT_PORTMASTER_GX_STATS=1` was removed from the default env block.
Timing logs still appear because they are INFO-level and keyed off the
PortMaster EGL path.

### 2026-06-04 Follow-Up: 640x480 Run Was a Bypassed Launcher Path

After the 320x240 render cap was known working, one later run appeared higher
resolution. The latest log showed:

```text
Using framebuffer size 640x480 scale 1
```

This was not a renderer regression. The three PortMaster/muOS launcher locations
already had the correct env:

```text
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
--cvar game.internalResolutionScale=0
```

But the generated runtime helper still contained stale args:

```text
/mnt/mmc/ports/dusklight/runtime/run-dusklight.sh
--cvar game.internalResolutionScale=1
```

and it did not export the PortMaster render env. Running that helper directly
bypasses the wrapper, so Aurora sizes the framebuffer at the device native
`640x480`.

Action taken:

```text
packaging/portmaster/aarch64/dusklight.sh
```

now writes the render/presenter env exports into the generated
`runtime/run-dusklight.sh` before the `exec`, so direct/manual runs of the helper
use the same 320x240 path as the PortMaster launcher. The deployed stale helper
on the handheld was also patched once to:

```text
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
--cvar game.internalResolutionScale=0
```

If a future log again says `Using framebuffer size 640x480`, first check whether
the game was launched through a stale/bypassed runtime helper before chasing GPU
or swapchain sizing bugs.

2026-06-05 note: the same failure recurred on device `192.168.10.131`. The
binary was not stale:

```text
/mnt/mmc/ports/dusklight/dusklight.aarch64 Build ID d98b5b2135113590273b207d002d40372146187b
/roms/ports/dusklight/dusklight.aarch64 Build ID d98b5b2135113590273b207d002d40372146187b
```

The stale files were both generated helpers:

```text
/mnt/mmc/ports/dusklight/runtime/run-dusklight.sh
/roms/ports/dusklight/runtime/run-dusklight.sh
```

Both still had `game.internalResolutionScale=1`; the `/roms` helper also still
had `video.maxFrameRate=30`. Both were patched on-device to export the 320x240
PortMaster env, use `game.internalResolutionScale=0`, and cap
`video.maxFrameRate=5`. If this happens again, check both runtime helper
locations, not just `/mnt/mmc`.

Follow-up from the next launch: the `/mnt/mmc` helper was rewritten stale again,
and the union overlay helper was also stale:

```text
/mnt/union/ports/dusklight/runtime/run-dusklight.sh
```

The latest run still logged:

```text
Using framebuffer size 640x480 scale 1
```

The more robust workaround is now deployed on-device: keep the real ELF as:

```text
dusklight.aarch64.real
```

and replace `dusklight.aarch64` with a Bash wrapper that exports the PortMaster
EGL/render env and rewrites stale cvars before execing the real ELF:

```text
game.internalResolutionScale=1 -> game.internalResolutionScale=0
video.maxFrameRate=30 -> video.maxFrameRate=5
```

This wrapper was installed in:

```text
/mnt/mmc/ports/dusklight
/roms/ports/dusklight
/mnt/union/ports/dusklight
```

The package staging script now emits the same wrapper and stores the real binary
as `dusklight.aarch64.real`.

### 2026-06-05 Follow-Up: 5 FPS Cap Was Too Aggressive

During a live gameplay tail, the game felt smooth-ish but slow, roughly
`70% speed` by user feel. The log showed the current heavy gameplay area was
actually much lower than full speed:

```text
fps ~= 6.0-6.4
avg_cpu_frame_ms ~= 116-122
avg_submit_ms ~= 34-40
draws_last ~= 300-450
gx_draws[tex] ~= 470k-485k per timing window
```

A lighter area later jumped to:

```text
fps=12.55
avg_cpu_frame_ms=51.7
avg_submit_ms=16.6
draws_last=165
```

So performance is still strongly scene/draw-count dependent. However, the
launcher/wrapper was also forcing:

```text
--cvar video.maxFrameRate=5
```

Source inspection showed the default is `240`, so `5` is a real frame-rate cap,
not a preset. That cap can cause odd pacing and should not be the default now
that direct EGL works.

Action taken on-device and locally:

```text
--cvar video.maxFrameRate=30
```

The executable wrapper now rewrites stale `video.maxFrameRate=5` args to `30`.
This only applies on the next launch; an already-running process keeps its old
command-line args.

### 2026-06-05 Follow-Up: 256x192 Render Cap and GX Layout Stats

Current correctly-launched 320x240 runs were scene dependent:

```text
heavy-ish gameplay: fps ~= 8-11
lighter interval:    fps ~= 20
```

The command line was correct:

```text
--cvar video.maxFrameRate=30
--cvar game.internalResolutionScale=0
Using framebuffer size 320x240 scale 1
```

A new fail-fast test lowers the PortMaster render cap to:

```text
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=256
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=192
```

and enables low-volume GX layout stats:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
```

Instrumentation change:

```text
extern/aurora/lib/gx/command_processor.cpp
```

The existing layout stat collector now includes texture-vertex-fetch and
storage-vertex-fetch paths, not just CPU/native expansion attempts. It logs the
top five layouts once per existing PortMaster timing interval, sorted by
FIFO/native byte weight. Expected log shape:

```text
PortMaster GX layout stats: draws=...
  prim=... fmt=... fifoStride=... desc=... draws=... vertices=... fifoBytes=... paths[...,tex=...,stor=...]
```

Deployment:

```text
Build ID: 24ca6ba570bb323ae25b5254b18c1fccdb85bdb1
```

was installed as `dusklight.aarch64.real` in:

```text
/mnt/mmc/ports/dusklight
/roms/ports/dusklight
/mnt/union/ports/dusklight
```

The wrapper and clean launchers now force `256x192` and `GX_STATS=1`. The next
run should be checked for both:

```text
Using framebuffer size 256x192 scale 1
PortMaster GX layout stats:
```

### 2026-06-05 Follow-Up: Safe Pacing Probe

A safer alternative to the earlier unsafe draw-skip experiment was added:

```text
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS
```

Implementation:

```text
include/dusk/game_clock.h
src/dusk/game_clock.cpp
src/m_Do/m_Do_main.cpp
```

When `DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS` is greater than zero, the main loop
forces the existing frame-interpolation/catch-up shape instead of the normal
one-render-one-game-tick path:

```text
run up to N 30 Hz simulation ticks
then render one complete presentation frame
cap the outer render loop to DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS
```

Default test values:

```text
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=15
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=2
```

Observed good result:

```text
PortMaster safe pacing enabled: target_fps=15
PortMaster safe pacing max sim ticks per render=2
fps=15.00
fapGm_Execute frame advanced roughly 300 ticks per 10 seconds
```

This confirmed the mode can keep lighter scenes at stable 15 FPS while advancing
game logic near 30 Hz.

Observed failure:

```text
After selecting a save slot / entering the file-menu transition, the screen
blanked or got stuck on stale presentation.
```

The log continued to show steady rendering of the name/menu layout:

```text
fps=15.00
draws_last=98
gx_draws[tex] ~= 49k
```

Interpretation: forcing the interpolation/sim-catch-up lifecycle still is not
safe across this scene transition. It is a better-shaped version of frame skip,
but it still affects assumptions in the file/menu-to-game handoff.

Action taken:

```text
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS="${DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS:-0}"
```

was set locally and on-device in:

```text
packaging/portmaster/aarch64/dusklight.sh
scripts/portmaster/build_aarch64_bundle.sh
/roms/ports/dusklight.sh
/mnt/mmc/ports/dusklight.sh
/mnt/union/ports/dusklight.sh
/mnt/mmc/ports/dusklight/dusklight.aarch64
/roms/ports/dusklight/dusklight.aarch64
/mnt/union/ports/dusklight/dusklight.aarch64
```

Keep this mode as an opt-in diagnostic only unless a later patch makes scene
transitions explicitly safe.

### 2026-06-05 Follow-Up: Triangle No-Index Specialization Finding

The no-index `GX_TRIANGLES` specialization did not fire in the tested hot paths.
New timing instrumentation added:

```text
gx_index[noidx=... idx=... avoided=... bytes=...]
```

Observed:

```text
gx_index[noidx=0 idx=403750 avoided=0 bytes=7265244]
gx_index[noidx=0 idx=49476 avoided=0 bytes=596448]
```

The hot primitives in the logs are mostly:

```text
prim=152
prim=128
```

not the exact plain `GX_TRIANGLES` path guarded by the specialization. The
result is useful: further specialization should target the real hot primitive
types/layouts, especially the `prim=152` and `prim=128` texture-vertex-fetch
layouts, rather than only plain triangle lists.

### 2026-06-05 Follow-Up: Draw-Cull Probes Added

The next path is to find cullable visual systems before doing deeper renderer surgery. The village slowdown appears to scale with world draw workload, so reducing optional GX work may be more productive than only optimizing presentation or pacing.

Added opt-in draw-only probes:

```text
DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SKYBOX_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SIMPLE_MODEL_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW=1
```

These intentionally skip draw calls only. They do not stop actor execution, collision, weather movement, room state, audio, or resource lifetime. This makes them safer for quick A/B testing.

Suggested test order:

```text
1. GRASS_DRAW
2. SHADOW_DRAW
3. SIMPLE_MODEL_DRAW
4. WEATHER_DRAW
5. SKYBOX_DRAW
```

For each run, compare:

```text
fps
avg_cpu_frame_ms
draws_last
gx_draws[tex]
gx_bytes[fifo]
```

If one toggle materially improves village performance, keep it as a candidate default or convert it into a subtler distance/quality rule. If none move the numbers, the next renderer path should target the hot `prim=152` and `prim=128` layouts directly.

### 2026-06-05 Renderer Note: Native Vertex Buffers vs Texture Vertex Fetch

The GLES storage-buffer path failed because this Mali stack reports zero vertex-stage shader storage buffers. That prevents the general SSBO-style GX vertex fetch shader from linking.

Native vertex-attribute buffers are different. They are handled by the GPU fixed-function vertex input path, can be cached efficiently, and avoid per-vertex texture fetch plus bit unpacking in the shader. They are likely faster than the current texture-backed vertex fetch path.

The tradeoff is flexibility. GX vertex layouts are dynamic, so a generic native path requires CPU expansion into fixed attributes and can increase upload traffic. That generic path already regressed. A narrower path may still be worth it: specialize the exact hot `prim=152` / `prim=128` layouts and only use native attributes where the conversion is cheap and common.

Smart pacing remains worth keeping, but the current global safe-pacing mode is not transition-safe. Revisit it after render cost is lower, preferably with scene-transition guards or by enabling it only in known-stable gameplay states.

### 2026-06-05 Follow-Up: Guarded Smart Pacing

The smart-pacing path is now guarded instead of global. The main loop calls into `game_clock` each frame with a stable-play decision. If the guard blocks pacing, the game falls back to normal one-simulation-tick rendering for that frame and resets the pacing timer when the state changes.

Safe pacing is allowed only when:

```text
PLAY_SCENE exists
!fopOvlpM_IsPeek()
!fopOvlpM_IsDoingReq()
!dComIfGp_isEnableNextStage()
!dComIfGp_isPauseFlag()
!dScnPly_c::isPause()
!dComIfGp_event_runCheck() by default
```

Events/cutscenes are excluded at first because scene transitions and scripted state are the riskiest timing paths. This can be relaxed without rebuilding:

```text
DUSKLIGHT_PORTMASTER_SAFE_PACING_ALLOW_EVENTS=1
```

The pacing feature itself is still opt-in:

```text
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=15
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=2
```

Expected log lines when enabled:

```text
PortMaster safe pacing enabled: target_fps=15
PortMaster safe pacing guard: suppressed (<reason>)
PortMaster safe pacing guard: allowed
```

This should avoid repeating the earlier file-select blank/stale-screen failure, because the Name/Menu/transition states should now suppress pacing before `advance_main_loop()` chooses the interpolation/catch-up path.

### 2026-06-05 PortMaster Test Package

Created a local PortMaster test archive:

```text
artifacts/portmaster-test/Dusklight-aarch64-portmaster.zip
```

Contents:

```text
dusklight.sh
dusklight/dusklight.aarch64
dusklight/dusklight.aarch64.real
dusklight/lib.aarch64/libwebgpu_dawn.so
dusklight/res/*
```

The package intentionally does not include a disc image. Users must place a
supported `.iso`, `.gcm`, or `.rvz` in:

```text
dusklight/assets/
```

Test-package defaults:

```text
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=256
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=192
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=15
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=4
DUSKLIGHT_PORTMASTER_GX_STATS=0
```

Binary Build ID:

```text
4f21fbf82fc8b159a59cde06d2dc17530b2b024d
```

Dawn shared library Build ID:

```text
2e953d29450a8ff6d04ca9e6b4a843f6668adec6
```

Known caveat: this is playable enough for focused PortMaster testing on RG35XX H
/ muOS, but not release-candidate quality. Heavy village scenes still render
around 7-8 FPS and rely on guarded pacing to keep simulation closer to real time.

### 2026-06-05 Controller/Menu and Durable Build Follow-Up

The next local package iteration moves the known device defaults to 320x240 for
cleaner half-scale output on 640x480 panels. The older 256x192 test package was
faster, but UI readability suffered.

Durability changes:

```text
packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch
packaging/portmaster/cmake-aarch64.Dockerfile
```

The Dawn fbdev/EGL native-window hack is now a real patch applied during the
Docker build. The Docker image also installs `patch` and disables SDL
XScrnSaver/XTest checks, which were unnecessary cross-build dependency traps.

Control changes:

```text
src/dusk/ui/input.cpp
src/m_Do/m_Do_main.cpp
packaging/portmaster/aarch64/dusklight/dusklight.gptk
packaging/portmaster/aarch64/dusklight.sh
```

The native SDL UI path treats Back, Guide/Home/Super, Misc1, and Touchpad as the
Dusklight menu key when no explicit `OPEN_DUSKLIGHT_MENU` binding is configured.
This is important because the menu is the practical handheld quit path.

Follow-up correction: the package should not start `gptokeyb` by default. On the
muOS test device, no `gptokeyb` process was actually running, and layering
keyboard synthesis over native SDL gamepad input risks scrambled controls. The
launcher now relies on native SDL gamepad input and leaves
`DUSKLIGHT_PORTMASTER_KEYBOARD_FALLBACK=1` as an explicit future experiment only.

Native PortMaster system controls were added in `src/m_Do/m_Do_main.cpp`:

```text
Guide/Home/Super, Misc1, Touchpad -> open Dusklight menu
Start + Back/Select              -> graceful emergency quit
Start + Guide/Home/Super         -> graceful emergency quit
```

The menu path calls `dusk::ui::open_menu()`, added in `src/dusk/ui/ui.cpp`, so
quit can happen through the normal Dusklight UI when the firmware exposes a
system/menu button. The emergency chord sets `dusk::IsRunning=false` and exits
through the main-loop shutdown path.

Reference docs added:

```text
docs/portmaster-technical-notes.md
docs/portmaster-controls.md
```

Local package build succeeded after fixing the bundler path from the old
`Binaries/dusklight` layout to the current CMake install output.

Current test artifact:

```text
artifacts/portmaster-test/Dusklight-aarch64-portmaster.zip
```

Current Build IDs:

```text
dusklight.aarch64.real: 31600f0b9f13e41019ca511f96fadb8cd1cff38d
libwebgpu_dawn.so:     db950566da16af01a75e79c5448df0124af3155d
```

Deployed to the live muOS device at `192.168.10.131`:

```text
/mnt/mmc/ports/dusklight/
/mnt/mmc/ROMS/Ports/dusklight.sh
/mnt/mmc/ports/dusklight.sh
```

Remote verification confirmed executable bits, `dusklight.gptk`, bundled SDL3,
bundled nod, bundled Dawn, and matching Dusklight Build ID.

### 2026-06-05 Control Strategy Correction: SDL Mapping First

Latest hardware testing showed the previous native PortMaster control hook made
button diagnosis worse: a physical shoulder was opening the Dusklight menu,
which means SDL button identity and/or the extra translation layer were not
trustworthy enough to stack together.

Current direction is to make controls tractable by keeping one source of truth:

```text
firmware input device -> SDL controller DB -> Dusklight normal SDL/PAD mapping
```

Changes made for this baseline:

```text
res/gamecontrollerdb.txt
packaging/portmaster/aarch64/dusklight.sh
packaging/portmaster/aarch64/dusklight/dusklight.gptk
scripts/portmaster/build_aarch64_bundle.sh
src/m_Do/m_Do_main.cpp
src/dusk/ui/input.cpp
docs/portmaster-controls.md
docs/portmaster-technical-notes.md
```

The package now bundles a `muOS-Keys` SDL controller DB entry and the launcher
uses it only as a fallback when PortMaster did not provide `sdl_controllerconfig`
and `/proc/bus/input/devices` reports `muOS-Keys`. This keeps the package more
general than an RG35XXH-specific hardcode while still allowing the known muOS
input device to work when PortMaster's environment is incomplete.

`DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING` and the main-loop PortMaster menu/quit
hook were removed from the package path. The goal is to stop transforming the
same button in multiple places. If controls are still wrong, the next useful
step is an SDL button/axis identity probe on hardware and then correcting the
controller DB entry, not adding another in-game mapping table.

`gptokeyb` remains disabled by default. The packaged `dusklight.gptk` is now
inert/no-op so an accidental or future `gptokeyb` launch does not synthesize
keyboard input on top of SDL gamepad input.

The UI fallback was narrowed: SDL Back/Select is no longer treated as a generic
Dusklight menu button. Menu access should come from SDL Guide/Home-style buttons
(`Guide`, `Misc1`, or `Touchpad`) or Dusklight's existing Start + R UI chord.
This avoids stealing Back/Select from normal gameplay mapping.

Built and deployed this SDL-mapping baseline to `192.168.10.131`:

```text
dusklight.aarch64.real Build ID: bafaec8af0d5468e7ac35e9ee57ef28025e287d6
/mnt/mmc/ports/dusklight/
/mnt/mmc/ROMS/Ports/dusklight.sh
/mnt/mmc/ports/dusklight.sh
```

Remote verification confirmed executable bits, bundled `muOS-Keys`, and no
remote references to `DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING`,
`DUSKLIGHT_PORTMASTER_KEYBOARD_FALLBACK`, or `gptokeyb` in the active launcher
and wrapper files.

### 2026-06-05 Control Diagnostic Build: Correct DB, Then Probe SDL

Built and deployed a control diagnostic build to `192.168.10.131`:

```text
dusklight.aarch64.real Build ID: 998469bdbcd0733f4c3f9a6d51660356a5a93403
/mnt/mmc/ports/dusklight/
/mnt/mmc/ROMS/Ports/dusklight.sh
/mnt/mmc/ports/dusklight.sh
```

The bundled `muOS-Keys` SDL controller DB entry was tentatively corrected from
the best raw evtest capture:

```text
a:b4,b:b3,x:b5,y:b6
```

This intentionally keeps the fix at the SDL controller abstraction instead of
adding another in-game PortMaster mapping layer. If controls are still wrong,
do not re-add `gptokeyb` or `DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING`; inspect the
Dusklight log and correct `res/gamecontrollerdb.txt`.

For this build only, the launcher enables:

```text
DUSKLIGHT_PORTMASTER_INPUT_DIAG=1
```

On every SDL gamepad button press, Dusklight should log a compact line like:

```text
PortMaster input: sdl_button=A(0) port=0 pad=A(0x0100)
```

Use that to map physical button -> SDL button -> Dusklight PAD button. The
current known user-observed mismatch before this build was:

```text
physical X acted as game A
physical R2 opened the Dusklight menu
physical L2 acted like pause/options
Start and Select appeared to do nothing
Home/Guide did not open the Dusklight menu
```

Remote verification after deploy confirmed executable bits, Build ID
`998469bdbcd0733f4c3f9a6d51660356a5a93403`, bundled `muOS-Keys`, enabled input
diagnostics, and no active launcher/wrapper references to `gptokeyb`,
`DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING`, or
`DUSKLIGHT_PORTMASTER_KEYBOARD_FALLBACK`.

Follow-up launcher correction: the first diagnostic deploy still treated the
packaged `muOS-Keys` DB entry as a fallback only when PortMaster did not export
`sdl_controllerconfig`. Since muOS/PortMaster can export the older system
mapping, the launcher was corrected to prefer:

```text
$GAMEDIR/res/gamecontrollerdb.txt
```

whenever `/proc/bus/input/devices` reports `Name="muOS-Keys"`. Other devices
still use PortMaster's provided `sdl_controllerconfig` path. The corrected
launcher and `res/gamecontrollerdb.txt` were redeployed to `192.168.10.131` and
verified on-device.

### 2026-06-05 SDL Raw Probe: Final muOS-Keys Mapping

The Dusklight button config showed the previous DB line was badly scrambled:

```text
L1 -> A
X -> B
Start -> Left Shoulder
Select -> Y
Home -> Right Shoulder
L2 -> Start
R1 -> X
R2 -> Guide
Y/B/A -> undetected
```

A small SDL3 probe was added:

```text
packaging/portmaster/probes/sdl3_input_probe.c
```

It was built as an aarch64 binary against the bundled SDL3 and run on
`192.168.10.131` with:

```text
PROBE_GAMECONTROLLERDB=/mnt/mmc/ports/dusklight/res/gamecontrollerdb.txt
LD_LIBRARY_PATH=/mnt/mmc/ports/dusklight/lib.aarch64:/mnt/mmc/ports/dusklight/libs.aarch64
```

User pressed this sequence:

```text
X Y A B L1 R1 L2 R2 Start Select Home L3 R3 Vol- Vol+
```

Raw joystick results for the relevant controls:

```text
X      -> b3
Y      -> b2
A      -> b0
B      -> b1
L1     -> b4
R1     -> b5
L2     -> b10
R2     -> b11
Start  -> b7
Select -> b6
Home   -> b8
L3     -> b9
R3     -> b12
```

The corrected packaged `muOS-Keys` DB entry is now:

```text
19000000010000000100000000010000,muOS-Keys,a:b0,b:b1,x:b3,y:b2,leftshoulder:b4,rightshoulder:b5,lefttrigger:b10,righttrigger:b11,guide:b8,start:b7,back:b6,dpup:h0.1,dpleft:h0.8,dpright:h0.2,dpdown:h0.4,leftx:a0,lefty:a1,leftstick:b9,rightx:a2,righty:a3,rightstick:b12,platform:Linux,
```

This was deployed directly to:

```text
/mnt/mmc/ports/dusklight/res/gamecontrollerdb.txt
```

The runtime controller config was reset again by moving active files into:

```text
/mnt/mmc/ports/dusklight/runtime/controller-reset-20260605-132029
```

Moved file patterns:

```text
controller_ports.dat
keyboard_bindings.dat
*.controller
```

The PortMaster test zip was rebuilt after the DB correction so the local
artifact matches the deployed resource.

### 2026-06-05 Local PortMaster Test Release Package

A local PortMaster test release archive was built:

```text
artifacts/portmaster-release/Dusklight-aarch64-portmaster.zip
```

Verified package properties:

```text
entries: 48
launcher present: yes
dusklight.aarch64 ELF present: yes
libwebgpu_dawn.so present: yes
ROM/disc images included: no
Weston runtime metadata: no
```

Binary Build ID:

```text
998469bdbcd0733f4c3f9a6d51660356a5a93403
```

Release cleanup done for this package:

```text
DUSKLIGHT_PORTMASTER_INPUT_DIAG defaults to 0
port.json no longer declares weston_pkg_0.2.squashfs
res/gamecontrollerdb.txt includes the probed muOS-Keys mapping
the .real wrapper was removed; dusklight.aarch64 is now the actual ELF binary
```

Build and fork instructions were consolidated in:

```text
docs/portmaster-release.md
```

### 2026-06-06 Follow-Up: GX Batching / Hot Primitive Instrumentation

The hot primitive IDs were verified against Aurora's GX enum:

```text
prim=128 -> GX_QUADS
prim=152 -> GX_TRIANGLESTRIP
prim=144 -> GX_TRIANGLES
```

This means the earlier note calling `prim=152` triangles was wrong. The current
hot paths are quads and triangle strips.

Changes added in `extern/aurora`:

```text
lib/gx/command_processor.cpp
  - layout stats now print primitive names, e.g. 128(GX_QUADS)
  - adjacent draw merge logic now reports why merges fail
  - GX_QUADS and GX_TRIANGLESTRIP primitive index patterns are cached by
    primitive + vertex count

lib/gfx/common.cpp
  - PortMaster timing line now includes:
    gx_index[..., cache_hit=..., cache_miss=...]
    gx_merge[try=..., ok=..., dirty=..., none=..., line=..., inst=..., tex=..., idx=...]

lib/gx/gx.hpp
  - PortmasterTimingStats has the new cache and merge counters
```

Intent:

```text
1. Stop confusing numeric primitive IDs in logs.
2. Measure whether batching is blocked mostly by state changes or by index mode.
3. Reduce repeated CPU index-pattern construction for the observed hot primitive
   types without changing draw semantics.
```

Important caution:

```text
Do not jump straight to native TriangleStrip topology yet.
```

Using WebGPU `TriangleStrip` directly for `GX_TRIANGLESTRIP` may reduce index
buffer generation and upload, but it can also reduce the existing adjacent-draw
merge opportunities because the current merger turns everything into triangle
list indices. Decide only after a village run with the new `gx_merge[...]`
counters.

Next test run:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
```

Compare:

```text
fps
draws_last
gx_index[cache_hit/cache_miss/bytes]
gx_merge[try/ok/dirty/idx]
top layout stats with primitive names
```

### 2026-06-06 Follow-Up: Safe Pacing Freeze After Transitions

Observed issue:

```text
When leaving a transition with safe pacing enabled, the game can appear to
freeze completely.
```

Likely cause found in `src/dusk/game_clock.cpp`:

```text
advance_main_loop() treated frame gaps over 250ms as abnormal and returned
sim_ticks_to_run=0.
```

That is reasonable for ordinary desktop frame interpolation because it prevents
a huge catch-up burst. It is unsafe for this PortMaster path because heavy
renders on the handheld can exceed 250ms repeatedly. If every frame is
"abnormal", safe pacing keeps presenting/interpolating without ever advancing
the simulation, which looks like a hard freeze after scene/exit transitions.

Patch applied:

```text
If DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS is enabled and an abnormal gap occurs,
reset the snapshot clock but still run one simulation tick.
```

This preserves the anti-catch-up behavior while guaranteeing forward progress
on very slow frames.
