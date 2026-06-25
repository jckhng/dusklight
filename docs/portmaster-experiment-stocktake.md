# Dusklight PortMaster Experiment Stocktake

This file is the current experiment ledger for the Dusklight PortMaster port.
Use it to avoid repeating dead ends and to understand what each graphics,
performance, and input path was trying to prove.

## Current Best Path

The best tested path is the bmdhacks SDL2-backend shim path:

```text
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim-owned
SDL_VIDEODRIVER=sdl2
SDL3SHIM_SDL2_LIB=libSDL2-2.0.so.0
DUSKLIGHT_PORTMASTER_SDL2SHIM_EGL_SURFACE=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_OWNED_EGL=1
DUSKLIGHT_PORTMASTER_SDL2SHIM_SWAP_PRESENT=1
DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1
```

What it does:

```text
Dusklight -> Aurora GX -> WebGPU/Dawn OpenGLES
          -> bmdhacks SDL3 shim
          -> firmware SDL2 EGL/GLES display path
          -> SDL2 swap/present
```

Why it is preferred:

- It works on the RG35XX H / muOS / Mali-G31 test device.
- It avoids the old CPU fbdev presenter.
- It avoids the fake/offscreen fbdev sentinel being the only working path.
- It lets the firmware SDL2 backend handle more of the display stack.
- It preserves the required GLES vertex-texture workaround.

Known limits:

- Heavy scenes are still not smooth.
- Some non-muOS devices still fail or render incorrectly.
- It is still Dawn/OpenGLES, not a direct GLES renderer.
- The exact EGL/surface behavior remains firmware-sensitive.

## Device Outcomes

| Device / firmware | GPU / stack | Current result | Notes |
| --- | --- | --- | --- |
| RG35XX H / muOS | Mali-G31, SDL2 shim path | Works | Current reference device. Video, audio, input, and quit path are usable. Heavy areas remain slow. |
| TrimUI Smart Pro S / Knulli | PowerVR Rogue GE8300 | Partially works | Real-surface path finally shows video/audio, but has severe z/depth-looking corruption and UI/menu crop. |
| R36S / ArkOS class | Mali, older PortMaster stack | Unsolved | Earlier failures include no video, no adapter, or surface/present failure. Current SDL2-shim path still needs tester confirmation per mode. |
| Older Ubuntu 19.10 PortMaster devices | old glibc/libstdc++ | Build issue mostly separate | Focal-based build was introduced to avoid newer glibc requirements. Display failure is a separate problem. |

## Graphics Mode Matrix

| Mode / path | Status | What it does | Result |
| --- | --- | --- | --- |
| `dawn-sdl2shim-owned` | Current best | SDL3 shim over firmware SDL2, publishes SDL2-owned EGL display/surface/context metadata to Dawn, presents with SDL swap hook. | Best muOS result. Current compatibility-test default. |
| `dawn-sdl2shim-real-surface` | Useful experiment | Similar real SDL2 EGL surface path, with Dawn context-present handling. | Got Knulli to visible video/audio, but PowerVR corruption and UI crop remain. |
| `dawn-sdl2shim-native` | A/B only | Alpha10-style SDL2-shim borrowed surface with less explicit metadata. | Hit `EGL_BAD_SURFACE`/CRTC restore failures on some testers. |
| `dawn-sdl2shim-borrow` | Legacy A/B only | Borrows SDL2 `EGLSurface` into Dawn without SDL swap. | Too brittle; not a release path. |
| `dawn-sdl2shim-borrow-sdlswap` | Legacy A/B only | Borrows SDL2 surface and calls SDL swap. | Worked in some muOS tests, failed or blanked on others. |
| `dawn-sdl2shim` | Slow fallback | Dawn renders offscreen, then Aurora readback/presents through SDL2-owned GLES/renderer path. | Too slow and did not reliably fix ArkOS/Knulli blank video. |
| `dawn-sdl2shim-fbdev-present` | Diagnostic fallback | Dawn renders offscreen, then writes to `/dev/fb0`. | Useful to prove first pixels on some stacks; too slow for gameplay. |
| `dawn-fbdev-sentinel` | Old muOS fallback | `SDL_VIDEODRIVER=offscreen` plus custom Dawn fbdev/native-window sentinel. | Worked on RG35XX H, but was firmware-specific and not portable enough. |
| `dawn-kmsdrm` | Diagnostic only | Forces `SDL_VIDEODRIVER=kmsdrm`. | SDL KMSDRM is not enough; Aurora/Dawn still needs a compatible GBM/EGL surface bridge. |
| WestonPack / X11 / Xwayland | Ruled out | Runs under Weston/X11/GL4ES style runtime. | Dawn did not reliably enumerate/use the needed GLES adapter/surface. Do not revisit unless a new upstream Dawn path exists. |
| plain `dawn-sdl`, `dawn-wayland`, `dawn-x11` | Diagnostic only | Let or force SDL desktop-style window paths. | Mostly useful for logs; PortMaster handhelds often have no compositor/display server. |

