// Test runner: checks the DSP engine directly and, when given a path,
// loads the built VST3 the way a DAW would.
//
//   NegativeAtomiumTests [--plugin <path.vst3>] [--no-editor] [--screenshot <out.png>] [--demo <out.wav>]
//                        [--export-skin <dir>]

#include "PluginEditor.h"
#include "SkinExport.h"
#include "dsp/SynthEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <iostream>
#include <tuple>

namespace
{

int failures = 0;

void check (bool condition, const juce::String& description)
{
    std::cout << (condition ? "  ok    " : "  FAIL  ") << description << std::endl;
    if (! condition)
        ++failures;
}

struct Stats
{
    float peak = 0.0f;
    double rms = 0.0;
    bool finite = true;
};

Stats analyse (const float* data, size_t n)
{
    Stats s;
    double sum = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        if (! std::isfinite (data[i]))
        {
            s.finite = false;
            continue;
        }

        s.peak = std::max (s.peak, std::abs (data[i]));
        sum += (double) data[i] * data[i];
    }

    s.rms = n > 0 ? std::sqrt (sum / (double) n) : 0.0;
    return s;
}

constexpr double engineRate = 96000.0;

void testEngineBasics()
{
    std::cout << "engine: note on / off" << std::endl;

    na::dsp::SynthEngine engine;
    na::dsp::EngineParams params;
    engine.setParameters (params);
    engine.prepare (engineRate);

    std::vector<float> buffer ((size_t) engineRate * 2);

    engine.render (buffer.data(), 9600);
    check (analyse (buffer.data(), 9600).rms < 1.0e-6, "silent before any note");

    engine.noteOn (48);
    engine.render (buffer.data(), (int) engineRate);
    const auto held = analyse (buffer.data(), (size_t) engineRate);
    check (held.finite, "output is finite while a note is held");
    check (held.rms > 0.02, "note is audible (rms " + juce::String (held.rms, 3) + ")");
    check (held.peak < 2.0f, "peak stays sane (" + juce::String (held.peak, 2) + ")");

    engine.noteOff (48);
    engine.render (buffer.data(), (int) buffer.size());
    const auto tail = analyse (buffer.data() + buffer.size() - 19200, 19200);
    check (tail.rms < 1.0e-3, "release decays to silence");
}

void testPitch()
{
    std::cout << "engine: tuning" << std::endl;

    na::dsp::SynthEngine engine;
    na::dsp::EngineParams params;
    params.osc1.shape = params.osc2.shape = 0.0f; // sine
    params.vcfFreq  = 20000.0f;
    params.vcfReso  = 0.0f;
    params.envDepth = 0.0f;
    params.odDrive  = 0.0f;
    params.env1     = { 0.001f, 0.1f, 1.0f, 0.1f };
    engine.setParameters (params);
    engine.prepare (engineRate);

    std::vector<float> buffer ((size_t) engineRate);
    engine.noteOn (69);
    engine.render (buffer.data(), 9600); // settle
    engine.render (buffer.data(), (int) buffer.size());

    int crossings = 0;
    for (size_t i = 1; i < buffer.size(); ++i)
        if (buffer[i - 1] < 0.0f && buffer[i] >= 0.0f)
            ++crossings;

    check (std::abs (crossings - 440) <= 2, "A4 plays at 440 Hz (measured " + juce::String (crossings) + ")");
}

void testOscillatorRange()
{
    std::cout << "engine: oscillator octave range" << std::endl;

    na::dsp::SynthEngine engine;
    na::dsp::EngineParams params;
    params.osc1.shape = params.osc2.shape = 0.0f;
    params.osc1.tune = 1.0f;
    params.oscMix = 0.0f;
    params.vcfFreq = 20000.0f;
    params.vcfReso = 0.0f;
    params.envDepth = 0.0f;
    params.odDrive = 0.0f;
    params.odLevel = 1.0f;
    params.env1 = { 0.001f, 0.1f, 1.0f, 0.1f };
    engine.setParameters (params);
    engine.prepare (engineRate);

    std::vector<float> buffer ((size_t) engineRate);
    engine.noteOn (69);
    engine.render (buffer.data(), 9600);
    engine.render (buffer.data(), (int) buffer.size());

    int crossings = 0;
    for (size_t i = 1; i < buffer.size(); ++i)
        if (buffer[i - 1] < 0.0f && buffer[i] >= 0.0f)
            ++crossings;

    check (std::abs (crossings - 880) <= 4,
           "tune reaches one octave (measured " + juce::String (crossings) + " Hz)");
}

void testRangeAndShapeDisplay()
{
    std::cout << "parameters: range, tune and shape display" << std::endl;

    NegativeAtomiumProcessor processor;
    auto& state = processor.getState();
    auto* tune  = state.getParameter (na::id::osc1Tune);
    auto* shape = state.getParameter (na::id::osc1Shape);
    auto* range = state.getParameter (na::id::osc1Octave);
    auto* blend = state.getParameter (na::id::osc1Blend);

    check (range->getText (range->getValue(), 32) == "8'", "RANGE defaults to 8'");
    check (tune->getText (tune->convertTo0to1 (0.5f), 32) == "+6.00 st", "TUNE shows semitones");

    range->setValueNotifyingHost (range->convertTo0to1 ((float) na::fullRangeIndex));
    check (tune->getText (tune->convertTo0to1 (0.5f), 32) == "+5.00 oct", "TUNE shows octaves in FULL range");

    check (shape->getText (0.5f, 32) == "Saw", "SHAPE centre is the saw");
    check (shape->getText (1.0f, 32) == "Tone Mod", "SHAPE fully clockwise is tone mod");
    check (shape->getText (0.6f, 32).startsWith ("Saw"), "SHAPE between two waveforms names both ("
                                                            + shape->getText (0.6f, 32) + ")");

    blend->setValueNotifyingHost (0.0f);
    check (shape->getText (0.6f, 32) == "Saw", "switched SHAPE snaps to the nearest waveform");

    // engine: FULL range ignores the octave switch and spans ±10 octaves
    na::dsp::EngineParams params;
    params.osc1 = { 0.1f, 0.0f, 0.5f, 0, true };
    params.osc2 = params.osc1;
    params.oscMix = 0.0f;
    params.vcfFreq = 20000.0f;
    params.vcfReso = 0.0f;
    params.envDepth = 0.0f;
    params.odDrive = 0.0f;
    params.env1 = { 0.001f, 0.1f, 1.0f, 0.1f };

    na::dsp::SynthEngine engine;
    engine.setParameters (params);
    engine.prepare (engineRate);
    std::vector<float> buffer ((size_t) engineRate);
    engine.noteOn (69);
    engine.render (buffer.data(), 9600);
    engine.render (buffer.data(), (int) buffer.size());

    int crossings = 0;
    for (size_t i = 1; i < buffer.size(); ++i)
        if (buffer[i - 1] < 0.0f && buffer[i] >= 0.0f)
            ++crossings;

    check (std::abs (crossings - 880) <= 4, "FULL range: tune +10 % = +1 octave (measured " + juce::String (crossings) + " Hz)");
}

