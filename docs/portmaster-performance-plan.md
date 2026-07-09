# Dusklight PortMaster Performance Plan And Result

This started as a review plan for the RG35XX H / muOS performance pass. The
draw-distance and actor-cull experiments have now been run far enough to make a
decision: stop chasing broad gameplay performance for this release line unless a
new renderer/backend path becomes available.

## Result Summary

The experiment did not find a low-risk optimization that can move heavy village
scenes into a substantially better performance class.

The most useful heavy-village profile with extra instrumentation showed:

```text
fps ~= 5.5 in the worst village windows
avg_total_ms ~= 80 ms
avg_submit_ms ~= 50 ms
avg_render_encode_ms ~= 7 ms
avg_fbdev_present_ms = 0
```

Interpretation:

- Presentation is no longer the bottleneck on the current SDL2-shim path.
- Pure actor CPU draw time is not the main bottleneck.
- The remaining cost is mostly GX submission/state churn plus Dawn/OpenGLES
  driver/GPU synchronization.
- Low-resolution rendering helps less than expected, so this is not primarily a
  pixel-fill problem.
- Broad object culling reduces some draw calls but does not produce a large
  enough gain to justify the visual damage as a default.

Current recommendation:

- Keep the existing release defaults.
- Keep safe pacing as the main playability layer.
- Keep extra profiling and culling flags disabled by default.
- Treat heavy-scene performance as a known limitation.
- Resume performance work only if we are willing to make a larger renderer/GX
  backend change, or if a very specific high-cost visual effect is identified.

## Goal

Raise heavy-scene performance above the current rough 10-12 FPS range on
RG35XX H / muOS-class hardware without breaking the working alpha11 graphics
path.

The current bottleneck is not the launcher, frontend, input, or framebuffer
presenter. Recent profiling points to draw submission, GX state churn, vertex
texture uploads, and heavy scene object density. Lowering render resolution
helps less than expected, which means pixel fill is not the only limiter.

## Ground Rules

- Do not work on ArkOS/Knulli surface compatibility until the R36S Plus arrives.
- Do not change launcher, controls, SDL2-shim surface ownership, or presenter
  code for this performance pass.
- Do not re-enable global draw skip as the main strategy. It caused blanking and
  transition bugs.
- All experiments must be behind `DUSKLIGHT_PORTMASTER_*` environment flags.
- Each experiment needs a before/after run over the same route.
- If a visual cut gives no clear FPS or frame-time gain, revert it or keep it
  disabled by default.

## Baseline Route And Metrics

Use one repeatable heavy route before making changes:

```text
1. Boot alpha11/current build on RG35XX H / muOS.
2. Load an Ordon/village save.
3. Run the same 2-3 minute loop through the village and first outdoor area.
4. Repeat once in a lighter/indoor area for contrast.
```

Enable:

```sh
DUSKLIGHT_PORTMASTER_GX_STATS=1
```

Capture:

```text
fps
avg_cpu_frame_ms
max_cpu_frame_ms
draws_last
uploads[v/i/s]
gx_draws[tex/cpu/native/storage]
gx_bytes[fifo/native]
gx_merge[try/ok/dirty/line/tex/idx]
gx_dirty[bp/xf/cp/arr/tex]
gx_prim[q/tri/strip/line/lstrip]
dl_opt[try/ok/pass/draw/batch_*]
end_frame timing: gfx_end / render_encode / submit / present
```

Add a short summary table to the handoff after each run:

```text
Build / flags / route / average FPS band / worst FPS band / visible problems
```

## Current Decision

This section records the decision that led to the final experiment.

For this experiment, Phase 1 and Phase 2 were skipped initially and Phase 3 was
tested directly. We later added the actor/process profilers because the draw
distance and broad cull results were not conclusive enough.

The first implementation should keep defaults unchanged:

```sh
DUSKLIGHT_PORTMASTER_DRAW_DISTANCE_SCALE=1.0
```

Manual test values:

```text
0.85 = conservative
0.70 = likely useful if draw distance matters
0.55 = aggressive proof point
```

## Phase 1: Instrument Actor Draw Pressure

Before culling more things, measure which game-level draw paths feed the GX
pressure.

Add low-frequency actor draw stats around:

```text
src/f_op/f_op_actor.cpp
  fopAc_Draw()

src/f_op/f_op_actor_mng.cpp
  fopAcM_cullingCheck()
```

Suggested env:

```sh
DUSKLIGHT_PORTMASTER_ACTOR_STATS=1
```

Counters:

