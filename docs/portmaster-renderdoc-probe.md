# PortMaster RenderDoc Probe

Date: 2026-07-06

Goal: find an existing arm64 RenderDoc package that can capture Dusklight running on PortMaster handhelds, without building RenderDoc from source.

## Runtime Candidates

- Debian bookworm arm64 RenderDoc 1.24 extracted and staged locally.
  - Too new for broad compatibility: requires newer glibc/libstdc++ symbols such as `GLIBC_2.34` and `GLIBCXX_3.4.29`.
- Debian bullseye arm64 RenderDoc 1.11 extracted and staged locally.
  - Better compatibility candidate: requires up to roughly `GLIBC_2.29` and `GLIBCXX_3.4.26`.
  - Staged files:
    - `bin/renderdoccmd`
    - `lib/librenderdoc.so`
    - `lib/libstb.so.0`
    - `lib/libxcb-keysyms.so.1`
    - `lib/libX11-xcb.so.1`
    - `python/renderdoc.so`
  - On RG35XX H / muOS, `renderdoccmd version` runs when `LD_LIBRARY_PATH` includes the staged `renderdoc/lib`.

## Dusklight Hook Added

An optional RenderDoc hook was added in `extern/aurora/lib/aurora.cpp`.

Environment variables:

- `DUSKLIGHT_RENDERDOC_CAPTURE_AFTER`
  - `0` disables the hook.
  - Positive value requests a capture at that Aurora frame.
- `DUSKLIGHT_RENDERDOC_CAPTURE_PATH`
  - Capture path template, defaulting to `renderdoc/captures/dusklight`.
- `DUSKLIGHT_RENDERDOC_LIB`
  - Path to `librenderdoc.so`.

Important: the hook is initialized before Dawn/OpenGLES setup, after SDL/window system initialization but before Aurora creates the graphics window/device.

The PortMaster launcher keeps capture disabled by default.

## RG35XX H / muOS Results

Device: RG35XX H, muOS 2601.0.

Working:

- The bullseye `renderdoccmd` binary starts on-device.
- `librenderdoc.so` can be loaded by Dusklight.
- `RENDERDOC_GetAPI` succeeds.
- RenderDoc API reports `1.4.1` through the API struct.
- Dusklight runs normally when RenderDoc is loaded via `dlopen` and capture is disabled or only the hook is initialized.

Failed:

- `StartFrameCapture(nullptr, nullptr)` followed by `EndFrameCapture(nullptr, nullptr)` returns `0` and writes no `.rdc`.
- `TriggerCapture()` logs that it was requested, but writes no `.rdc`.
- Launching through `renderdoccmd capture` crashes before Dawn/OpenGLES initialization, in the SDL2/shim stack.
- `LD_PRELOAD=/path/to/librenderdoc.so` also crashes before Dawn/OpenGLES initialization, in the SDL2/shim stack.

Observed crash shape for `renderdoccmd capture` / `LD_PRELOAD`:

- `SIGSEGV`
- Backtrace enters `/usr/lib/libSDL2-2.0.so.0`, then bundled `libSDL3.so.0`, before graphics adapter creation.

## Tiny SDL2-Backend GLES Probe

Added and rebuilt:

- `packaging/portmaster/probes/sdl2shim_gles_present_probe.c`
- `packaging/portmaster/probes/sdl2shim_gles_renderdoc_probe.sh`
- `packaging/portmaster/probes/sdl2shim_gles_renderdoc_probe_port.json`
- `scripts/portmaster/build_sdl2shim_gles_present_probe.sh`
- artifact: `artifacts/sdl2shim-gles-present-probe.tar.gz`
- PortMaster package:
  `artifacts/sdl2shim-gles-renderdoc-portmaster/Sdl2ShimGlesRenderdocProbe-portmaster.zip`

The probe uses the same bmdhacks SDL3 SDL2-backend path, but no Dusklight and
no Dawn. It creates a GLES2 context, draws a colored triangle and a textured
quad, swaps with `SDL_GL_SwapWindow`, and optionally requests RenderDoc capture.

A PortMaster launcher is also available so the probe can be run through the
firmware menu instead of SSH. This matters because SSH tests may fight the
frontend, while PortMaster launchers call `pm_platform_helper` and `pm_finish`.
On muOS RG35XX H, it was deployed as:

