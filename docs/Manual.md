# NegativeAtomium – Manual

*[Deutsche Fassung](Handbuch.md)*

![The NegativeAtomium front panel](images/panel.png)

NegativeAtomium is a semi-modular, paraphonic analog-style synthesizer plugin (VST3, CLAP, AU on
the Mac) and standalone app. It plays without a single cable: all modules are connected
internally into a classic signal path. The patchbay on the right lets you break that path at
32 inputs and rewire it.

---

## Contents

1. [Installation](#1-installation)
2. [Operation](#2-operation)
3. [The signal path](#3-the-signal-path)
4. [The sections](#4-the-sections)
5. [Patchbay](#5-patchbay)
6. [Settings](#6-settings)
7. [MIDI](#7-midi)
8. [Presets](#8-presets)
9. [Tips and troubleshooting](#9-tips-and-troubleshooting)

---

## 1. Installation

NegativeAtomium comes as **AU** (macOS only), **VST3** and **CLAP** for macOS, Windows and Linux,
plus a **Standalone** app that runs without a DAW. Ready-made packages are on the
[releases page](https://github.com/sscheppert-blip/NegativeAtomium/releases).

| System | Folder |
|---|---|
| macOS AU | `~/Library/Audio/Plug-Ins/Components/` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |
| Windows VST3 | `C:\Program Files\Common Files\VST3\` |
| Windows CLAP | `C:\Program Files\Common Files\CLAP\` |
| Linux VST3 / CLAP | `~/.vst3/` / `~/.clap/` |

Then let your DAW rescan its plugins. NegativeAtomium shows up as an instrument by the manufacturer
**NegativeAtomium**. When updating, delete the old plugin (e.g. the `NegativeAtomium.vst3` folder)
first.

**Standalone app:** unpack and start it from anywhere – on macOS `NegativeAtomium.app` (e.g. into
Applications), on Windows `NegativeAtomium.exe`, on Linux the `NegativeAtomium` program. Pick your
audio and MIDI devices under **Options → Audio/MIDI Settings**.

### Unblocking plugins on macOS

Every file you download gets a hidden "quarantine" mark from macOS. For plugins that are not
notarized by Apple – like NegativeAtomium and many free or open-source plugins – this can mean:

- a message that the plugin "cannot be opened" or "is damaged",
- or your DAW silently skips it during the scan.

**The general way – works for any plugin:** remove the mark once in the **Terminal**
(Applications → Utilities → Terminal), then restart your DAW:

```bash
xattr -dr com.apple.quarantine "/path/to/the/plugin"
```

Tip: type `xattr -dr com.apple.quarantine ` (with the space at the end), drag the plugin file from the
Finder into the Terminal window – the path is filled in – and press Return. `-d` deletes the mark,
`-r` includes everything inside the plugin bundle. `xattr -l "/path/to/the/plugin"` shows whether a
mark is still there.

**For NegativeAtomium – all formats at once:**

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/NegativeAtomium.component ~/Library/Audio/Plug-Ins/VST3/NegativeAtomium.vst3 ~/Library/Audio/Plug-Ins/CLAP/NegativeAtomium.clap /Applications/NegativeAtomium.app
```

If you only installed some of the formats, the command reports the missing ones – that's fine, the
others are unblocked anyway.

Only do this for plugins from sources you trust: the mark is macOS's way of asking "are you sure?".

---

## 2. Operation

- **Knobs:** drag up/down or left/right. The value shows while dragging. **Double-click** resets
  a knob to its default.
- **Keys** (e.g. BLEND, KEY TRK): click to switch on and off; a key lights up while on.
- **MODE and RANGE:** every press steps on to the next setting; the LEDs next to them show the
  current one. You can also click the LEDs directly.
- **Window size:** drag the bottom-right corner (60–250 %). The size is saved with the project.
- **Header:** PRESET loads a saved sound, SAVE stores the current sound, SETTINGS opens the
  advanced options. Clicking the logo shows the version and the preset folder.

Frames drawn with a **white line and a white name tab** belong to the audio path, frames with a
grey line to modulation.

---

## 3. The signal path

Without cables, the signal flows like this:

```
OSC 1 ─┐
       ├─ OSC MIX ─┐
OSC 2 ─┘           ├─► VCF ─► OVERDRIVE ─► VCA ─► DELAY ─► OUTPUT
NOISE ─────────────┘    ▲  ▲                ▲  ▲
                 LFO ───┘  └─ ENV 2   ENV 1 ┘  └─ VCA BIAS
```

- Envelope 1 controls the volume (VCA), envelope 2 the filter.
- The LFO reaches the filter through MOD DEPTH and the pulse width of both oscillators through
  ATTENUATOR 2.
- The whole sound engine runs at twice the sample rate.

---

## 4. The sections

### OSC 1 / OSC 2

| Control | Function |
|---|---|
| **TUNE** | detune: ±1 octave, ±10 octaves in FULL |
| **OSC MIX** | balance of the two oscillators: left only OSC 1, right only OSC 2 |
| **SHAPE** | waveform: sine → triangle → saw → square → tone mod. The current shape is shown under the knob |
| **BLEND** | on: SHAPE morphs smoothly between shapes. Off: SHAPE steps between the five shapes (dots on the scale) |
| **OCTAVE / RANGE** | footage 8', 16', 32' or FULL (TUNE spans ±10 octaves). RANGE steps through them, the LEDs show the footage |
| **WIDTH** | pulse width 1–99 %. Affects square and tone mod |
| **OSC SYNC** | hard-syncs OSC 2 to OSC 1 – tune OSC 2 with TUNE for classic sync sounds |
| **PARAPHONIC** | two held keys are spread across the two oscillators (shared filter and VCA) |

*Tone mod* is a saw whose ramp is split by the pulse – WIDTH moves a notch through the harmonic
spectrum.

### VCF (filter)

12 dB multimode filter that self-oscillates.

| Control | Function |
|---|---|
| **MODE** | steps low-pass (LP) → band-pass (BP) → high-pass (HP) |
| **FREQ** | cutoff 10 Hz – 15 kHz |
| **RESO** | resonance; turned far up, the filter whistles on its own |
| **KEY TRK** | the cutoff follows the played note |
| **MOD DEPTH** | how far the LFO (or a cable in FREQ MOD) moves the filter |
| **ENV DEPTH** | how far envelope 2 opens (right) or closes (left) the filter |

The patchbay output **VCF 2** delivers the complementary response at the same time: high-pass
in LP mode, low-pass in BP, band-pass in HP.

### LFO

| Control | Function |
|---|---|
| **RATE** | speed 0.01 Hz – 10 kHz. With SYNC: note value 4/1 … 1/64 |
| **SHAPE** | sine → triangle → saw → square → falling saw |
| **KEY SYNC** | the LFO restarts with every note |
| **SYNC** | the LFO follows the DAW tempo; RATE picks the note value and the phase follows the bar |
| **BLEND** | on: SHAPE morphs smoothly; off: SHAPE steps between shapes |

The order of the shapes and the start phase per knob position can be changed in SETTINGS.

### NOISE / VCA BIAS

| Control | Function |
|---|---|
| **NOISE** | level of the white noise in the mixer |
| **VCA BIAS** | opens the VCA regardless of the envelope – for drones, or to hear the external input |

### ENVELOPE 1 · VCA / ENVELOPE 2 · VCF

Two ADSR envelopes: **A** attack 0.3 ms – 5 s, **D** decay 2.4 ms – 10 s, **S** sustain,
**R** release 1.5 ms – 6 s. Envelope 1 controls the volume, envelope 2 the filter (via
ENV DEPTH).

Legato playing does not restart the envelopes – this can be changed in SETTINGS (ENV RETRIG).

### OVERDRIVE

| Control | Function |
|---|---|
| **DRIVE** | distortion 0–11 |
| **TONE** | darker (left) or brighter (right) |
| **LEVEL** | output level of the drive stage |

### DELAY

Modelled on an analog bucket-brigade (BBD) delay: the longer the time, the darker the echoes.

| Control | Function |
|---|---|
| **TIME** | delay 25–640 ms |
| **REPEATS** | feedback – how many repeats |
| **MIX** | amount of the effect (fully left: no delay) |

### OUTPUT

| Control | Function |
|---|---|
| **VOLUME** | overall volume |
| **ASSIGN** | what the ASSIGN patch output carries: pitch of OSC 1 or OSC 2 (1 V/octave), velocity, mod wheel or aftertouch |

### SAMPLE & HOLD

| Control | Function |
|---|---|
| **RATE** | clock 0.26–28 Hz at which the input (without a cable: noise) is sampled |
| **GLIDE** | smooths the steps (0.5 ms – 1 s) |

### SLEW RATE LIMITER

| Control | Function |
|---|---|
| **SLEW** | how slowly the SLEW output follows a signal at SLEW IN (1 ms – 3 s) |
| **PORTA TIME** | glide between played notes (portamento, up to 10 s) |

### ATTENUATORS

| Control | Function |
|---|---|
| **1** | attenuator 1, up to +4 dB gain. Without a cable it receives ATT 2 |
| **2** | attenuator 2. Without a cable it receives the LFO, and its output is wired to the pulse width of both oscillators |

---

## 5. Patchbay

On the left the 32 **inputs (IN)**, on the right the 24 **outputs (OUT)**.

- **Patch a cable:** click an output first ("SELECT DESTINATION" appears above), then an input.
- **Remove a cable:** patch the same connection again, or **right-click** the input – that
  removes every cable at that input.
- Up to **four cables** per input are summed.
- Cables are saved with the project and in presets.

A patched cable **replaces** the internal connection of that input – like a jack socket whose
switch opens when you plug in. Without cables, these connections apply:

| Input | Receives without a cable |
|---|---|
| VCF IN | OSC MIX + NOISE (+ external input) |
| FREQ MOD | LFO, amount set by MOD DEPTH |
| OD IN | filter output |
| VCA IN | overdrive output |
| VCA CV | envelope 1 |
| DELAY IN | VCA output |
| PW 1, PW 2 | ATT 2 |
| ATT 2 IN | LFO |
| ATT 1 IN | ATT 2 |
| ATT 1 CV | ASSIGN |
| S&H IN | NOISE (S&H CLOCK replaces the internal clock) |
| INVERT IN | envelope 2 |
| MULT | LFO (at MULT 1 and MULT 2) |
| E. GATE 1 | MIDI gate |
| E. GATE 2 | MIDI gate, or E. GATE 1 when a cable is patched there |

The other inputs (OSC 1, OSC 2, OSC 1+2, SHAPE 1/2, RES, DELAY TIME, LFO RATE/SHAPE/TRIG,
SLEW IN, SUM 1 A/B, SUM 2 A/B) do nothing without a cable.

### Inputs

| Jack | Effect |
|---|---|
| OSC 1, OSC 2, OSC 1+2 | pitch of OSC 1, OSC 2 or both (1 V/octave) |
| INVERT IN | comes out inverted at INVERT |
| SHAPE 1, SHAPE 2 | shift the waveform |
| PW 1, PW 2 | pulse width |
| VCF IN, OD IN, VCA IN, DELAY IN | audio inputs of each stage |
| FREQ MOD | filter modulation (amount via MOD DEPTH) |
| RES | resonance |
| VCA CV | VCA control voltage |
| DELAY TIME | delay time |
| E. GATE 1, E. GATE 2 | gate for envelope 1 or 2 – runs the envelopes without any MIDI note |
| S&H IN, S&H CLOCK | signal and clock for sample & hold |
| LFO RATE, LFO SHAPE | speed and shape of the LFO |
| LFO TRIG | restarts the LFO on every rising edge |
| MULT | splitter: the signal appears at MULT 1 and MULT 2 |
| ATT 1 IN, ATT 1 CV, ATT 2 IN | attenuator inputs; ATT 1 CV sets the gain of ATT 1 |
| SLEW IN | input of the slew limiter |
| SUM 1 A/B, SUM 2 A/B | summed and sent out at SUM 1 or SUM 2 |

### Outputs

OSC 1, OSC 2, OSC MIX, VCF 1, VCF 2, OVERDRIVE, VCA, OUTPUT, NOISE, ENV 1, ENV 2, INVERT, LFO,
LFO UNI (positive only, 0…1), S&H, MULT 1, MULT 2, MIDI GATE, ATT 1, ATT 2, SLEW, SUM 1, SUM 2,
ASSIGN.

Pitch inputs and the ASSIGN output (OSC 1/2) follow 1 V/octave; full level of a patch signal
corresponds to ±5 V. Feedback patches (e.g. OUTPUT → VCF IN) are allowed and stay stable.

### External input

The plugin has a sidechain input called **"Ext In"**. Audio your DAW sends there runs through
mixer, filter, overdrive, VCA and delay, like the hardware's rear input. To hear it, open the
VCA with VCA BIAS or play notes.

---

## 6. Settings

The **SETTINGS** key in the header opens options the original hides behind key combinations.
**CLOSE** closes them again.

| Option | Function |
|---|---|
| **PRIORITY** | which note plays when several keys are held: LAST, HIGH (highest), LOW (lowest), HIGH/LOW (paraphonic: lowest on OSC 1, highest on OSC 2) |
| **BEND RANGE** | pitch bend range 0–24 semitones |
| **KEY SPLIT** | split the keyboard: below the split note OSC 1 plays, from it upwards OSC 2 |
| **ENV RETRIG** | envelopes restart on legato notes too |
| **PEDAL** | OSC 2 stays on the note played before switching on (pedal tone) |
| **MUTE C1–C7 OUT** | notes outside C1–C7 are muted instead of moved into the range |
| **AUTOGLIDE 1 / 2** | every note glides in from up to ±12 semitones away (speed set by PORTA TIME) |
| **DRIFT** | analog pitch drift |
| **TUNE BYPASS** | ignores the TUNE knob of OSC 1 or OSC 2 – exact tuning |
| **DEPTH** (LFO) | overall LFO amount |
| **KEY RATE** | from the chosen note upwards, the LFO doubles its speed per octave |
| **ONE SHOT** | the LFO runs a single cycle after starting (needs KEY SYNC) |
| **RETRIG** (LFO) | KEY SYNC restarts the LFO on legato notes too |
| **VCF MOD / AMOUNT** | velocity, mod wheel or aftertouch additionally opens the filter |
| **ASSIGN OUT** | same as the ASSIGN selector in OUTPUT |
| **LFO SHAPE ORDER / START PHASE** | which waveform sits at each of the five SHAPE positions, and its start phase |

---

## 7. MIDI

- The plugin listens on all MIDI channels.
- Notes are kept within **C1–C7** (lower or higher notes are moved into the range, see also
  MUTE C1–C7 OUT).
- **Pitch bend** (range in SETTINGS), **mod wheel** (CC 1), **aftertouch** and the **sustain
  pedal** (CC 64) are used.
- Velocity only acts where it is assigned: through ASSIGN or VCF MOD.
- Every knob and key can be automated in the DAW.

---

## 8. Presets

**SAVE** stores the current sound including all patch cables under a name, **PRESET** loads it
again.

Presets are single `.napreset` files and can be copied between computers:

| System | Folder |
|---|---|
| Windows | `%APPDATA%\NegativeAtomium\Presets` |
| macOS | `~/Library/Application Support/NegativeAtomium/Presets` |
| Linux | `~/.local/share/NegativeAtomium/Presets` (or `$XDG_DATA_HOME/NegativeAtomium/Presets`) |

Independently of that, your DAW project always stores the current sound.

---

## 9. Tips and troubleshooting

- **The sound wobbles although MOD DEPTH is at zero?** Usually **ATTENUATOR 2** is turned up:
  through it, the LFO is hard-wired to the pulse width of both oscillators (audible with square
  and tone mod). Turn ATT 2 down – or patch a cable into PW 1/PW 2.
- **No sound?** Check VCA BIAS and VOLUME, and whether a cable in VCA CV, VCA IN or VCF IN
  replaces the internal connection.
- **The pitch drifts slightly?** That is DRIFT in SETTINGS – set it to zero for stable tuning.
- **The filter whistles?** Turn RESO down; fully up, the filter oscillates on its own.
- **Bass drone:** turn up VCA BIAS, put OSC 2 an octave lower (RANGE), slow LFO on the filter
  via MOD DEPTH.
- **Random sequence:** patch S&H into FREQ MOD, turn up MOD DEPTH, set S&H RATE to the tempo.
