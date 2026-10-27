# NegativeAtomium

Semi-modular, paraphonic analog-style synthesizer with a patchbay, in the arrangement of a classic
desktop synth: anthracite panel, glossy star knobs, rubber keys and real-looking jacks. Built with
JUCE as AU, VST3, CLAP and Standalone for macOS, Windows and Linux.

- Two oscillators with five shapes each (sine → triangle → saw → square → tone mod), morphing or
  switched, pulse width, 32' / 16' / 8' ranges or a ±10 octave FULL range, hard sync
- Paraphonic mode: two notes at once, one per oscillator, with selectable note priority
- 12 dB multimode filter (low-, band- and high-pass) that self-oscillates, key tracking, and a
  second output with the complementary response
- Overdrive, analog-style bucket-brigade delay, two ADSR envelopes
- LFO with five shapes, free or synced to the host tempo (4/1 … 1/64), key sync and one-shot
- Sample & hold, slew limiter, portamento, two attenuators
- Patchbay with 32 inputs and 24 outputs: every input is wired internally, so it plays without a
  single cable – patch one in to re-route; feedback patches allowed, DAW side-chain into the mixer
- The options the hardware hides behind key combinations in one SETTINGS page: note priority, key
  split, pedal tone, autoglide, analog drift, velocity / mod wheel / aftertouch to the filter and more
- Presets with all cables, saved as plain files in your user folder; the window scales from 60 to 250 %
- Oversampled 2×

**Manual:** [English](docs/Manual.md) · [Deutsch](docs/Handbuch.md) · **Schnellstart (German quick start):** [docs/Schnellstart.md](docs/Schnellstart.md)

![NegativeAtomium front panel](docs/images/panel.png)

## Download

Released versions are under **Releases**: AU (macOS), VST3, CLAP and a Standalone app for macOS,
Windows and Linux.
Installation is explained in the manual ([English](docs/Manual.md#1-installation) ·
[Deutsch](docs/Handbuch.md#1-installation)).

**macOS:** the plugins are not notarized by Apple, so macOS may block them. The manual explains how
to unblock any downloaded plugin – and NegativeAtomium in one go
([English](docs/Manual.md#unblocking-plugins-on-macos) · [Deutsch](docs/Handbuch.md#plugins-auf-macos-freigeben)).

## Credits

Idea, direction, testing and the "these look like little flowers" calls:
[sscheppert-blip](https://github.com/sscheppert-blip).
Every line of code, DSP and documentation was typed by AI: [Claude Code](https://claude.com/claude-code)
and [OpenAI Codex](https://openai.com/codex). So if you like it, send a good part of the flowers to the
robots. 🤖💐

Inspired by a classic semi-modular desktop synth; not affiliated with or endorsed by its manufacturer.

## License

Public domain ([Unlicense](LICENSE)) – do whatever you like with it, no credit needed.
The plugins are built with [JUCE](https://juce.com), which has its own license terms.

<sub>Building from source: [docs/Development.md](docs/Development.md)</sub>
