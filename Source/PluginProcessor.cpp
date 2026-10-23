#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

#include <algorithm>

NegativeAtomiumProcessor::NegativeAtomiumProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          // Instruments get no main input in most hosts, so EXT INPUT is a sidechain bus.
                          .withInput ("Input", juce::AudioChannelSet::stereo(), false)
                          .withInput ("Ext In", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "NegativeAtomium", na::createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            rawParams[withId->getParameterID().toStdString()] = state.getRawParameterValue (withId->getParameterID());

    publishPatch();
}

float NegativeAtomiumProcessor::param (const char* id) const
{
    const auto it = rawParams.find (std::string_view (id));
    jassert (it != rawParams.end());
    return it->second->load (std::memory_order_relaxed);
}

bool NegativeAtomiumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto monoOrStereo = [] (const juce::AudioChannelSet& set, bool allowDisabled)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo()
            || (allowDisabled && set.isDisabled());
    };

    if (! monoOrStereo (layouts.getMainOutputChannelSet(), false))
        return false;

    for (const auto& input : layouts.inputBuses)
        if (! monoOrStereo (input, true))
            return false;

    return true;
}

void NegativeAtomiumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, oversamplingFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    extOversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, oversamplingFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    extOversampling->initProcessing ((size_t) samplesPerBlock);

    monoBuffer.setSize (1, samplesPerBlock);
    extBuffer.setSize (1, samplesPerBlock);

    engine.setParameters (readParameters());
    engine.setPatchConnections (readSharedPatch());
    engine.prepare (sampleRate * (double) oversampling->getOversamplingFactor());

    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));
}

void NegativeAtomiumProcessor::publishPatch() noexcept
{
    std::size_t i = 0;
    for (const auto& slots : patchConnections)
        for (const auto source : slots)
            sharedPatch[i++].store (source, std::memory_order_relaxed);
}

na::patch::Connections NegativeAtomiumProcessor::readSharedPatch() const noexcept
{
    na::patch::Connections connections {};
    std::size_t i = 0;
    for (auto& slots : connections)
        for (auto& source : slots)
            source = sharedPatch[i++].load (std::memory_order_relaxed);
    return connections;
}

void NegativeAtomiumProcessor::setPatchConnection (na::patch::Destination destination, int sourceIndex)
{
    const auto index = static_cast<std::size_t> (destination);
    if (index >= patchConnections.size())
        return;

    auto& slots = patchConnections[index];
    if (sourceIndex < 0)
    {
        slots.fill (-1);
    }
    else
    {
        const auto found = std::find (slots.begin(), slots.end(), sourceIndex);
        if (found != slots.end())
            *found = -1;
        else if (const auto free = std::find (slots.begin(), slots.end(), -1); free != slots.end())
            *free = sourceIndex;
    }
    publishPatch();
}

void NegativeAtomiumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    if (numSamples > monoBuffer.getNumSamples())
    {
        // Host sent a bigger block than announced: prepare again for the new size.
        prepareToPlay (getSampleRate(), numSamples);
    }

    auto params = readParameters();
    applyTempoSync (params);
    engine.setParameters (params);
    engine.setPatchConnections (readSharedPatch());

    // EXT INPUT: sidechain summed to mono, read before the outputs overwrite the shared channels.
    extBuffer.clear (0, 0, numSamples);
    bool hasExt = false;
    if (getBusCount (true) > 1)
    {
        const auto ext = getBusBuffer (buffer, true, 1);
        for (int ch = 0; ch < ext.getNumChannels(); ++ch)
            extBuffer.addFrom (0, 0, ext, ch, 0, numSamples, 1.0f / (float) ext.getNumChannels());
        hasExt = ext.getNumChannels() > 0;
    }

    juce::dsp::AudioBlock<float> extBlock (extBuffer.getArrayOfWritePointers(), 1, (size_t) numSamples);
    const float* extUp = hasExt ? extOversampling->processSamplesUp (extBlock).getChannelPointer (0) : nullptr;

    juce::dsp::AudioBlock<float> monoBlock (monoBuffer.getArrayOfWritePointers(), 1, (size_t) numSamples);
    auto upBlock = oversampling->processSamplesUp (monoBlock);

    float* up       = upBlock.getChannelPointer (0);
    const int upLen = (int) upBlock.getNumSamples();
    const int factor = (int) oversampling->getOversamplingFactor();
    int pos = 0;

    for (const auto metadata : midi)
    {
        const int eventPos = std::clamp (metadata.samplePosition * factor, 0, upLen);
        engine.render (up + pos, eventPos - pos, extUp != nullptr ? extUp + pos : nullptr);
        pos = std::max (pos, eventPos);
        handleMidi (metadata.getMessage());
    }

    engine.render (up + pos, upLen - pos, extUp != nullptr ? extUp + pos : nullptr);
    oversampling->processSamplesDown (monoBlock);

    for (int ch = 0; ch < getTotalNumOutputChannels(); ++ch)
        buffer.copyFrom (ch, 0, monoBuffer, 0, 0, numSamples);
    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