## Important Failed Experiments

### `DUSKLIGHT_PORTMASTER_USE_FBDEV_SIZE`

Intent:

```text
Use `/dev/fb0` physical dimensions to compensate for Knulli/TrimUI drawable
size mismatch and fix menu crop.
```

Observed:

```text
SDL2 drawable reported 1216x896
surface/log path reported 1280x720
fbset reported 1280x720 physical with large virtual height
```

Result:

- The experiment did not fix the menu crop.
- It made RmlUI/ImGui/dialog rendering worse.
- It should stay off in release/tester builds.
- Any related ImGui viewport/scissor changes should be treated as experimental
  until proven with a clean Knulli capture.

### Broad Generic Native Vertex Expansion

Intent:

```text
Avoid vertex-texture cost by expanding GX input layouts into native vertex
buffers.
```

Result:

- It avoided the storage-buffer shader limit in some cases.
- It produced graphical corruption and did not become a reliable fast path.
- Future native work should be narrow and layout-specific, not generic.

### Global Draw Skip / Aggressive Pacing

Intent:

```text
Skip rendering work to simulate a playable frame rate.
```

Result:

- Blind draw skip blanked frames or broke transitions.
- Catch-up rendering during transitions caused hangs or fading that did not end.
- Safe pacing should remain guarded and conservative.

## GLES Storage Buffer Workaround

The original hard blocker was the Mali GLES limit:

```text
GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0
```

The failure was:

```text
The number of vertex shader storage blocks is greater than the maximum number allowed (0)
```

Current solution:

```text
DUSKLIGHT_PORTMASTER_FORCE_VERTEX_TEXTURE=1
```

Aurora GX uploads vertex stream data into texture-backed storage and generated
WGSL fetches vertex data via texture reads instead of vertex-stage storage
buffers. This is slower than proper vertex/storage buffers, but it is the reason
the game can run on GLES drivers with zero vertex shader storage blocks.

This is not only a Mali quirk. GLES implementations may expose storage buffers
only in limited ways compared with desktop GL/Vulkan. The exact limit varies by
driver.

## Performance Work

The main performance gains so far came from reducing GX draw/upload work.

Important flags:

```text
DUSKLIGHT_PORTMASTER_NOINDEX_TRIANGLES=1
DUSKLIGHT_PORTMASTER_STRIP_TOPOLOGY=1
DUSKLIGHT_PORTMASTER_BATCH_STRIPS=1
DUSKLIGHT_PORTMASTER_BATCH_QUADS=1
DUSKLIGHT_PORTMASTER_BATCH_REUSE_CACHE=1
```

What worked:

- Triangle-strip topology support avoids expanding every strip into triangle
  lists.
- Triangle-strip batching combines compatible adjacent strips.
- Primitive restart (`0xffff`) separates old strips inside a combined strip
  batch so unrelated strips do not connect.
