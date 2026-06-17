# Dusklight PortMaster package scaffold

This scaffold is for a fail-fast Linux aarch64 PortMaster spike. It stages an
already-built `dusklight` binary and keeps the launcher simple enough to
iterate on-device across PortMaster-capable handhelds.

Expected test package layout:

- `dusklight.sh`
- `dusklight/dusklight.aarch64`
- `dusklight/res/`
- `dusklight/assets/` for a user-provided disc image
- `dusklight/runtime/` for config, saves, logs, and cache
- `dusklight/lib.${DEVICE_ARCH}/` or `dusklight/libs.${DEVICE_ARCH}/` for bundled shared libraries

The launcher forces the OpenGL ES renderer and low-end settings through
`--backend opengles` and repeatable `--cvar` overrides. These overrides are
transient and do not permanently rewrite the user's config.

Graphics surface/display selection is controlled with:

```sh
DUSKLIGHT_PM_GRAPHICS_MODE=dawn-sdl2shim
```

Supported values:

- `dawn-sdl2shim`: use the bundled SDL3 shim over the firmware SDL2 backend, render Dawn offscreen without a WebGPU swapchain surface, then present the final frame through an SDL2-owned GLES context. This is the default compatibility-test path.
- `dawn-sdl2shim-borrow`: old SDL2-shim mode that borrows SDL2's `EGLSurface` into Dawn but does not use the SDL swap hook. Keep for A/B testing only.
- `dawn-sdl2shim-borrow-sdlswap`: alpha6-style SDL2-shim mode that borrows SDL2's `EGLSurface` into Dawn and calls SDL's swap function. Keep for A/B testing only.
- `auto`: legacy heuristic that prefers the fbdev sentinel path on headless PortMaster firmware with `/dev/fb0`; use `dawn-kmsdrm` only if no fbdev device is present but `/dev/dri/card*` exists; otherwise let SDL choose the default video driver.
- `dawn-sdl`: do not force `SDL_VIDEODRIVER`; use SDL's default window path.
- `dawn-wayland`: force `SDL_VIDEODRIVER=wayland`.
- `dawn-x11`: force `SDL_VIDEODRIVER=x11`.
- `dawn-kmsdrm`: force `SDL_VIDEODRIVER=kmsdrm`; useful as a diagnostic mode, but Aurora/Dawn currently does not create a Dawn surface from SDL KMSDRM/GBM handles.
- `dawn-fbdev-sentinel`: force the older PortMaster fbdev sentinel path with `SDL_VIDEODRIVER=offscreen`.
- `diag`: write platform diagnostics to `log.txt` and exit without launching the game.

For older devices that fail with `No supported adapters`, ask testers to run
`diag` first and then try `dawn-sdl2shim`. Use `dawn-sdl`, `dawn-wayland` or
`dawn-x11` only if their firmware provides a display server, and `dawn-kmsdrm`
only to confirm whether SDL can reach KMSDRM. Keep `dawn-fbdev-sentinel` as the
playable fallback for firmware where that path is known to work.

For device tests, place a Twilight Princess disc image in `dusklight/assets/`.
Public PortMaster archives must not redistribute game data.

## Local live-device loop

Build a fresh test package:

```sh
scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh --out-dir artifacts/portmaster-test
```

Deploy the staged binary and launcher to a live muOS/PortMaster device:

```sh
PM_PASSWORD=<device-password> scripts/portmaster/live_device.py --host <device-ip> deploy
```

The helper deliberately targets the known PortMaster paths:

- `/mnt/mmc/ports/dusklight/dusklight.aarch64`
- `/mnt/mmc/ROMS/Ports/dusklight.sh`

Useful follow-up commands:

```sh
PM_PASSWORD=<device-password> scripts/portmaster/live_device.py --host <device-ip> kill
PM_PASSWORD=<device-password> scripts/portmaster/live_device.py --host <device-ip> verify
PM_PASSWORD=<device-password> scripts/portmaster/live_device.py --host <device-ip> tail
```

Primary expected failure points:

- Dawn/OpenGLES not built into the binary.
- EGL/GLES shared libraries missing or incompatible on the target firmware.
- Dawn no-surface OpenGLES adapter creation fails on the target firmware.
- SDL2-shim GL context creation or `SDL_GL_SwapWindow` presentation fails.
- SDL3 runtime or game controller mapping issues.
- Runtime memory use exceeds the target device envelope.
