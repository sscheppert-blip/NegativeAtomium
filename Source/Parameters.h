#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace na::id
{
    inline constexpr auto osc1Tune   = "osc1_tune";
    inline constexpr auto osc1Shape  = "osc1_shape";
    inline constexpr auto osc1Width  = "osc1_width";
    inline constexpr auto osc1Octave = "osc1_octave";
    inline constexpr auto osc1Blend  = "osc1_shape_blend";

    inline constexpr auto osc2Tune   = "osc2_tune";
    inline constexpr auto osc2Shape  = "osc2_shape";
    inline constexpr auto osc2Width  = "osc2_width";
    inline constexpr auto osc2Octave = "osc2_octave";
    inline constexpr auto osc2Blend  = "osc2_shape_blend";

    inline constexpr auto oscMix     = "osc_mix";
    inline constexpr auto oscSync    = "osc_sync";
    inline constexpr auto paraphonic = "paraphonic";
    inline constexpr auto noise      = "noise";

    inline constexpr auto vcfMode     = "vcf_mode";
    inline constexpr auto vcfFreq     = "vcf_freq";
    inline constexpr auto vcfReso     = "vcf_reso";
    inline constexpr auto vcfKeyTrack = "vcf_keytrack";
    inline constexpr auto vcfModDepth = "vcf_mod_depth";
    inline constexpr auto vcfEnvDepth = "vcf_env_depth";

    inline constexpr auto vcaBias = "vca_bias";

    inline constexpr auto lfoRate    = "lfo_rate";
    inline constexpr auto lfoShape   = "lfo_shape";
    inline constexpr auto lfoKeySync = "lfo_keysync";
    inline constexpr auto lfoBlend   = "lfo_shape_blend";
    inline constexpr auto lfoSync    = "lfo_sync";

    inline constexpr auto delayTime    = "delay_time";
    inline constexpr auto delayRepeats = "delay_repeats";
    inline constexpr auto delayMix     = "delay_mix";

    inline constexpr auto odDrive = "od_drive";
    inline constexpr auto odTone  = "od_tone";
    inline constexpr auto odLevel = "od_level";

    inline constexpr auto env1Attack  = "env1_attack";
    inline constexpr auto env1Decay   = "env1_decay";
    inline constexpr auto env1Sustain = "env1_sustain";
    inline constexpr auto env1Release = "env1_release";

    inline constexpr auto env2Attack  = "env2_attack";
    inline constexpr auto env2Decay   = "env2_decay";
    inline constexpr auto env2Sustain = "env2_sustain";
    inline constexpr auto env2Release = "env2_release";

    inline constexpr auto shRate  = "sh_rate";
    inline constexpr auto shGlide = "sh_glide";

    inline constexpr auto slew      = "slew";
    inline constexpr auto portaTime = "porta_time";

    inline constexpr auto att1 = "att1";
    inline constexpr auto att2 = "att2";

    inline constexpr auto volume = "volume";
    inline constexpr auto assignSource = "assign_source";
    inline constexpr auto envRetrigger = "env_retrigger";

    // user configurable options (manual chapter 5)
    inline constexpr auto notePriority   = "note_priority";
    inline constexpr auto bendRange      = "bend_range";
    inline constexpr auto keySplit       = "key_split";
    inline constexpr auto pedalMode      = "pedal_mode";
    inline constexpr auto autoglide1     = "osc1_autoglide";
    inline constexpr auto autoglide2     = "osc2_autoglide";
    inline constexpr auto tuneBypass1    = "osc1_tune_bypass";
    inline constexpr auto tuneBypass2    = "osc2_tune_bypass";
    inline constexpr auto lfoDepth       = "lfo_depth";
    inline constexpr auto lfoOneShot     = "lfo_oneshot";
    inline constexpr auto lfoRetrigger   = "lfo_retrigger";
    inline constexpr auto lfoKeyRate     = "lfo_keyrate";
    inline constexpr auto vcfModSource   = "vcf_mod_source";
    inline constexpr auto vcfModAmount   = "vcf_mod_amount";
    inline constexpr auto muteOutOfRange = "mute_out_of_range";
    inline constexpr auto drift          = "osc_drift";

    inline const char* lfoSlot (int position)  { static const char* ids[] { "lfo_slot1", "lfo_slot2", "lfo_slot3", "lfo_slot4", "lfo_slot5" }; return ids[position]; }
    inline const char* lfoPhase (int position) { static const char* ids[] { "lfo_phase1", "lfo_phase2", "lfo_phase3", "lfo_phase4", "lfo_phase5" }; return ids[position]; }
}

namespace na
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** ATTENUATOR 1 goes up to +4 dB (specification), ATTENUATOR 2 to 0 dB. */
    inline constexpr float att1MaxGain = 1.585f;

    /** Lowest value of the key split / LFO key rate parameters, meaning "off". */
    inline constexpr int keySplitOff = 23, lfoKeyRateOff = 11;

    /** RANGE switch position meaning "tune spans ±10 octaves" instead of an octave. */
    inline constexpr int fullRangeIndex = 3;

    enum class ShapeSet { oscillator, lfo };

    /** Waveform names in knob order (counter-clockwise → clockwise), as on the hardware panel. */
    const juce::StringArray& shapeNames (ShapeSet set, bool shortNames = false);

    /** LFO clock divisions from the manual (5.7), counter-clockwise → clockwise.
        Each is the length of one LFO cycle in whole notes. */
    const juce::StringArray& lfoDivisionNames();
    int lfoDivisionIndex (float rateNormalised);
    double lfoDivisionBeats (int index); // cycle length in quarter notes

    /** Describes a shape knob position, e.g. "Saw", or "Saw › Square 40 %" while blending. */
    juce::String describeShape (float value, bool blend, ShapeSet set, bool shortNames = false);

    /** Same, with the shape names per knob position given explicitly (custom LFO order). */
    juce::String describeShape (float value, bool blend, const juce::StringArray& namesPerPosition, bool shortNames);

    /** LFO names per knob position for a custom shape order (slot = shape index 0..4). */
    juce::StringArray lfoNamesForSlots (const std::array<int, 5>& slots, bool shortNames);
}
