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

Primary expected failure points:

- Dawn/OpenGLES not built into the binary.
- EGL/GLES shared libraries missing or incompatible on the target firmware.
- SDL3 runtime or game controller mapping issues.
- Adapter/surface creation succeeds but presentation fails on the device GPU/driver.
- Runtime memory use exceeds the target device envelope.
