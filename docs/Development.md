# NegativeAtomium – Development

## Building

Needs CMake ≥ 3.28, a C++20 compiler and Ninja (optional). JUCE and clap-juce-extensions are
fetched by CMake. On Linux, install JUCE's dependencies first (see `.github/workflows/build.yml`
for the list).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target NegativeAtomium_VST3 NegativeAtomium_CLAP NegativeAtomium_Standalone NegativeAtomiumTests
./build/NegativeAtomiumTests_artefacts/Release/NegativeAtomiumTests
```

On macOS add `NegativeAtomium_AU`, and `"-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"` for universal
binaries. The plugins end up in `build/NegativeAtomium_artefacts/Release/`; add
`-DNA_COPY_PLUGIN=ON` to copy them into the user plugin folders after building.

## Tests and tools

The test runner checks the DSP engine (pitch, ranges, every patchbay input and output, normalled
routings, envelopes, LFO, random feedback patches, the specification ranges) and, given a path,
loads the built plugin the way a DAW would. Options:

- `--plugin <path.vst3>` loads the built VST3 and opens its editor (`--no-editor` to skip that,
  e.g. on a headless Linux runner)
- `--screenshot <png>` renders the editor at 2× (plus `-200.png` resized and `-settings.png`)
- `--demo <wav>` renders a short demo, `--scenarios <folder>` a set of test sounds
- `--export-skin <folder>` writes the GUI as layers for Photoshop (see `design/README.md`)
- `--bench` measures the CPU load

Manual image: `docs/images/panel.png` is the `--screenshot` render, downscaled to 1×
(1624 × 588 px).

## Releases

Builds run only on demand (pushes don't trigger them, to save GitHub Actions minutes) and publish
release `v<version>` with AU (macOS), VST3, CLAP and Standalone. Bump `VERSION` in
`CMakeLists.txt` first – the version must match.

- **On a Mac:** `tools/release-mac.sh 0.5.0` builds, tests (incl. `auval`) and packages the macOS
  plugins locally as universal binaries, creates a draft release with them, then lets GitHub build
  only Linux and Windows and publish. `--dry-run` stops after packaging.
- **On a PC:** `gh workflow run build -f version=0.5.0` (or Actions → build → Run workflow) builds
  all three platforms on GitHub.

## Validation (macOS)

Before a release, validate the installed plugins:

```bash
auval -v aumu NgA1 Ngat
pluginval --strictness-level 10 --validate-in-process --validate ~/Library/Audio/Plug-Ins/VST3/NegativeAtomium.vst3
clap-validator validate ~/Library/Audio/Plug-Ins/CLAP/NegativeAtomium.clap
```

`tools/check-sensitive-data.sh` (needs ripgrep) scans the tracked files for credentials and local
paths; `.githooks/pre-push` runs it on every push once enabled with
`git config core.hooksPath .githooks`.

## Engine notes

- The whole engine runs 2× oversampled; the EXT IN side-chain is oversampled the same way and
  summed into the mixer.
- Patchbay: 24 sources, 32 destinations, up to four cables per destination, stored with the plugin
  state and in presets. A cable replaces the normalled signal, as on the hardware jacks (the table
  is in the manual). Every cable carries the previous sample's value, so feedback patches are
  stable. Pitch inputs and ASSIGN's OSC CV follow 1 V/oct, with ±1 in the patchbay representing ±5 V.
- Control ranges follow the specification page of the reference instrument: attack 0.3 ms–5 s
  (linear), decay 2.4 ms–10 s, release 1.5 ms–6 s, cutoff 10 Hz–15 kHz, S&H 0.26–28 Hz with
  0.5 ms–1 s glide, slew 1 ms–3 s, portamento up to 10 s, ATT 1 up to +4 dB, pulse width 1–99 %,
  delay 25–640 ms.
- The delay models an 8192-stage BBD whose bandwidth follows its clock (stages / 2·time).
- MIDI notes are clamped to C1–C7 (MIDI 24–96) unless MUTE C1–C7 OUT is on. Envelope
  retriggering is off by default: legato notes don't restart the envelopes.
- LFO SYNC: the RATE knob selects one of 21 clock divisions (4/1 … 1/64); while the song plays,
  the phase follows the bar grid unless KEY SYNC restarts it per note.
- Tone mod is an approximation, not yet verified against the hardware.
- The look is fixed and drawn in code; there is no theming. Presets and the per-user folder:
  `na::ui::userDataFolder()` (paths in the manual).
