#pragma once

#include <cmath>

namespace na::dsp
{

/** ADSR as specified for the hardware: linear attack, exponential decay and release. */
class Envelope
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    void setParameters (float attackSec, float decaySec, float sustainLevel, float releaseSec) noexcept
    {
        attackStep  = attackSec <= 0.0f ? 1.0 : 1.0 / ((double) attackSec * sampleRate);
        decayCoef   = coefficient (decaySec / 4.6);    // ~ -40 dB after decaySec
        releaseCoef = coefficient (releaseSec / 4.6);
        sustain     = sustainLevel;
    }

    void gateOn() noexcept  { stage = Stage::attack; }
    void gateOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }

    void reset() noexcept
    {
        value = 0.0;
        stage = Stage::idle;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }

    float process() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                value += attackStep;
                if (value >= 1.0)
                {
                    value = 1.0;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                value += (sustain - value) * (1.0 - decayCoef);
                break;

            case Stage::release:
                value += -value * (1.0 - releaseCoef);
                if (value < 1.0e-5)
                    reset();
                break;

            case Stage::idle:
                break;
        }

        return (float) value;
    }

private:
    enum class Stage { idle, attack, decay, release };

    double coefficient (double tau) const noexcept
    {
        return tau <= 0.0 ? 0.0 : std::exp (-1.0 / (tau * sampleRate));
    }

    double sampleRate = 48000.0;
    double attackStep = 1.0, decayCoef = 0.0, releaseCoef = 0.0, sustain = 1.0;
    double value = 0.0;
    Stage stage = Stage::idle;
};

} // namespace na::dsp
