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
- Draw-distance scaling and village-clutter actor culling were tested as
  low-spec experiments. They worked mechanically, but did not produce enough
  improvement in heavy village scenes to justify enabling them by default.

Current low-spec culls:

```text
DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW=1
```

These are pragmatic rough-port cuts. They improve headroom but reduce fidelity.

### July 2026 Village Performance Wrap-Up

The RG35XX H / muOS village performance pass should be considered wrapped for
now.

Instrumentation added during this pass:

```text
DUSKLIGHT_PORTMASTER_GX_STATS=1
DUSKLIGHT_PORTMASTER_ACTOR_STATS=1
DUSKLIGHT_PORTMASTER_PROCESS_DRAW_STATS=1
```

The process draw profiler now reports both inclusive and exclusive process draw
time. The useful line is:

```text
PortMaster process_draw_top[exclusive_usec]
```

All of these are disabled by default in the PortMaster launcher so release logs
remain quiet.

Tested performance experiments:

```text
DUSKLIGHT_PORTMASTER_DRAW_DISTANCE_SCALE=0.70
DUSKLIGHT_PORTMASTER_DRAW_DISTANCE_SCALE=0.55
DUSKLIGHT_PORTMASTER_CULL_VILLAGE_CLUTTER=1
DUSKLIGHT_PORTMASTER_BIND_FULL_INDEX_BUFFER=1
```

The village clutter cull skipped drawing selected low-importance actors:

```text
item
Fish
Obj_Tbi / Obj_Yobikusa
Obj_Tie / Obj_OnCloth
341-1 / Obj_Laundry
Pumpkin
```

It was draw-only and disabled during events, so it did not intentionally remove
actor execution, collision, audio, events, or save behavior.

Representative heavy-village result after cull/profiling:

```text
fps ~= 5.5
avg_total_ms ~= 80 ms
avg_submit_ms ~= 50 ms
avg_render_encode_ms ~= 7 ms
avg_fbdev_present_ms = 0
```

Exclusive draw timing showed there is no single safe leaf actor to delete for a
large win. `item`, `Pumpkin`, `Fish`, `Bg`, `Obj_Tie`, `Link`, and `kdoor` each
showed measurable cost, but none was individually large enough to rescue the
worst scenes. Removing several of them would be visually destructive and still
unlikely to turn the village into stable 12-15 FPS gameplay.

Conclusion:

- Do not enable draw-distance scaling by default.
- Do not enable village clutter culling by default.
- Do not enable full-index-buffer binding by default. It binds the full index
  buffer once and uses `firstIndex` for texture-vertex draws, but village submit
  timing stayed in the same broad band.
- Do not keep expanding broad actor culls unless an explicit ugly low-spec mode
  is desired.
- The remaining bottleneck is primarily Dawn/OpenGLES/GX submission and driver
  synchronization, not presentation and not one obvious game actor.
- Further meaningful performance work likely requires larger renderer/GX
  architecture changes, or adaptive pacing/quality work that improves feel
  rather than raw FPS.

Additional merge finding:

Strict dirty-merge probing compared adjacent draw uniforms while ignoring only
the `vtx_start` prefix. In village logs, almost all promising dirty-merge
candidates still had different uniform bytes:

```text
dirty_probe ... usame=0 udiff=130 could=0
dirty_probe ... usame=0 udiff=737 could=0
dirty_probe ... usame=0 udiff=1101 could=0
```

This means dirty blockers are usually real per-draw state/uniform differences,
not just harmless dirty flags. Do not implement broad "merge across dirty" logic
without a narrower proof for a specific state class.

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

Paused. The July 2026 village pass did not find a safe, high-payoff optimization
beyond the existing batching/pacing work. Do not continue broad profiling or
random culls for now. Reopen only if there is a new renderer/GX architecture
change to test, or a specific log points to one isolated high-cost effect.

Long-term durable path:

```text
Make Dawn/Aurora consume bmdhacks SDL2-backend EGL metadata as a proper external
surface provider, with explicit ownership and present semantics.
```

That is still the most plausible portable direction short of writing a direct
GLES renderer.