void NegativeAtomiumProcessor::applyTempoSync (na::dsp::EngineParams& params)
{
    if (! flag (na::id::lfoSync))
        return;

    const auto rateRange = state.getParameterRange (na::id::lfoRate);
    const auto beats = na::lfoDivisionBeats (na::lfoDivisionIndex (rateRange.convertTo0to1 (params.lfoRate)));

    double bpm = 120.0;
    juce::Optional<double> ppq;
    bool playing = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            ppq = position->getPpqPosition();
            playing = position->getIsPlaying();
        }
    }

    params.lfoRate = (float) (bpm / 60.0 / beats);

    // While the song plays, lock the phase to the bar grid (unless KEY SYNC restarts it per note).
    if (playing && ppq.hasValue() && ! params.lfoKeySync)
        engine.syncLfoPhase (*ppq / beats);
}

void NegativeAtomiumProcessor::handleMidi (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        engine.noteOff (message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        engine.allNotesOff();
    else if (message.isPitchWheel())
        engine.setPitchBend ((float) (message.getPitchWheelValue() - 8192) / 8192.0f * param (na::id::bendRange));
    else if (message.isSustainPedalOn() || message.isSustainPedalOff())
        engine.setSustainPedal (message.isSustainPedalOn());
    else if (message.isController() && message.getControllerNumber() == 1)
        engine.setModWheel ((float) message.getControllerValue() / 127.0f);
    else if (message.isChannelPressure())
        engine.setAftertouch ((float) message.getChannelPressureValue() / 127.0f);
    else if (message.isAftertouch())
        engine.setAftertouch ((float) message.getAfterTouchValue() / 127.0f);
}

na::dsp::EngineParams NegativeAtomiumProcessor::readParameters() const
{
    namespace id = na::id;
    na::dsp::EngineParams p;

    auto oscillator = [this] (const char* tune, const char* shape, const char* width, const char* range, const char* blend)
    {
        na::dsp::EngineParams::Osc osc;
        osc.tune   = param (tune);
        osc.shape  = param (shape);
        osc.width  = param (width);
        osc.range  = choice (range) == na::fullRangeIndex;
        osc.octave = std::min (choice (range), 2);
        osc.blend  = flag (blend);
        return osc;
    };

    p.osc1 = oscillator (id::osc1Tune, id::osc1Shape, id::osc1Width, id::osc1Octave, id::osc1Blend);
    p.osc2 = oscillator (id::osc2Tune, id::osc2Shape, id::osc2Width, id::osc2Octave, id::osc2Blend);

    p.oscMix     = param (id::oscMix);
    p.sync       = flag (id::oscSync);
    p.paraphonic = flag (id::paraphonic);
    p.noise      = param (id::noise);

    p.vcfMode  = choice (id::vcfMode);
    p.vcfFreq  = param (id::vcfFreq);
    p.vcfReso  = param (id::vcfReso);
    p.keyTrack = flag (id::vcfKeyTrack);
    p.modDepth = param (id::vcfModDepth);
    p.envDepth = param (id::vcfEnvDepth);

    p.vcaBias = param (id::vcaBias);

    p.lfoRate    = param (id::lfoRate);
    p.lfoShape   = param (id::lfoShape);
    p.lfoBlend   = flag (id::lfoBlend);
    p.lfoKeySync = flag (id::lfoKeySync);

    p.delayTime    = param (id::delayTime);
    p.delayRepeats = param (id::delayRepeats);
    p.delayMix     = param (id::delayMix);

    p.odDrive = param (id::odDrive);
    p.odTone  = param (id::odTone);
    p.odLevel = param (id::odLevel);

    p.env1 = { param (id::env1Attack), param (id::env1Decay), param (id::env1Sustain), param (id::env1Release) };
    p.env2 = { param (id::env2Attack), param (id::env2Decay), param (id::env2Sustain), param (id::env2Release) };

    p.shRate    = param (id::shRate);
    p.shGlide   = param (id::shGlide);
    p.slew      = param (id::slew);
    p.portaTime = param (id::portaTime);
    p.att1      = param (id::att1);
    p.att2      = param (id::att2);

    p.volume = param (id::volume);
    p.envRetrigger = flag (id::envRetrigger);

    p.notePriority   = choice (id::notePriority);
    p.keySplit       = choice (id::keySplit) <= na::keySplitOff ? 0 : choice (id::keySplit);
    p.pedalMode      = flag (id::pedalMode);
    p.autoglide1     = choice (id::autoglide1);
    p.autoglide2     = choice (id::autoglide2);
    p.tuneBypass1    = flag (id::tuneBypass1);
    p.tuneBypass2    = flag (id::tuneBypass2);
    p.lfoDepth       = param (id::lfoDepth);
    p.lfoOneShot     = flag (id::lfoOneShot);
    p.lfoRetrigger   = flag (id::lfoRetrigger);
    p.lfoKeyRateNote = choice (id::lfoKeyRate) <= na::lfoKeyRateOff ? 0 : choice (id::lfoKeyRate);
    p.vcfModSource   = choice (id::vcfModSource);
    p.vcfModDepth    = param (id::vcfModAmount);
    p.muteOutOfRange = flag (id::muteOutOfRange);
    p.drift          = param (id::drift);

    for (int position = 0; position < 5; ++position)
    {
        p.lfoSlots[(size_t) position]  = choice (id::lfoSlot (position));
        p.lfoPhases[(size_t) position] = choice (id::lfoPhase (position));
    }
    p.assignSource = choice (id::assignSource);
    return p;
}

juce::AudioProcessorEditor* NegativeAtomiumProcessor::createEditor()
{
    return new NegativeAtomiumEditor (*this);
}

void NegativeAtomiumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = copyPresetState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NegativeAtomiumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            // Projects saved by versions with themes carry the theme name; it is not a sound setting.
            xml->removeAttribute ("theme");
            applyPresetState (juce::ValueTree::fromXml (*xml));
        }
}

