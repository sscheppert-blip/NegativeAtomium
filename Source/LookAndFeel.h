#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace na::ui
{

/** The fixed palette: an anthracite powder-coated front panel with white print and amber LEDs. */
namespace colours
{
    inline const juce::Colour background { 0xff0b0c0d }; // black metal frame and recessed parts
    inline const juce::Colour panel      { 0xff2b2d31 }; // anthracite front panel
    inline const juce::Colour outline    { 0xff6a6e75 }; // print of the modulation sections
    inline const juce::Colour track      { 0xff3d4046 };
    inline const juce::Colour print      { 0xffefece4 }; // white print, frames of the audio path
    inline const juce::Colour accent     { 0xffffa933 }; // amber LEDs and lit keys
    inline const juce::Colour accentDim  { 0xff6e4615 };
    inline const juce::Colour text       { 0xffefece4 };
    inline const juce::Colour textDim    { 0xffa3a7ae };
}

/** Per-user folder holding Presets/:
    Windows %APPDATA%\NegativeAtomium, macOS ~/Library/Application Support/NegativeAtomium,
    Linux $XDG_DATA_HOME/NegativeAtomium (default ~/.local/share/NegativeAtomium). */
juce::File userDataFolder();
/** Folders earlier versions used on macOS/Linux; still read for presets. */
juce::Array<juce::File> legacyUserDataFolders();

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    /** Labels tagged with the "naColour" property ("text" or "dim") shrink to fit. */
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // Hardware parts, also used by the patchbay, the panel and the skin exporter.
    static void drawPanelSurface (juce::Graphics&, juce::Rectangle<float> area);
    static void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);
    static void drawSwitchKey (juce::Graphics&, juce::Rectangle<float> key, bool on, bool highlighted);
    static void drawLed (juce::Graphics&, juce::Point<float> centre, float diameter, bool on);
    static void drawPatchSocket (juce::Graphics&, juce::Rectangle<float>, bool active);

    // Knob parts, drawn separately so they can be exported as layers: the printed scale
    // (track), marks at the shape positions (ticks), the knob (cap: a glossy black star
    // knob turning with the value under fixed light) and its printed pointer.
    struct KnobGeometry
    {
        juce::Point<float> centre;
        float arcRadius;  // outer end of the printed scale
        float capRadius;  // outer radius of the star knob
        float stroke;     // length of the scale marks
    };

    static KnobGeometry knobGeometry (juce::Rectangle<float> bounds);
    static void drawKnobTrack (juce::Graphics&, const KnobGeometry&, float startAngle, float endAngle, bool bipolar);
    static void drawKnobTicks (juce::Graphics&, const KnobGeometry&, float startAngle, float endAngle, int numTicks);
    static void drawKnobCap (juce::Graphics&, const KnobGeometry&, float angle);
    static void drawKnobPointer (juce::Graphics&, const KnobGeometry&, float angle);
};

} // namespace na::ui
