# Dusklight PortMaster Graphics Compatibility Handoff

## Goal

Make the Dusklight PortMaster build more generalizable across PortMaster devices by moving away from the current hard-coded fbdev/Dawn sentinel path.

The immediate target is not a full direct GLES backend rewrite. The target is a medium-scope compatibility improvement:

```text
SDL-owned window/surface
  -> Dawn OpenGLES backend
  -> device EGL/GLES stack
```

The current working RG35XX H / muOS path must remain available as a fallback, but it should no longer be the only/default path.

## Current Problem

The current PortMaster launcher forces:

```sh
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
SDL_VIDEODRIVER=offscreen
```

This makes the build depend on a custom Dawn fbdev/native-window path. It works on the known RG35XX H / muOS firmware, but fails on some other PortMaster devices/firmwares, for example R36S-class systems, with logs like:

```text
Creating WebGPU instance
Attempting to initialize OpenGLES
Warning: Couldn't create the default EGL display.
Adapter request failed: No supported adapters
Failed to create adapter
Surface "Surface" is not configured.
Fatal: Error creating window
```

Interpretation: this is likely an EGL/display/surface-discovery failure, not a GX batching or GPU-performance issue.

## Existing Renderer Path

Current path:

```text
Dusklight
  -> Aurora GX
  -> WebGPU API
  -> Dawn OpenGLES backend
  -> EGL / Mali / display surface
```

The current package uses a custom Dawn patch that treats an Xlib surface descriptor with null display as a PortMaster fbdev sentinel, then passes the stored native window handle to `eglCreateWindowSurface`.

This is too firmware-specific.

## Important Constraint

Do not start by replacing Dawn.

A direct GLES backend would be cleaner long-term, but it is much larger because Aurora GX currently uses WebGPU/Dawn types throughout renderer setup, pipelines, bind groups, buffers, render passes, shader generation, and presentation.

This handoff is for the medium path:

```text
Keep Dawn.
Fix surface/window/backend selection.
Make the PortMaster launcher select a graphics mode.
Add diagnostics.
Keep fbdev sentinel as fallback only.
```

## Main Hypothesis

SDL should own the window where possible.

Preferred default:

```text
SDL video backend creates a real fullscreen window
Aurora gets native window info from SDL
Dawn creates a compatible WebGPU surface from that native handle
Dawn OpenGLES backend renders/presents through that surface
```

This is more portable than:

```text
SDL offscreen
fake Xlib descriptor
custom fbdev native-window handoff
Dawn EGL patch
```

But SDL surface is not automatically universal. It depends on which SDL video driver is active and whether Aurora/Dawn can map that SDL window to a valid WebGPU surface descriptor.

Currently, Aurora’s Linux Dawn surface helper appears to handle only:

```text
SDL video driver == wayland
SDL video driver == x11
```

If SDL is using `kmsdrm`, `offscreen`, fbdev, or another backend, current Aurora likely cannot create a Dawn surface descriptor without extra work.

## Desired Graphics Modes