void testStress()
{
    std::cout << "engine: random parameter stress" << std::endl;

    na::dsp::SynthEngine engine;
    na::dsp::EngineParams params;
    engine.prepare (engineRate);

    juce::Random random (42);
    std::vector<float> buffer (4096);
    bool finite = true;
    float peak = 0.0f;

    auto r = [&random] { return random.nextFloat(); };

    for (int iteration = 0; iteration < 400; ++iteration)
    {
        params.osc1         = { r() * 2 - 1, r(), r(), random.nextInt (3), random.nextBool() };
        params.osc2         = { r() * 2 - 1, r(), r(), random.nextInt (3), random.nextBool() };
        params.oscMix       = r();
        params.sync         = random.nextBool();
        params.paraphonic   = random.nextBool();
        params.vcfMode      = random.nextInt (3);
        params.vcfFreq      = 20.0f * std::pow (1000.0f, r());
        params.vcfReso      = iteration % 4 == 0 ? 1.0f : r();
        params.keyTrack     = random.nextBool();
        params.modDepth     = r();
        params.envDepth     = r() * 2 - 1;
        params.noise        = r();
        params.vcaBias      = r();
        params.lfoRate      = 0.02f + 40.0f * r();
        params.lfoShape     = r();
        params.delayTime    = 0.02f + r();
        params.delayRepeats = r();
        params.delayMix     = r();
        params.odDrive      = r();
        params.odTone       = r();
        params.odLevel      = r();
        params.env1         = { 0.001f + r(), 0.001f + r(), r(), 0.001f + r() };
        params.env2         = { 0.001f + r(), 0.001f + r(), r(), 0.001f + r() };
        params.portaTime    = r();
        params.att2         = r();
        params.volume       = r();
        engine.setParameters (params);

        if (random.nextInt (3) == 0)
            engine.noteOn (24 + random.nextInt (72));
        if (random.nextInt (4) == 0)
            engine.noteOff (24 + random.nextInt (72));

        engine.render (buffer.data(), (int) buffer.size());
        const auto stats = analyse (buffer.data(), buffer.size());
        finite = finite && stats.finite;
        peak   = std::max (peak, stats.peak);
    }

    check (finite, "no NaN/Inf with 400 random settings (incl. full resonance)");
    check (peak <= 4.0f, "output bounded (peak " + juce::String (peak, 2) + ")");
}

void testPatchRouting()
{
    std::cout << "engine: patchbay routing" << std::endl;

    na::dsp::SynthEngine engine;
    na::dsp::EngineParams params;
    params.vcfFreq = 1200.0f;
    params.vcfReso = 0.4f;
    params.envDepth = 0.0f;
    params.noise = 0.0f;
    engine.setParameters (params);
    engine.prepare (engineRate);

    na::patch::Connections connections {};
    for (auto& slots : connections)
        slots.fill (-1);

    // Exercise several source classes and mixed destinations at once.
    connections[(size_t) na::patch::Destination::osc1Pitch][0]
        = (int) na::patch::Source::lfo;
    connections[(size_t) na::patch::Destination::osc2Pitch][0]
        = (int) na::patch::Source::sampleHold;
    connections[(size_t) na::patch::Destination::shape1][0]
        = (int) na::patch::Source::env2;
    connections[(size_t) na::patch::Destination::freqMod][0]
        = (int) na::patch::Source::lfo;
    connections[(size_t) na::patch::Destination::freqMod][1]
        = (int) na::patch::Source::env1;
    connections[(size_t) na::patch::Destination::vcaIn][0]
        = (int) na::patch::Source::env1;
    engine.setPatchConnections (connections);
    engine.noteOn (48);

    std::vector<float> buffer ((size_t) (engineRate / 2.0));
    engine.render (buffer.data(), (int) buffer.size());
    const auto stats = analyse (buffer.data(), buffer.size());
    check (stats.finite, "patched signal remains finite");
    check (stats.peak <= 4.0f, "patched signal remains bounded (peak "
                                + juce::String (stats.peak, 2) + ")");
}

na::patch::Connections emptyPatch()
{
    na::patch::Connections connections {};
    for (auto& slots : connections)
        slots.fill (-1);
    return connections;
}

void patch (na::patch::Connections& c, na::patch::Source from, na::patch::Destination to)
{
    for (auto& slot : c[(size_t) to])
        if (slot < 0) { slot = (int) from; return; }
}

na::dsp::EngineParams quietParams()
{
    na::dsp::EngineParams params;
    params.osc1.shape = params.osc2.shape = 0.0f;
    params.vcfFreq = 20000.0f;
    params.vcfReso = 0.0f;
    params.envDepth = 0.0f;
    params.odDrive = 0.0f;
    params.delayMix = 0.0f;
    params.env1 = { 0.001f, 0.05f, 1.0f, 0.05f };
    params.env2 = { 0.001f, 0.05f, 1.0f, 0.05f };
    return params;
}

int countCrossings (const std::vector<float>& buffer)
{
    int crossings = 0;
    for (size_t i = 1; i < buffer.size(); ++i)
        if (buffer[i - 1] < 0.0f && buffer[i] >= 0.0f)
            ++crossings;
    return crossings;
}

void testNormalling()
{
    std::cout << "patchbay: normalled routings (manual chapter 8)" << std::endl;
    using S = na::patch::Source;

    auto params = quietParams();
    params.att1 = 0.5f;
    params.att2 = 0.5f;
    params.assignSource = 2; // velocity
    params.lfoRate = 3.0f;

    na::dsp::SynthEngine engine;
    engine.setParameters (params);
    engine.prepare (engineRate);
    engine.noteOn (48, 1.0f);

    std::vector<float> buffer (12345);
    engine.render (buffer.data(), (int) buffer.size());

    auto v = [&engine] (S s) { return engine.getSourceValue (s); };
    check (std::abs (v (S::invert) + v (S::env2)) < 1.0e-6f, "ENV 2 → INVERT");
    check (std::abs (v (S::mult1) - v (S::lfo)) < 1.0e-6f && v (S::mult2) == v (S::mult1), "bipolar LFO → MULT 1/2");
    check (std::abs (v (S::att2) - v (S::lfo) * 0.5f) < 1.0e-6f, "bipolar LFO → ATT 2");
    const float att1Gain = std::min (0.5f * 1.585f + 0.5f, 1.585f);
    check (std::abs (v (S::att1) - v (S::att2) * att1Gain) < 1.0e-5f,
           "ATT 2 → ATT 1 IN, ASSIGN → ATT 1 CV (up to +4 dB)");
    check (std::abs (v (S::assign) - 1.0f) < 1.0e-6f, "ASSIGN outputs note velocity");

    params.assignSource = 0;
    engine.setParameters (params);
    engine.render (buffer.data(), 100);
    check (std::abs (v (S::assign) - 0.4f) < 1.0e-4f, "ASSIGN OSC 1 CV is 1 V/oct pitch (C3 = 2 V → 0.4), not audio");
}

void testPatchedInputs()
{
    std::cout << "patchbay: patched inputs" << std::endl;
    using S = na::patch::Source;
    using D = na::patch::Destination;
    std::vector<float> buffer ((size_t) engineRate);

    {   // SLEW limits the rate of change of SLEW IN
        auto params = quietParams();
        params.slew = 1.25f; // seconds for the full -1..+1 range
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        auto c = emptyPatch();
        patch (c, S::midiGate, D::slewIn);
        engine.setPatchConnections (c);
        engine.noteOn (60);
        engine.render (buffer.data(), (int) (engineRate * 0.1));
        const float early = engine.getSourceValue (S::slew);
        engine.render (buffer.data(), (int) engineRate);
        const float late = engine.getSourceValue (S::slew);
        check (early > 0.1f && early < 0.25f, "SLEW rises slowly (" + juce::String (early, 3) + " after 100 ms)");
        check (std::abs (late - 1.0f) < 1.0e-4f, "SLEW reaches the input value");
    }

    {   // S&H IN replaces the noise, S&H CLOCK replaces the internal clock
        auto params = quietParams();
        params.shRate = 0.1f;
        params.assignSource = 0;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        auto c = emptyPatch();
        patch (c, S::assign, D::sampleHoldIn);
        patch (c, S::midiGate, D::sampleHoldClock);
        engine.setPatchConnections (c);
        engine.render (buffer.data(), 1000);
        const float before = engine.getSourceValue (S::sampleHold);
        engine.noteOn (48);
        engine.render (buffer.data(), 1000);
        const float after = engine.getSourceValue (S::sampleHold);
        check (std::abs (before) < 1.0e-6f, "S&H holds until the external clock ticks");
        check (std::abs (after - 0.4f) < 1.0e-3f, "S&H samples S&H IN (not noise) on the clock edge (" + juce::String (after, 3) + ")");
    }

    {   // VCA CV replaces ENV 1
        auto params = quietParams();
        params.env1 = { 0.001f, 0.01f, 0.0f, 0.01f };
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (60);
        engine.render (buffer.data(), (int) buffer.size());
        const auto closed = analyse (buffer.data() + buffer.size() - 4800, 4800);

        auto c = emptyPatch();
        patch (c, S::midiGate, D::vcaCv);
        engine.setPatchConnections (c);
        engine.render (buffer.data(), (int) buffer.size());
        const auto open = analyse (buffer.data() + buffer.size() - 4800, 4800);
        check (closed.rms < 1.0e-4 && open.rms > 0.05, "a cable in VCA CV replaces ENV 1");
    }

    {   // OSC 1+2 is a pitch input for both oscillators (1 V/oct)
        auto params = quietParams();
        params.assignSource = 2; // velocity
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        auto c = emptyPatch();
        patch (c, S::assign, D::osc12Pitch);
        engine.setPatchConnections (c);
        engine.noteOn (69, 0.2f); // 0.2 = 1 V = +1 octave
        engine.render (buffer.data(), 9600);
        engine.render (buffer.data(), (int) buffer.size());
        const int hz = countCrossings (buffer);
        check (std::abs (hz - 880) <= 4, "OSC 1+2 input: +1 V = +1 octave (measured " + juce::String (hz) + " Hz)");
    }
}