juce::String NegativeAtomiumProcessor::serialisePatchConnections (const na::patch::Connections& connections)
{
    juce::String patches;
    for (const auto& slots : connections)
    {
        for (const auto source : slots)
            patches << source << ',';
        patches << ';';
    }
    return patches;
}

juce::ValueTree NegativeAtomiumProcessor::copyPresetState()
{
    auto snapshot = state.copyState();
    snapshot.setProperty ("patchConnections", serialisePatchConnections (patchConnections), nullptr);
    return snapshot;
}

void NegativeAtomiumProcessor::restorePatchConnections (const juce::String& patchText)
{
    patchConnections = []
    {
        na::patch::Connections connections {};
        for (auto& destination : connections)
            destination.fill (-1);
        return connections;
    }();

    const auto destinations = juce::StringArray::fromTokens (patchText, ";", "");
    for (int d = 0; d < (int) patchConnections.size() && d < destinations.size(); ++d)
    {
        const auto slots = juce::StringArray::fromTokens (destinations[d], ",", "");
        for (int s = 0; s < (int) patchConnections[(std::size_t) d].size() && s < slots.size(); ++s)
            patchConnections[(std::size_t) d][(std::size_t) s] = slots[s].getIntValue();
    }
    publishPatch();
}

bool NegativeAtomiumProcessor::applyPresetState (const juce::ValueTree& presetState)
{
    if (! presetState.isValid() || presetState.getType() != state.state.getType())
        return false;

    state.replaceState (presetState);
    restorePatchConnections (state.state.getProperty ("patchConnections").toString());
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NegativeAtomiumProcessor();
}
