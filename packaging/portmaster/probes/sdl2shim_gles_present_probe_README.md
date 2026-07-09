# SDL2 Shim GLES Present Probe

This probe isolates the PortMaster/bmdhacks SDL2-backend presentation path from
Dusklight and Dawn.

It uses the bundled SDL3 shim, selects the nested SDL2 backend with
`SDL_VIDEODRIVER=sdl2`, creates an OpenGL ES 2 context, clears the window,
draws a colored triangle and a textured quad, calls `SDL_GL_SwapWindow`, then
exits.

It can also load `librenderdoc.so` and request a capture after a warmup frame.
This isolates RenderDoc from Dusklight and Dawn: if this probe cannot produce
an `.rdc`, RenderDoc is probably not viable on that firmware/SDL2-backend
combination.

Expected result:

- If the screen shows changing solid colors, the firmware can present through
  bmdhacks SDL2-backend using a normal SDL GLES window.
- If the log shows context/window creation errors, the SDL2-backend path is not
  available on that firmware.
- If the log succeeds but the screen remains blank, SDL can create a GLES
  context but presentation is not reaching the display on that firmware.
- If RenderDoc is enabled and `captures/*.rdc` appears, RenderDoc can capture a
  plain SDL2-backend GLES app on this device.
- If RenderDoc is enabled but no `.rdc` appears, check the log for
  `renderdoc ready`, `TriggerCapture`, or `EndFrameCapture`.

Run from a device shell:

```sh
cd /userdata/roms/ports/sdl2shim-gles-present-probe
./run-sdl2shim-gles-present-probe.sh
```

On muOS the path is usually:

```sh
cd /mnt/mmc/ports/sdl2shim-gles-present-probe
./run-sdl2shim-gles-present-probe.sh
```

The probe exits by itself after roughly five seconds. Pressing any keyboard,
gamepad, or joystick button also exits.

## RenderDoc tests

The wrapper disables RenderDoc by default:

```sh
./run-sdl2shim-gles-present-probe.sh
```

To try RenderDoc trigger mode:

```sh
PROBE_RENDERDOC_CAPTURE_AFTER=60 ./run-sdl2shim-gles-present-probe.sh 180
ls -l captures
```

To try explicit start/end mode:

```sh
PROBE_RENDERDOC_CAPTURE_AFTER=60 PROBE_RENDERDOC_MODE=startend ./run-sdl2shim-gles-present-probe.sh 180
ls -l captures
```

To test early RenderDoc hooking with `LD_PRELOAD`:

```sh
LD_PRELOAD="$PWD/renderdoc/lib/librenderdoc.so" PROBE_RENDERDOC_CAPTURE_AFTER=60 ./run-sdl2shim-gles-present-probe.sh 180
ls -l captures
```

If `renderdoc/bin/renderdoccmd` is bundled, this should print the RenderDoc
version:

```sh
LD_LIBRARY_PATH="$PWD/renderdoc/lib:$PWD" ./renderdoc/bin/renderdoccmd version
```