// Every jack, one by one: patch a known signal into it and check that the target
// does what the label says. ASSIGN set to velocity is a DC control voltage.
// With MOD DEPTH and ATT 2 at zero and no cables, the LFO must not be audible:
// its only normalled paths are FREQ MOD (scaled by MOD DEPTH) and ATT 2 -> PW 1/2.
void testLfoSilentWithoutDepth()
{
    std::cout << "LFO: no effect with MOD DEPTH and ATT 2 at zero" << std::endl;

    auto render = [] (float lfoRate, float modDepth, float att2)
    {
        auto params = quietParams();
        params.osc1.shape = params.osc2.shape = 0.75f; // square: pulse width is audible
        params.vcfFreq = 800.0f;
        params.lfoRate = lfoRate;
        params.modDepth = modDepth;
        params.att2 = att2;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (48);
        std::vector<float> buffer ((size_t) (engineRate * 0.5));
        engine.render (buffer.data(), (int) buffer.size());
        return buffer;
    };
    auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            sum += std::pow ((double) a[i] - (double) b[i], 2.0);
        return std::sqrt (sum / (double) a.size());
    };

    check (difference (render (0.5f, 0.0f, 0.0f), render (9.0f, 0.0f, 0.0f)) < 1.0e-6,
           "LFO rate makes no difference at MOD DEPTH 0, ATT 2 0");
    check (difference (render (0.5f, 0.5f, 0.0f), render (9.0f, 0.5f, 0.0f)) > 1.0e-3,
           "MOD DEPTH up: the LFO moves the filter");
    check (difference (render (0.5f, 0.0f, 0.5f), render (9.0f, 0.0f, 0.5f)) > 1.0e-3,
           "ATT 2 up: the LFO moves the pulse width (normalled ATT 2 -> PW)");
}

