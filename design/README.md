# Skin design notes

The layered Photoshop files are generated from the current GUI:

```bash
cmake --build build --target NegativeAtomiumTests
./build/NegativeAtomiumTests_artefacts/Release/NegativeAtomiumTests --export-skin build/skin-export
python3 tools/make_psd.py build/skin-export design/psd      # needs: pip install psd-tools
```

| File | Content | Scale |
|---|---|---|
| `NegativeAtomium-GUI.psd` | whole editor, every element on its own layer | 4× (5056 × 1936 px) |
| `Knob-Template.psd` | one knob, pointer at 12 o'clock | 8× (640 × 640 px) |
| `Switch-Template.psd` | toggle off / on, choice box | 4× |

## Layer structure

```
Background
Header/            Logo, Title + Subtitle
Sections/<NAME>/   Panel, Title
    <KNOB>/        Track, Value Arc, Cap, Pointer, Label
    <SWITCH>/      Off, On (one hidden)
    Choice <x>     the drop-down box
Reference (flattened, hidden)
```

All text is rasterised; retype it with your own font.

## Geometry the code relies on

- Layout size at 1×: **1264 × 484 px**. The window scales freely from **60 % to 250 %**
  with a locked aspect ratio, and everything scales with it.
- 250 % on a Retina display means **5×** the 1× pixels. Bitmaps therefore have to be
  delivered at 4–5× (they are only ever scaled down), or better as vectors (SVG).
- Knob rotation: **288°**, from 216° to 504° clockwise from 12 o'clock
  (≈ 7 o'clock → 5 o'clock). Bipolar knobs (TUNE, ENV DEPTH) start their value arc at 12 o'clock.
- The knob size is up to 76 px (1×), centred in its cell.
- Toggles and choice boxes are 24 px high (1×).

## What to deliver for the skin

Vectors stay sharp at every size; bitmaps only down to the size they were made for.

1. **Background**: panels, section titles and knob labels, as SVG or a 5× PNG
   (6320 × 2420 px).
2. **Knob cap**: SVG or a transparent PNG at ≥ 400 px with the pointer at 12 o'clock. The code rotates it.
   (A filmstrip with all positions works too, if the cap has lighting that must not rotate.)
3. **Toggle**: off and on states (PNG), or just colours if they stay vector-drawn.
4. **Colours** for the value arc and track (hex), plus your font if text should stay live.
