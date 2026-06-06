# Dusklight PortMaster / RG35XX H — Current Handoff

## Purpose

Continue the Dusklight RG35XX H / muOS PortMaster experiment from the current working renderer state.

The port has moved past launch, build, ROM loading, Dawn initialization, and first pixels. The active bottleneck is now the Aurora GX vertex-fetch model on a Mali OpenGL ES backend that does not support vertex-stage storage buffers.

## Current verified state

Device / environment:

```text
Device: RG35XX H
OS: muOS / PortMaster
SoC: Allwinner H700
GPU: Mali-G31
Display: 640x480
RAM: 1 GB class
Graphics path currently used: Dawn OpenGLES compatibility mode + offscreen render + fbdev presenter
```

Verified working:

```text
Linux ARM64 Dusklight build: works
Source-built Dawn OpenGLES device creation: works
Dawn selects Mali-G31: works
RVZ loads: works
Dusklight game loop starts: works
Dawn platform swapchain: bypassed
fbdev presenter: works enough for visible output
Prelaunch/UI first pixels: works
Opening scene renders coherent geometry: works
Animation advances: works, but very slowly
```

Known current launcher/runtime shape:

```text
DUSKLIGHT_PORTMASTER_X11_DAWN=1
DUSKLIGHT_PORTMASTER_NO_SURFACE=1
DUSKLIGHT_PORTMASTER_SKIP_COMPAT_SURFACE=1
DUSKLIGHT_PORTMASTER_FBDEV_PRESENT=1
SDL_VIDEODRIVER=offscreen
--backend opengles
--cvar backend.graphicsBackend=opengles
--cvar backend.skipPreLaunchUI=true
```

Useful paths:

```text
Launcher:
  /mnt/mmc/roms/ports/dusklight.sh

Canonical payload:
  /mnt/mmc/ports/dusklight

Binary:
  /mnt/mmc/ports/dusklight/dusklight.aarch64

Runtime log:
  /mnt/mmc/ports/dusklight/log.txt
```

Do not rely only on `/roms/ports/dusklight.sh`; on muOS the menu launcher belongs under `/mnt/mmc/roms/ports`, and PortMaster resolves payload under `/mnt/mmc/ports/dusklight`.

## Current visual status

Current build produces recognizable Twilight Princess scene output:

```text
castle / gate / terrain / sky / horse/rider / lighting are coherent
frames update over time
no longer just random exploded triangles
not meaningfully playable
visible framebuffer update / tearing / scanline-like presentation artifacts
```

The visible update is likely a combination of:

```text
1. very slow frame generation from CPU-side GX vertex expansion
2. crude fbdev presentation with unsynchronized framebuffer writes
```

Do not treat the presenter as solved long term, but do not prioritize it yet. Fix the vertex storage problem first.

## Main current blocker

Aurora’s normal GX shader path uses WebGPU storage buffers in the vertex shader:

```wgsl
@group(0) @binding(0)
var<storage, read> vbuf: array<u32>;

@group(0) @binding(1)
var<storage, read> abuf: array<u32>;
```

The generated vertex shader manually fetches GameCube vertex data:

```wgsl
let in_pos = fetch_f32_3(&vbuf, ubuf.vtx_start + vidx * stride + offset, false);
let in_tex0_uv = vec2f(
  fetch_s16_1(&vbuf, ..., frac, false),
  fetch_s16_1(&vbuf, ..., frac, false)
);
```

On the RG35XX H Mali GLES stack:

```text
GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0
```

So any generated pipeline that reaches the original storage-buffer path fails with:

```text
The number of vertex shader storage blocks (1) is greater than the maximum number allowed (0).
```

The current CPU expansion fallback avoids this by decoding GX vertex/index data on CPU and uploading ordinary native vertex attributes. This now renders coherently, but it is too slow.

## Why CPU expansion is probably not enough

Current fallback does:

```text
for many GX draws:
  read GX descriptors/VAT
  decode direct/index8/index16 attrs
  endian-convert
  expand into interleaved native vertex buffer
  upload expanded vertex data
  build/use native vertex pipeline
  draw
```

This is correct enough, but on H700/Mali-G31 it is probably the wrong main path. It moves what Aurora intended to be GPU-side vertex fetch into repeated CPU work.

Expected CPU-only optimization ceiling:

```text
If current is ~1 FPS or less, even a 3x-5x CPU optimization may still be too slow.
```

CPU expansion should remain as a fallback/correctness path, not the main performance strategy.

## Presenter problem — note, but deprioritize

Current presentation path is roughly:

```text
Aurora GX
  -> Dawn/OpenGLES offscreen render target
  -> CPU readback/copy
  -> write/blit to /dev/fb0
  -> LCD scanout
```

This causes likely issues:

```text
GPU/CPU sync on readback
full-frame CPU copy
single-buffer fbdev writes
no vsync/page flip
visible tearing or progressive scanline-like updates
```

