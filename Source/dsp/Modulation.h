#pragma once

#include "Oscillator.h"

#include <array>
#include <cstdint>

namespace na::dsp
{

/** One-pole coefficient for a given time constant (seconds). */
inline float smoothingCoefficient (double seconds, double sampleRate) noexcept
{
    return seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
}

/** One-pole low-pass coefficient for a cutoff frequency. */
inline float lowPassCoefficient (double hz, double sampleRate) noexcept
{
    return (float) (1.0 - std::exp (-twoPi * hz / sampleRate));
}

class WhiteNoise
{
public:
    explicit WhiteNoise (std::uint32_t seed = 0x9e3779b9u) noexcept : state (seed | 1u) {}

    float process() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) state * (2.0f / 4294967295.0f) - 1.0f;
    }

private:
    std::uint32_t state;
};

/** LFO morphing sine → triangle → ramp up → ramp down → square. Output is bipolar. */
class Lfo
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }
    void reset (double startPhase = 0.0) noexcept
    {
        phase = startPhase - std::floor (startPhase);
        finished = false;
    }

    /** Which of the five shapes sits at each of the five knob positions (manual 5.18). */
    void setSlots (const std::array<int, 5>& newSlots) noexcept { slots = newSlots; }

    void setPhase (double newPhase) noexcept { phase = newPhase - std::floor (newPhase); }

    /** One-shot: after a reset the LFO runs a single cycle, then holds its end value. */
    void setOneShot (bool shouldRunOnce) noexcept { oneShot = shouldRunOnce; }

    float process (float rateHz, float shape) noexcept
    {
        const float pos  = std::clamp (shape, 0.0f, 1.0f) * 4.0f;
        const int   idx  = std::min ((int) pos, 3);
        const float frac = pos - (float) idx;
        const float a    = waveform (idx);
        const float out  = frac <= 0.0f ? a : a + (waveform (idx + 1) - a) * frac;

        if (oneShot && finished)
            return out;

        phase += rateHz / sampleRate;

        if (phase >= 1.0)
        {
            if (oneShot)
            {
                phase = std::nextafter (1.0, 0.0);
                finished = true;
            }
            else
            {
                phase -= std::floor (phase);
            }
        }

        return out;
    }

private:
    float waveform (int slot) const noexcept
    {
        switch (slots[(size_t) std::clamp (slot, 0, 4)])
        {
            case 0:  return (float) std::sin (twoPi * phase);
            case 1:  return (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            case 2:  return (float) (2.0 * phase - 1.0);
            case 3:  return phase < 0.5 ? 1.0f : -1.0f;
            default: return (float) (1.0 - 2.0 * phase);
        }
    }

    double sampleRate = 48000.0, phase = 0.0;
    bool oneShot = false, finished = false;
    std::array<int, 5> slots { 0, 1, 2, 3, 4 };
};

/** Sample & hold with its own clock and a glide (lag) on the held value. */
class SampleAndHold
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    float process (float input, float rateHz, float glideCoef) noexcept
    {
        phase += rateHz / sampleRate;

        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = input;
        }

        value += (held - value) * glideCoef;
        return value;
    }

    /** Clocked from the S&H CLOCK input instead of the internal rate. */
    float processClocked (float input, bool sampleNow, float glideCoef) noexcept
    {
        if (sampleNow)
            held = input;

        value += (held - value) * glideCoef;
        return value;
    }

private:
    double sampleRate = 48000.0, phase = 1.0;
    float held = 0.0f, value = 0.0f;
};

} // namespace na::dsp
