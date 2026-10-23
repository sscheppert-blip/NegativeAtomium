#pragma once

#include <array>
#include <cstdint>

namespace na::patch
{

enum class Source : std::uint8_t
{
    osc1, osc2, oscMix, vcf1, vcf2, overdrive, vca, output,
    noise, env1, env2, invert, lfo, lfoUni, sampleHold, mult1,
    mult2, midiGate, att1, att2, slew, sum1, sum2, assign, count,
    key = assign, gate = midiGate
};

enum class Destination : std::uint8_t
{
    osc1Pitch, osc2Pitch, osc12Pitch, invertIn, shape1, shape2, pw1, pw2,
    vcfIn, freqMod, res, odIn, vcaIn, vcaCv, delayIn, delayTime,
    envGate1, envGate2, sampleHoldIn, sampleHoldClock, lfoRate, lfoShape,
    lfoTrig, mult, att1In, att1Cv, att2In, slewIn, sum1A, sum1B, sum2A, sum2B,
    count
};

struct Connection
{
    Source source = Source::lfo;
    Destination destination = Destination::vcfIn;
    float amount = 1.0f;
};

constexpr auto sourceCount = static_cast<std::size_t> (Source::count);
constexpr auto destinationCount = static_cast<std::size_t> (Destination::count);
constexpr std::size_t maxConnectionsPerDestination = 4;

using Connections = std::array<std::array<int, maxConnectionsPerDestination>, destinationCount>;

inline constexpr const char* sourceName (Source source) noexcept
{
    switch (source)
    {
        case Source::osc1:       return "OSC 1";
        case Source::osc2:       return "OSC 2";
        case Source::oscMix:     return "OSC MIX";
        case Source::vcf1:       return "VCF 1";
        case Source::vcf2:       return "VCF 2";
        case Source::overdrive:  return "OVERDRIVE";
        case Source::vca:        return "VCA";
        case Source::output:     return "OUTPUT";
        case Source::noise:      return "NOISE";
        case Source::env1:       return "ENV 1";
        case Source::env2:       return "ENV 2";
        case Source::invert:     return "INVERT";
        case Source::lfo:        return "LFO";
        case Source::lfoUni:     return "LFO UNI";
        case Source::sampleHold: return "S&H";
        case Source::mult1:      return "MULT 1";
        case Source::mult2:      return "MULT 2";
        case Source::midiGate:   return "MIDI GATE";
        case Source::att1:       return "ATT 1";
        case Source::att2:       return "ATT 2";
        case Source::slew:       return "SLEW";
        case Source::sum1:       return "SUM 1";
        case Source::sum2:       return "SUM 2";
        case Source::assign:     return "ASSIGN";
        case Source::count:      return "";
        default:                 return "";
    }
}

inline constexpr const char* destinationName (Destination destination) noexcept
{
    switch (destination)
    {
        case Destination::osc1Pitch:        return "OSC 1 PITCH";
        case Destination::osc2Pitch:        return "OSC 2 PITCH";
        case Destination::osc12Pitch:       return "OSC 1+2";
        case Destination::invertIn:         return "INVERT IN";
        case Destination::shape1:           return "SHAPE 1";
        case Destination::shape2:           return "SHAPE 2";
        case Destination::pw1:              return "PW 1";
        case Destination::pw2:              return "PW 2";
        case Destination::vcfIn:            return "VCF IN";
        case Destination::freqMod:         return "FREQ MOD";
        case Destination::res:             return "RES";
        case Destination::odIn:             return "OD IN";
        case Destination::vcaIn:            return "VCA IN";
        case Destination::vcaCv:            return "VCA CV";
        case Destination::delayIn:          return "DELAY IN";
        case Destination::delayTime:        return "DELAY TIME";
        case Destination::envGate1:         return "E. GATE 1";
        case Destination::envGate2:         return "E. GATE 2";
        case Destination::sampleHoldIn:     return "S&H IN";
        case Destination::sampleHoldClock:  return "S&H CLOCK";
        case Destination::lfoRate:          return "LFO RATE";
        case Destination::lfoShape:         return "LFO SHAPE";
        case Destination::lfoTrig:          return "LFO TRIG";
        case Destination::mult:             return "MULT";
        case Destination::att1In:           return "ATT 1 IN";
        case Destination::att1Cv:           return "ATT 1 CV";
        case Destination::att2In:           return "ATT 2 IN";
        case Destination::slewIn:           return "SLEW IN";
        case Destination::sum1A:            return "SUM 1 A";
        case Destination::sum1B:            return "SUM 1 B";
        case Destination::sum2A:            return "SUM 2 A";
        case Destination::sum2B:            return "SUM 2 B";
        case Destination::count:     return "";
        default:                    return "";
    }
}

} // namespace na::patch