void testEveryPatchPoint()
{
    std::cout << "patchbay: every input and output" << std::endl;
    using S = na::patch::Source;
    using D = na::patch::Destination;

    struct Run
    {
        std::array<std::vector<float>, na::patch::sourceCount> out;
        const std::vector<float>& operator[] (S s) const { return out[(size_t) s]; }
    };

    // Renders sample by sample and records every patchbay output.
    auto run = [] (const na::dsp::EngineParams& params, const na::patch::Connections& c, float velocity,
                   double seconds, bool playNote = true, int noteAtSample = 0)
    {
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.setPatchConnections (c);
        Run r;
        const auto n = (int) (engineRate * seconds);
        for (auto& o : r.out)
            o.reserve ((size_t) n);
        float sample = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (playNote && i == noteAtSample)
                engine.noteOn (57, velocity); // A3 = 220 Hz at 8'
            engine.render (&sample, 1);
            for (size_t s = 0; s < r.out.size(); ++s)
                r.out[s].push_back (engine.getSourceValue ((S) s));
        }
        return r;
    };
    auto patched = [] (std::initializer_list<std::pair<S, D>> cables)
    {
        auto c = emptyPatch();
        for (auto [from, to] : cables)
            patch (c, from, to);
        return c;
    };
    auto hz = [] (const std::vector<float>& v, size_t from = 0)
    {
        int crossings = 0;
        for (size_t i = from + 1; i < v.size(); ++i)
            if (v[i - 1] < 0.0f && v[i] >= 0.0f)
                ++crossings;
        return (double) crossings * engineRate / (double) (v.size() - from);
    };
    auto tail = [] (const std::vector<float>& v) { return v.back(); };
    auto mean = [] (const std::vector<float>& v, size_t from = 0)
    {
        double sum = 0.0;
        for (size_t i = from; i < v.size(); ++i)
            sum += v[i];
        return sum / (double) (v.size() - from);
    };
    auto rms = [] (const std::vector<float>& v, size_t from = 0)
    {
        return analyse (v.data() + from, v.size() - from).rms;
    };
    auto peak = [] (const std::vector<float>& v) { return analyse (v.data(), v.size()).peak; };
    auto near = [] (double value, double target, double tolerance) { return std::abs (value - target) <= tolerance; };
    auto f = [] (double v, int digits = 1) { return digits == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, digits); };

    auto base = quietParams();
    base.assignSource = 2; // ASSIGN = velocity: a steady control voltage
    base.lfoDepth = 0.0f;  // keep the normalled LFO out of the way unless a test wants it
    base.osc2.octave = 1;  // OSC 2 at 16' = 110 Hz, so audio inputs can be told apart
    base.oscMix = 0.0f;    // the mixer passes OSC 1 only
    const size_t settle = (size_t) (engineRate * 0.05);
    const auto none = emptyPatch();

    // ── outputs ──────────────────────────────────────────────────────────
    {
        auto p = base;
        p.oscMix = 0.5f;
        p.lfoDepth = 1.0f;
        p.att2 = 0.5f;
        p.vcfFreq = 1000.0f;
        p.noise = 0.3f;
        const auto r = run (p, none, 0.8f, 0.5);
        juce::StringArray dead;
        for (auto s : { S::osc1, S::osc2, S::oscMix, S::vcf1, S::vcf2, S::overdrive, S::vca, S::output,
                        S::noise, S::env1, S::env2, S::invert, S::lfo, S::lfoUni, S::sampleHold,
                        S::mult1, S::mult2, S::midiGate, S::att1, S::att2, S::assign })
            if (peak (r[s]) < 0.01f)
                dead.add (na::patch::sourceName (s));
        check (dead.isEmpty(), "all normalled outputs carry a signal" + (dead.isEmpty() ? juce::String() : " – silent: " + dead.joinIntoString (", ")));
        bool bounded = true;
        for (const auto& o : r.out)
            bounded = bounded && analyse (o.data(), o.size()).finite && analyse (o.data(), o.size()).peak <= 4.0f;
        check (bounded, "all outputs finite and within ±4");
        check (near (hz (r[S::osc1], settle), 220.0, 3.0) && near (hz (r[S::osc2], settle), 110.0, 3.0),
               "OSC 1 / OSC 2 outputs play their oscillator (" + f (hz (r[S::osc1], settle)) + " / " + f (hz (r[S::osc2], settle)) + " Hz)");
        check (rms (r[S::vcf2], settle) > 0.01 && std::abs (rms (r[S::vcf1], settle) - rms (r[S::vcf2], settle)) > 0.01,
               "VCF 2 is a different filter response than VCF 1");
        check (tail (r[S::midiGate]) == 1.0f && near (tail (r[S::assign]), 0.8, 1.0e-4), "MIDI GATE high while a key is held, ASSIGN = velocity");
        check (mean (r[S::lfoUni]) > 0.2 && *std::min_element (r[S::lfoUni].begin(), r[S::lfoUni].end()) >= 0.0f,
               "LFO UNI is the unipolar (0..1) LFO");
    }

    // ── pitch inputs: +0.2 = +1 V = +1 octave ────────────────────────────
    {
        const auto off = run (base, none, 0.2f, 1.0);
        const auto one = run (base, patched ({ { S::assign, D::osc1Pitch } }), 0.2f, 1.0);
        const auto two = run (base, patched ({ { S::assign, D::osc2Pitch } }), 0.2f, 1.0);
        const auto both = run (base, patched ({ { S::assign, D::osc12Pitch } }), 0.2f, 1.0);
        check (near (hz (one[S::osc1], settle), 2.0 * hz (off[S::osc1], settle), 4.0) && near (hz (one[S::osc2], settle), hz (off[S::osc2], settle), 2.0),
               "OSC 1 PITCH: +1 V raises only OSC 1 by an octave (" + f (hz (off[S::osc1], settle)) + " → " + f (hz (one[S::osc1], settle)) + " Hz)");
        check (near (hz (two[S::osc2], settle), 2.0 * hz (off[S::osc2], settle), 4.0) && near (hz (two[S::osc1], settle), hz (off[S::osc1], settle), 2.0),
               "OSC 2 PITCH: +1 V raises only OSC 2 by an octave (" + f (hz (off[S::osc2], settle)) + " → " + f (hz (two[S::osc2], settle)) + " Hz)");
        check (near (hz (both[S::osc1], settle), 2.0 * hz (off[S::osc1], settle), 4.0) && near (hz (both[S::osc2], settle), 2.0 * hz (off[S::osc2], settle), 4.0),
               "OSC 1+2: +1 V raises both oscillators by an octave");
    }

    // ── waveform inputs ──────────────────────────────────────────────────
    {
        auto p = base;
        p.osc1.shape = p.osc2.shape = 0.0f; // sine
        const auto off = run (p, none, 1.0f, 0.3);
        const auto s1 = run (p, patched ({ { S::assign, D::shape1 } }), 1.0f, 0.3);
        const auto s2 = run (p, patched ({ { S::assign, D::shape2 } }), 1.0f, 0.3);
        check (std::abs (rms (s1[S::osc1], settle) - rms (off[S::osc1], settle)) > 0.1 && near (rms (s1[S::osc2], settle), rms (off[S::osc2], settle), 1.0e-3),
               "SHAPE 1 changes only OSC 1's waveform (rms " + f (rms (off[S::osc1], settle), 2) + " → " + f (rms (s1[S::osc1], settle), 2) + ")");
        check (std::abs (rms (s2[S::osc2], settle) - rms (off[S::osc2], settle)) > 0.1 && near (rms (s2[S::osc1], settle), rms (off[S::osc1], settle), 1.0e-3),
               "SHAPE 2 changes only OSC 2's waveform (rms " + f (rms (off[S::osc2], settle), 2) + " → " + f (rms (s2[S::osc2], settle), 2) + ")");

        p.osc1.shape = p.osc2.shape = 0.75f; // square
        const auto sq = run (p, none, 1.0f, 0.3);
        const auto w1 = run (p, patched ({ { S::assign, D::pw1 } }), 1.0f, 0.3);
        const auto w2 = run (p, patched ({ { S::assign, D::pw2 } }), 1.0f, 0.3);
        // The oscillator removes the DC of asymmetric pulses, so measure the duty cycle.
        auto duty = [&] (const std::vector<float>& v)
        {
            return (double) std::count_if (v.begin() + (long) settle, v.end(), [] (float x) { return x > 0.0f; })
                 / (double) (v.size() - settle);
        };
        check (std::abs (duty (w1[S::osc1]) - duty (sq[S::osc1])) > 0.3 && near (duty (w1[S::osc2]), duty (sq[S::osc2]), 0.02),
               "PW 1 changes only OSC 1's pulse width (duty " + f (duty (sq[S::osc1]) * 100.0, 0) + " → " + f (duty (w1[S::osc1]) * 100.0, 0) + " %)");
        check (std::abs (duty (w2[S::osc2]) - duty (sq[S::osc2])) > 0.3 && near (duty (w2[S::osc1]), duty (sq[S::osc1]), 0.02),
               "PW 2 changes only OSC 2's pulse width (duty " + f (duty (sq[S::osc2]) * 100.0, 0) + " → " + f (duty (w2[S::osc2]) * 100.0, 0) + " %)");
    }

    // ── audio inputs: OSC 2 (110 Hz) replaces the normalled OSC 1 (220 Hz) ─
    {
        auto p = base;
        p.delayMix = 0.0f;
        const auto check110 = [&] (D input, S watched)
        {
            const auto off = run (p, none, 1.0f, 0.5);
            const auto on = run (p, patched ({ { S::osc2, input } }), 1.0f, 0.5);
            check (near (hz (off[watched], settle), 220.0, 4.0) && near (hz (on[watched], settle), 110.0, 4.0),
                   juce::String (na::patch::destinationName (input)) + ": the patched signal replaces the normalled one at "
                       + na::patch::sourceName (watched) + " (" + f (hz (off[watched], settle)) + " → " + f (hz (on[watched], settle)) + " Hz)");
        };
        check110 (D::vcfIn, S::vcf1);
        check110 (D::odIn, S::overdrive);
        check110 (D::vcaIn, S::vca);
        check110 (D::delayIn, S::output);
    }

    // ── filter modulation ────────────────────────────────────────────────
    {
        auto p = base;
        p.osc1.shape = 0.5f; // saw
        p.vcfFreq = 300.0f;
        p.modDepth = 1.0f;
        const auto off = run (p, none, 1.0f, 0.3);
        const auto fm = run (p, patched ({ { S::assign, D::freqMod } }), 1.0f, 0.3);
        check (rms (fm[S::vcf1], settle) > 1.3 * rms (off[S::vcf1], settle),
               "FREQ MOD opens the filter (rms " + f (rms (off[S::vcf1], settle), 3) + " → " + f (rms (fm[S::vcf1], settle), 3) + ")");

        p.vcfFreq = 1000.0f;
        p.vcfReso = 0.0f;
        const auto dry = run (p, none, 1.0f, 0.3);
        const auto res = run (p, patched ({ { S::assign, D::res } }), 1.0f, 0.3);
        double diff = 0.0;
        for (size_t i = settle; i < dry[S::vcf1].size(); ++i)
            diff += std::pow (res[S::vcf1][i] - dry[S::vcf1][i], 2.0);
        diff = std::sqrt (diff / (double) (dry[S::vcf1].size() - settle));
        check (diff > 0.05 * rms (dry[S::vcf1], settle), "RES changes the filter resonance (difference " + f (diff, 3) + ")");
    }

    // ── VCA CV, delay time ───────────────────────────────────────────────
    {
        auto p = base;
        p.env1 = { 0.001f, 0.01f, 0.0f, 0.01f }; // ENV 1 decays to silence
        const auto off = run (p, none, 1.0f, 0.3);
        const auto on = run (p, patched ({ { S::midiGate, D::vcaCv } }), 1.0f, 0.3);
        const auto last = off[S::vca].size() - 4800;
        check (rms (off[S::vca], last) < 1.0e-4 && rms (on[S::vca], last) > 0.05, "VCA CV replaces ENV 1 as the VCA's control");

        p = base;
        p.delayMix = 1.0f;
        p.delayRepeats = 0.0f;
        p.delayTime = 0.1f;
        p.env1 = { 0.001f, 0.01f, 0.0f, 0.01f }; // a short click to time the echo
        auto onset = [&] (const Run& r)
        {
            // first echo after the dry click has died away
            const auto& o = r[S::output];
            for (size_t i = (size_t) (engineRate * 0.05); i < o.size(); ++i)
                if (std::abs (o[i]) > 0.02f)
                    return (double) i / engineRate;
            return -1.0;
        };
        const auto t0 = onset (run (p, none, 0.5f, 0.6));
        const auto t1 = onset (run (p, patched ({ { S::assign, D::delayTime } }), 0.5f, 0.6));
        check (t0 > 0.05 && t1 > t0 + 0.1, "DELAY TIME lengthens the echo (" + f (t0 * 1000.0, 0) + " → " + f (t1 * 1000.0, 0) + " ms)");
    }

    // ── envelope gates, without any MIDI note ────────────────────────────
    {
        auto p = base;
        p.lfoDepth = 1.0f;
        p.lfoRate = 4.0f;
        const auto off = run (p, none, 1.0f, 0.5, false);
        const auto g1 = run (p, patched ({ { S::lfoUni, D::envGate1 } }), 1.0f, 0.5, false);
        const auto g2 = run (p, patched ({ { S::lfoUni, D::envGate2 } }), 1.0f, 0.5, false);
        check (peak (off[S::env1]) == 0.0f && peak (g1[S::env1]) > 0.5f && peak (g1[S::env2]) > 0.5f,
               "E. GATE 1 fires ENV 1 (and ENV 2 while E. GATE 2 is empty)");
        check (peak (g2[S::env2]) > 0.5f && peak (g2[S::env1]) == 0.0f, "E. GATE 2 fires only ENV 2");
    }

    // ── LFO inputs ───────────────────────────────────────────────────────
    {
        auto p = base;
        p.lfoDepth = 1.0f;
        p.lfoRate = 2.0f;
        p.lfoShape = 0.0f;
        const auto off = run (p, none, 0.2f, 2.0);
        const auto rate = run (p, patched ({ { S::assign, D::lfoRate } }), 0.2f, 2.0);
        check (near (hz (rate[S::lfo]), 2.0 * hz (off[S::lfo]), 1.0),
               "LFO RATE: +1 V doubles the LFO rate (" + f (hz (off[S::lfo])) + " → " + f (hz (rate[S::lfo])) + " Hz)");

        const auto shape = run (p, patched ({ { S::assign, D::lfoShape } }), 1.0f, 2.0);
        float maxDiff = 0.0f;
        for (size_t i = 0; i < off[S::lfo].size(); ++i)
            maxDiff = std::max (maxDiff, std::abs (shape[S::lfo][i] - off[S::lfo][i]));
        check (maxDiff > 0.2f, "LFO SHAPE changes the LFO waveform");

        // LFO TRIG restarts the LFO on a rising edge (the note starts 0.37 s in)
        p.lfoRate = 1.0f;
        const auto fresh = run (p, none, 1.0f, 0.05, false);
        const auto freeRun = run (p, none, 1.0f, 0.42, true, (int) (engineRate * 0.37));
        const auto trig = run (p, patched ({ { S::midiGate, D::lfoTrig } }), 1.0f, 0.42, true, (int) (engineRate * 0.37));
        const auto at = (size_t) (engineRate * 0.37) + 10;
        check (near (trig[S::lfo][at], fresh[S::lfo][10], 0.02) && ! near (freeRun[S::lfo][at], fresh[S::lfo][10], 0.2),
               "LFO TRIG restarts the LFO (" + f (freeRun[S::lfo][at], 2) + " free-running vs " + f (trig[S::lfo][at], 2) + " restarted)");
    }

    // ── utilities ────────────────────────────────────────────────────────
    {
        auto p = base;
        p.att1 = 0.5f;
        p.att2 = 0.5f;
        p.lfoDepth = 1.0f;
        const auto mult = run (p, patched ({ { S::assign, D::mult } }), 0.5f, 0.05);
        check (near (tail (mult[S::mult1]), 0.5, 1.0e-5) && near (tail (mult[S::mult2]), 0.5, 1.0e-5), "MULT copies its input to MULT 1 and MULT 2");

        const auto inv = run (p, patched ({ { S::assign, D::invertIn } }), 0.5f, 0.05);
        check (near (tail (inv[S::invert]), -0.5, 1.0e-5), "INVERT IN comes out inverted");

        const auto a2 = run (p, patched ({ { S::assign, D::att2In } }), 0.5f, 0.05);
        check (near (tail (a2[S::att2]), 0.25, 1.0e-4), "ATT 2 IN is scaled by the ATT 2 knob (0.5 × 0.5 = " + f (tail (a2[S::att2]), 3) + ")");

        // ATT 1 gain = knob × 1.585 (+4 dB) + ½ × ATT 1 CV (normalled: ASSIGN = 0.5)
        const auto a1 = run (p, patched ({ { S::assign, D::att1In } }), 0.5f, 0.05);
        const auto gain = std::min (0.5f * 1.585f + 0.25f, 1.585f);
        check (near (tail (a1[S::att1]), 0.5 * gain, 1.0e-4), "ATT 1 IN is scaled by the ATT 1 gain (" + f (tail (a1[S::att1]), 3) + ")");

        const auto cv = run (p, patched ({ { S::assign, D::att1In }, { S::midiGate, D::att1Cv } }), 0.5f, 0.05);
        const auto cvGain = std::min (0.5f * 1.585f + 0.5f, 1.585f);
        check (near (tail (cv[S::att1]), 0.5 * cvGain, 1.0e-4), "ATT 1 CV raises the ATT 1 gain (" + f (tail (cv[S::att1]), 3) + ")");

        const auto sums = run (p, patched ({ { S::assign, D::sum1A }, { S::assign, D::sum1B },
                                             { S::assign, D::sum2A }, { S::midiGate, D::sum2B } }), 0.3f, 0.05);
        const auto sumA = run (p, patched ({ { S::assign, D::sum1A }, { S::assign, D::sum2B } }), 0.3f, 0.05);
        check (near (tail (sums[S::sum1]), 0.6, 1.0e-5) && near (tail (sumA[S::sum1]), 0.3, 1.0e-5),
               "SUM 1 A + SUM 1 B add up to SUM 1");
        check (near (tail (sums[S::sum2]), 1.0, 1.0e-5) && near (tail (sumA[S::sum2]), 0.3, 1.0e-5),
               "SUM 2 A + SUM 2 B add up to SUM 2 (clipped at 1)");
    }
}