- This was the biggest playability gain so far.
- Quad batching exists, but heavy gameplay profiles showed triangle strips were
  the more important primitive.

What had limited payoff:

- TEV/K-color BP deferral reduced merge-block counters but did not produce a
  large visible FPS jump.
- XF duplicate scalar skip helped remove redundant false dirties, but remaining
  XF blockers are mostly real matrix/material changes.
- Lower render resolutions help less than expected in heavy scenes, which means
  CPU/GX submission/upload/state churn often dominates over pure pixel fill.

Current low-spec culls:

```text
DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW=1
```

These are pragmatic rough-port cuts. They improve headroom but reduce fidelity.

## Presentation Findings

Old CPU fbdev presentation was a major bottleneck:

```text
avg_fbdev_present_ms often hundreds of milliseconds
```

After moving away from CPU fbdev copy, presentation is no longer the primary
muOS bottleneck. Heavy scenes are now dominated by GX submission/state/upload
work plus driver/GPU cost.

For compatibility devices, presentation is still the most fragile area:

- Some devices get audio/input but no video.
- Some report `EGL_BAD_SURFACE`.
- Some report `eglSwapBuffers failed`.
- Some report `Could not restore CRTC`.
- Knulli/PowerVR can show video but corrupt depth/geometry.

## Depth Format Experiment

An opt-in depth-format test exists:

```text
DUSKLIGHT_PORTMASTER_DEPTH24PLUS=1
```

What it does:

```text
Use WebGPU `Depth24Plus` instead of `Depth32Float`.
```

Why it exists:

```text
Knulli/PowerVR shows corruption that looks like z/depth or framebuffer
attachment trouble. Depth24Plus may map to a more native mobile depth format.
```

Status:

- Keep it as an A/B test for Knulli/PowerVR.
- Do not assume it is safe globally until muOS and Knulli are both checked.

## Input / Quit Handling

Current strategy:

```text
firmware input device -> SDL controller DB -> Dusklight SDL/PAD mapping
```

Important notes:

- Avoid layering multiple in-game controller remaps over SDL unless debugging a
  specific controller DB issue.
- `gptokeyb2` is started by the launcher when available so testers have a
  PortMaster quit combo even when the game UI is broken.
- Quit hotkeys have been inconsistent across firmware, so releases should still
  warn testers that hard quit may be needed on some devices.
- Delegating all joystick handling to the SDL2 shim broke controls in earlier
  tests; normal SDL3/Linux joystick handling is currently safer for gameplay.

## What Not To Revisit Casually

Do not spend more time on these unless a new log gives a specific reason:

- WestonPack/X11/Xwayland as a primary path.
- CPU fbdev presenter as a playable path.
- `DUSKLIGHT_PORTMASTER_USE_FBDEV_SIZE` as a Knulli menu-crop fix.
- Global draw skip.
- Broad generic native vertex expansion.
- Random grass/model culls without A/B counters.

## Next Useful Work

Short-term compatibility:

1. A/B `DUSKLIGHT_PORTMASTER_DEPTH24PLUS=1` on Knulli/PowerVR.
2. If depth does not help, inspect actual depth/stencil attachment setup and
   PowerVR-supported formats rather than changing viewport math again.
3. Preserve the current muOS `dawn-sdl2shim-owned` path while testing Knulli.

Short-term performance:

1. Keep profiling with `DUSKLIGHT_PORTMASTER_GX_STATS=1` on one known heavy
   village route.
2. Optimize only where counters show a real blocker: strip/quad merge blockers,
   index/reuse cache misses, or high GX upload bytes.
3. Treat low-spec culls as A/B experiments, not guesses.

Long-term durable path:

```text
Make Dawn/Aurora consume bmdhacks SDL2-backend EGL metadata as a proper external
surface provider, with explicit ownership and present semantics.
```

That is still the most plausible portable direction short of writing a direct
GLES renderer.
