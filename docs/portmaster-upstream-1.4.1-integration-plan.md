# PortMaster Upstream 1.4.1 Integration Plan

## Goal

Evaluate whether the latest upstream Dusklight performance work can be merged
into the PortMaster branch without regressing the current RG35XX H / muOS
package.

The main target is performance. This is not expected to solve all ArkOS,
Knulli, or TrimUI display-stack issues by itself.

## Baseline

- Current branch: `portmaster-aarch64`
- Current known package family: alpha9/alpha10 SDL2-shim native path
- Current default graphics mode: `dawn-sdl2shim-native`
- Current tested device: RG35XX H / muOS
- Current upstream target: `v1.4.1`

Important local work to preserve:

- PortMaster packaging under `packaging/portmaster/`
- PortMaster build scripts under `scripts/portmaster/`
- PortMaster technical handoff docs
- PortMaster controller mapping support, including `res/gamecontrollerdb.txt`
- Aurora PortMaster GLES/fbdev/SDL2-shim work
- Dawn and SDL2-shim patches used by the focal aarch64 package

## Why This Is Worth Trying

Upstream `v1.4.0` and `v1.4.1` contain performance work in areas that matter
for low-power handhelds:

- `J3DShapeDraw` display-list optimization
- JPA particle batching
- flower and grass draw batching
- special `kankyo` effect packet optimization
- texture caching improvements
- Aurora renderer, RmlUi, pipeline cache, and GPU submission changes

These overlap with our observed bottlenecks: GX draw count, particle/effect
load, texture churn, and frame submission cost.

## Main Risk

The top-level Dusklight merge is manageable, but `extern/aurora` is the high
risk area.

Current PortMaster Aurora commit:

```text
8794d406 Add PortMaster SDL2 shim native surface bridge
```

Upstream `v1.4.1` Aurora commit:

```text
22351fb0 Patch RmlUi to fix IME keyboard state
```

Conflicts are expected in core renderer and platform files, including:

- `lib/aurora.cpp`
- `lib/gfx/common.cpp`
- `lib/gx/command_processor.cpp`
- `lib/gx/gx.cpp`
- `lib/gx/pipeline.cpp`
- `lib/gx/shader.cpp`
- `lib/webgpu/gpu.cpp`
- `lib/window.cpp`

Because of this, do not merge directly into `portmaster-aarch64`.

## Step 1: Stabilize Alpha10 State

Checkpoint the current alpha10 work before attempting integration.

Actions:

- Review current dirty state.
- Preserve the alpha10 Dawn SDL2-shim surface patch.
- Commit only the alpha10 patch if needed.
- Leave unrelated untracked files alone unless explicitly needed.

Stop condition:

- If alpha10 state cannot be cleanly isolated, stop before branching.

## Step 2: Create Integration Branch

Create a side branch from current `portmaster-aarch64`.

Suggested branch:

```text
portmaster-upstream-1.4.1-exp
```

Rules:

- Do not force-push existing branches.
- Do not rewrite existing alpha tags.
- Keep `portmaster-aarch64` as the known working baseline.

## Step 3: Merge Top-Level Dusklight v1.4.1

Merge `v1.4.1` into the experiment branch.

Conflict policy:

- Keep PortMaster packaging and build scripts.
- Keep `res/gamecontrollerdb.txt` for PortMaster input stability.
- Keep PortMaster launcher defaults and low-spec defaults.
- Prefer upstream gameplay/performance changes where they do not conflict with
  PortMaster-specific behavior.
- Review overlapping files manually:
  - `extern/aurora`
  - `include/dusk/settings.h`
  - `res/gamecontrollerdb.txt`
  - `src/d/d_drawlist.cpp`
  - `src/dusk/config.cpp`
  - `src/dusk/data.cpp`
  - `src/dusk/settings.cpp`
  - `src/dusk/ui/controller_config.cpp`
  - `src/dusk/ui/settings.cpp`
  - `src/dusk/ui/ui.cpp`
  - `src/m_Do/m_Do_main.cpp`

Stop condition:

- If a conflict requires removing PortMaster packaging or input mapping support,
  stop and reassess.

## Step 4: Rebase Aurora PortMaster Work

In `extern/aurora`, start from the upstream Aurora commit used by Dusklight
`v1.4.1`, then reapply PortMaster Aurora work.

PortMaster Aurora work to preserve:

- fbdev/sentinel compatibility path
- GLES vertex-texture compatibility work
- SDL2-shim support
- SDL2-shim external presenter path
- SDL2-shim native surface bridge
- PortMaster GX batching/diagnostics that remain relevant

Expected conflict files:

- `lib/aurora.cpp`
- `lib/gfx/common.cpp`
- `lib/gfx/common.hpp`
- `lib/gx/command_processor.cpp`
- `lib/gx/gx.cpp`
- `lib/gx/gx.hpp`
- `lib/gx/pipeline.cpp`
- `lib/gx/shader.cpp`
- `lib/gx/shader_info.cpp`
- `lib/webgpu/gpu.cpp`
- `lib/window.cpp`
- `lib/input.cpp`
- `cmake/AuroraSDL3Provider.cmake`
- `extern/CMakeLists.txt`

Stop condition:

- If upstream renderer changes invalidate the PortMaster surface path in a way
  that is not obvious, stop and document the blocker before continuing.

## Step 5: Reapply Build And Dawn/SDL Patches

Verify that the PortMaster focal/GCC10 build still applies the required patches.

Patch areas:

- Dawn GCC10 compatibility patches
- Dawn fbdev surface patch
- Dawn SDL2-shim borrowed/native surface patch
- SDL2-shim EGL property publishing
- SDL2-shim video-only / joystick split patch
- SDL2-shim nested video driver handling

Also check whether newer upstream Aurora/Dawn integration makes any local patch
obsolete or unsafe.

Stop condition:

- If a patch applies only with heavy fuzz in EGL/surface code, inspect the
  resulting generated Dawn source before building.

## Step 6: Build And Test

Build a package from the experiment branch only after top-level and Aurora
conflicts are resolved.

First test target:

- RG35XX H / muOS

Minimum test checklist:

- Launches from PortMaster/muOS menu
- Video appears
- Audio plays
- Controls work
- Dusklight menu can be opened
- Gameplay loads
- No immediate `EGL_BAD_SURFACE`, `eglSwapBuffers failed`, or device-lost loop
- Compare startup time and FPS against alpha10

If RG35XX H passes:

- Create an experimental tester zip.
- Mark ArkOS/Knulli/TrimUI as experimental.

If RG35XX H fails:

- Fix only obvious merge regressions.
- If not obvious, abandon the experiment branch and keep alpha10 as baseline.

## Expected Outcome

Best case:

- Upstream batching and Aurora renderer changes improve low-spec performance
  while preserving the current muOS PortMaster path.

Likely limitation:

- ArkOS/Knulli/TrimUI surface and CRTC failures may remain. Those appear to be
  firmware/display-stack issues rather than simple Dusklight renderer problems.

## Notes

Do not treat this as a release merge until the experiment branch is tested on
the RG35XX H / muOS baseline device.
