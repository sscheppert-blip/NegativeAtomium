#pragma once

#include "Oscillator.h"

#include <array>

namespace na::dsp
{

/** 12 dB/oct multimode state-variable filter (TPT topology), with a soft-saturated
    integrator so high resonance stays bounded and can self-oscillate. */
class StateVariableFilter
{
public:
    enum class Mode { lowPass, bandPass, highPass };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept { ic1 = ic2 = 0.0; }

    float process (float input, float cutoffHz, float resonance, Mode mode) noexcept
    {
        const auto outputs = processAll (input, cutoffHz, resonance);
        switch (mode)
        {
            case Mode::bandPass: return outputs[1];
            case Mode::highPass: return outputs[2];
            case Mode::lowPass:
            default:             return outputs[0];
        }
    }

    std::array<float, 3> processAll (float input, float cutoffHz, float resonance) noexcept
    {
        const double fc = std::clamp ((double) cutoffHz, 10.0, sampleRate * 0.45);
        const double g  = std::tan (pi * fc / sampleRate);
        // Damping crosses zero at ~93 % resonance (k = -0.15 at full): above that the filter self-oscillates
        // (a sine at the cutoff, as on the hardware) and the integrator saturation sets its
        // level. The values keep the oscillation alive alongside a full-level input signal.
        const double k  = 2.0 - 2.15 * std::clamp ((double) resonance, 0.0, 1.0);

        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;

        const double v0 = input;
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;

        ic1 = saturate (2.0 * v1 - ic1);
        ic2 = 2.0 * v2 - ic2;

        if (! std::isfinite (ic1) || ! std::isfinite (ic2))
            reset();

        return { (float) v2,
                 (float) (v1 * std::max (k, 0.3)),
                 (float) (v0 - k * v1 - v2) };
    }

private:
    static double saturate (double s) noexcept
    {
        constexpr double limit = 5.0;
        return limit * std::tanh (s / limit);
    }

    double sampleRate = 48000.0;
    double ic1 = 0.0, ic2 = 0.0;
};

} // namespace na::dsp