```text
actor_draw[attempted/drawn/nodraw/status_culled/pm_distance_culled]
top actor proc names by draw attempts
top actor proc names by successful draw
top actor proc names by PortMaster distance cull
```

Keep this once per timing window, not per actor per frame. The goal is to learn
whether the expensive village work is mostly:

- many unique actors;
- a few repeated object classes;
- background/simple model systems;
- weather/particles;
- GX display-list expansion from specific draw paths.

Do not optimize blindly until this profile exists.

## Phase 2: A/B Existing Low-Spec Culls

Several draw toggles already exist. Test them individually and in combinations
instead of assuming the default set is optimal.

Existing or known flags:

```sh
DUSKLIGHT_PORTMASTER_DISABLE_GRASS_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SHADOW_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_WEATHER_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SIMPLE_MODEL_DRAW=1
DUSKLIGHT_PORTMASTER_DISABLE_SKYBOX_DRAW=1
```

Current package already defaults grass/shadow/weather off. The low-hanging A/B
tests are:

```text
A. current default
B. default + DISABLE_SIMPLE_MODEL_DRAW
C. default + DISABLE_SKYBOX_DRAW
D. default + simple model + skybox
E. default but re-enable one existing cull at a time to measure whether the
   fidelity loss is actually paying for itself
```

Expected outcome:

- If simple models dominate village scenery, `DISABLE_SIMPLE_MODEL_DRAW` may be
  a meaningful gain but visually harsh.
- If skybox is cheap, leave it alone.
- If grass/shadow/weather give little benefit in current routes, reconsider
  whether they should be default-off.

## Phase 3: Draw Distance Culling Experiment

This is the most promising low-hanging experiment, but it must be implemented
carefully.

Current draw gate:

```text
src/f_op/f_op_actor.cpp
  fopAc_Draw()
    skips draw if fopAcM_cullingCheck(actor) says culled
```

Current culling implementation:

```text
src/f_op/f_op_actor_mng.cpp
  fopAcM_cullingCheck()
    uses mDoLib_clipper
    supports actor cull boxes/spheres
    honors actor cullSizeFar when present
```

Proposed env:

```sh
DUSKLIGHT_PORTMASTER_DRAW_DISTANCE_SCALE=1.0
```

Test values:

```text
1.00 = off / baseline
0.85 = very conservative
0.70 = likely useful
0.55 = aggressive
0.40 = visual stress test only
```

Implementation shape:

```text
Only affect draw culling, not actor execution.
Do not delete actors.
Do not suppress collision, AI, events, audio, or save behavior.
Do not apply during cutscenes/events at first.
Do not apply when dComIfGp_event_runCheck() is true.
Clamp to sane range, for example 0.35-1.00.
Log the selected scale once.
```

Possible implementation options:

### Option A: Scale clipper far during actor draw cull

In `fopAcM_cullingCheck()`, temporarily call:

```text
mDoLib_clipper::changeFar(mDoLib_clipper::getFar() * scale)
```

around the existing `mDoLib_clipper::clip()` call, then reset.

Pros:

- General.
- Uses existing culling math.
- Easy to A/B.

Risks:

- May pop scenery/object actors abruptly.
- Actors with large cull boxes may still draw.
- Some important far actors may disappear.

### Option B: Apply only to selected actor classes

After actor stats identify high-volume draw classes, apply distance culling only
to selected non-critical proc names.

Pros:

- Safer visually.
- Less likely to break gameplay/cutscenes.

Risks:

- More whack-a-mole.
- Requires good actor stats first.

Recommendation:

Start with Option A as an opt-in A/B experiment, then use actor stats to decide
whether Option B is needed for a shippable default.

Final result:

- `DUSKLIGHT_PORTMASTER_DRAW_DISTANCE_SCALE=0.70` and `0.55` did not create a
  convincing FPS uplift in the heavy village route.
- Visual risk increases quickly as the scale gets aggressive.
- Keep the flag as an opt-in diagnostic/experiment only.
- Do not enable draw-distance culling by default.

## Phase 4: Village Clutter Cull Experiment

An opt-in draw-only cull was added:

```sh
DUSKLIGHT_PORTMASTER_CULL_VILLAGE_CLUTTER=1
```

It skips drawing selected low-importance actor classes while leaving execution,
collision, events, and save behavior alone. It is disabled during events.

Tested cull classes:

```text
item
Fish
Obj_Tbi / Obj_Yobikusa
Obj_Tie / Obj_OnCloth
341-1 / Obj_Laundry
Pumpkin
```