void testRetrigger()
{
    std::cout << "envelopes: retrigger" << std::endl;
    using S = na::patch::Source;
    std::vector<float> buffer ((size_t) (engineRate / 2));

    for (const bool retrigger : { false, true })
    {
        auto params = quietParams();
        params.env1 = { 0.002f, 0.05f, 0.5f, 0.05f };
        params.envRetrigger = retrigger;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (60);
        engine.render (buffer.data(), (int) buffer.size());
        engine.noteOn (62); // legato
        engine.render (buffer.data(), (int) (engineRate * 0.002));
        const float env = engine.getSourceValue (S::env1);

        if (retrigger)
            check (env > 0.9f, "retrigger on: legato note restarts the attack (" + juce::String (env, 2) + ")");
        else
            check (std::abs (env - 0.5f) < 0.05f, "retrigger off (default): legato note keeps the sustain (" + juce::String (env, 2) + ")");
    }
}

void testRandomPatching()
{
    std::cout << "patchbay: random cables incl. feedback" << std::endl;
    juce::Random random (7);
    std::vector<float> buffer (4096);
    bool finite = true;
    float peak = 0.0f;

    for (int run = 0; run < 60; ++run)
    {
        auto params = quietParams();
        params.vcfReso = random.nextFloat();
        params.delayMix = random.nextFloat();
        params.delayRepeats = random.nextFloat();
        params.odDrive = random.nextFloat();
        params.slew = random.nextFloat();
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);

        auto c = emptyPatch();
        for (int i = 0; i < 24; ++i)
            patch (c, (na::patch::Source) random.nextInt ((int) na::patch::sourceCount),
                   (na::patch::Destination) random.nextInt ((int) na::patch::destinationCount));
        engine.setPatchConnections (c);
        engine.noteOn (36 + random.nextInt (48));

        for (int block = 0; block < 8; ++block)
        {
            engine.render (buffer.data(), (int) buffer.size());
            const auto stats = analyse (buffer.data(), buffer.size());
            finite = finite && stats.finite;
            peak = std::max (peak, stats.peak);
        }
    }

    check (finite, "60 random patches with 24 cables each stay finite");
    check (peak <= 4.0f, "and bounded (peak " + juce::String (peak, 2) + ")");
}

