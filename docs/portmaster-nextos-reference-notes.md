# nextos_ports_android Reference Notes

Reference repo cloned locally:

```text
reference/nextos_ports_android
```

Upstream:

```text
https://github.com/felc18-blip/nextos_ports_android
```

## Scope

This repo is mostly an Android `.so` loader framework, not ordinary Linux
PortMaster ports. Most of its core loader/JNI/bionic work does not apply
directly to Dusklight.

The useful part for Dusklight is its display-stack strategy across handheld
firmwares and devices.

## Relevant Patterns

### 1. Let SDL own the display backend

The launchers generally avoid forcing `SDL_VIDEODRIVER` unless a specific
device/backend experiment needs it.

Examples:

- `ports/dysmantle/DYSMANTLE.sh`
- `ports/sor4/port/package/StreetsOfRage4.sh`

Notes from those scripts:

- Do not set `SDL_VIDEODRIVER` or `SDL_AUDIODRIVER` by default.
- Let the device SDL pick `mali`/fbdev, KMSDRM, Wayland, Pulse, ALSA, etc.
- Avoid putting bundled libraries in `LD_LIBRARY_PATH` before helper UI tools
  like progressors, because that can make the helper pick the wrong SDL.

Dusklight relevance:

- This supports the bmdhacks SDL2-backend direction more than the old
  fbdev-sentinel direction.
- It argues against per-firmware launcher hacks as the default path.

### 2. KMSDRM/fbdev selection by actual device nodes

`ports/cuphead/run.sh` and `ports/cuphead/HANDOFF-X5M-KMSDRM.md` use:

```sh
if [ -e /dev/dri/card0 ]; then
  # KMSDRM path
else
  # fbdev path
fi
```

They also explicitly handle DRM master ownership:

- stop likely frontend/compositor services;
- kill stale frontend processes;
- wait for `/dev/dri/card0` to be released.

Dusklight relevance:

- Test packages should log `/dev/dri`, `/dev/fb0`, active video driver, and
  process holders of `/dev/dri/card0`.
- If we ever retry a KMSDRM path, we need a clean DRM-master handoff, not just
  `SDL_VIDEODRIVER=kmsdrm`.

### 3. SDL-created EGL objects are the reliable surface source

The key successful pattern is in:

```text
core/egl_shim.c
facilitando_o_trabalho/kit_essencial/core/egl_shim.c
ports/cuphead/src/egl_shim.c
```

They create an SDL GL window/context, then use the EGL objects created by SDL:

```c
eglGetCurrentDisplay()
eglGetCurrentSurface(EGL_DRAW)
eglGetCurrentContext()
SDL_GL_SwapWindow()
```

For Android-loader ports, they then reroute the game engine's imported `egl*`
symbols to the shim. See `egl_patch_unity_got()` in
`ports/cuphead/src/main.c`.

Dusklight relevance:

- This matches the intuition that bmdhacks SDL2-backend is the best portability
  layer.
- But Dusklight uses Dawn/WebGPU, so the equivalent fix must be inside the
  Dawn/Aurora surface bridge rather than GOT-patching raw game `egl*` imports.
- Dawn must treat the SDL-created EGL surface as the owner of presentation, not
  invent or query a mismatched surface path.

### 4. fbdev direct/native EGL is not universal

The Cuphead notes explicitly say the old direct Mali fbdev path works on
Mali-450/Amlogic-old, but fails on KMSDRM/Valhall devices that do not expose EGL
fbdev.

Dusklight relevance:

- The old `dawn-fbdev-sentinel` mode should remain only a fallback, not the
  primary compatibility story.
- For ArkOS/Knulli/TrimUI, `/dev/fb0` existing does not prove direct fbdev EGL
  is usable.

### 5. The repo's Dusklight experiment is not directly reusable

`ports/dusklight/STATUS.md` investigated an Android APK build of Dusklight.
That APK had SDL3 Android internals and Dawn backends compiled as Vulkan/Null,
with no usable GLES path for Mali-450.

Dusklight relevance:

- It confirms that the Android APK path is a dead end for our current goal.
- It does not solve our Linux/Dawn GLES surface problem.

## Practical Takeaways For Our Port

Recommended compatibility direction:

1. Keep bmdhacks SDL2-backend as the preferred portability layer.
2. Make Aurora/Dawn consume SDL's real EGL display/surface/context cleanly.
3. Avoid forcing `SDL_VIDEODRIVER` in the default launcher once the SDL-backed
   path is robust.
4. Keep explicit probe modes for:
   - `dawn-sdl2shim-native`
   - `dawn-fbdev-sentinel`
   - future `dawn-sdl-egl-owned` / SDL-present path
5. Improve diagnostics around:
   - selected SDL video driver;
   - EGL display/surface/context pointers;
   - `/dev/dri/card0` availability and holders;
   - whether Dawn is presenting through SDL-owned objects or a fallback path.

Most durable long-term design:

```text
SDL owns window/context/surface selection.
Aurora asks SDL for the EGL objects.
Dawn is patched to use those EGL objects without second-guessing the platform.
Launcher stays mostly generic.
```

Open risk:

The nextos ports use raw EGL/GL engines where presentation can be redirected at
the `egl*` function boundary. Dusklight's Dawn/WebGPU stack has deeper surface
ownership rules, so a superficial "borrow the pointer" bridge can still fail
with `EGL_BAD_SURFACE` or blank output on some firmware stacks.