But the presenter is good enough for correctness testing. Do not spend more time on Weston/X11/Wayland launcher iteration right now.

Measure later:

```text
frame_total_ms
gx_expand_ms
vertex_upload_ms
webgpu_submit_ms
gpu_readback_ms
fbdev_write_ms
```

If fbdev readback/write dominates after vertex work improves, then revisit presenter.

## Key discussion conclusion from today

Focus on replacing vertex-stage storage-buffer fetches with a texture-backed vertex fetch path.

Desired replacement:

```text
Current failing model:
  GX FIFO / attribute arrays
    -> vbuf / abuf storage buffers
    -> vertex shader storage-buffer fetch/decode

Proposed GLES-compatible model:
  GX FIFO / attribute arrays
    -> vbuf_tex / abuf_tex integer textures
    -> vertex shader textureLoad() fetch/decode
```

This avoids CPU expansion for most draws if complete.

This is a hack, but it is a reasonable GPU compatibility hack: use texture memory as a read-only data store because vertex-stage storage buffers are unavailable.

Do not think of this as “store calculated expanded vertices in a texture.” Better mental model:

```text
store the original compact GX vertex/index stream in a texture;
use the vertex shader to read and decode it.
```

## Why texture-backed fetch is plausible in Aurora

Aurora’s shader generation already has a centralized fetch abstraction:

```text
load_word()
load_u8()
load_u16()
load_f32()
fetch_u8/fetch_s16/fetch_f32/etc.
attr_load()
```

Currently these helpers take:

```wgsl
ptr<storage, array<u32>>
```

The texture-fetch path should replace the underlying load helpers with `textureLoad()` while leaving most TEV/material/lighting/fog shader logic intact.

Target WGSL direction:

```wgsl
@group(0) @binding(0)
var vbuf_tex: texture_2d<u32>;

@group(0) @binding(1)
var abuf_tex: texture_2d<u32>;

fn load_vbuf_word(word_idx: u32) -> u32 {
  let x = word_idx & 255u;
  let y = word_idx >> 8u;
  return textureLoad(vbuf_tex, vec2u(x, y), 0).r;
}

fn load_abuf_word(word_idx: u32) -> u32 {
  let x = word_idx & 255u;
  let y = word_idx >> 8u;
  return textureLoad(abuf_tex, vec2u(x, y), 0).r;
}
```

Then implement byte/halfword/f32 loads on top of those word loads.

Prefer `texture_2d<u32>` over OpenGL `GL_TEXTURE_BUFFER` initially, because Aurora is going through WGSL/WebGPU/Dawn. A 2D integer texture is more likely to survive the Dawn OpenGLES path than trying to force an OpenGL texture-buffer abstraction through WebGPU.

## Important: CPU expansion is not needed when texture fetch handles the draw

Routing goal:

```text
simple/common cheap layouts:
  native vertex attributes if already efficient

general/complex indexed GX layouts:
  texture-backed vertex fetch

fallback only:
  CPU expansion
```

If texture-backed vertex fetch handles a draw, CPU expansion should not run for that draw.

## First required experiment

Before wiring into all of Aurora, build a tiny Dawn/OpenGLES probe using the same source-built Dawn configuration.

Probe goal:

```text
Can Dawn/OpenGLES on Mali-G31 run a vertex shader that does textureLoad() from texture_2d<u32>?
```

Minimum probe:

```text
1. Create Dawn OpenGLES compatibility device.
2. Create a 2D R32Uint texture or RGBA8Uint/RGBA32Uint texture with TextureBinding usage.
3. Bind it with ShaderStage::Vertex visibility.
4. WGSL vertex shader calls textureLoad() to fetch packed vertex data.
5. Draw a triangle or quad.
6. Present through current fbdev path or any existing diagnostic readback.
```

Probe should test:

```text
texture_2d<u32> in vertex stage
textureLoad() in vertex stage
integer texture format support through Dawn compatibility mode
correct returned values
basic performance versus CPU-expanded native vertex path
```

If this fails, do not integrate the path into Dusklight yet.

## If the probe succeeds: Aurora implementation target

Add a PortMaster/GLES no-SSBO vertex fetch mode.

High-level patch plan:

```text
1. Add a shader-generation mode flag:
   nativeVertexFetch / textureVertexFetch / storageVertexFetch

2. In textureVertexFetch mode:
   - do not emit var<storage>
   - do not emit ptr<storage>
   - emit texture_2d<u32> vbuf_tex / abuf_tex instead
   - replace load_word/load_u8/load_u16/load_f32 helpers with texture-backed variants

3. Keep ubuf uniform group as-is.

4. Keep normal TEV texture group separate.

5. Create/bind vbuf_tex and abuf_tex for the active draw/frame.

6. Upload compact raw GX data into those textures instead of expanding to native vertex buffers.

7. Keep CPU expansion path as fallback for unsupported cases.
```