void testExtInputAndSync()
{
    std::cout << "ext input and LFO tempo sync" << std::endl;

    {   // engine: EXT INPUT is summed into the mixer → VCF
        auto params = quietParams();
        params.oscMix = 0.0f;
        params.vcaBias = 1.0f;
        na::dsp::SynthEngine withExt, without;
        for (auto* e : { &withExt, &without })
        {
            e->setParameters (params);
            e->prepare (engineRate);
        }

        std::vector<float> ext (9600), a (9600), b (9600);
        for (size_t i = 0; i < ext.size(); ++i)
            ext[i] = 0.5f * std::sin ((float) i * 0.05f);

        withExt.render (a.data(), (int) a.size(), ext.data());
        without.render (b.data(), (int) b.size());

        double diff = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            diff += std::abs (a[i] - b[i]);
        check (diff / (double) a.size() > 0.05, "EXT INPUT reaches the output through the VCF");
    }

    {   // plugin: sidechain bus feeds EXT INPUT
        auto render = [] (bool feedSidechain)
        {
            NegativeAtomiumProcessor processor;
            auto layout = processor.getBusesLayout();
            check (layout.inputBuses.size() == 2 && layout.inputBuses[1] == juce::AudioChannelSet::stereo(),
                   "sidechain 'Ext In' bus is stereo and enabled by default");
            processor.getState().getParameter (na::id::vcaBias)->setValueNotifyingHost (1.0f);
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            double energy = 0.0;
            for (int block = 0; block < 20; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample (ch, i, feedSidechain ? 0.5f * std::sin ((float) (block * 512 + i) * 0.03f) : 0.0f);
                processor.processBlock (buffer, midi);
                energy += buffer.getRMSLevel (0, 0, 512);
            }
            return energy;
        };

        const double fed = render (true), silent = render (false);
        check (std::abs (fed - silent) > 0.1, "audio on the sidechain bus changes the output (EXT INPUT)");
    }

    {   // tempo divisions from the manual
        check (na::lfoDivisionNames().size() == 21, "21 LFO clock divisions");
        check (na::lfoDivisionNames()[na::lfoDivisionIndex (0.0f)] == "4/1"
                   && na::lfoDivisionNames()[na::lfoDivisionIndex (1.0f)] == "1/64",
               "divisions run from 4/1 (CCW) to 1/64 (CW)");
        check (std::abs (na::lfoDivisionBeats (na::lfoDivisionIndex (1.0f)) - 0.0625) < 1.0e-9,
               "1/64 = a sixteenth of a beat");

        NegativeAtomiumProcessor processor;
        auto* rate = processor.getState().getParameter (na::id::lfoRate);
        auto* sync = processor.getState().getParameter (na::id::lfoSync);
        sync->setValueNotifyingHost (1.0f);
        const auto index = na::lfoDivisionNames().indexOf ("1/4");
        const float position = (float) index / 20.0f;
        check (rate->getText (position, 32) == "1/4", "RATE shows the division while synced");
        sync->setValueNotifyingHost (0.0f);
        check (rate->getText (position, 32).endsWith ("Hz"), "RATE shows Hz while free running");
    }
}

void testUserOptions()
{
    std::cout << "user options (manual chapter 5)" << std::endl;
    using S = na::patch::Source;
    std::vector<float> buffer (2000);

    auto pitchOf = [] (na::dsp::SynthEngine& e) { return e.getSourceValue (S::assign) * 60.0f + 24.0f; };

    {   // note priority, mono
        for (const auto& [priority, expected, name] : { std::tuple { 0, 64, "LAST" }, std::tuple { 1, 67, "HIGH" }, std::tuple { 2, 60, "LOW" } })
        {
            auto params = quietParams();
            params.notePriority = priority;
            na::dsp::SynthEngine engine;
            engine.setParameters (params);
            engine.prepare (engineRate);
            engine.noteOn (60); engine.noteOn (67); engine.noteOn (64);
            engine.render (buffer.data(), 100);
            check (std::abs (pitchOf (engine) - (float) expected) < 0.01f,
                   juce::String ("note priority ") + name + " plays " + juce::String (expected));
        }
    }

    {   // paraphonic HIGH/LOW: OSC 1 lowest, OSC 2 highest
        auto params = quietParams();
        params.paraphonic = true;
        params.notePriority = 3;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (64); engine.noteOn (72); engine.noteOn (60);
        engine.render (buffer.data(), 100);
        params.assignSource = 1;
        engine.setParameters (params);
        engine.render (buffer.data(), 100);
        const float osc2 = pitchOf (engine);
        params.assignSource = 0;
        engine.setParameters (params);
        engine.render (buffer.data(), 100);
        check (std::abs (pitchOf (engine) - 60.0f) < 0.01f && std::abs (osc2 - 72.0f) < 0.01f,
               "paraphonic HIGH/LOW: OSC 1 = lowest, OSC 2 = highest");
    }

    {   // sustain pedal holds released notes
        auto params = quietParams();
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.setSustainPedal (true);
        engine.noteOn (60);
        engine.noteOff (60);
        engine.render (buffer.data(), 100);
        const bool heldWhileDown = engine.isGateOpen();
        engine.setSustainPedal (false);
        check (heldWhileDown && ! engine.isGateOpen(), "sustain pedal holds the note until released");
    }

    {   // key split
        auto params = quietParams();
        params.keySplit = 60;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (48); engine.noteOn (72);
        engine.render (buffer.data(), 100);
        const float osc1 = pitchOf (engine);
        params.assignSource = 1;
        engine.setParameters (params);
        engine.render (buffer.data(), 100);
        check (std::abs (osc1 - 48.0f) < 0.01f && std::abs (pitchOf (engine) - 72.0f) < 0.01f,
               "key split at C4: low note on OSC 1, high note on OSC 2");
    }

    {   // pedal mode
        auto params = quietParams();
        params.assignSource = 1;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (50); engine.noteOff (50);
        params.pedalMode = true;
        engine.setParameters (params);
        engine.noteOn (70);
        engine.render (buffer.data(), 100);
        check (std::abs (pitchOf (engine) - 50.0f) < 0.01f, "pedal mode: OSC 2 keeps the note played before enabling");
    }

    {   // autoglide
        auto params = quietParams();
        params.autoglide1 = 12;
        params.portaTime = 0.5f;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (60);
        engine.render (buffer.data(), 10);
        const float start = pitchOf (engine);
        std::vector<float> long_ ((size_t) engineRate * 2);
        engine.render (long_.data(), (int) long_.size());
        check (start > 71.0f && std::abs (pitchOf (engine) - 60.0f) < 0.05f,
               "autoglide +12: note starts an octave up and glides to the key");
    }

    {   // LFO one-shot and depth
        auto params = quietParams();
        params.lfoKeySync = true;
        params.lfoOneShot = true;
        params.lfoShape = 0.5f; // saw
        params.lfoRate = 20.0f;
        params.lfoDepth = 0.5f;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (60);
        std::vector<float> lfoRun ((size_t) (engineRate * 0.2));
        engine.render (lfoRun.data(), (int) lfoRun.size()); // 4 cycles worth
        const float a = engine.getSourceValue (S::lfo);
        engine.render (buffer.data(), 500);
        const float b = engine.getSourceValue (S::lfo);
        check (a == b && std::abs (a - 0.5f) < 0.01f, "LFO one-shot runs once and holds; depth 50 % halves it");
    }
}

