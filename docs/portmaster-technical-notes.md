# Dusklight PortMaster Technical Notes

This document records the local PortMaster test-port changes and why they exist.
It is not an upstream design document and should be treated as a fail-fast
engineering log for low-power Linux handhelds.

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

## Renderer Path

Dusklight is not a simple SDL2/GLES renderer. The path is:

```text
Dusklight -> Aurora GX -> WebGPU API -> Dawn OpenGLES backend -> Mali EGL/GLES
```

The original Weston/fbdev experiments proved useful for presentation, but the
CPU fbdev copy path was too slow. At 640x480 it spent roughly 380 ms per present;
at lower resolutions it was still a major frame-time cost. The current package
uses a Mali EGL/fbdev native-window path instead.

To make that durable, the Docker build applies:

```text
packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch
```

That patch lets Dawn accept an Xlib surface descriptor with a null display as a
PortMaster sentinel, then passes the stored native window handle directly to
`eglCreateWindowSurface`. This avoids maintaining an ad hoc generated-tree edit.

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
DUSKLIGHT_PORTMASTER_RENDER_WIDTH=320
DUSKLIGHT_PORTMASTER_RENDER_HEIGHT=240
DUSKLIGHT_PORTMASTER_LOW_SPEC=1
DUSKLIGHT_PORTMASTER_EGL_FBDEV_SURFACE=1
DUSKLIGHT_PORTMASTER_NOINDEX_TRIANGLES=1
DUSKLIGHT_PORTMASTER_DISABLE_DEPTH_PEEK=1
DUSKLIGHT_PORTMASTER_DRAW_SKIP=1
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=15
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=4
SDL_VIDEODRIVER=offscreen
```

The 320x240 choice is deliberate. Lower resolutions helped speed, but made UI
text harder to read. 320x240 is a better compromise because it scales cleanly on
640x480 panels.

Safe pacing is guarded so it should not run during risky transitions, menus, or
scene loads. It is a playability hack, not a correctness feature. It helps the
game feel closer to real time when rendering is below target, but it cannot make
heavy village scenes truly full speed.

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
