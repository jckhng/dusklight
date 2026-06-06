# Dusklight PortMaster Release Notes

This document describes how to build the local PortMaster test package and how
to preserve the porting work in a fork.

## Release Artifact

The local release/test archive is:

```text
artifacts/portmaster-release/Dusklight-aarch64-portmaster.zip
```

The archive must not contain game data. Users must provide their own supported
disc image and place it in:

```text
dusklight/assets/
```

Supported local test extensions are `.iso`, `.gcm`, and `.rvz`, case
insensitive.

## Build From Source

Prerequisites:

- Docker
- Git with submodule support
- A recursive checkout of Dusklight

Fresh checkout:

```sh
git clone --recursive <your-dusklight-fork-url> dusklight
cd dusklight
git submodule update --init --recursive
```

Build the PortMaster aarch64 package:

```sh
scripts/portmaster/build_docker_aarch64_bundle.sh --out-dir artifacts/portmaster-release
```

For private device testing only, a local ROM/assets directory can be copied into
the package:

```sh
scripts/portmaster/build_docker_aarch64_bundle.sh \
  --assets-dir rom \
  --out-dir artifacts/portmaster-private-test
```

Do not distribute packages built with `--assets-dir`.

## What The Docker Build Does

The Docker path is the supported build path for this port. It:

- cross-builds Dusklight for Linux aarch64
- source-builds Dawn with OpenGLES enabled
- applies `packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch`
- bundles SDL3, Dawn, and nod shared libraries into `dusklight/lib.aarch64`
- stages the PortMaster launcher and resources
- emits `Dusklight-aarch64-portmaster.zip`

The Dawn fbdev surface change is not a submodule commit. It is intentionally
stored as a patch and applied to CMake's generated Dawn source tree during the
Docker build.

## Runtime Layout

Expected PortMaster layout:

```text
dusklight.sh
dusklight/
  dusklight.aarch64
  lib.aarch64/
  res/
  assets/
  runtime/
```

On muOS, the launcher should live under:

```text
/mnt/mmc/ROMS/Ports/dusklight.sh
```

and the payload under:

```text
/mnt/mmc/ports/dusklight/
```

Avoid installing test copies under stale `/roms` paths on this setup.

## Fork Strategy

Dusklight uses `extern/aurora` as a Git submodule. The PortMaster work currently
has required changes both in the main Dusklight repository and inside that
Aurora submodule.

Recommended organization:

1. Fork Dusklight into your own GitHub account or organization.
2. Fork Aurora too, because `extern/aurora` has local PortMaster changes.
3. Commit the Aurora changes to a branch in your Aurora fork, for example
   `portmaster-aarch64`.
4. In your Dusklight fork, update the `extern/aurora` submodule pointer to that
   Aurora branch/commit.
5. Commit the Dusklight-side PortMaster files and code changes.

Dawn does not need to be a submodule in your Dusklight fork for this approach.
Keep the Dawn change as:

```text
packaging/portmaster/patches/dawn-portmaster-fbdev-surface.patch
```

That keeps the Dawn delta explicit and avoids vendoring a large generated Dawn
source tree.

## Source Files To Keep

Keep the PortMaster-specific source and packaging changes in:

```text
packaging/portmaster/
scripts/portmaster/
docs/portmaster-controls.md
docs/portmaster-technical-notes.md
docs/portmaster-release.md
dusklight_portmaster_handoff.md
res/gamecontrollerdb.txt
src/dusk/ui/input.cpp
src/dusk/game_clock.cpp
include/dusk/game_clock.h
src/m_Do/m_Do_main.cpp
```

Also keep the performance and rendering changes already made in Dusklight and
Aurora. Use `git status --short` in both repositories before committing.

Do not commit:

```text
artifacts/
build/
rom/
muOS_*.png
```

unless you intentionally want local evidence artifacts. Never commit ROM/disc
images.

## Current Control Mapping

The package uses native SDL gamepad input. It does not start `gptokeyb` by
default.

For `muOS-Keys`, the bundled SDL mapping is:

```text
a:b0,b:b1,x:b3,y:b2,
leftshoulder:b4,rightshoulder:b5,
lefttrigger:b10,righttrigger:b11,
guide:b8,start:b7,back:b6,
leftstick:b9,rightstick:b12
```

If controls regress, enable diagnostics:

```sh
DUSKLIGHT_PORTMASTER_INPUT_DIAG=1
```

and inspect:

```text
/mnt/mmc/ports/dusklight/log.txt
```

## Known Release Caveats

This should be labeled as a PortMaster test build, not a stable release.

Known caveats:

- heavy areas can still run below real time
- safe pacing is a playability compromise
- texture-backed vertex fetch is slower than a true vertex-buffer path
- the current package is verified mainly on RG35XX H / muOS / Mali-G31
- other PortMaster devices may need controller DB or EGL tweaks