void testSelfOscillation()
{
    std::cout << "filter self-oscillation" << std::endl;
    auto params = quietParams();
    params.vcfReso = 1.0f;
    params.vcfFreq = 1000.0f;
    params.osc1.shape = params.osc2.shape = 0.0f;

    na::dsp::SynthEngine engine;
    engine.setParameters (params);
    engine.prepare (engineRate);
    engine.noteOn (60);
    std::vector<float> buffer ((size_t) engineRate);
    engine.render (buffer.data(), (int) buffer.size());
    engine.render (buffer.data(), (int) buffer.size());

    // Goertzel: level at the cutoff relative to the oscillator fundamental
    auto level = [&buffer] (double hz)
    {
        const double w = 2.0 * na::dsp::pi * hz / engineRate, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (const auto x : buffer) { const double s0 = x + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2);
    };

    double best = 0.0, bestHz = 0.0;
    for (double hz = 850.0; hz <= 1150.0; hz += 2.0)
        if (const auto l = level (hz); l > best) { best = l; bestHz = hz; }

    const double ratio = 20.0 * std::log10 (best / level (261.63));
    check (std::abs (bestHz - 1000.0) < 60.0 && ratio > -12.0,
           "full resonance sings at the cutoff alongside the oscillators (" + juce::String (bestHz, 0) + " Hz, "
               + juce::String (ratio, 1) + " dB)");
}

void testSpecification()
{
    std::cout << "hardware specification" << std::endl;
    using S = na::patch::Source;

    {   // linear attack: halfway after half the attack time
        auto params = quietParams();
        params.env1 = { 1.0f, 1.0f, 1.0f, 1.0f };
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (60);
        std::vector<float> buffer ((size_t) (engineRate * 0.5));
        engine.render (buffer.data(), (int) buffer.size());
        check (std::abs (engine.getSourceValue (S::env1) - 0.5f) < 0.01f, "attack is linear (50 % after half the time)");
    }

    {   // LFO slot order + start phase
        auto params = quietParams();
        params.lfoShape = 0.0f;            // position 1
        params.lfoSlots = { 4, 1, 2, 3, 0 }; // position 1 now plays the ramp (rev saw)
        params.lfoPhases = { 2, 0, 0, 0, 0 }; // starts at 90°
        params.lfoKeySync = true;
        params.lfoRate = 0.01f;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (60);
        std::vector<float> buffer (10);
        engine.render (buffer.data(), (int) buffer.size());
        // reverse saw 1 - 2·phase at phase 0.25 = 0.5
        check (std::abs (engine.getSourceValue (S::lfo) - 0.5f) < 0.01f,
               "LFO position 1 plays the assigned shape from its start phase");
    }

    {   // the LFO SHAPE display follows a custom shape order
        NegativeAtomiumProcessor processor;
        auto& state = processor.getState();
        auto* shape = state.getParameter (na::id::lfoShape);
        auto* slot1 = state.getParameter (na::id::lfoSlot (0));
        check (shape->getText (0.0f, 32) == "Sine", "LFO position 1 shows Sine by default");
        slot1->setValueNotifyingHost (slot1->convertTo0to1 (4.0f));
        check (shape->getText (0.0f, 32) == "Rev Saw", "LFO SHAPE display follows the custom order");
    }

    {   // mute out-of-range notes
        auto params = quietParams();
        params.muteOutOfRange = true;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (100);
        const bool mutedHigh = ! engine.isGateOpen();
        params.muteOutOfRange = false;
        engine.setParameters (params);
        engine.noteOn (100);
        check (mutedHigh && engine.isGateOpen(), "notes above C7 are muted when enabled, clamped otherwise");
    }

    {   // drift stays within a few cents but does move
        auto params = quietParams();
        params.drift = 1.0f;
        params.oscMix = 0.0f;
        na::dsp::SynthEngine engine;
        engine.setParameters (params);
        engine.prepare (engineRate);
        engine.noteOn (69);
        std::vector<float> buffer ((size_t) engineRate * 4);
        engine.render (buffer.data(), (int) buffer.size());
        const int hz = countCrossings (std::vector<float> (buffer.end() - (long) engineRate, buffer.end()));
        check (std::abs (hz - 440) <= 3, "full drift keeps A4 within a few cents (measured " + juce::String (hz) + " Hz)");
    }
}

void benchmark()
{
    std::cout << "benchmark: 30 s of audio through the plugin at 48 kHz" << std::endl;

    NegativeAtomiumProcessor processor;
    processor.getState().getParameter (na::id::delayMix)->setValueNotifyingHost (0.4f);
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    const int blocks = 48000 * 30 / 256;
    const auto start = juce::Time::getMillisecondCounterHiRes();

    for (int block = 0; block < blocks; ++block)
    {
        juce::MidiBuffer midi;
        if (block % 40 == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 36 + (block / 40) % 24, (juce::uint8) 100), 0);
        if (block % 40 == 20)
            midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    const double seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    std::cout << "  " << juce::String (seconds * 1000.0 / 30.0, 2) << " ms per second of audio ("
              << juce::String (seconds / 30.0 * 100.0, 2) << " % of one core)" << std::endl;
}

void renderScenarios (const juce::File& dir)
{
    std::cout << "rendering analysis scenarios to " << dir.getFullPathName() << std::endl;
    dir.createDirectory();

    struct Scenario { const char* name; int note; std::vector<std::pair<const char*, float>> settings; };
    const std::vector<Scenario> scenarios {
        { "saw_c7",       96, { { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 0.0f } } },
        { "square_c7",    96, { { na::id::osc1Shape, 0.75f }, { na::id::osc2Shape, 0.75f }, { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 0.0f } } },
        { "sync_c5",      72, { { na::id::oscSync, 1.0f }, { na::id::oscMix, 1.0f }, { na::id::osc2Tune, 0.73f }, { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 0.0f } } },
        { "sync_square",  72, { { na::id::oscSync, 1.0f }, { na::id::oscMix, 1.0f }, { na::id::osc2Shape, 0.75f }, { na::id::osc2Tune, 0.41f }, { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 0.0f } } },
        { "sync_tri",     72, { { na::id::oscSync, 1.0f }, { na::id::oscMix, 1.0f }, { na::id::osc2Shape, 0.25f }, { na::id::osc2Tune, 0.97f }, { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 0.0f } } },
        { "sync_tonemod", 72, { { na::id::oscSync, 1.0f }, { na::id::oscMix, 1.0f }, { na::id::osc2Shape, 1.0f }, { na::id::osc2Tune, 0.58f }, { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 0.0f } } },
        { "drive_c6",     84, { { na::id::vcfFreq, 15000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::odDrive, 1.0f }, { na::id::odTone, 1.0f } } },
        { "selfosc",      60, { { na::id::vcfReso, 1.0f }, { na::id::vcfFreq, 1000.0f }, { na::id::vcfEnvDepth, 0.0f }, { na::id::oscMix, 0.5f }, { na::id::osc1Shape, 0.0f }, { na::id::osc2Shape, 0.0f } } },
    };

    for (const auto& scenario : scenarios)
    {
        NegativeAtomiumProcessor processor;
        auto& state = processor.getState();
        state.getParameter (na::id::env1Sustain)->setValueNotifyingHost (1.0f);
        state.getParameter (na::id::delayMix)->setValueNotifyingHost (0.0f);
        state.getParameter (na::id::drift)->setValueNotifyingHost (0.0f);
        for (const auto& [id, value] : scenario.settings)
        {
            auto* param = state.getParameter (id);
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        }

        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> out (1, 48000 * 2);
        juce::AudioBuffer<float> block (2, 512);

        for (int pos = 0; pos < out.getNumSamples(); pos += 512)
        {
            juce::MidiBuffer midi;
            if (pos == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, scenario.note, (juce::uint8) 100), 0);
            block.clear();
            processor.processBlock (block, midi);
            out.copyFrom (0, pos, block, 0, 0, std::min (512, out.getNumSamples() - pos));
        }

        const auto file = dir.getChildFile (juce::String (scenario.name) + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0)
                                                                                   .withNumChannels (1)
                                                                                   .withBitsPerSample (32));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    }
}