Observed in village:

```text
actor_stats_top[nodraw] Pumpkin, item, Obj_Tbi, Obj_Tie, 341-1
```

The flag worked mechanically. It did remove some object draws. It did not
materially change the worst-scene performance class. In heavy village windows,
FPS remained around 4.5-6 FPS and submit time often remained around 30-60 ms.

Decision:

- Keep disabled by default.
- Do not spend more time expanding broad actor culls for now.
- Only revisit if a user explicitly wants an ugly low-spec mode.

## Exclusive Draw Profiling Result

The inclusive process profiler initially reported large values for scene-wrapper
processes such as `EndCode(11)` and `12+0`. That was misleading because those
numbers included nested child draw work.

An exclusive-time variant was added under:

```sh
DUSKLIGHT_PORTMASTER_PROCESS_DRAW_STATS=1
```

Useful output:

```text
PortMaster process_draw_top[exclusive_usec]
```

Representative heavy-village result:

```text
EndCode(11): ~512 ms per sample window
item:        ~50 ms
Pumpkin:     ~46 ms
Fish:        ~42 ms
Bg:          ~28 ms
Obj_Tie:     ~25 ms
Link:        ~24 ms
kdoor:       ~24 ms
```

Interpretation:

- There is no single safe actor target large enough to rescue performance.
- Removing several visible object classes still does not recover enough time to
  turn the worst village scenes into stable 12-15 FPS gameplay.
- The largest remaining cost is below or around the scene/GX submission layer,
  not in a single easily removable game actor.

## Stop Criteria

Stop this optimization thread for now because:

- Presentation has already been fixed for the working muOS path.
- Triangle-strip batching and related GX work already delivered the major
  playability gain.
- Resolution scaling, draw-distance scaling, and broad actor culls have limited
  payoff.
- Further culling risks visual/gameplay damage without a clear performance
  upside.
- The remaining bottleneck points to deeper Dawn/OpenGLES/GX architecture work.

Reasonable future work, if performance is revisited:

- Safer adaptive pacing/quality to improve feel rather than raw FPS.
- More targeted GX state reduction if a specific redundant state source is
  identified.
- A direct GLES or non-Dawn renderer path, which is much larger than this pass.

Current implementation:

```text
src/f_op/f_op_actor_mng.cpp
  fopAcM_cullingCheck()
```

The implementation scales `mDoLib_clipper` far distance only during actor draw
culling. It does not delete actors or skip actor execution. It is disabled while
`dComIfGp_event_runCheck()` is true, so cutscenes/events keep original culling.
Values are clamped to `0.35` through `1.0`.

The launcher logs the active value:

```text
DUSKLIGHT_PORTMASTER_DRAW_DISTANCE_SCALE=<value>
```

Success criteria:

```text
At 0.70 scale:
  +2 FPS or better in village average, or materially lower submit/frame time
  no broken events
  no missing critical interactables nearby
```

Stop criteria:

```text
If 0.55 scale gives little gain, draw distance is not the dominant bottleneck.
Move to GX/cache work instead.
```

## Phase 4: Particle And Effect Pressure

Weather draw is already cut, but particles/effects may still produce GX churn.

Potential files to inspect after actor stats:

```text
src/d/d_particle.cpp
src/d/d_kankyo_rain.cpp
src/d/d_kyeff.cpp
src/d/d_kyeff2.cpp
```

Possible flags:

```sh
DUSKLIGHT_PORTMASTER_DISABLE_PARTICLE_DRAW=1
DUSKLIGHT_PORTMASTER_PARTICLE_DISTANCE_SCALE=0.7
DUSKLIGHT_PORTMASTER_PARTICLE_BUDGET=<count>
```

Do not start here unless profiling shows particle/effect draw paths are high.
Particle cuts can make the game look broken quickly, and may not help static
village geometry.

## Phase 5: GX Submission And Cache Work

If culling does not move FPS enough, return to the renderer-side counters.

Targets:

```text
gx_merge dirty reasons
gx_index cache hits/misses/bytes
gx_reuse candidates/hits/misses
dl_opt pass/draw/batch counts
top primitive/layout buckets
```

Likely work:

- Improve display-list reuse when the same static geometry emits identical GX
  command streams across frames.
- Expand the current display-list optimization to more safe primitive/layout
  cases.
- Reduce redundant vertex-texture uploads for static geometry.
- Improve quad/strip merging only when counters show a clear blocker.

Current focused probe:

