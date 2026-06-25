# Dusklight PortMaster Technical Notes

This document records the local PortMaster test-port changes and why they exist.
It is not an upstream design document and should be treated as a fail-fast
engineering log for low-power Linux handhelds.

For the current experiment ledger, including failed graphics modes and device
outcomes, see `docs/portmaster-experiment-stocktake.md`.

## Target

The current package targets PortMaster-style aarch64 Linux handhelds, with the
RG35XX H / muOS / Mali-G31 stack as the first real test device.

The package assumes:

- a user-provided disc image in `dusklight/assets/`
- bundled SDL3 and Dawn/WebGPU libraries
- OpenGL ES through the device Mali/EGL stack
- launch from the real PortMaster storage path, for example
  `/mnt/mmc/ROMS/Ports/dusklight.sh`

Do not install or test from `/roms` on this muOS setup. That path has been stale
or wrong during testing.

## Current Status Snapshot

As of the current PortMaster test build, Dusklight launches, loads user-provided
game data, enters gameplay, and is playable enough for route testing on the
RG35XX H. It is still not smooth. Heavy village scenes generally sit around
low-double-digit FPS, while lighter or indoor scenes can be much better.

The largest practical playability improvement so far came from GX primitive
work, especially triangle-strip batching. Earlier builds were spending enormous
time issuing many tiny GX draws. Batching compatible triangle-strip runs,
keeping strip topology where possible, and reusing generated/indexed primitive
data reduced draw submission pressure enough to move the port from technical
demo territory into "rough but testable gameplay" territory.

Do not lose this context: triangle-strip batching was the major win. Later TEV
register dirty deferral was technically correct and reduced one class of merge
block, but it did not produce the same visible jump in playability.

## Renderer Path

Dusklight is not a simple SDL2/GLES renderer. The path is:

```text
Dusklight -> Aurora GX -> WebGPU API -> Dawn OpenGLES backend -> Mali EGL/GLES
```

The original Weston/fbdev experiments proved useful for presentation, but the
CPU fbdev copy path was too slow. At 640x480 it spent roughly 380 ms per present;
at lower resolutions it was still a major frame-time cost.

The current compatibility-test package defaults to the bmdhacks SDL2 backend:

```text
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim-owned
SDL_VIDEODRIVER=sdl2
DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_OWNED_EGL=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT=1
DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1
```

In this mode the bmdhacks SDL3 shim delegates video to the firmware SDL2
backend, exposes SDL2-owned EGL metadata to Aurora/Dawn, and presents through
the SDL swap hook. This is the current fastest muOS path and the current best
compatibility-test path.

The older fbdev sentinel, no-surface/readback, and borrowed-surface modes remain
available as fallback/A-B paths. They are useful for regression isolation, but
they should not be described as the preferred compatibility direction.

## GLES Vertex Limitation

The RG35XX H Mali GLES stack reports:

```text
max vertex shader storage blocks = 0
```

That made Dawn fail shader linking whenever Aurora generated GX shaders using
vertex-stage storage buffers. The error looked like:

```text
The number of vertex shader storage blocks (1) is greater than the maximum number allowed (0)
```

The workable fallback is texture-backed vertex fetch. Vertex data is uploaded to
a texture-like buffer and unpacked in the shader instead of being fetched from a
storage buffer. This is slower than a real vertex-buffer path, but it runs on the
Mali GLES driver.

Native vertex-buffer experiments are still valuable, but the broad generic path
created graphical corruption and performance regressions. A future attempt
should specialize only the hottest known GX layouts rather than converting every
layout generically.

## Current Performance Defaults

The PortMaster launcher currently uses conservative low-end settings:

```text
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim-owned
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_OWNED_EGL=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT=1
DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1
DUSKLIGHT_PORTMASTER_NOINDEX_TRIANGLES=1
DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY=1
DUSKLIGHT_PORTMASTER_BATCH_STRIPS=1
DUSKLIGHT_PORTMASTER_BATCH_QUADS=1
DUSKLIGHT_PORTMASTER_BATCH_REUSE_CACHE=1
DUSKLIGHT_PORTMASTER_DISABLE_DEPTH_PEEK=1
DUSKLIGHT_PORTMASTER_DRAW_SKIP=0
DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW=1
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=30
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=4
SDL_VIDEODRIVER=sdl2
```

The 320x240 choice is deliberate. Lower resolutions helped speed, but made UI
text harder to read. 320x240 is a better compromise because it scales cleanly on
640x480 panels.