void renderDemo (const juce::File& file)
{
    std::cout << "demo: rendering " << file.getFullPathName() << std::endl;

    na::dsp::SynthEngine engine;
    na::dsp::EngineParams params;
    params.osc2.tune     = 0.08f;
    params.vcfFreq       = 400.0f;
    params.vcfReso       = 0.65f;
    params.envDepth      = 0.6f;
    params.env2          = { 0.002f, 0.25f, 0.0f, 0.2f };
    params.env1          = { 0.002f, 0.4f, 0.6f, 0.15f };
    params.odDrive       = 0.35f;
    params.delayMix      = 0.3f;
    params.delayRepeats  = 0.45f;
    params.delayTime     = 0.375f;
    params.portaTime     = 0.04f;
    engine.setParameters (params);
    engine.prepare (engineRate);

    const int notes[] = { 36, 36, 48, 36, 39, 36, 46, 43, 36, 36, 48, 36, 41, 39, 43, 31 };
    const int step    = (int) (engineRate * 0.1875); // 16th notes at 80 bpm
    const int total   = step * 32 + (int) engineRate * 2;

    juce::AudioBuffer<float> audio (1, total);
    auto* out = audio.getWritePointer (0);
    int pos = 0;

    for (int i = 0; i < 32; ++i)
    {
        params.osc1.shape = params.osc2.shape = 0.5f + 0.25f * (float) (i / 16);
        params.vcfFreq = 300.0f + 1200.0f * (float) i / 31.0f;
        engine.setParameters (params);

        const int note = notes[i % 16];
        engine.noteOn (note);
        engine.render (out + pos, step / 2);
        engine.noteOff (note);
        engine.render (out + pos + step / 2, step - step / 2);
        pos += step;
    }

    engine.render (out + pos, total - pos);
    audio.applyGain (0.8f / std::max (0.001f, audio.getMagnitude (0, total)));

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::WavAudioFormat wav;
    auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream());
    auto writer = wav.createWriterFor (stream,
                                       juce::AudioFormatWriterOptions{}.withSampleRate (engineRate)
                                                                       .withNumChannels (1)
                                                                       .withBitsPerSample (24));
    check (writer != nullptr, "demo wav written");

    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (audio, 0, total);
}

void testPlugin (const juce::File& path, bool withEditor)
{
    std::cout << "plugin: " << path.getFileName() << std::endl;

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> types;
    vst3.findAllTypesForFile (types, path.getFullPathName());
    check (types.size() == 1, "VST3 scans as exactly one plugin");
    if (types.isEmpty())
        return;

    check (types[0]->isInstrument, "registered as instrument");

    juce::String error;
    auto instance = formats.createPluginInstance (*types[0], 48000.0, 512, error);
    check (instance != nullptr, "instantiates" + (error.isEmpty() ? juce::String() : " (" + error + ")"));
    if (instance == nullptr)
        return;

    instance->enableAllBuses();
    instance->prepareToPlay (48000.0, 512);
    check (instance->getTotalNumOutputChannels() == 2, "stereo output");

    juce::AudioBuffer<float> buffer (2, 512);
    double energy = 0.0;
    bool finite = true;

    for (int block = 0; block < 200; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 10);
        if (block == 100)
            midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);

        buffer.clear();
        instance->processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto stats = analyse (buffer.getReadPointer (ch), 512);
            finite = finite && stats.finite;
            energy += stats.rms;
        }
    }

    check (finite, "plugin output is finite");
    check (energy > 1.0, "plugin makes sound on MIDI note");

    // state round trip
    auto* first = instance->getParameters().getFirst();
    const float original = first->getValue();
    juce::MemoryBlock saved;
    instance->getStateInformation (saved);
    first->setValue (original < 0.5f ? 0.9f : 0.1f);
    instance->setStateInformation (saved.getData(), (int) saved.getSize());
    check (std::abs (first->getValue() - original) < 1.0e-3f, "state save/restore round trip");

    if (withEditor)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
        check (editor != nullptr && editor->getWidth() > 0, "editor opens");

    }

    instance->releaseResources();
}

void screenshotEditor (const juce::File& file)
{
    std::cout << "editor: snapshot to " << file.getFullPathName() << std::endl;

    NegativeAtomiumProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream (file);
    check (juce::PNGImageFormat().writeImageToStream (image, stream), "editor screenshot written");

    // resized editor: must keep its aspect ratio and scale everything with it
    const auto baseWidth = editor->getWidth(), baseHeight = editor->getHeight();
    editor->setSize (baseWidth * 2, baseHeight * 2);
    check (editor->getWidth() == baseWidth * 2 && editor->getHeight() == baseHeight * 2, "editor resizes to 200 %");

    auto* constrainer = editor->getConstrainer();
    check (constrainer != nullptr && editor->isResizable(), "editor is resizable");
    check (constrainer != nullptr
               && std::abs (constrainer->getFixedAspectRatio() - (double) baseWidth / baseHeight) < 0.001,
           "aspect ratio is locked");

    editor->setSize (baseWidth * 2, baseHeight * 2);
    const auto large = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    const auto largeFile = file.getSiblingFile (file.getFileNameWithoutExtension() + "-200.png");
    largeFile.deleteFile();
    juce::FileOutputStream largeStream (largeFile);
    check (juce::PNGImageFormat().writeImageToStream (large, largeStream), "200 % screenshot written");
    check (std::abs (processor.getUiScale() - 2.0f) < 0.01f, "UI size is stored in the plugin state");

    if (auto* naEditor = dynamic_cast<NegativeAtomiumEditor*> (editor.get()))
    {
        naEditor->showSettings (true);
        const auto settings = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto settingsFile = file.getSiblingFile (file.getFileNameWithoutExtension() + "-settings.png");
        settingsFile.deleteFile();
        juce::FileOutputStream settingsStream (settingsFile);
        check (juce::PNGImageFormat().writeImageToStream (settings, settingsStream), "settings screenshot written");
    }
}

} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    testEngineBasics();
    testPitch();
    testOscillatorRange();
    testRangeAndShapeDisplay();
    testStress();
    testPatchRouting();
    testNormalling();
    testPatchedInputs();
    testEveryPatchPoint();
    testLfoSilentWithoutDepth();
    testRetrigger();
    testRandomPatching();
    testExtInputAndSync();
    testUserOptions();
    testSelfOscillation();
    testSpecification();

    if (const int i = args.indexOf ("--scenarios"); i >= 0 && i + 1 < args.size())
        renderScenarios (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1]));

    if (args.contains ("--bench"))
        benchmark();

    if (const int i = args.indexOf ("--demo"); i >= 0 && i + 1 < args.size())
        renderDemo (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1]));

    if (const int i = args.indexOf ("--plugin"); i >= 0 && i + 1 < args.size())
        testPlugin (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1]), ! args.contains ("--no-editor"));

    if (const int i = args.indexOf ("--screenshot"); i >= 0 && i + 1 < args.size())
        screenshotEditor (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1]));

    if (const int i = args.indexOf ("--export-skin"); i >= 0 && i + 1 < args.size())
        check (exportSkin (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1])), "skin layers exported");

    std::cout << (failures == 0 ? "\nall tests passed" : "\n" + std::to_string (failures) + " test(s) failed")
              << std::endl;
    return failures == 0 ? 0 : 1;
}
