#pragma once

#include "Modulation.h"

#include <vector>

namespace na::dsp
{

/** Asymmetric tanh overdrive. TONE is a tilt: left cuts the highs for a warm sound,
    right thins out the lows and lifts the highs, centre is flat. */
class Overdrive
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        lp = dcIn = dcOut = 0.0f;
        lastX = bias; // silence sits at the bias point
        lastF = logCosh (bias);
    }

    void setParameters (float drive, float tone, float level) noexcept
    {
        preGain  = std::pow (10.0f, drive * 40.0f / 20.0f);
        toneCoef = lowPassCoefficient (700.0, sampleRate);
        lowGain  = tone < 0.5f ? 1.0f + (0.5f - tone) * 0.6f : 1.0f - (tone - 0.5f) * 1.5f;
        highGain = tone < 0.5f ? 0.1f + tone * 1.8f : 1.0f + (tone - 0.5f) * 1.2f;
        outGain  = level; // LEVEL: 0 dB to -inf
    }

    float process (float x) noexcept
    {
        // First-order antiderivative anti-aliasing of tanh: the shaper output is the mean of
        // tanh over the step between samples, i.e. (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1]),
        // with F = log cosh. This removes most of the aliasing a hard-driven tanh produces.
        const double in = (double) x * preGain + bias;
        const double f  = logCosh (in);
        const double dx = in - lastX;
        const double y  = std::abs (dx) < 1.0e-6 ? std::tanh (0.5 * (in + lastX)) : (f - lastF) / dx;
        lastX = in;
        lastF = f;

        const float shaped = (float) (y - std::tanh (bias));

        // DC blocker (removes the offset introduced by the asymmetry)
        const float blocked = shaped - dcIn + 0.9995f * dcOut;
        dcIn  = shaped;
        dcOut = blocked;

        lp += (blocked - lp) * toneCoef;
        return (lp * lowGain + (blocked - lp) * highGain) * outGain;
    }

private:
    static double logCosh (double v) noexcept
    {
        const double a = std::abs (v);
        return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; // stable for large |v|
    }

    static constexpr double bias = 0.2;
    double lastX = 0.0, lastF = 0.0;
    double sampleRate = 48000.0;
    float preGain = 1.0f, toneCoef = 1.0f, lowGain = 1.0f, highGain = 1.0f, outGain = 1.0f;
    float lp = 0.0f, dcIn = 0.0f, dcOut = 0.0f;
};

/** Bucket-brigade delay modelled on an 8192-stage BBD (25–640 ms): the clock is
    stages / (2 · time), so the bandwidth falls with longer times; repeats can be
    pushed past unity and saturate; changing the time bends the pitch; a little
    clock hiss rides on the wet signal. */
class BbdDelay
{
public:
    static constexpr double minSeconds = 0.025, maxSeconds = 0.64; // range of the hardware BBD
    static constexpr double stages = 8192.0;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        buffer.assign ((size_t) (maxSeconds * sampleRate) + 8, 0.0f);
        timeCoef   = smoothingCoefficient (0.08, sampleRate);
        hpCoef     = lowPassCoefficient (60.0, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lp = hp = 0.0f;
        currentDelay = -1.0;
    }

    void setParameters (float timeSec, float repeats, float mix) noexcept
    {
        targetDelay = std::clamp ((double) timeSec, minSeconds, maxSeconds) * sampleRate;
        feedback    = repeats * 1.1f; // "repeats will be infinite and keep building"

        // BBD clock = stages / (2 · delay); its anti-alias filters sit a bit below clock / 2.
        const double clockHz = stages / (2.0 * std::clamp ((double) timeSec, minSeconds, maxSeconds));
        lpCoef = lowPassCoefficient (std::min (0.4 * clockHz, 12000.0), sampleRate);

        dryGain = std::cos (mix * (float) pi * 0.5f);
        wetGain = std::sin (mix * (float) pi * 0.5f);
    }

    float process (float input) noexcept
    {
        if (currentDelay < 0.0)
            currentDelay = targetDelay;

        currentDelay += (targetDelay - currentDelay) * timeCoef;

        const auto size = (double) buffer.size();
        double readPos  = (double) writePos - currentDelay;
        if (readPos < 0.0)
            readPos += size;

        const auto  i0   = (size_t) readPos;
        const auto  i1   = (i0 + 1) % buffer.size();
        const float frac = (float) (readPos - (double) i0);
        const float wet  = buffer[i0] + (buffer[i1] - buffer[i0]) * frac;

        hp += (wet - hp) * hpCoef;
        const float repeat = std::tanh ((wet - hp) * feedback);

        lp += (input + repeat + hiss.process() * 0.0006f - lp) * lpCoef;
        buffer[writePos] = lp;
        writePos = (writePos + 1) % buffer.size();

        return input * dryGain + wet * wetGain;
    }

private:
    std::vector<float> buffer;
    size_t writePos = 0;
    double sampleRate = 48000.0, currentDelay = -1.0, targetDelay = 0.0;
    float timeCoef = 1.0f, feedback = 0.0f, lpCoef = 1.0f, hpCoef = 0.0f;
    float dryGain = 1.0f, wetGain = 0.0f, lp = 0.0f, hp = 0.0f;
    WhiteNoise hiss;
};

} // namespace na::dsp
