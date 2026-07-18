# Dusklight PortMaster Controls

Dusklight receives gameplay input through SDL's gamepad API. The PortMaster
launcher supplies the firmware controller mapping, except that a bundled
`muOS-Keys` mapping is preferred when that device is present.

| Handheld input | Game action |
| --- | --- |
| D-pad | D-pad and menu navigation |
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
| Super / Home / Guide | Dusklight menu when exposed by the firmware |
| Start + R | Dusklight menu chord |

Use the Dusklight menu to exit normally. The launcher also starts PortMaster's
`gptokeyb2` or `gptokeyb` when available so the firmware's emergency quit
hotkey remains active. `dusklight.gptk` deliberately maps no gameplay keys;
gameplay stays on the SDL path instead of receiving duplicate keyboard input.

The bundled `muOS-Keys` mapping is stored in `res/gamecontrollerdb.txt`. Fix a
wrong physical layout there before adding game-specific button swaps.