Add explicit graphics modes. Suggested env var:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=auto
```

Supported values:

```text
auto
dawn-sdl
dawn-wayland
dawn-x11
dawn-kmsdrm
dawn-fbdev-sentinel
diag
```

Mode meanings:

### auto

Try modes in order and log each failure clearly.

Suggested order:

```text
1. dawn-sdl default
2. dawn-kmsdrm if /dev/dri/card0 exists
3. dawn-wayland if WAYLAND_DISPLAY exists
4. dawn-x11 if DISPLAY exists
5. dawn-fbdev-sentinel if /dev/fb0 exists
6. fail with diagnostic summary
```

Do not silently fall through without logging.

### dawn-sdl

Do not force `SDL_VIDEODRIVER`.

Unset the old fbdev sentinel flags:

```sh
unset DUSKLIGHT_PORTMASTER_X11_DAWN
unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
```

Let SDL choose its default video backend.

Expected behavior:

```text
SDL creates fullscreen OpenGL-capable window.
Aurora creates Dawn surface from SDL native window properties.
Dawn requests OpenGLES adapter with compatibleSurface.
```

### dawn-wayland

Force:

```sh
export SDL_VIDEODRIVER=wayland
unset DUSKLIGHT_PORTMASTER_X11_DAWN
unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
```

Only try when Wayland environment is present.

### dawn-x11

Force:

```sh
export SDL_VIDEODRIVER=x11
unset DUSKLIGHT_PORTMASTER_X11_DAWN
unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
```

Only try when X11 is present.

### dawn-kmsdrm

Force:

```sh
export SDL_VIDEODRIVER=kmsdrm
unset DUSKLIGHT_PORTMASTER_X11_DAWN
unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
```

This is the important new experiment for many PortMaster devices.

Potential blocker: Aurora’s current Dawn surface descriptor helper may not support SDL `kmsdrm` windows. If SDL can create the window but Aurora reports “Failed to create surface descriptor for current window,” inspect SDL window properties for KMSDRM/DRM/GBM handles. If no WebGPU/Dawn surface descriptor exists for this path, this mode cannot work without a Dawn/Aurora surface extension.

### dawn-fbdev-sentinel

This is the current known-good fallback for RG35XX H / muOS.

Force:

```sh
export SDL_VIDEODRIVER=offscreen
export DUSKLIGHT_PORTMASTER_X11_DAWN=1
export DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
```

Do not delete this path yet. It is the only confirmed working path on the known test device.

But it should be treated as:

```text
last-resort fallback
not default
not general PortMaster solution
```

### diag

Do not run the game. Only collect platform diagnostics.

## Launcher Changes

Current launcher hard-codes the fbdev sentinel path. Change it to mode selection.

Pseudo-structure:

```sh
MODE="${DUSKLIGHT_PM_GRAPHICS_MODE:-auto}"

