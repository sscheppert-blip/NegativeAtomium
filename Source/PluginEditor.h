#pragma once

#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include "Parameters.h"

namespace na::ui
{

using Apvts = juce::AudioProcessorValueTreeState;

/** Rotary knob with its name underneath; the value pops up while dragging. */
class Knob final : public juce::Component, private juce::Timer
{
public:
    Knob (Apvts& state, const char* paramId, const juce::String& name, bool bipolar = false);
    void resized() override;

    /** Shape knob: dots at the waveform positions, and the label names the current waveform. */
    Knob& showsShape (const char* blendParamId, na::ShapeSet set);
    Knob& showsLfoShape (const char* blendParamId);

    /** LFO rate knob: the label shows the clock division while tempo sync is on. */
    Knob& showsDivisionWhenSynced (const char* syncParamId);

    /** Largest diameter the knob grows to inside its cell. */
    Knob& setKnobSize (int diameter);

    juce::Slider& getSlider() noexcept { return slider; }
    juce::Label& getLabel() noexcept { return label; }

private:
    void timerCallback() override;
    void setLabelSource (std::function<juce::String()> source);

    Apvts& state;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<Apvts::SliderAttachment> attachment;
    std::function<juce::String()> labelText;
    juce::String name;
    int knobSize = 56;
};

/** Rubber key that steps a choice parameter on to its next option, like the hardware's
    MODE and RANGE keys; its name is printed underneath. */
class StepButton final : public juce::Component
{
public:
    StepButton (Apvts& state, const char* paramId, juce::String name, std::vector<int> optionOrder = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::vector<int> order;
    juce::String name;
    int selected = 0;
    bool pressed = false;
};

/** Front-panel style selector for a choice parameter: one LED per option, click to pick
    (octave LEDs, filter mode). Labels can be hidden when a neighbour already shows them. */
class LedSelector final : public juce::Component
{
public:
    LedSelector (Apvts& state, const char* paramId, bool vertical, bool showLabels,
                 std::vector<int> optionOrder = {}, juce::StringArray labels = {});

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> slot (int position) const;

    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::vector<int> order;
    juce::StringArray labels;
    bool vertical, showLabels;
    int selected = 0;
};

/** A cell holding small switches (toggles and choice boxes) stacked vertically. */
class SwitchStack final : public juce::Component
{
public:
    explicit SwitchStack (Apvts& s) : state (s) {}

    SwitchStack& toggle (const char* paramId, const juce::String& text);
    SwitchStack& choice (const char* paramId);
    SwitchStack& caption (const juce::String& text);
    void resized() override;

    const juce::OwnedArray<juce::Component>& getItems() const noexcept { return items; }

private:
    Apvts& state;
    juce::OwnedArray<juce::Component> items;
    juce::OwnedArray<Apvts::ButtonAttachment> buttonAttachments;
    juce::OwnedArray<Apvts::ComboBoxAttachment> comboAttachments;
};

/** Framed panel with its name in a tab on the bottom edge, like the hardware.
    Cells are laid out in an even grid unless the editor places them itself. */
class Section final : public juce::Component
{
public:
    Section (juce::String title, int rows = 1);

    /** Tab names along the bottom edge, at fractions of the width (default: the title, centred).
        An empty list draws no tab. */
    void setTabs (std::vector<std::pair<juce::String, float>> newTabs) { tabs = std::move (newTabs); }
    /** Sections on the audio path get an accent frame so the signal flow stands out. */
    void setSignalPath (bool isOnPath) { signalPath = isOnPath; }
    /** The cells keep the bounds the editor gives them. */
    void setManualLayout (bool manual) { manualLayout = manual; }

    static constexpr int tabHeight = 16;

    template <typename CellType, typename... Args>
    CellType& add (Args&&... args)
    {
        auto* cell = new CellType (std::forward<Args> (args)...);
        cells.add (cell);
        addAndMakeVisible (cell);
        return *cell;
    }

    void setNote (juce::String text) { note = std::move (text); }

    void paint (juce::Graphics&) override;
    void resized() override;

    const juce::String& getTitle() const noexcept { return title; }
    const juce::OwnedArray<juce::Component>& getCells() const noexcept { return cells; }

    // paint parts (also used by the skin exporter)
    void paintPanel (juce::Graphics&) const;
    void paintTitle (juce::Graphics&) const;
    void paintNote (juce::Graphics&) const;

private:
    juce::String title, note;
    int rows;
    juce::OwnedArray<juce::Component> cells;
    std::vector<std::pair<juce::String, float>> tabs;
    bool signalPath = false, manualLayout = false;
};

/** The options the hardware hides behind button combinations (manual chapter 5). */
class SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel (Apvts& state);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Section& section (const char* utf8Title);
    juce::OwnedArray<Section> sections;
};

} // namespace na::ui

class NegativeAtomiumEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NegativeAtomiumEditor (NegativeAtomiumProcessor&);
    ~NegativeAtomiumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    const juce::OwnedArray<na::ui::Section>& getSections() const noexcept { return sections; }
    void showSettings (bool shouldShow);

    // paint parts (also used by the skin exporter)
    static void paintBackground (juce::Graphics&, juce::Rectangle<int> area);
    static void paintLogo (juce::Graphics&);
    static void paintHeaderText (juce::Graphics&, int editorWidth);

    static constexpr float minScale = 0.6f, maxScale = 2.5f;

private:
    /** Holds the whole UI at its 1x layout size; the editor scales it as one piece. */
    struct Content final : public juce::Component
    {
        void paint (juce::Graphics&) override;
    };

    na::ui::Section& section (const juce::String& title, int rows = 1);
    void layoutSections();
    void populatePresets();
    void selectPreset (int itemId);
    void savePreset();
    void showAbout();

    NegativeAtomiumProcessor& synthProcessor;
    bool sizeRestored = false;
    na::ui::LookAndFeel lookAndFeel;
    juce::TextButton settingsButton { "SETTINGS" };
    juce::TextButton aboutButton;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton { "SAVE" };
    juce::Array<juce::File> presets;
    std::unique_ptr<na::ui::SettingsPanel> settingsPanel;
    Content content;
    juce::OwnedArray<na::ui::Section> sections;

    na::ui::Section* oscillators = nullptr;
    na::ui::Section* vcf = nullptr;
    na::ui::Section* lfo = nullptr;
    na::ui::Section* env1 = nullptr;
    na::ui::Section* env2 = nullptr;
    na::ui::Section* noiseVca = nullptr;
    na::ui::Section* attenuators = nullptr;
    na::ui::Section* overdrive = nullptr;
    na::ui::Section* delay = nullptr;
    na::ui::Section* output = nullptr;
    na::ui::Section* sampleHold = nullptr;
    na::ui::Section* slew = nullptr;
    na::ui::Section* patchbay = nullptr;

    class PatchBay final : public juce::Component
    {
    public:
        explicit PatchBay (NegativeAtomiumProcessor& p) : processor (p) {}

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        struct Port
        {
            juce::Point<float> centre;
            juce::Rectangle<float> label;
        };

        juce::Rectangle<float> panelBounds (bool outputs) const;
        float socketSize() const;
        Port port (bool output, int index) const;
        juce::Point<float> sourcePort (int index) const;
        juce::Point<float> destinationPort (int index) const;
        int sourceAt (juce::Point<float>) const;
        int destinationAt (juce::Point<float>) const;

        NegativeAtomiumProcessor& processor;
        int selectedSource = -1;
    } patchBayView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegativeAtomiumEditor)
};
