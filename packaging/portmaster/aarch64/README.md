# Dusklight PortMaster Package

This directory contains the aarch64 PortMaster package template. The package
uses the bmdhacks SDL3 `sdl2-backend` shim to let the firmware's SDL2 backend
own the native display, EGL context, and presentation path. Aurora passes the
SDL-owned EGL objects to Dawn for OpenGL ES rendering.

## Package Layout

```text
dusklight.sh
dusklight/
  dusklight.aarch64
  dusklight.gptk
  gameinfo.xml
  port.json
  lib.aarch64/
  res/
  assets/
  runtime/
```

Users must place their own supported Twilight Princess disc image in
`dusklight/assets/`. Game data is not part of the package.

## Graphics Modes

The launcher supports one rendering path and one diagnostic mode:

- `dawn-sdl2shim-owned` is the default SDL2-owned EGL path.
- `auto` and `dawn-sdl2shim` are compatibility aliases for the default.
- `diag` records platform information in `dusklight/log.txt` and exits.

Override the mode before launch with `DUSKLIGHT_PM_GRAPHICS_MODE`. Unknown
values fall back to `dawn-sdl2shim-owned`.

## Build

From a recursive checkout:

```sh
scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh \
  --out-dir artifacts/portmaster-release
```

This produces `artifacts/portmaster-release/dusklight.zip`. The Docker build
uses Ubuntu 20.04/GCC 10 compatibility targets, builds Dawn with OpenGL ES,
builds the bmdhacks SDL shim over the device SDL2 runtime, bundles required
shared libraries, and strips release binaries.

`--assets-dir` exists for private device tests only. Never distribute an
archive built with local game assets.

## Device Helper

`scripts/portmaster/live_device.py` can deploy, stop, inspect, and tail a muOS
test installation. It does not contain a host or password; pass `--host` and
set `PM_PASSWORD` at invocation time.
