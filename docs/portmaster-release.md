# Dusklight PortMaster Build and Release

This is an experimental aarch64 PortMaster package. It is not a stable or
universal firmware release.

## Source Layout

Dusklight pins Aurora through the `extern/aurora` Git submodule. PortMaster
changes therefore use two branches:

1. An Aurora branch containing the SDL source override, Focal toolchain
   compatibility, strict SDL2-shim presentation synchronization, and GX setup
   caching.
2. A Dusklight branch pinning that Aurora commit and containing the launcher,
   package template, build scripts, and required Dawn/SDL source patches.

Dawn remains a vendored build dependency rather than a repository submodule.
The small GCC 10 compatibility patches are stored under
`packaging/portmaster/patches/` and applied by the Docker build.

## Build

Prerequisites are Docker and a recursive Git checkout:

```sh
git clone --recursive <dusklight-fork-url> dusklight
cd dusklight
git submodule update --init --recursive
scripts/portmaster/build_docker_aarch64_focal_sdl2shim_bundle.sh \
  --out-dir artifacts/portmaster-release
```

The output is:

```text
artifacts/portmaster-release/dusklight.zip
```

The archive contains a stripped aarch64 release binary and its required shared
libraries. It must not contain a disc image. Users provide a supported `.iso`,
`.gcm`, or `.rvz` file in `dusklight/assets/`.

For a private device-only build, `--assets-dir <path>` copies supported images
into the staged package. Do not distribute that artifact.

## Runtime Path

The launcher derives the payload directory from PortMaster's `directory`
variable. The expected logical layout is:

```text
<ports launcher directory>/dusklight.sh
<ports data directory>/dusklight/
```

Configuration, saves, caches, and logs are kept below `dusklight/runtime/`.

## Release Checks

Before publishing:

1. Build without `--assets-dir`.
2. Confirm the archive contains no disc images, logs, saves, configuration, or
   device addresses.
3. Confirm the executable and bundled shared libraries are stripped aarch64
   ELF files.
4. Test launch, gameplay input, the Dusklight menu, emergency quit, and clean
   return to the firmware menu.
5. Label the artifact as a test build and state the tested firmware/device.

Known limitations include firmware-specific EGL presentation behavior,
controller database differences, and reduced performance in heavy scenes.