Safe pacing is guarded so it should not run during risky transitions, menus, or
scene loads. It is a playability hack, not a correctness feature. It helps the
game feel closer to real time when rendering is below target, but it cannot make
heavy village scenes truly full speed.

## GX Optimization History

The important GX work so far:

- Texture-backed vertex fetch replaced the failing vertex-stage storage-buffer
  path on GLES devices whose Mali drivers report zero vertex shader storage
  blocks.
- Non-indexed triangle handling and primitive index reuse reduced some CPU-side
  index generation/upload work.
- Triangle-strip topology support avoided expanding every strip into standalone
  triangle lists.
- Triangle-strip batching combined compatible consecutive strip draws and was
  the biggest playability gain.
- Quad batching exists, but current profiles show quads are not usually the main
  limiting primitive during heavy gameplay. Strips still dominate many expensive
  scenes.
- Low-frequency GX timing logs were added to track FPS, CPU frame time, draw
  counts, upload bytes, merge reasons, primitive mix, batching, reuse, and
  submit/present timing without per-draw log spam.
- BP dirty instrumentation showed TEV color/K-color register writes (`E2-E7`)
  dominate raw BP traffic.
- TEV register dirty deferral now records TEV/K-color writes without forcing a
  draw split until the next shader proves it actually reads the changed
  register. This reduced `gx_bp reg` merge-block counts dramatically, but steady
  village FPS did not improve much.

The current bottleneck after TEV deferral is still draw submission/state churn,
not framebuffer presentation. In recent village logs, `fbdev_present_ms` is
effectively zero, while `submit_ms` is often roughly 24-30 ms in steady heavy
gameplay. `gx_dirty[xf]`, texture/state changes, and remaining BP changes now
matter more than TEV register dirtying alone.

Next promising measurement:

```text
Add XF dirty-source instrumentation similar to BP top-register instrumentation.
Find whether transform/matrix writes are redundant, deferable, or actually used
by the next draw.
```

Avoid spending more time on blind primitive culling or TEV-only work until that
XF/state profile exists.

## Input Strategy

Input currently uses native SDL gamepad mapping for normal gameplay.

The package intentionally does not start `gptokeyb` by default. Dusklight is an
SDL gamepad application, and layering a keyboard mapper over SDL gamepad input
risks duplicated or scrambled controls. The packaged `dusklight.gptk` is now
inert so an accidental `gptokeyb` launch does not synthesize extra keys.

The launcher uses PortMaster's `sdl_controllerconfig` for normal devices. If
`/proc/bus/input/devices` reports `muOS-Keys`, the launcher intentionally
prefers the bundled `muOS-Keys` entry in `res/gamecontrollerdb.txt`, because the
system DB observed on muOS mapped several buttons incorrectly.

This is the current control strategy:

```text
firmware input device -> SDL controller DB -> Dusklight's normal SDL/PAD mapping
```

The earlier `DUSKLIGHT_PORTMASTER_HANDHELD_MAPPING` path and main-loop system
menu hook were removed from the PortMaster package. They made a bad controller
DB harder to diagnose because physical button identity was being transformed in
multiple places. If controls are wrong now, the next step is to log/probe SDL
button and axis names on hardware and correct the controller DB entry, not add
another in-game mapping layer.

Menu access is intentionally limited to SDL Guide/Home-style buttons
(`Guide`, `Misc1`, or `Touchpad`) plus Dusklight's existing Start + R UI chord.
SDL Back/Select is no longer treated as a generic menu fallback because that can
steal a normal gameplay input on handhelds.

## Build System

The aarch64 Docker build is the supported local package build path:

```text
scripts/portmaster/build_docker_aarch64_bundle.sh --out-dir artifacts/portmaster-test
```

The Dockerfile installs `patch` and disables SDL XScrnSaver/XTest detection to
avoid unnecessary cross-build dependency failures:

```text
-DSDL_X11_XSCRNSAVER=OFF
-DSDL_X11_XTEST=OFF
```

The output archive is expected at:

```text
artifacts/portmaster-test/Dusklight-aarch64-portmaster.zip
```

## Known Limitations

- This is still a PortMaster test package, not a release candidate.
- Heavy areas can still feel slower than real time.
- Texture-backed vertex fetch is portable but expensive.
- Native vertex-buffer specialization remains a future optimization path.
- The package does not include game data.
- Storage or save hangs may be caused by the mounted SD/storage stack; keep logs
  and avoid hard power cycles during writes when possible.
