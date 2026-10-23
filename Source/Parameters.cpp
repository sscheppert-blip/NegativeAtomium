#include "Parameters.h"

namespace na
{

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Range  = juce::NormalisableRange<float>;

    juce::String formatTime (float seconds)
    {
        return seconds < 1.0f ? juce::String (seconds * 1000.0f, seconds < 0.1f ? 1 : 0) + " ms"
                              : juce::String (seconds, 2) + " s";
    }

    juce::String formatHz (float hz)
    {
        return hz < 1000.0f ? juce::String (hz, hz < 10.0f ? 2 : 1) + " Hz"
                            : juce::String (hz / 1000.0f, 2) + " kHz";
    }

    juce::String formatPercent (float value)
    {
        return juce::String (juce::roundToInt (value * 100.0f)) + " %";
    }

    juce::String formatBipolar (float value)
    {
        const auto pct = juce::roundToInt (value * 100.0f);
        return (pct > 0 ? "+" : "") + juce::String (pct) + " %";
    }

    Range skewed (float min, float max, float centre)
    {
        Range r (min, max);
        r.setSkewForCentre (centre);
        return r;
    }

    void addFloat (Layout& layout, const char* id, const juce::String& name, Range range, float defaultValue,
                   std::function<juce::String (float)> format)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [format] (float v, int) { return format (v); })));
    }

    void addBool (Layout& layout, const char* id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    }

    void addChoice (Layout& layout, const char* id, const juce::String& name, const juce::StringArray& choices,
                    int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices,
                                                                  defaultIndex));
    }

    juce::String formatDecibels (float gain)
    {
        return gain <= 0.0001f ? juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e dB"))
                               : juce::String (juce::Decibels::gainToDecibels (gain), 1) + " dB";
    }

    juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4); // MIDI 24 = C1, as in the manual
    }

    void addInt (Layout& layout, const char* id, const juce::String& name, int min, int max, int defaultValue,
                 std::function<juce::String (int)> format)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, min, max, defaultValue,
            juce::AudioParameterIntAttributes().withStringFromValueFunction ([format] (int v, int) { return format (v); })));
    }

    juce::String formatBipolarFromCentre (float value)
    {
        return formatBipolar (value * 2.0f - 1.0f);
    }

    juce::String formatSigned (float value, int decimals, const juce::String& unit)
    {
        return (value > 0.0f ? "+" : "") + juce::String (value, decimals) + " " + unit;
    }

    void addOscillator (Layout& layout, int index, const char* tune, const char* shape, const char* width,
                        const char* octave, const char* blend)
    {
        const auto prefix = "Osc " + juce::String (index) + " ";

        // RANGE: 32' / 16' / 8' as on the panel, plus the "all LEDs lit" ±10 octave mode.
        auto rangeParam = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { octave, 1 }, prefix + "Range", juce::StringArray { "32'", "16'", "8'", "FULL" }, 2);
        auto blendParam = std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { blend, 1 }, prefix + "Shape Blend", true);
        auto* range = rangeParam.get();
        auto* blendFlag = blendParam.get();

        // ±1 octave in the octave ranges, ±10 octaves in FULL.
        addFloat (layout, tune, prefix + "Tune", Range (-1.0f, 1.0f), 0.0f, [range] (float v)
        {
            return range->getIndex() == fullRangeIndex ? formatSigned (v * 10.0f, 2, "oct")
                                                       : formatSigned (v * 12.0f, 2, "st");
        });

        addFloat (layout, shape, prefix + "Shape", Range (0.0f, 1.0f), 0.5f, [blendFlag] (float v)
        {
            return describeShape (v, blendFlag->get(), ShapeSet::oscillator);
        });

        addFloat (layout, width, prefix + "Pulse Width", Range (0.0f, 1.0f), 0.5f, [] (float v)
        {
            return juce::String (juce::roundToInt (1.0f + v * 98.0f)) + " %"; // duty cycle of the square
        });

        layout.add (std::move (rangeParam));
        layout.add (std::move (blendParam));
    }

    void addEnvelope (Layout& layout, int index, const char* a, const char* d, const char* s, const char* r,
                      float attack, float decay, float sustain, float release)
    {
        // Specification: attack 300 µs–5 s, decay 2.4 ms–10 s, release 1.5 ms–6 s.
        const auto prefix = "Env " + juce::String (index) + " ";
        addFloat (layout, a, prefix + "Attack", skewed (0.0003f, 5.0f, 0.25f), attack, formatTime);
        addFloat (layout, d, prefix + "Decay", skewed (0.0024f, 10.0f, 0.5f), decay, formatTime);
        addFloat (layout, s, prefix + "Sustain", Range (0.0f, 1.0f), sustain, formatPercent);
        addFloat (layout, r, prefix + "Release", skewed (0.0015f, 6.0f, 0.4f), release, formatTime);
    }
}

