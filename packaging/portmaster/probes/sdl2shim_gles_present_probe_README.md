# SDL2 Shim GLES Present Probe

This probe isolates the PortMaster/bmdhacks SDL2-backend presentation path from
Dusklight and Dawn.

It uses the bundled SDL3 shim, selects the nested SDL2 backend with
`SDL_VIDEODRIVER=sdl2`, creates an OpenGL ES 2 context, clears the window with
changing colors, calls `SDL_GL_SwapWindow`, then exits.

Expected result:

- If the screen shows changing solid colors, the firmware can present through
  bmdhacks SDL2-backend using a normal SDL GLES window.
- If the log shows context/window creation errors, the SDL2-backend path is not
  available on that firmware.
- If the log succeeds but the screen remains blank, SDL can create a GLES
  context but presentation is not reaching the display on that firmware.

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
