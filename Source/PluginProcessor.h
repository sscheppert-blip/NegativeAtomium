#pragma once

#include "dsp/SynthEngine.h"
#include "PatchBay.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <map>
#include <string>
#include <string_view>

class NegativeAtomiumProcessor final : public juce::AudioProcessor
{
public:
    NegativeAtomiumProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::ValueTree copyPresetState();
    bool applyPresetState (const juce::ValueTree& presetState);

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    const na::patch::Connections& getPatchConnections() const noexcept { return patchConnections; }
    void setPatchConnection (na::patch::Destination destination, int sourceIndex);

    // Editor size, stored with the plugin state (message thread only).
    float getUiScale() const { return (float) state.state.getProperty ("uiScale", 1.0f); }
    void setUiScale (float scale) { state.state.setProperty ("uiScale", scale, nullptr); }

private:
    na::dsp::EngineParams readParameters() const;
    void handleMidi (const juce::MidiMessage& message);

    float param (const char* id) const;
    bool flag (const char* id) const { return param (id) >= 0.5f; }
    int choice (const char* id) const { return juce::roundToInt (param (id)); }

    juce::AudioProcessorValueTreeState state;
    // std::less<> allows lookup by string_view without allocating on the audio thread
    std::map<std::string, std::atomic<float>*, std::less<>> rawParams;

    na::dsp::SynthEngine engine;
    na::patch::Connections patchConnections = []
    {
        na::patch::Connections connections {};
        for (auto& destination : connections)
            destination.fill (-1);
        return connections;
    }();
    // Message thread edits patchConnections and publishes it here; the audio thread
    // reads these atomics once per block, so a cable change can never tear a read.
    std::array<std::atomic<int>, na::patch::destinationCount * na::patch::maxConnectionsPerDestination> sharedPatch;
    void publishPatch() noexcept;
    na::patch::Connections readSharedPatch() const noexcept;

    void applyTempoSync (na::dsp::EngineParams& params);

    static juce::String serialisePatchConnections (const na::patch::Connections&);
    void restorePatchConnections (const juce::String&);

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling, extOversampling;
    juce::AudioBuffer<float> monoBuffer, extBuffer;

    static constexpr size_t oversamplingFactorLog2 = 2; // 4x

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegativeAtomiumProcessor)
};