namespace
{
    struct Division { const char* name; double wholeNotes; };

    constexpr Division lfoDivisions[] = {
        { "4/1", 4.0 },  { "3/1", 3.0 },     { "2/1", 2.0 },  { "3/2", 1.5 },       { "1/1", 1.0 },
        { "1/2", 0.5 },  { "3/8", 0.375 },   { "1/3", 1.0 / 3.0 }, { "1/4", 0.25 }, { "1/5", 0.2 },
        { "3/16", 0.1875 }, { "1/6", 1.0 / 6.0 }, { "1/7", 1.0 / 7.0 }, { "1/8", 0.125 }, { "3/32", 0.09375 },
        { "1/12", 1.0 / 12.0 }, { "1/16", 0.0625 }, { "1/24", 1.0 / 24.0 }, { "1/32", 0.03125 },
        { "1/48", 1.0 / 48.0 }, { "1/64", 0.015625 }
    };

    constexpr int numLfoDivisions = (int) std::size (lfoDivisions);
}

const juce::StringArray& lfoDivisionNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& d : lfoDivisions)
            result.add (d.name);
        return result;
    }();
    return names;
}

int lfoDivisionIndex (float rateNormalised)
{
    return juce::jlimit (0, numLfoDivisions - 1, juce::roundToInt (rateNormalised * (float) (numLfoDivisions - 1)));
}

double lfoDivisionBeats (int index)
{
    return lfoDivisions[juce::jlimit (0, numLfoDivisions - 1, index)].wholeNotes * 4.0;
}

const juce::StringArray& shapeNames (ShapeSet set, bool shortNames)
{
    static const juce::StringArray osc      { "Sine", "Triangle", "Saw", "Square", "Tone Mod" };
    static const juce::StringArray oscShort { "SINE", "TRI", "SAW", "SQR", "TONE" };
    static const juce::StringArray lfo      { "Sine", "Triangle", "Saw", "Square", "Rev Saw" };
    static const juce::StringArray lfoShort { "SINE", "TRI", "SAW", "SQR", "R.SAW" };

    if (set == ShapeSet::oscillator)
        return shortNames ? oscShort : osc;
    return shortNames ? lfoShort : lfo;
}

juce::String describeShape (float value, bool blend, ShapeSet set, bool shortNames)
{
    return describeShape (value, blend, shapeNames (set, shortNames), shortNames);
}

juce::StringArray lfoNamesForSlots (const std::array<int, 5>& slots, bool shortNames)
{
    const auto& names = shapeNames (ShapeSet::lfo, shortNames);
    juce::StringArray result;
    for (const auto slot : slots)
        result.add (names[juce::jlimit (0, 4, slot)]);
    return result;
}

