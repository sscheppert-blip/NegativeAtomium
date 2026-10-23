# Skin design notes

The whole look is drawn in code (`Source/LookAndFeel.*`, panel and patchbay in
`Source/PluginEditor.cpp`); there is no artwork to load. For design work, layered Photoshop
files can be generated from the current GUI:

```bash
cmake --build build --target NegativeAtomiumTests
./build/NegativeAtomiumTests_artefacts/Release/NegativeAtomiumTests --export-skin build/skin-export
python3 tools/make_psd.py build/skin-export design/psd      # needs: pip install psd-tools
```

| File | Content | Scale |
|---|---|---|
| `NegativeAtomium-GUI.psd` | whole editor, every element on its own layer | 4× |
| `Knob-Template.psd` | one knob, pointer at 12 o'clock | 8× (640 × 640 px) |
| `Switch-Template.psd` | key off / on, choice box | 4× |

## Layer structure

```
Background
Header/            Logo, Title + Subtitle
Sections/<NAME>/   Panel (frame), Title (name tabs)
    <KNOB>/        Scale, Position Marks, Knob, Pointer, Label
    <SWITCH>/      Off, On (one hidden)
    Choice <x>     the drop-down box
```

The patchbay, the LED selectors and the MODE/RANGE keys are not exported as separate layers.
All text is rasterised; retype it with your own font.

## Geometry the code relies on

- Layout size at 1×: **1624 × 588 px** (front panel in a 16 px frame). The window scales freely
  from **60 % to 250 %** with a locked aspect ratio, and everything scales with it.
- Knob rotation: **288°**, from 216° to 504° clockwise from 12 o'clock
  (≈ 7 o'clock → 5 o'clock). Bipolar knobs (TUNE, ENV DEPTH) have a longer centre mark.
- Knobs are 44–88 px (1×): 88 for TUNE and LFO SHAPE, 66 for FREQ, 60 for SHAPE/WIDTH/VOLUME,
  50–56 elsewhere.
- Rubber keys are 16 px square, LEDs 8 px, patch sockets 27 px (1×).
- Light comes from the top left and never turns with a knob; only the knob body and its pointer do.
