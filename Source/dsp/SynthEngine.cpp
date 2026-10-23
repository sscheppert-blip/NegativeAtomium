#include "SynthEngine.h"

namespace na::dsp
{

void SynthEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    filter.prepare (sampleRate);
    overdrive.prepare (sampleRate);
    delay.prepare (sampleRate);
    env1.prepare (sampleRate);
    env2.prepare (sampleRate);
    lfo.prepare (sampleRate);
    sampleHold.prepare (sampleRate);

    smoothCoef = smoothingCoefficient (0.01, sampleRate);

    // Drift: white noise low-passed at 0.5 Hz, normalised to unit standard deviation.
    driftCoef = lowPassCoefficient (0.5, sampleRate);
    driftNorm = 1.0f / std::sqrt (driftCoef / 6.0f);
    setParameters (params);
    reset();
}

void SynthEngine::reset()
{
    osc1.reset();
    osc2.reset();
    filter.reset();
    overdrive.reset();
    delay.reset();
    env1.reset();
    env2.reset();
    lfo.reset();

    numHeld = 0;
    patchGate1 = patchGate2 = patchLfoTrigger = patchShClock = false;
    sources.fill (0.0f);
    slewOut = 0.0f;
    note1 = note1Target;
    note2 = note2Target;

    logCutoff  = std::log2 (params.vcfFreq);
    oscMix     = params.oscMix;
    noiseLevel = params.noise;
    vcaBias    = params.vcaBias;
    volume     = params.volume * params.volume;
    reso       = params.vcfReso;
}

void SynthEngine::setParameters (const EngineParams& newParams) noexcept
{
    // Pedal mode keeps OSC 2 on the note played just before it was enabled.
    if (newParams.pedalMode && ! params.pedalMode)
        pedalNote = lastPlayed;

    const bool priorityChanged = newParams.notePriority != params.notePriority
                              || newParams.keySplit != params.keySplit
                              || newParams.pedalMode != params.pedalMode
                              || newParams.paraphonic != params.paraphonic;
    params = newParams;

    if (priorityChanged && numHeld > 0)
        assignNotes();

    lfo.setOneShot (params.lfoOneShot && params.lfoKeySync); // one-shot needs KEY SYNC (manual 5.10)

    env1.setParameters (params.env1.attack, params.env1.decay, params.env1.sustain, params.env1.release);
    env2.setParameters (params.env2.attack, params.env2.decay, params.env2.sustain, params.env2.release);
    overdrive.setParameters (params.odDrive, params.odTone, params.odLevel);
    delay.setParameters (params.delayTime, params.delayRepeats, params.delayMix);

    glideCoef   = smoothingCoefficient (params.portaTime / 4.0, sampleRate);
    shGlideCoef = smoothingCoefficient (params.shGlide / 3.0, sampleRate);

    // SLEW: time to travel the full -1..+1 range (1 ms – 3 s on the hardware).
    slewStep = params.slew <= 0.0f ? 2.0f : (float) (2.0 / ((double) params.slew * sampleRate));

    lfo.setSlots (params.lfoSlots);
}

bool SynthEngine::isPatched (na::patch::Destination destination) const noexcept
{
    for (const auto source : patchConnections[static_cast<std::size_t> (destination)])
        if (source >= 0)
            return true;
    return false;
}

float SynthEngine::patchValue (na::patch::Destination destination) const noexcept
{
    float sum = 0.0f;
    for (const auto source : patchConnections[static_cast<std::size_t> (destination)])
        if (source >= 0 && source < (int) sources.size())
            sum += sources[(std::size_t) source];
    return std::clamp (sum, -1.0f, 1.0f);
}

void SynthEngine::gateEnvelopesFromMidi (bool open, bool retrigger) noexcept
{
    // MIDI gates the envelopes unless their E.GATE inputs are patched;
    // E.GATE 2 is normalled to E.GATE 1.
    const bool gate1Patched = isPatched (na::patch::Destination::envGate1);
    const bool gate2FollowsMidi = ! isPatched (na::patch::Destination::envGate2) && ! gate1Patched;

    if (open && ! retrigger)
        return;

    if (! gate1Patched)
        open ? env1.gateOn() : env1.gateOff();
    if (gate2FollowsMidi)
        open ? env2.gateOn() : env2.gateOff();
}