```sh
DUSKLIGHT_PORTMASTER_UNBATCHED_REUSE_PROBE=1
```

This hashes raw unbatched texture-vertex payloads larger than 4 KB and reports
the opportunity through the existing `gx_reuse[...]` counters. It does not
change rendering. This matters because the current persistent reuse cache only
runs through the strip/quad batch paths; logs with `gx_batch[...]` and
`gx_reuse[...]` at zero do not prove raw unbatched geometry has no cross-frame
reuse.

Decision rule:

- If `gx_reuse[cand/hit/cross/avoid]` stays low, stop the raw GX cache route.
- If cross-frame hits and avoidable bytes are high, add a guarded raw
  texture-vertex cache for the same candidates.

Another narrow A/B flag exists for unmergeable line/point primitives:

```sh
DUSKLIGHT_PORTMASTER_SKIP_LINE_PRIMS=1
```

This skips `GX_LINES`, `GX_LINESTRIP`, and `GX_POINTS`. Use it only as a
performance/fidelity experiment. Heavy village logs show line strips are a
steady merge blocker, but skipping them may remove weather, effect, or debug-like
visual elements. Do not make it a default unless the FPS gain is material and
the visible loss is acceptable.

Observed result on RG35XX H / muOS village route:

- The flag worked mechanically: `gx_merge line=0`.
- `gx_prim[lstrip]` still appears in logs because primitive accounting happens
  before the skip.
- Steady submit time remained roughly in the same band, commonly around
  `29-39 ms`.
- FPS did not improve enough to justify making this a default low-spec cut.

Conclusion: line/point primitive skipping is not a meaningful next optimization
path unless a specific scene is proven to be dominated by these primitives.

Avoid:

- Broad native vertex expansion. It previously caused corruption.
- Random TEV/BP/XF changes without counters.
- Reworking the Dawn/presenter path during this performance pass.

## Phase 6: Perceived Smoothness

This does not increase raw FPS, but can make 12-18 FPS less unpleasant.

Keep:

```sh
DUSKLIGHT_PORTMASTER_SAFE_PACING_FPS=30
DUSKLIGHT_PORTMASTER_SAFE_PACING_MAX_TICKS=4
```

Do not unguard it globally yet. Previous catch-up during transitions caused
stuck fades and transition bugs.

Potential future work:

- More precise transition/event guard detection.
- Resume pacing earlier after transition end.
- Add a short catch-up cap after loading screens only if fade state is stable.

This should be treated as polish after raw draw pressure is reduced.

## Proposed Execution Order

1. Implement opt-in draw-distance scale.
2. Test scale values `1.0`, `0.85`, `0.70`, `0.55` over the same route.
3. If draw-distance helps, refine it into a safer targeted/default mode.
4. If draw-distance does not help, profile particle/effect pressure.
5. If culling does not materially improve FPS, return to display-list/GX cache
   reuse work.

## Benchmark Replay Follow-Up

Automated performance runs are possible, but should be a separate patch from
draw-distance culling.

The safer design is to record and replay Dusklight's mapped controller state
after SDL/controller mapping, not raw evdev events. A manual route would record
`interface_of_controller_pad` samples from `mDoCPd_c::read()`, and later replay
the same samples before the game consumes input.

Suggested env:

```sh
DUSKLIGHT_PORTMASTER_PAD_RECORD=/path/to/route.padlog
DUSKLIGHT_PORTMASTER_PAD_REPLAY=/path/to/route.padlog
```

Benefits:

- firmware/device button numbering does not matter;
- one manual village route can be reused for A/B tests;
- benchmark logs become comparable across scale values.

Risks:

- replay must start from the same save/menu state;
- safe-pacing changes can alter how many input samples are consumed;
- file I/O during recording must be buffered enough not to distort timing.

Recommendation:

Only add this after the first draw-distance run proves the experiment is worth
repeating several times.

## Expected Result

Realistic target:

```text
Heavy village: move from around 10-12 FPS toward 14-18 FPS
Lighter areas: stay near or at the 30 FPS pacing cap more often
```

A stable 30 FPS in the heaviest outdoor scenes is unlikely on this hardware
without much deeper renderer changes or much harsher visual cuts.

## Review Questions

Before implementation, decide:

1. Should draw-distance scale be tested as a global draw-cull first, or only
   after actor stats identify safe classes?
2. How much object pop-in is acceptable for a PortMaster low-spec mode?
3. Should simple model and skybox culls be exposed in the launcher as tester
   toggles?
4. Should the default target prioritize higher FPS or visual stability?