- launcher: `/mnt/mmc/ROMS/Ports/sdl2shim-gles-renderdoc-probe.sh`
- payload: `/mnt/mmc/ports/sdl2shim-gles-renderdoc-probe/`
- log: `/mnt/mmc/ports/sdl2shim-gles-renderdoc-probe/log.txt`
- captures: `/mnt/mmc/ports/sdl2shim-gles-renderdoc-probe/captures/`

RG35XX H / muOS results:

- Baseline without RenderDoc works.
  - `SDL_VIDEODRIVER=sdl2`
  - current driver `sdl2`
  - `GL_VENDOR=ARM`
  - `GL_RENDERER=Mali-G31`
  - `GL_VERSION=OpenGL ES 3.2 ...`
  - frame logs show `gl_error=0x0000`
- RenderDoc trigger mode:
  - `renderdoc ready=1 api=1.4.1`
  - `renderdoc TriggerCapture frame=60`
  - no `.rdc` written
- RenderDoc start/end mode:
  - `renderdoc StartFrameCapture frame=60`
  - `renderdoc EndFrameCapture frame=60 result=0`
  - no `.rdc` written
- RenderDoc `LD_PRELOAD` mode:
  - immediate `Segmentation fault` before probe logs complete
  - no `.rdc` written

This is a stronger negative result than the Dusklight-only test: RenderDoc does
not capture even a minimal SDL2-backend GLES app on the same firmware stack.

## Current Conclusion

Existing Debian RenderDoc packages are useful enough to load and query, but not enough to capture this PortMaster SDL2-shim/OpenGLES path on RG35XX H.

The likely issue is not package discovery anymore. It is capture integration:

- `dlopen` is too passive for RenderDoc to hook the active GLES stream in this stack.
- `LD_PRELOAD`/`renderdoccmd capture` hook early enough, but destabilize bmdhacks SDL2 backend before Dawn starts.
- We do not currently have a reliable native EGL window/device handle path to pass to `StartFrameCapture`.

## Remote Server / Target Control Test

The official RenderDoc remote-server path was also tested on-device, using the
handheld's own Python 3.11 plus the staged `renderdoc.so` module:

- Start server:
  - `renderdoccmd remoteserver -d`
- Python flow:
  - `rd.InitialiseReplay(...)`
  - `rd.CreateRemoteServerConnection("localhost")`
  - `remote.ExecuteAndInject(...)`
  - `rd.CreateTargetControl("localhost", ident, ..., True)`
  - `target.TriggerCapture(1)`
  - `target.QueueCapture(30, 1)`
  - poll `target.ReceiveMessage(None)`

Result:

- Remote server starts.
- Python `renderdoc` module imports successfully on-device.
- `CreateRemoteServerConnection("localhost")` returns success.
- `ExecuteAndInject` returns success and a target ident.
- `EnumerateRemoteTargets("localhost", 0)` returns the same ident.
- `CreateTargetControl(...)` succeeds.
- `TriggerCapture(1)` and `QueueCapture(30, 1)` are accepted.
- No real `NewCapture` message is emitted.
- The target-control stream changes from `Noop` to `Disconnected`.
- No `.rdc` appears.

This rules out our previous concern that only the in-application API path was
wrong. Even RenderDoc's remote target-control workflow can connect but still
cannot capture this SDL2-backend GLES stream.

## Recommendation

Do not spend more time on RenderDoc as the main profiling route until one of these changes is available:

- A known-good RenderDoc build/package for the exact bmdhacks SDL2-backend + Mali stack.
- A tiny SDL2-backend GLES sample that can be captured successfully first. The
  current sample does not capture on RG35XX H / muOS.
- A stable way to get the exact EGL display/context/surface/window handles accepted by RenderDoc on this stack.

For near-term performance work, prefer the built-in profiling/capture counters already added to Aurora:

- `DUSKLIGHT_PORTMASTER_GX_STATS=1`
- `DUSKLIGHT_PORTMASTER_UPLOAD_STATS=1`
- `DUSKLIGHT_PORTMASTER_FRAME_CAPTURE=1`
- `DUSKLIGHT_PORTMASTER_FRAME_CAPTURE_AFTER=N`
- `DUSKLIGHT_PORTMASTER_FRAME_CAPTURE_TOP=N`

Those counters do not provide GPU pipeline inspection like RenderDoc, but they are stable on-device and already identify the dominant GX layouts, upload volume, merge blockers, and submit timing.