log_platform_info() {
  echo "--- Dusklight PortMaster platform diagnostic ---"
  echo "CFW_NAME=$CFW_NAME"
  echo "DEVICE_ARCH=$DEVICE_ARCH"
  echo "directory=$directory"
  echo "controlfolder=$controlfolder"
  echo "uname=$(uname -a)"
  cat /etc/os-release 2>/dev/null || true

  echo "--- devices ---"
  ls -l /dev/fb* /dev/dri/* /dev/mali* 2>/dev/null || true

  echo "--- display env ---"
  echo "DISPLAY=$DISPLAY"
  echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
  echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"

  echo "--- graphics libs ---"
  find /usr /lib /opt -name 'libEGL*' -o -name 'libGLES*' -o -name 'libgbm*' -o -name 'libdrm*' 2>/dev/null | head -100

  echo "--- binary deps ---"
  ldd "$BIN" 2>&1 | grep -Ei 'not found|egl|gles|gbm|drm|sdl|wayland|x11' || true
}

apply_graphics_mode() {
  case "$1" in
    dawn-sdl)
      unset SDL_VIDEODRIVER
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      ;;

    dawn-wayland)
      export SDL_VIDEODRIVER=wayland
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      ;;

    dawn-x11)
      export SDL_VIDEODRIVER=x11
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      ;;

    dawn-kmsdrm)
      export SDL_VIDEODRIVER=kmsdrm
      unset DUSKLIGHT_PORTMASTER_X11_DAWN
      unset DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE
      ;;

    dawn-fbdev-sentinel)
      export SDL_VIDEODRIVER=offscreen
      export DUSKLIGHT_PORTMASTER_X11_DAWN=1
      export DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
      ;;

    *)
      echo "Unknown graphics mode: $1"
      return 1
      ;;
  esac
}
```

For first implementation, do not attempt complicated auto-relaunch yet. A simpler version is acceptable:

```text
1. default to dawn-sdl
2. allow user override with DUSKLIGHT_PM_GRAPHICS_MODE
3. keep dawn-fbdev-sentinel as override/fallback
4. log enough to diagnose failure
```

Auto retry can come later.

## Required Aurora Logging

Add logs in Aurora before and after surface creation:

```text
SDL current video driver
SDL window flags
SDL native window properties available
selected Dawn surface descriptor type
whether descriptor creation returned null
Dawn backend requested
Dawn adapter request result
Dawn adapter info if successful
```

In `SetupWindowAndGetSurfaceDescriptor(SDL_Window*)`, log:

```text
driver = SDL_GetCurrentVideoDriver()
wayland display ptr
wayland surface ptr
x11 display ptr
x11 window id
unsupported driver name
```

If driver is unsupported, error should say:

```text
Unsupported SDL video driver for Dawn surface: <driver>
```

not just:

```text
Failed to create surface descriptor for current window
```

## Required Diagnostic Probe

Add a tiny standalone `egl_probe.aarch64` or equivalent.

It should not depend on game data.

Minimum checks:

```text
dlopen libEGL.so
dlopen libGLESv2.so
eglGetDisplay(EGL_DEFAULT_DISPLAY)
eglInitialize(default display)
eglQueryString(EGL_VENDOR)
eglQueryString(EGL_VERSION)
eglQueryString(EGL_EXTENSIONS)
presence of /dev/fb0
presence of /dev/dri/card0
presence of libgbm
presence of libdrm
current SDL video driver after SDL_Init(SDL_INIT_VIDEO)
available SDL video drivers if possible
```

Output plain text, not JSON.

Example output:

```text
EGL_LOADER=ok
GLES_LOADER=ok
EGL_DEFAULT_DISPLAY=fail
HAS_FBDEV=yes
HAS_DRI=yes
HAS_GBM=yes
SDL_CURRENT_DRIVER=kmsdrm
RECOMMENDED_MODE=dawn-kmsdrm
```

If possible, also test whether SDL can create a small hidden OpenGL window under:

```text
default
kmsdrm
wayland
x11
offscreen
```

But keep the first version simple.

## Acceptance Criteria

### Minimum success

Package still boots on known RG35XX H / muOS using:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-fbdev-sentinel
```

No regression from current known-good path.

### Better success

Package boots on a second firmware/device using:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl
```

or:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-kmsdrm
```

without using the fbdev sentinel.

### Diagnostic success

On failing devices, `log.txt` clearly identifies one of:

```text
SDL could not create video window
Aurora could not create Dawn surface descriptor for SDL driver
Dawn could not create compatible OpenGLES adapter
EGL default display failed
missing libEGL/libGLES/libgbm/libdrm
permission/device-node issue
```

The next failure should be specific. Avoid “No supported adapters” without surrounding context.

## Important Non-Goals

Do not rewrite Aurora GX to direct GLES in this task.

Do not remove the current fbdev sentinel path.

Do not optimize GX batching, TEV, XF dirty tracking, draw submission, or vertex fetch in this task.

Do not spend time on WestonPack unless it is required to test Wayland/X11. Previous WestonPack path was problematic and should not be the primary compatibility strategy.

Do not assume Vulkan.

Do not assume X11.

Do not assume Wayland.

Do not assume `/dev/fb0` native-window EGL works outside the known firmware.

## Main Files to Inspect

Dusklight launcher:

```text
packaging/portmaster/aarch64/dusklight.sh
```

Current Dawn fbdev patch:

```text
packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch
```

Aurora window creation:

```text
extern/aurora/lib/window.cpp
```

Aurora Dawn surface descriptor helper:

```text
extern/aurora/lib/dawn/BackendBinding.cpp
```

Aurora WebGPU init / surface / adapter:

```text
extern/aurora/lib/webgpu/gpu.cpp
```

Aurora GX CMake:

```text
extern/aurora/cmake/aurora_gx.cmake
```

## Specific Implementation Plan

### Step 1: Add logging only

Before changing behavior, add logs:

```text
current SDL video driver
window creation flags
surface descriptor type
backend requested
compatibleSurface used yes/no
adapter success/failure message
```

Build and confirm no behavior change on current working device.

### Step 2: Add launcher mode selection

Implement:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE
```

Modes:

```text
dawn-sdl
dawn-wayland
dawn-x11
dawn-kmsdrm
dawn-fbdev-sentinel
diag
```

Default initially can remain `dawn-fbdev-sentinel` to avoid breaking known-good behavior.

After testing, default can move to `auto` or `dawn-sdl`.

### Step 3: Test `dawn-sdl`

Use:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl
```

Expected outcomes:

```text
If SDL chooses x11/wayland and Aurora maps it, Dawn should proceed to adapter request.
If SDL chooses kmsdrm and Aurora does not map it, expect descriptor failure.
If SDL cannot create window, log SDL error.
```

### Step 4: Test `dawn-kmsdrm`

Use:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-kmsdrm
```

Expected possible outcomes:

```text
SDL window creation succeeds, but Aurora surface descriptor returns unsupported driver.
SDL window creation fails due to missing KMSDRM support.
Dawn fails adapter creation due to incompatible surface/display.
```

If `kmsdrm` reaches unsupported descriptor, inspect SDL3 window properties for KMSDRM/DRM/GBM native handles. If none are exposed in a Dawn-compatible way, document that Dawn cannot use SDL KMSDRM directly without lower-level GBM/Dawn support.

### Step 5: Keep current path as fallback

Verify:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-fbdev-sentinel
```

still exports:

```sh
SDL_VIDEODRIVER=offscreen
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
```

and still boots on known-good muOS/RG35XX H.

### Step 6: Add `diag` mode

Use:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=diag
```

It should produce `log.txt` without requiring game data.

Remote testers should only need to run the port once and send `log.txt`.

## Expected Findings

Most likely cases:

### Case A: SDL default picks Wayland/X11

This is the easiest path. Current Aurora helper already supports Wayland/X11 surface descriptors. If Dawn’s OpenGLES backend works with that surface, this mode may fix some devices.

### Case B: SDL default picks KMSDRM

Current Aurora helper likely does not support it. This is still useful because the failure will become explicit:

```text
Unsupported SDL video driver for Dawn surface: kmsdrm
```

## 2026-06-12 Implementation Update

Implemented first compatibility slice:

- `packaging/portmaster/aarch64/dusklight.sh` now supports `DUSKLIGHT_PM_GRAPHICS_MODE`.
- Supported launcher modes:
  - `auto`: resolves to `dawn-sdl` for now, letting SDL choose its default video backend.
  - `dawn-sdl`: unsets forced SDL/fbdev sentinel variables.
  - `dawn-wayland`: forces `SDL_VIDEODRIVER=wayland`.
  - `dawn-x11`: forces `SDL_VIDEODRIVER=x11`.
  - `dawn-kmsdrm`: forces `SDL_VIDEODRIVER=kmsdrm`; diagnostic only unless Aurora/Dawn gains a KMSDRM/GBM surface path.
  - `dawn-fbdev-sentinel`: preserves the older known-good PortMaster fbdev sentinel path.
  - `diag`: writes platform diagnostics and exits before launching the game.
- Launcher diagnostics now log display env, device nodes, graphics libraries, relevant binary deps, and input device names/handlers.
- Aurora now logs:
  - selected SDL video driver when creating the Dawn surface descriptor,
  - Wayland/X11 native handles when present,
  - explicit unsupported SDL driver errors, e.g. `Unsupported SDL video driver for Dawn surface: kmsdrm`,
  - adapter request backend, feature level, and compatible-surface usage.

Current conclusion:

- This can be tested with the same binary. The launcher can choose different environment/surface modes without rebuilding.
- It is not yet a guaranteed same-binary fix for R36S-class devices. If those devices have no working Wayland/X11 path and SDL chooses KMSDRM, current Aurora likely cannot create a Dawn surface descriptor.
- The useful next user-facing step is to ship/test this launcher and ask failing users to run:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=diag
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-wayland
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-x11
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-kmsdrm
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-fbdev-sentinel
```

and send `log.txt` for each mode that fails.

Build note:

- `bash -n packaging/portmaster/aarch64/dusklight.sh` passes.
- Existing local focal Ninja build tree could not be reused from this sandbox because it references Docker `/work` paths. A full Docker rebuild is still needed before release/deployment.

Then decide whether to add a real KMSDRM/GBM Dawn path.

### Case C: SDL cannot create OpenGL window

This is a firmware/runtime issue. Log it and fall back to fbdev sentinel only where supported.

### Case D: SDL creates window but Dawn cannot get EGL display

Then the problem remains inside Dawn OpenGLES EGL display selection. Next task would be Dawn EGL platform support, not Aurora GX.

## Suggested First Commit

Commit title:

```text
portmaster: add graphics mode selection and surface diagnostics
```

Scope:

```text
launcher mode selection
diagnostic logging
Aurora surface descriptor logging
no renderer rewrite
no GX changes
```

## Suggested Second Commit

Commit title:

```text
aurora: make Dawn SDL surface failures explicit on Linux
```

Scope:

```text
log SDL video driver
log unsupported Linux SDL drivers
distinguish descriptor failure from adapter failure
```

## Suggested Third Commit

Commit title:

```text
portmaster: add diagnostic-only launch mode
```

Scope:

```text
DUSKLIGHT_PM_GRAPHICS_MODE=diag
platform/device/lib logging
optional egl_probe binary
no game asset required
```

## Core Question To Answer

Can PortMaster devices run Dusklight through:

```text
SDL-created fullscreen window
Dawn OpenGLES compatible surface
```

without the fbdev sentinel?

The answer may be:

```text
yes for X11/Wayland devices
maybe/no for KMSDRM unless Dawn/Aurora gets GBM support
no for pure fbdev without the existing sentinel
```

The handoff should produce logs good enough to classify devices into those buckets.

## 2026-06-16 Revisit: bmdhacks SDL2 Backend vs Borrowed Dawn Surface

The original "SDL-owned window/surface -> Dawn OpenGLES" hypothesis needs to be
split into two separate claims:

```text
Claim A: bmdhacks SDL2 backend is the right way to reach native handheld video.
Claim B: Dawn can safely borrow SDL2's raw EGLSurface as a WebGPU Surface.
```

Current evidence supports Claim A but does **not** fully support Claim B.

The bmdhacks SDL2 backend is valuable because it delegates SDL3 window/context
creation to the firmware's patched SDL2 stack. On RG35XX H / muOS this exposes a
working Mali EGL/GLES path without Weston/X11/Wayland and avoids the old CPU
fbdev presenter. It is probably the best available PortMaster video entry point.

However, our current integration still adds a brittle Dawn bridge:

```text
SDL3 bmdhacks sdl2 backend creates SDL2 window/context/EGLSurface
SDL shim publishes raw EGLDisplay/EGLSurface/getProc/window/swap pointers
Aurora passes the raw EGLDisplay/getProc to Dawn adapter creation
Aurora encodes the raw EGLSurface as a fake Xlib surface descriptor
Dawn patch treats fake Xlib/null-display as a borrowed EGLSurface
Dawn OpenGL swapchain still queries/configures/blits around that surface
alpha6 additionally calls SDL2_GL_SwapWindow instead of eglSwapBuffers
```

The brittle assumptions are:

- A raw `EGLSurface` can be smuggled through `SurfaceSourceXlibWindow` without
  upsetting Dawn's surface/swapchain model.
- The SDL2-created EGLSurface remains valid while Dawn uses it from Dawn's own
  OpenGL/EGL context path.
- Dawn can `eglQuerySurface`, make the surface current, blit to the default
  framebuffer, and then let SDL2 present it.
- Calling SDL2's `SDL_GL_SwapWindow` after Dawn rendering is equivalent to SDL2
  owning the full present path.
- KMSDRM/fbdev-style firmware stacks tolerate this mixed ownership model.

The latest KNULLI / TrimUI tester result argues against those assumptions:

```text
alpha4: reached game/audio but no visible video; quit combo broken
alpha6: no video/no audio/no game input; hotkey quit works
log: Device lost: getting surface width failed with EGL_BAD_SURFACE
log: ERROR: Could not restore CRTC
```

This narrows the regression boundary:

```text
alpha4 -> alpha5: launcher/gptokeyb only
alpha5 -> alpha6: SDL2 swap hook and extra SDL2 window/swap pointer publishing
```

So alpha6's idea is correct for muOS, but not proven as a universal PortMaster
path. It removes only the final `eglSwapBuffers` bypass. It does not remove the
larger borrowed-surface/lifecycle mismatch between SDL2 and Dawn.

### What This Means

Do not describe the current `dawn-sdl2shim` mode as "pure SDL2 owns video." It is
still a hybrid:

```text
SDL2 owns native window/surface lifetime
Dawn owns WebGPU swapchain, render pass, and GL blit logic
```

That hybrid works on RG35XX H / muOS. It may fail on KNULLI/TrimUI/R36S-class
firmware even when the underlying bmdhacks SDL2 backend is healthy.

### Better Direction If We Continue This Path

The next robust design should keep bmdhacks SDL2 as the native video owner, but
avoid making Dawn believe a borrowed `EGLSurface` is a normal Dawn surface.

Preferred technical directions:

```text
Option 1: SDL2-owned external-present mode
  Dawn renders internally/offscreen.
  A PortMaster presenter owned by SDL2 presents the final frame.
  First proof may use slow readback/copy; optimize only after it works.

Option 2: First-class Dawn SDL2-shim surface type
  Add an explicit PortMaster/SDL2-shim surface path instead of fake Xlib.
  Store SDL2 window/context/swap callbacks in that surface.
  Ensure Dawn's EGL context/current-surface lifecycle is compatible with SDL2.
```

Avoid blind Dawn KMSDRM/GBM work for now. We do not have local hardware coverage
for that path, and it would be a driver/platform project with slow tester
iteration.

### Immediate Test Strategy

For compatibility testers, isolate the regression instead of mixing launcher and
renderer changes:

```text
1. alpha4 clean install:
   Expected baseline: game/audio may run, video may be absent, quit combo broken.

2. alpha4-style renderer with fixed simple `$GPTOKEYB` launcher:
   Tests whether quit can be fixed without alpha6's swap hook.

3. alpha6 clean install:
   Tests whether SDL2_GL_SwapWindow path causes EGL_BAD_SURFACE/device loss.
```

If alpha4-style plus fixed gptokey returns to "audio but no video", then alpha6
is a KNULLI/TrimUI regression and should stay muOS-only until the borrowed
surface model is replaced.

## 2026-06-16 Implementation: SDL2-Owned External Present

The `dawn-sdl2shim` mode has been redesigned to stop borrowing SDL2's
`EGLSurface` as a Dawn swapchain surface.

New default flow:

```text
bmdhacks SDL3 shim selects its `sdl2` video backend
SDL2 owns native window/context/swap
Dawn creates an OpenGLES device without a compatible surface
Dawn renders Dusklight into Aurora's internal/offscreen present texture
Aurora copies that final texture to a CPU readback buffer
Aurora makes the SDL2 GL context current
Aurora uploads the pixels to a small GLES texture
Aurora draws a fullscreen quad and calls SDL_GL_SwapWindow
```

The goal is compatibility first, not performance. This path should avoid the
`EGL_BAD_SURFACE` class of failures caused by the alpha6 borrowed-surface bridge,
because Dawn no longer queries or presents the SDL2-owned `EGLSurface`.

Implemented pieces:

- `packaging/portmaster/aarch64/dusklight.sh`
  - Default `DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim`.
  - `dawn-sdl2shim` now sets:
    - `SDL_VIDEODRIVER=sdl2`
    - `DUSKLIGHT_PORTMASTER_NO_SURFACE=1`
    - `DUSKLIGHT_PORTMASTER_SDL2SHIM_EXTERNAL_PRESENT=1`
    - `DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1`
  - The default mode explicitly unsets borrowed-surface flags:
    - `DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE`
    - `DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT`
  - Old modes remain available for A/B tests:
    - `dawn-sdl2shim-borrow`
    - `dawn-sdl2shim-borrow-sdlswap`
- `extern/aurora/lib/aurora.cpp`
  - Adds `portmaster_sdl_present`.
  - Allocates a WebGPU readback buffer for the final present texture.
  - Uses SDL window properties to reuse the SDL2-shim GL context when exposed,
    otherwise creates an SDL GL context.
  - Uploads readback pixels to a GLES texture and presents through
    `SDL_GL_SwapWindow`.
  - Routes `begin_frame` / `end_frame` through the external-present path when
    `DUSKLIGHT_PORTMASTER_SDL2SHIM_EXTERNAL_PRESENT=1`.
- `packaging/portmaster/patches/dawn-portmaster-sdl2shim-borrow-egl-surface.patch`
  - Fixed patch formatting.
  - Borrowed modes can now disable the old SDL swap hook with
    `DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT=0`.

Build status:

```text
scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh \
  --out-dir artifacts/portmaster-sdl2-external-present

Built: artifacts/portmaster-sdl2-external-present/Dusklight-aarch64-portmaster.zip
```

Device deployment status:

```text
Deployed to RG35XX H / muOS test hardware
Launcher: /mnt/mmc/ROMS/Ports/dusklight.sh
Binary:   /mnt/mmc/ports/dusklight/dusklight.aarch64
```

Runtime status:

```text
Pending user launch from muOS.
Expected fresh log markers:
  DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim
  DUSKLIGHT_PORTMASTER_NO_SURFACE=1
  DUSKLIGHT_PORTMASTER_SDL2SHIM_EXTERNAL_PRESENT=1
  DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=
  DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT=
  PortMaster SDL2-shim external presenter active
```

Risks to check on first launch:

- This path uses CPU readback, so it may be slower than alpha6. That is
  acceptable for the first compatibility proof.
- If it opens but shows black, inspect whether the log reaches
  `PortMaster SDL2-shim external presenter active`. If it does, debug the GLES
  upload/fullscreen-quad path. If it does not, debug SDL GL context creation.
- If adapter creation fails before gameplay, Dawn no-surface OpenGLES adapter
  creation still depends on the device's EGL stack and may need a separate
  display-selection patch.

## 2026-06-21 Implementation: SDL2-Shim Owned EGL Metadata

Problem being targeted:

```text
alpha10-style borrowed SDL2 EGL surface works on RG35XX H / muOS, but Knulli,
ArkOS, and TrimUI testers still report EGL_BAD_SURFACE, eglSwapBuffers failed,
or blank video/audio-only behavior.
```

The hypothesis is that bmdhacks' SDL2 backend is still the right abstraction,
but our Dawn bridge was too implicit. It borrowed SDL2's EGL surface and swap
function while passing only partial EGL metadata into Dawn. Some firmware stacks
tolerate that; others appear to need the SDL-owned EGL display/surface/context
and drawable size to be treated as one explicit surface record.

New mode:

```text
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim-owned
```

Launcher behavior:

```text
SDL_VIDEODRIVER=sdl2
SDL3SHIM_SDL2_LIB=libSDL2-2.0.so.0
DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_OWNED_EGL=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT=1
DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1
```

Implemented pieces:

- `packaging/portmaster/patches/sdl2shim-publish-egl-properties.patch`
  - Extends the bmdhacks SDL3 shim over SDL2 backend to publish:
    - SDL2 window/context
    - EGL display
    - EGL surface
    - EGL context
    - drawable width/height
    - GL getProc, makeCurrent, swapWindow, and EGL makeCurrent callbacks
- `extern/aurora/lib/dawn/BackendBinding.cpp`
  - Extends the PortMaster SDL2-shim Dawn table to version 2.
  - Passes EGL context and drawable dimensions into Dawn.
  - Marks the table with `OwnedEgl` and `SdlSwap` flags for diagnostics and
    future behavior toggles.
- `packaging/portmaster/patches/dawn-portmaster-sdl2shim-borrow-egl-surface.patch`
  - Accepts table versions 1 and 2.
  - Uses version 2 drawable dimensions instead of querying brittle EGL surface
    size on PortMaster SDL2-shim paths.
  - Makes SDL/EGL make-current failures explicit instead of ignored.
  - Uses the published EGL context when available.
- `packaging/portmaster/aarch64/dusklight.sh`
  - Defaults to `dawn-sdl2shim-owned`.
  - Keeps `dawn-sdl2shim-native`, `dawn-sdl2shim`, and old borrowed modes as
    explicit A/B fallback choices.

Build status:

```text
scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh \
  --out-dir artifacts/portmaster-test

Built:
  artifacts/portmaster-test/dusklight.zip
```

Deployment status:

```text
Deployed to RG35XX H / muOS test hardware
Launcher: /mnt/mmc/ROMS/Ports/dusklight.sh
Binary:   /mnt/mmc/ports/dusklight/dusklight.aarch64
```

Expected fresh log markers:

```text
Dusklight PortMaster graphics mode: requested=dawn-sdl2shim-owned resolved=dawn-sdl2shim-owned SDL_VIDEODRIVER=sdl2
DUSKLIGHT_PORTMASTER_SDL2SHIM_OWNED_EGL=1
SDL2-shim Dawn surface properties: display=... surface=... eglContext=... drawable=... flags=0x3
```

If this fails:

- If RG35XX H / muOS regresses to blank video, first try switching only the
  launcher mode back to `dawn-sdl2shim-native`.
- If Knulli/ArkOS still fail with `EGL_BAD_SURFACE` or `eglSwapBuffers failed`,
  the likely remaining issue is deeper EGL surface ownership or firmware DRM
  behavior, not missing SDL2-shim metadata.
- Do not return to the CPU-readback `dawn-sdl2shim` path as a performance fix;
  it exists only as a slow compatibility proof.
