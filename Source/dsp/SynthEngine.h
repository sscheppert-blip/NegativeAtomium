#pragma once

#include "Effects.h"
#include "Envelope.h"
#include "Filter.h"
#include "Modulation.h"
#include "Oscillator.h"
#include "../PatchBay.h"

#include <array>

namespace na::dsp
{

struct EngineParams
{
    struct Osc
    {
        float tune  = 0.0f;  // -1..1
        float shape = 0.5f;  // 0..1 (sine, triangle, saw, square, tone mod)
        float width = 0.5f;  // 0..1
        int   octave = 2;    // 0 = 32', 1 = 16', 2 = 8'
        bool  range = false; // false: tune ±1 octave, true: tune ±10 octaves (octave ignored)
        bool  blend = true;  // false: shape switches between the five waveforms
    };

    struct Adsr
    {
        float attack = 0.005f, decay = 0.3f, sustain = 0.7f, release = 0.3f;
    };

    Osc osc1, osc2;
    float oscMix = 0.5f;
    bool  sync = false, paraphonic = false;

    int   vcfMode = 0; // 0 LP, 1 BP, 2 HP
    float vcfFreq = 2000.0f, vcfReso = 0.2f;
    bool  keyTrack = false;
    float modDepth = 0.0f, envDepth = 0.4f;

    float noise = 0.0f, vcaBias = 0.0f;

    float lfoRate = 2.0f, lfoShape = 0.0f;
    bool  lfoKeySync = false, lfoBlend = true;

    float delayTime = 0.3f, delayRepeats = 0.3f, delayMix = 0.0f;
    float odDrive = 0.1f, odTone = 0.7f, odLevel = 0.7f;

    Adsr env1, env2;

    float shRate = 4.0f, shGlide = 0.0f;
    float slew = 0.0f, portaTime = 0.0f;
    float att1 = 0.5f, att2 = 0.0f;

    float volume = 0.7f;
    int assignSource = 0; // 0 OSC1 CV, 1 OSC2 CV, 2 velocity, 3 modwheel, 4 aftertouch
    bool envRetrigger = false; // hardware default: legato notes don't restart the envelopes

    // ── user configurable options (manual chapter 5) ─────────────────────
    int   notePriority = 0;     // 0 last, 1 high, 2 low, 3 high/low (paraphonic)
    int   keySplit = 0;         // 0 off, else first note of OSC 2's range
    bool  pedalMode = false;    // OSC 2 holds the note played before enabling
    int   autoglide1 = 0, autoglide2 = 0; // semitones the note glides in from
    bool  tuneBypass1 = false, tuneBypass2 = false;
    float lfoDepth = 1.0f;
    bool  lfoOneShot = false, lfoRetrigger = true; // retrigger on overlapping notes
    int   lfoKeyRateNote = 0;   // 0 off, else base note of LFO key rate
    int   vcfModSource = 0;     // 0 off, 1 velocity, 2 mod wheel, 3 aftertouch
    float vcfModDepth = 1.0f;
    bool  muteOutOfRange = false;               // ignore notes outside C1–C7 instead of clamping
    std::array<int, 5> lfoSlots { 0, 1, 2, 3, 4 }; // shape at each knob position
    std::array<int, 5> lfoPhases {};               // start phase per position, in eighths of a cycle
    float drift = 0.0f;                         // analog pitch drift, 0..1
    float noteVelocity = 1.0f, modWheel = 0.0f, aftertouch = 0.0f;
};

/** The complete mono/paraphonic voice with the default (normalled) signal path. */
class SynthEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    void setParameters (const EngineParams& newParams) noexcept;
    void setPatchConnections (const na::patch::Connections& newConnections) noexcept { patchConnections = newConnections; }

    void noteOn (int note, float velocity = 1.0f) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;
    void setPitchBend (float semitones) noexcept { pitchBend = semitones; }
    void setModWheel (float value) noexcept { modWheel = std::clamp (value, 0.0f, 1.0f); }
    void setAftertouch (float value) noexcept { aftertouch = std::clamp (value, 0.0f, 1.0f); }
    void setSustainPedal (bool down) noexcept;

    /** extInput (optional): the EXT INPUT signal at the engine rate, summed into the mixer. */
    void render (float* output, int numSamples, const float* extInput = nullptr) noexcept;

    /** Locks the LFO to the host tempo grid (phase 0..1 at the current sample). */
    void syncLfoPhase (double phase) noexcept { lfo.setPhase (phase); }

    bool isGateOpen() const noexcept { return numHeld > 0; }
    float getSampleAndHold() const noexcept { return sampleHoldOut; }

    /** Value of a patchbay output after the most recent sample. */
    float getSourceValue (na::patch::Source source) const noexcept { return sources[(std::size_t) source]; }

private:
    float processSample (float extInput) noexcept;
    void assignNotes() noexcept;
    static float oscillatorOffset (const EngineParams::Osc& osc, bool tuneBypass) noexcept;
    static float shapePosition (float shape, bool blend) noexcept;
    bool isPatched (na::patch::Destination destination) const noexcept;
    float patchValue (na::patch::Destination destination) const noexcept;
    void gateEnvelopesFromMidi (bool open, bool retrigger) noexcept;
    void releaseNote (int note) noexcept;

    EngineParams params;
    na::patch::Connections patchConnections = []
    {
        na::patch::Connections connections {};
        for (auto& destination : connections)
            destination.fill (-1);
        return connections;
    }();
    double sampleRate = 96000.0;

    std::array<int, 16> held {};
    int numHeld = 0;
    int lastPlayed = 60, pedalNote = 60;
    bool sustainDown = false;
    std::array<bool, 128> sustained {}; // released while the sustain pedal was down

    float note1Target = 60.0f, note2Target = 60.0f, note1 = 60.0f, note2 = 60.0f;
    float pitchBend = 0.0f, noteVelocity = 1.0f, modWheel = 0.0f, aftertouch = 0.0f;
    float glideCoef = 1.0f, shGlideCoef = 1.0f, slewStep = 1.0f;

    // per-sample smoothed controls
    float smoothCoef = 1.0f;
    float logCutoff = 11.0f, oscMix = 0.5f, noiseLevel = 0.0f, vcaBias = 0.0f, volume = 0.5f, reso = 0.2f;

    float sampleHoldOut = 0.0f, slewOut = 0.0f;
    bool patchGate1 = false, patchGate2 = false, patchLfoTrigger = false, patchShClock = false;

    // Patchbay outputs of the previous sample. Patched inputs read these, so every
    // cable (including feedback loops) sees a consistent one-sample delay.
    std::array<float, na::patch::sourceCount> sources {};

    double lfoStartPhase() const noexcept;

    Oscillator osc1, osc2;
    WhiteNoise noise, driftNoise1 { 0x1234567u }, driftNoise2 { 0x7654321u };
    float drift1 = 0.0f, drift2 = 0.0f, driftCoef = 0.0f, driftNorm = 0.0f;
    StateVariableFilter filter;
    Overdrive overdrive;
    BbdDelay delay;
    Envelope env1, env2;
    Lfo lfo;
    SampleAndHold sampleHold;
};

} // namespace na::dsp