juce::String describeShape (float value, bool blend, const juce::StringArray& names, bool shortNames)
{
    const float pos   = juce::jlimit (0.0f, 1.0f, value) * 4.0f;

    if (! blend)
        return names[juce::roundToInt (pos)];

    const int idx    = std::min ((int) pos, 3);
    const float frac = pos - (float) idx;

    if (frac < 0.03f) return names[idx];
    if (frac > 0.97f) return names[idx + 1];

    const auto arrow = juce::String (juce::CharPointer_UTF8 (" \xe2\x80\xba "));
    return shortNames ? names[idx] + arrow + names[idx + 1]
                      : names[idx] + arrow + names[idx + 1] + " " + juce::String (juce::roundToInt (frac * 100.0f)) + " %";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;

    addOscillator (layout, 1, id::osc1Tune, id::osc1Shape, id::osc1Width, id::osc1Octave, id::osc1Blend);
    addOscillator (layout, 2, id::osc2Tune, id::osc2Shape, id::osc2Width, id::osc2Octave, id::osc2Blend);

    addFloat (layout, id::oscMix, "Osc Mix", Range (0.0f, 1.0f), 0.5f, formatPercent);
    addBool (layout, id::oscSync, "Osc Sync", false);
    addBool (layout, id::paraphonic, "Paraphonic", false);
    addFloat (layout, id::noise, "Noise", Range (0.0f, 1.0f), 0.0f, formatPercent);

    addChoice (layout, id::vcfMode, "VCF Mode", { "LP", "BP", "HP" }, 0);
    addFloat (layout, id::vcfFreq, "VCF Freq", skewed (10.0f, 15000.0f, 700.0f), 2000.0f, formatHz);
    addFloat (layout, id::vcfReso, "VCF Reso", Range (0.0f, 1.0f), 0.2f, formatPercent);
    addBool (layout, id::vcfKeyTrack, "VCF Key Track", false);
    addFloat (layout, id::vcfModDepth, "VCF Mod Depth", Range (0.0f, 1.0f), 0.0f, formatPercent);
    addFloat (layout, id::vcfEnvDepth, "VCF Env Depth", Range (-1.0f, 1.0f), 0.4f, formatBipolar);

    addFloat (layout, id::vcaBias, "VCA Bias", Range (0.0f, 1.0f), 0.0f, formatPercent);

    {
        // In SYNC the rate knob selects a clock division (manual 5.7).
        auto syncParam = std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::lfoSync, 1 },
                                                                     "LFO Tempo Sync", false);
        auto* sync = syncParam.get();
        const auto rateRange = skewed (0.01f, 10000.0f, 2.0f);
        addFloat (layout, id::lfoRate, "LFO Rate", rateRange, 2.0f, [sync, rateRange] (float hz)
        {
            return sync->get() ? lfoDivisionNames()[lfoDivisionIndex (rateRange.convertTo0to1 (hz))] : formatHz (hz);
        });
        layout.add (std::move (syncParam));
    }
    {
        auto blendParam = std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::lfoBlend, 1 },
                                                                      "LFO Shape Blend", true);
        auto* blendFlag = blendParam.get();

        // Shape order per knob position (manual 5.18), so the display can follow it.
        std::array<juce::AudioParameterChoice*, 5> slotParams {};
        std::vector<std::unique_ptr<juce::AudioParameterChoice>> slots;
        for (int position = 0; position < 5; ++position)
        {
            slots.push_back (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { id::lfoSlot (position), 1 }, "LFO Position " + juce::String (position + 1) + " Shape",
                shapeNames (ShapeSet::lfo), position));
            slotParams[(size_t) position] = slots.back().get();
        }

        addFloat (layout, id::lfoShape, "LFO Shape", Range (0.0f, 1.0f), 0.0f, [blendFlag, slotParams] (float v)
        {
            std::array<int, 5> order {};
            for (size_t i = 0; i < order.size(); ++i)
                order[i] = slotParams[i]->getIndex();
            return describeShape (v, blendFlag->get(), lfoNamesForSlots (order, false), false);
        });
        layout.add (std::move (blendParam));
        for (auto& slot : slots)
            layout.add (std::move (slot));
    }
    addBool (layout, id::lfoKeySync, "LFO Key Sync", false);

    addFloat (layout, id::delayTime, "Delay Time", skewed (0.025f, 0.64f, 0.2f), 0.3f, formatTime);
    addFloat (layout, id::delayRepeats, "Delay Repeats", Range (0.0f, 1.0f), 0.3f, formatPercent);
    addFloat (layout, id::delayMix, "Delay Mix", Range (0.0f, 1.0f), 0.0f, formatPercent);

    addFloat (layout, id::odDrive, "Drive", Range (0.0f, 1.0f), 0.1f, [] (float v) { return juce::String (v * 11.0f, 1); }); // 0–11, as printed on the panel
    addFloat (layout, id::odTone, "Tone", Range (0.0f, 1.0f), 0.5f, formatBipolarFromCentre);
    addFloat (layout, id::odLevel, "Level", Range (0.0f, 1.0f), 0.7f, formatDecibels);

    addEnvelope (layout, 1, id::env1Attack, id::env1Decay, id::env1Sustain, id::env1Release, 0.003f, 0.5f, 0.7f, 0.3f);
    addEnvelope (layout, 2, id::env2Attack, id::env2Decay, id::env2Sustain, id::env2Release, 0.005f, 0.3f, 0.2f, 0.3f);

    addFloat (layout, id::shRate, "S&H Rate", skewed (0.26f, 28.0f, 3.0f), 4.0f, formatHz);
    addFloat (layout, id::shGlide, "S&H Glide", skewed (0.0005f, 1.0f, 0.05f), 0.0005f, formatTime);

    addFloat (layout, id::slew, "Slew", skewed (0.001f, 3.0f, 0.15f), 0.001f, formatTime);
    addFloat (layout, id::portaTime, "Porta Time", skewed (0.0f, 10.0f, 0.5f), 0.0f, formatTime);

    addFloat (layout, id::att1, "Attenuator 1", Range (0.0f, 1.0f), 0.5f, [] (float v) { return formatDecibels (v * att1MaxGain); });
    addFloat (layout, id::att2, "Attenuator 2", Range (0.0f, 1.0f), 0.0f, formatDecibels);

    addFloat (layout, id::volume, "Volume", Range (0.0f, 1.0f), 0.7f, formatPercent);
    addChoice (layout, id::assignSource, "Assign Output",
               { "OSC 1", "OSC 2", "VELOCITY", "MODWHEEL", "AFTERTOUCH" }, 0);
    addBool (layout, id::envRetrigger, "Envelope Retrigger", false);

    // ── user configurable options (manual chapter 5) ─────────────────────
    addChoice (layout, id::notePriority, "Note Priority", { "LAST", "HIGH", "LOW", "HIGH/LOW" }, 0);
    addInt (layout, id::bendRange, "Pitch Bend Range", 0, 24, 2, [] (int v) { return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (v) + " st"; });
    addInt (layout, id::keySplit, "Key Split", keySplitOff, 88, keySplitOff, [] (int v) { return v <= keySplitOff ? juce::String ("Off") : noteName (v); });
    addBool (layout, id::pedalMode, "Pedal Mode", false);
    auto glideText = [] (int v) { return v == 0 ? juce::String ("Off") : (v > 0 ? "+" : "") + juce::String (v) + " st"; };
    addInt (layout, id::autoglide1, "Osc 1 Autoglide", -12, 12, 0, glideText);
    addInt (layout, id::autoglide2, "Osc 2 Autoglide", -12, 12, 0, glideText);
    addBool (layout, id::tuneBypass1, "Osc 1 Tune Bypass", false);
    addBool (layout, id::tuneBypass2, "Osc 2 Tune Bypass", false);
    addFloat (layout, id::lfoDepth, "LFO Depth", Range (0.01f, 1.0f), 1.0f, formatPercent);
    addBool (layout, id::lfoOneShot, "LFO One Shot", false);
    addBool (layout, id::lfoRetrigger, "LFO Retrigger", true);
    addInt (layout, id::lfoKeyRate, "LFO Key Rate", lfoKeyRateOff, 108, lfoKeyRateOff, [] (int v) { return v <= lfoKeyRateOff ? juce::String ("Off") : noteName (v); });
    addChoice (layout, id::vcfModSource, "VCF Mod Source", { "OFF", "VELOCITY", "MOD WHEEL", "AFTERTOUCH" }, 0);
    addFloat (layout, id::vcfModAmount, "VCF Mod Amount", Range (0.01f, 1.0f), 0.5f, formatPercent);
    addBool (layout, id::muteOutOfRange, "Mute Out Of Range Notes", false);
    addFloat (layout, id::drift, "Analog Drift", Range (0.0f, 1.0f), 0.3f, formatPercent);

    // LFO shape order and start phase per knob position (manual 5.18)
    juce::StringArray phases;
    for (int eighth = 0; eighth < 8; ++eighth)
        phases.add (juce::String (eighth * 45) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")));

    for (int position = 0; position < 5; ++position)
    {
        const auto prefix = "LFO Position " + juce::String (position + 1) + " ";
        addChoice (layout, id::lfoPhase (position), prefix + "Start Phase", phases, 0);
    }

    return layout;
}

} // namespace na