double SynthEngine::lfoStartPhase() const noexcept
{
    const auto position = (size_t) std::lround (std::clamp (params.lfoShape, 0.0f, 1.0f) * 4.0f);
    return params.lfoPhases[position] / 8.0;
}

void SynthEngine::noteOn (int note, float velocity) noexcept
{
    if (params.muteOutOfRange && (note < 24 || note > 96))
        return;

    note = std::clamp (note, 24, 96);
    noteVelocity = std::clamp (velocity, 0.0f, 1.0f);
    sustained[(size_t) note] = false;

    // move an already-held note to the top of the stack
    for (int i = 0; i < numHeld; ++i)
    {
        if (held[(size_t) i] == note)
        {
            std::copy (held.begin() + i + 1, held.begin() + numHeld, held.begin() + i);
            --numHeld;
            break;
        }
    }

    if (numHeld == (int) held.size())
    {
        std::copy (held.begin() + 1, held.end(), held.begin());
        --numHeld;
    }

    const bool legato = numHeld > 0;
    held[(size_t) numHeld++] = note;
    lastPlayed = note;
    assignNotes();

    // Autoglide: the note starts a few semitones off and glides in at PORTA speed.
    if (params.autoglide1 != 0)
        note1 = note1Target + (float) params.autoglide1;
    if (params.autoglide2 != 0)
        note2 = note2Target + (float) params.autoglide2;

    gateEnvelopesFromMidi (true, ! legato || params.envRetrigger);

    if (params.lfoKeySync && (! legato || params.lfoRetrigger))
        lfo.reset (lfoStartPhase());
}

void SynthEngine::noteOff (int note) noexcept
{
    if (params.muteOutOfRange && (note < 24 || note > 96))
        return;

    note = std::clamp (note, 24, 96);

    if (sustainDown)
    {
        sustained[(size_t) note] = true;
        return;
    }

    releaseNote (note);
}

void SynthEngine::releaseNote (int note) noexcept
{
    for (int i = 0; i < numHeld; ++i)
    {
        if (held[(size_t) i] == note)
        {
            std::copy (held.begin() + i + 1, held.begin() + numHeld, held.begin() + i);
            --numHeld;
            break;
        }
    }

    if (numHeld > 0)
        assignNotes();
    else
        gateEnvelopesFromMidi (false, true);
}

void SynthEngine::setSustainPedal (bool down) noexcept
{
    sustainDown = down;
    if (down)
        return;

    for (int note = 0; note < (int) sustained.size(); ++note)
    {
        if (sustained[(size_t) note])
        {
            sustained[(size_t) note] = false;
            releaseNote (note);
        }
    }
}

void SynthEngine::allNotesOff() noexcept
{
    numHeld = 0;
    sustained.fill (false);
    gateEnvelopesFromMidi (false, true);
}

void SynthEngine::assignNotes() noexcept
{
    if (numHeld == 0)
        return;

    const int newest = held[(size_t) numHeld - 1];

    std::array<int, 16> sorted {};
    std::copy (held.begin(), held.begin() + numHeld, sorted.begin());
    std::sort (sorted.begin(), sorted.begin() + numHeld); // ascending
    const int lowest  = sorted[0];
    const int highest = sorted[(size_t) numHeld - 1];

    if (params.keySplit > 0)
    {
        // Key split (manual 5.18): notes below the split play OSC 1, the rest OSC 2.
        for (int i = numHeld - 1; i >= 0; --i)
        {
            if (held[(size_t) i] < params.keySplit) { note1Target = (float) held[(size_t) i]; break; }
        }
        for (int i = numHeld - 1; i >= 0; --i)
        {
            if (held[(size_t) i] >= params.keySplit) { note2Target = (float) held[(size_t) i]; break; }
        }
    }
    else if (params.paraphonic && numHeld >= 2)
    {
        // Paraphonic priorities (manual 5.16).
        switch (params.notePriority)
        {
            case 1:  note1Target = (float) highest;  note2Target = (float) sorted[(size_t) numHeld - 2]; break; // two highest
            case 2:  note1Target = (float) sorted[1]; note2Target = (float) lowest;  break;                    // two lowest
            case 3:  note1Target = (float) lowest;   note2Target = (float) highest; break;                     // high/low
            default: note1Target = (float) held[(size_t) numHeld - 2]; note2Target = (float) newest; break;    // last
        }
    }
    else
    {
        const int note = params.notePriority == 1 ? highest
                       : params.notePriority == 2 ? lowest
                                                  : newest;
        note1Target = note2Target = (float) note;
    }

    if (params.pedalMode)
        note2Target = (float) pedalNote;
}

