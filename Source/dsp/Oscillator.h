#pragma once

#include <algorithm>
#include <cmath>

namespace na::dsp
{

inline constexpr double pi    = 3.14159265358979323846;
inline constexpr double twoPi = 2.0 * pi;

/** Morphing VCO: sine → triangle → saw → square → tone mod (knob order of the hardware).
    Pulse width affects square and tone mod only. Edges are band-limited with PolyBLEP. */
class Oscillator
{
public:
    void reset (double startPhase = 0.0) noexcept
    {
        phase   = startPhase;
        wrapped = false;
        pendingCorrection = 0.0f;
    }

    void setFrequency (double hz, double sampleRate) noexcept
    {
        inc = std::clamp (hz / sampleRate, 0.0, 0.45);
    }

    double getIncrement() const noexcept { return inc; }
    double getPhase() const noexcept     { return phase; }
    bool hasWrapped() const noexcept     { return wrapped; }

    /** shape 0..1 morphs through the five waveforms, width 0..1 sets the pulse width (1..99 %). */
    float process (float shape, float width) noexcept
    {
        const float out = render (shape, width) + pendingCorrection;
        pendingCorrection = 0.0f;

        phase  += inc;
        wrapped = false;

        if (phase >= 1.0)
        {
            phase  -= 1.0;
            wrapped = true;
        }

        return out;
    }

    /** Hard sync: restart this oscillator where the master's cycle restarted.

        Call right after process() for the sample whose step contained the master's wrap.
        The reset is a step in the waveform that falls between two samples; it is smoothed
        with PolyBLEP like the regular saw and pulse edges: the returned value corrects the
        sample just rendered, and the rest is added to the next process() call. */
    float hardSync (double masterPhase, double masterInc, float shape, float width) noexcept
    {
        const double samplesSinceWrap = masterInc > 0.0 ? std::clamp (masterPhase / masterInc, 0.0, 1.0) : 0.0;

        double phaseAtSync = phase - samplesSinceWrap * inc;
        phaseAtSync -= std::floor (phaseAtSync);

        const float step = naive (0.0, shape, width) - naive (phaseAtSync, shape, width);
        const auto  d    = (float) samplesSinceWrap;

        // After the reset, render() sees phase ≈ 0 and already smooths the oscillator's own
        // wrap (a step from the end of the cycle to its start). Only the difference between
        // that and the real sync step still needs correcting on the next sample.
        const float ownWrapStep = naive (0.0, shape, width) - naive (std::nextafter (1.0, 0.0), shape, width);

        phase = std::fmod (samplesSinceWrap * inc, 1.0);
        pendingCorrection = -0.5f * (step - ownWrapStep) * (1.0f - d) * (1.0f - d);
        return 0.5f * step * d * d;
    }

private:
    static double polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }

        return 0.0;
    }

    float pulse (double pw) const noexcept
    {
        double v = phase < pw ? 1.0 : -1.0;
        v += polyBlep (phase, inc);
        v -= polyBlep (std::fmod (phase - pw + 1.0, 1.0), inc);
        return (float) (v - (2.0 * pw - 1.0)); // remove DC of asymmetric pulses
    }

    static double pulseWidth (float width) noexcept { return 0.01 + (double) width * 0.98; } // 1..99 % duty

    /** Approximation of the "tone mod" shape: a saw whose ramp is split by the pulse,
        so the pulse width moves a notch through the harmonic spectrum.
        Not yet verified against recordings of the hardware. */
    float toneMod (float width) const noexcept
    {
        const float saw = (float) (2.0 * phase - 1.0 - polyBlep (phase, inc));
        return 0.6f * (saw + pulse (pulseWidth (width)));
    }

    float waveform (int index, float width) const noexcept
    {
        switch (index)
        {
            case 0:  return (float) -std::cos (twoPi * phase);
            case 1:  return (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            case 2:  return (float) (2.0 * phase - 1.0 - polyBlep (phase, inc));
            case 3:  return pulse (pulseWidth (width));
            default: return toneMod (width);
        }
    }

    /** The waveform without band-limiting, at any phase (used to size sync steps). */
    static float naive (double p, float shape, float width) noexcept
    {
        auto pulseAt = [p] (double pw) { return (float) ((p < pw ? 1.0 : -1.0) - (2.0 * pw - 1.0)); };
        auto value = [&] (int index) -> float
        {
            switch (index)
            {
                case 0:  return (float) -std::cos (twoPi * p);
                case 1:  return (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p);
                case 2:  return (float) (2.0 * p - 1.0);
                case 3:  return pulseAt (pulseWidth (width));
                default: return 0.6f * ((float) (2.0 * p - 1.0) + pulseAt (pulseWidth (width)));
            }
        };

        const float pos  = std::clamp (shape, 0.0f, 1.0f) * 4.0f;
        const int   idx  = std::min ((int) pos, 3);
        const float frac = pos - (float) idx;
        const float a    = value (idx);
        return frac <= 0.0f ? a : a + (value (idx + 1) - a) * frac;
    }

    float render (float shape, float width) const noexcept
    {
        const float pos  = std::clamp (shape, 0.0f, 1.0f) * 4.0f;
        const int   idx  = std::min ((int) pos, 3);
        const float frac = pos - (float) idx;
        const float a    = waveform (idx, width);

        if (frac <= 0.0f)
            return a;

        return a + (waveform (idx + 1, width) - a) * frac;
    }

    double phase = 0.0, inc = 0.0;
    bool wrapped = false;
    float pendingCorrection = 0.0f;
};

} // namespace na::dsp