Binding layout note:

```text
Normal path:
  group 0 binding 0: storage vbuf
  group 0 binding 1: storage abuf

Texture-fetch path:
  group 0 binding 0: texture_2d<u32> vbuf_tex
  group 0 binding 1: texture_2d<u32> abuf_tex
```

The bind group layout must use `ShaderStage::Vertex` visibility for these vertex-data textures.

## Data packing suggestion

Initial simple packing:

```text
one u32 word per texel
texture format: R32Uint if supported
texture width: fixed power of two, e.g. 256 or 1024 words
word_idx -> x/y:
  x = word_idx & (width - 1)
  y = word_idx >> log2(width)
```

Use separate textures:

```text
vbuf_tex: raw FIFO vertex stream packed as u32 words
abuf_tex: raw attribute arrays packed as u32 words
```

Keep byte addressing exactly like existing storage-buffer code:

```text
byte_off -> word_idx = byte_off >> 2
sub = byte_off & 3
load_u8/load_u16/load_u32_raw handle unaligned byte extraction
```

This should preserve existing endianness handling.

## Performance expectations

Expected speed ranking:

```text
Best:
  native vertex attributes for simple layouts

Good if available:
  original storage-buffer vertex fetch

Potentially acceptable:
  texture-backed vertex fetch

Bad:
  generic CPU expansion every draw

Worst:
  CPU expansion + readback fbdev presenter + debug logging
```

Texture-backed fetch is not guaranteed fast. It can be slow because vertex-stage integer texture loads have random/indexed access and multiple dependent reads. But it is structurally better than CPU-expanding every draw.

## Dolphin comparison

Dolphin mostly uses a CPU vertex-loader architecture: it translates GX vertex data into native vertex buffers using compiled/specialized vertex loaders, then draws with native host vertex attributes.

That proves CPU vertex loading can be a valid architecture, but Dolphin is built around it and optimized over many years. Aurora’s CPU fallback is currently a workaround, not a mature Dolphin-style vertex manager.

Lesson:

```text
Dolphin-style CPU loading is a fallback route only if heavily optimized and cached.
For RG35XX H, texture-backed GPU fetch is the more promising next experiment.
```

## Current stop/go rules

Proceed if:

```text
Dawn texture_2d<u32> vertex-stage probe works.
Texture fetch removes most CPU expansion.
Frame rate improves materially.
```

Stop or retarget hardware if:

```text
textureLoad() in vertex shader fails on Dawn/OpenGLES/Mali-G31
integer texture binding fails in compatibility mode
performance improves less than ~2x
fbdev readback/presenter dominates even after vertex path improves
```

If texture fetch gives a 3x-5x improvement, continue. If it gives little, RG35XX H probably remains a technical demo target rather than a playable PortMaster release.

## Immediate agent task list

### Task 1 — Tiny Dawn texture vertex-fetch probe

Build and run on RG35XX H:

```text
- Dawn OpenGLES compatibility device
- texture_2d<u32> bound in vertex stage
- WGSL textureLoad() fetches positions
- draw one visible triangle/quad
```

Report:

```text
success/failure
shader compile errors
Dawn validation errors
runtime GL errors if visible
approx frame time if looped
```

### Task 2 — If probe works, add Aurora shader mode

Implement a PortMaster-only texture-backed vertex-fetch mode:

```text
- no var<storage>
- no ptr<storage>
- emit vbuf_tex / abuf_tex
- implement load_vbuf_word/load_abuf_word
- adapt existing fetch helpers
```

### Task 3 — Minimal integration

Start with one known layout currently handled by CPU expansion:

```text
FIFO stride 16:
  POS f32x3 at offset 0
  TEX0 s16x2 frac8 at offset 12
```

Then generalize once correctness is proven.

### Task 4 — Timing logs

Add low-frequency timing, not per-draw spam:

```text
frame_total_ms
gx_expand_ms / texture_fetch_path_draw_count
vertex_upload_ms
webgpu_submit_ms
gpu_readback_ms
fbdev_write_ms
draw_count
expanded_bytes
```

No `DUSKLIGHT_PORTMASTER_GX_DEBUG=1` in menu launcher by default.

## Things not to spend time on right now

```text
More Weston/X11/Wayland launcher experiments
More one-off CPU expansion layout patches unless needed for correctness fallback
More visual effects disabling
Presenter polish before vertex path timing is understood
Full GLES renderer rewrite
```

## Bottom line

The current port is no longer blocked by launch, ROM, Dawn device creation, or first pixels. It renders coherent Twilight Princess frames, but the CPU-side GX vertex expansion path is too slow.

The next serious chance is to replace Aurora’s failing vertex-stage storage-buffer fetch with a texture-backed vertex fetch path using `texture_2d<u32>` and `textureLoad()` in the vertex shader. CPU expansion should remain only as fallback/correctness support.