float SynthEngine::oscillatorOffset (const EngineParams::Osc& osc, bool tuneBypass) noexcept
{
    // In the three octave ranges the tune control spans roughly one octave.
    // The full-range mode spans approximately ten octaves, as on the hardware.
    // Tune bypass (manual 5.6) takes the TUNE pot out of circuit.
    const float tune = tuneBypass ? 0.0f : osc.tune;

    if (osc.range)
        return tune * 120.0f;

    return (float) (osc.octave - 2) * 12.0f + tune * 12.0f;
}

float SynthEngine::shapePosition (float shape, bool blend) noexcept
{
    shape = std::clamp (shape, 0.0f, 1.0f);
    return blend ? shape : std::round (shape * 4.0f) / 4.0f;
}

void SynthEngine::render (float* output, int numSamples, const float* extInput) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        output[i] = processSample (extInput != nullptr ? extInput[i] : 0.0f);
}

float SynthEngine::processSample (float extInput) noexcept
{
    using D = na::patch::Destination;
    using S = na::patch::Source;
    const auto& p = params;

    // Patched inputs replace their normalled signal, like a jack breaking the
    // normalling contact. Patch values are the sources of the previous sample.
    auto input = [this] (D destination, float normalled) noexcept
    {
        return isPatched (destination) ? patchValue (destination) : normalled;
    };

    std::array<float, na::patch::sourceCount> out {};
    auto set = [&out] (S source, float value) noexcept { out[(std::size_t) source] = value; };

    // ── smoothing ────────────────────────────────────────────────────────
    logCutoff  += (std::log2 (p.vcfFreq) - logCutoff) * smoothCoef;
    oscMix     += (p.oscMix - oscMix) * smoothCoef;
    noiseLevel += (p.noise - noiseLevel) * smoothCoef;
    vcaBias    += (p.vcaBias - vcaBias) * smoothCoef;
    volume     += (p.volume * p.volume - volume) * smoothCoef;
    reso       += (p.vcfReso - reso) * smoothCoef;

    // ── gates and triggers from the patchbay ─────────────────────────────
    constexpr float gateThreshold = 0.3f;   // ≈ 1.5 V of 5 V
    constexpr float clockThreshold = 0.6f;  // ≈ 3 V of 5 V

    if (isPatched (D::envGate1))
    {
        const bool gate = patchValue (D::envGate1) > gateThreshold;
        if (gate != patchGate1)
            gate ? env1.gateOn() : env1.gateOff();
        patchGate1 = gate;
    }

    if (isPatched (D::envGate2) || isPatched (D::envGate1))
    {
        const bool gate = isPatched (D::envGate2) ? patchValue (D::envGate2) > gateThreshold : patchGate1;
        if (gate != patchGate2)
            gate ? env2.gateOn() : env2.gateOff();
        patchGate2 = gate;
    }

    if (isPatched (D::lfoTrig))
    {
        const bool trigger = patchValue (D::lfoTrig) > gateThreshold;
        if (trigger && ! patchLfoTrigger)
            lfo.reset (lfoStartPhase());
        patchLfoTrigger = trigger;
    }

    // ── PORTA TIME glides between MIDI notes (before ASSIGN reads the pitch) ─
    note1 += (note1Target - note1) * glideCoef;
    note2 += (note2Target - note2) * glideCoef;

    // ── modulation sources ───────────────────────────────────────────────
    const float e1    = env1.process();
    const float e2    = env2.process();
    const float white = noise.process();

    // LFO key rate (manual 5.11): doubles per octave above the base note.
    const float keyRate  = p.lfoKeyRateNote > 0 ? std::exp2 ((note1 - (float) p.lfoKeyRateNote) / 12.0f) : 1.0f;
    const float lfoRate  = std::clamp (p.lfoRate * keyRate * std::exp2 (patchValue (D::lfoRate) * 5.0f), 0.01f, 10000.0f);
    const float lfoShape = shapePosition (p.lfoShape + 0.5f * patchValue (D::lfoShape), p.lfoBlend);
    const float lfoBi    = lfo.process (lfoRate, lfoShape) * p.lfoDepth; // LFO depth (manual 5.9)
    const float lfoUni   = 0.5f * (lfoBi + 1.0f);

    // S&H: NOISE normalled to S&H IN; S&H CLOCK replaces the internal clock.
    const float shInput = input (D::sampleHoldIn, white);
    if (isPatched (D::sampleHoldClock))
    {
        const bool clock = patchValue (D::sampleHoldClock) > clockThreshold;
        sampleHoldOut = sampleHold.processClocked (shInput, clock && ! patchShClock, shGlideCoef);
        patchShClock = clock;
    }
    else
    {
        sampleHoldOut = sampleHold.process (shInput, p.shRate, shGlideCoef);
    }

    // Attenuators: bipolar LFO → ATT 2 → ATT 1 IN; ASSIGN → ATT 1 CV.
    const float att2Out = input (D::att2In, lfoBi) * p.att2;

    const float assignOut = p.assignSource == 0 ? std::clamp ((note1 - 24.0f) / 60.0f, 0.0f, 1.0f) // 1 V/oct, C1 = 0 V
                          : p.assignSource == 1 ? std::clamp ((note2 - 24.0f) / 60.0f, 0.0f, 1.0f)
                          : p.assignSource == 2 ? noteVelocity
                          : p.assignSource == 3 ? modWheel
                                                : aftertouch;

    constexpr float att1MaxGain = 1.585f; // +4 dB
    const float att1Gain = std::clamp (p.att1 * att1MaxGain + 0.5f * input (D::att1Cv, assignOut), 0.0f, att1MaxGain);
    const float att1Out  = input (D::att1In, att2Out) * att1Gain;

    // Slew limiter: limits the rate of change of SLEW IN.
    const float slewTarget = patchValue (D::slewIn);
    slewOut += std::clamp (slewTarget - slewOut, -slewStep, slewStep);

    set (S::env1, e1);
    set (S::env2, e2);
    set (S::noise, white);
    set (S::lfo, lfoBi);
    set (S::lfoUni, lfoUni);
    set (S::sampleHold, sampleHoldOut);
    set (S::att1, att1Out);
    set (S::att2, att2Out);
    set (S::slew, slewOut);
    set (S::assign, assignOut);
    set (S::midiGate, numHeld > 0 ? 1.0f : 0.0f);
    set (S::invert, -input (D::invertIn, e2));          // ENV 2 → INVERT
    set (S::mult1, input (D::mult, lfoBi));             // bipolar LFO → MULT
    set (S::mult2, input (D::mult, lfoBi));
    set (S::sum1, std::clamp (patchValue (D::sum1A) + patchValue (D::sum1B), -1.0f, 1.0f));
    set (S::sum2, std::clamp (patchValue (D::sum2A) + patchValue (D::sum2B), -1.0f, 1.0f));

    // ── pitch: CV inputs are 1 V/oct ─────────────────────────────────────
    constexpr float semitonesPerUnit = 60.0f; // ±1 = ±5 V = ±5 octaves

    // analog drift: each oscillator wanders independently, up to ~5 cents (σ) at full amount
    drift1 += (driftNoise1.process() - drift1) * driftCoef;
    drift2 += (driftNoise2.process() - drift2) * driftCoef;
    const float driftSemis = p.drift * 0.05f * driftNorm;
    const float pitchCv12 = patchValue (D::osc12Pitch) * semitonesPerUnit;
    const float pitch1 = note1 + oscillatorOffset (p.osc1, p.tuneBypass1) + pitchBend + pitchCv12 + drift1 * driftSemis
                       + patchValue (D::osc1Pitch) * semitonesPerUnit;
    const float pitch2 = note2 + oscillatorOffset (p.osc2, p.tuneBypass2) + pitchBend + pitchCv12 + drift2 * driftSemis
                       + patchValue (D::osc2Pitch) * semitonesPerUnit;

    osc1.setFrequency (440.0 * std::exp2 ((std::clamp (pitch1, -60.0f, 160.0f) - 69.0f) / 12.0f), sampleRate);
    osc2.setFrequency (440.0 * std::exp2 ((std::clamp (pitch2, -60.0f, 160.0f) - 69.0f) / 12.0f), sampleRate);

    const float shape1 = shapePosition (p.osc1.shape + patchValue (D::shape1) * 0.5f, p.osc1.blend);
    const float shape2 = shapePosition (p.osc2.shape + patchValue (D::shape2) * 0.5f, p.osc2.blend);

    // ATT 2 is normalled to both pulse width inputs.
    const float pwm1 = input (D::pw1, att2Out) * 0.45f;
    const float pwm2 = input (D::pw2, att2Out) * 0.45f;

    const float width2 = std::clamp (p.osc2.width + pwm2, 0.0f, 1.0f);
    const float s1 = osc1.process (shape1, std::clamp (p.osc1.width + pwm1, 0.0f, 1.0f));
    float s2 = osc2.process (shape2, width2);

    if (p.sync && osc1.hasWrapped())
        s2 += osc2.hardSync (osc1.getPhase(), osc1.getIncrement(), shape2, width2);

    const float oscMixed = s1 * (1.0f - oscMix) + s2 * oscMix;
    set (S::osc1, s1);
    set (S::osc2, s2);
    set (S::oscMix, oscMixed);

    // ── VCF: mixer (OSC MIX + EXT INPUT + NOISE) normalled to VCF IN ──────
    const float vcfInput = input (D::vcfIn, oscMixed + extInput + white * noiseLevel);

    // FREQ MOD is normalled to the bipolar LFO and scaled by MOD DEPTH.
    float octaves = logCutoff
                  + p.modDepth * input (D::freqMod, lfoBi) * 3.0f
                  + p.envDepth * e2 * 6.0f;

    if (p.keyTrack)
        octaves += (note1 - 60.0f) / 12.0f;

    // VCF mod source (manual 5.13/5.14): velocity, mod wheel or aftertouch opens the filter.
    const float vcfMod = p.vcfModSource == 1 ? noteVelocity
                       : p.vcfModSource == 2 ? modWheel
                       : p.vcfModSource == 3 ? aftertouch
                                             : 0.0f;
    octaves += vcfMod * p.vcfModDepth * 4.0f;

    const auto modeIndex = std::clamp (p.vcfMode, 0, 2);
    const float modReso  = std::clamp (reso + patchValue (D::res) * 0.5f, 0.0f, 1.0f);
    // A faint noise floor (≈ -100 dB), like analog circuitry, lets the filter start self-oscillating.
    const auto filterOutputs = filter.processAll (vcfInput * 0.8f + white * 1.0e-5f, std::exp2 (std::clamp (octaves, 3.0f, 15.5f)), modReso);
    const float filtered = filterOutputs[(std::size_t) modeIndex];

    // VCF 2 is the complementary output: LP→HP, BP→LP, HP→BP.
    const int alternateMode = modeIndex == 0 ? 2 : modeIndex == 1 ? 0 : 1;
    set (S::vcf1, filtered);
    set (S::vcf2, filterOutputs[(std::size_t) alternateMode]);

    // ── VCF → OD IN → VCA IN → DELAY IN, each replaceable by a cable ─────
    const float driven = overdrive.process (input (D::odIn, filtered));
    set (S::overdrive, driven);

    // ENV 1 is normalled to VCA CV; VCA BIAS opens the VCA on top of it.
    const float vcaGain = std::clamp (input (D::vcaCv, e1) + vcaBias, 0.0f, 1.0f);
    const float amp     = input (D::vcaIn, driven) * vcaGain;
    set (S::vca, amp);

    const float delayTime = std::clamp (p.delayTime + patchValue (D::delayTime) * 0.3f, 0.024f, 0.64f);
    delay.setParameters (delayTime, p.delayRepeats, p.delayMix);
    const float output = delay.process (input (D::delayIn, amp)) * volume;
    set (S::output, output);

    sources = out;
    return std::clamp (output, -4.0f, 4.0f);
}

} // namespace na::dsp
