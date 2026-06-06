# Dusklight PortMaster Controls

These controls describe the current PortMaster test package.

## Handheld Controls

| Handheld input | Dusklight / game action |
| --- | --- |
| D-pad | D-pad / menu navigation |
| Left stick | Movement |
| Right stick | Camera / C-stick |
| A | Action / confirm |
| B | Cancel / back |
| X | X button |
| Y | Y button |
| L1 or L2 | L trigger |
| R1 or R2 | R trigger |
| Start | Start / pause |
| Back / Select | Z trigger |
| Super / Home / Guide | Dusklight menu, when exposed by SDL |
| Start + R | Dusklight menu UI chord |

Use Super / Home / Guide to open the Dusklight menu, then quit from there. If
the firmware does not expose a real guide/menu button, use Start + R, which is
handled by Dusklight's existing UI input path.

## SDL Controller Mapping

The PortMaster package relies on SDL gamepad input for gameplay. The launcher
uses PortMaster's `sdl_controllerconfig` for normal devices. If the firmware
reports `muOS-Keys`, the launcher intentionally prefers the bundled
`muOS-Keys` entry in `res/gamecontrollerdb.txt`, because the system DB observed
on muOS mapped several buttons incorrectly.

The current control diagnostic build uses this probed `muOS-Keys` mapping:

```text
a:b0,b:b1,x:b3,y:b2,
leftshoulder:b4,rightshoulder:b5,
lefttrigger:b10,righttrigger:b11,
guide:b8,start:b7,back:b6,
leftstick:b9,rightstick:b12
```

This is the tractable control path: fix the SDL controller database entry first,
then let Dusklight's normal SDL/PAD mapping consume the corrected gamepad
buttons and axes. Do not add more game-specific button swaps unless the SDL
identity is proven correct and Dusklight still maps it incorrectly.

For diagnostics, launch with `DUSKLIGHT_PORTMASTER_INPUT_DIAG=1`. Button presses
should produce log lines in `/mnt/mmc/ports/dusklight/log.txt` like:

```text
PortMaster input: sdl_button=A(0) port=0 pad=A(0x0100)
```

Use those lines to correct `res/gamecontrollerdb.txt` if a physical button still
lands on the wrong SDL name.

## gptokeyb

The current package does not start `gptokeyb` by default. Dusklight is already
using native SDL gamepad input, and running a keyboard mapper on top can scramble
controls on muOS.

The packaged `dusklight.gptk` is intentionally inert. It exists so an accidental
or future `gptokeyb` launch does not synthesize a second control scheme on top
of SDL.

## Notes

If the right stick does nothing, first confirm whether the firmware exposes a
right stick axis through SDL for the active controller. The current package no
longer assumes `gptokeyb` is available or safe to layer over native input.
