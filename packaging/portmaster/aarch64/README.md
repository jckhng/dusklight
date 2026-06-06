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

The launcher forces the OpenGL ES backend and low-end settings through
`--backend opengles` and repeatable `--cvar` overrides. These overrides are
transient and do not permanently rewrite the user's config.

For device tests, place a Twilight Princess disc image in `dusklight/assets/`.
Public PortMaster archives must not redistribute game data.

## Local live-device loop

Build a fresh test package:

```sh
scripts/portmaster/build_docker_aarch64_bundle.sh --out-dir artifacts/portmaster-test
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

Use `deploy --script-only` when only launcher defaults changed. This avoids
copying the full binary during quick runtime-toggle tests.

Primary expected failure points:

- Dawn/OpenGLES not built into the binary.
- EGL/GLES shared libraries missing or incompatible on the target firmware.
- SDL3 runtime or game controller mapping issues.
- Adapter/surface creation succeeds but presentation fails on the device GPU/driver.
- Runtime memory use exceeds the target device envelope.
