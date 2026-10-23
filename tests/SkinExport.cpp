// Exports the editor as separate transparent layers (PNG) plus a manifest, which
// tools/make_psd.py turns into layered Photoshop files for redesigning the skin.

#include "SkinExport.h"
#include "PluginEditor.h"

namespace
{

using namespace na::ui;

class LayerWriter
{
public:
    LayerWriter (juce::File directory, juce::String documentName, juce::Rectangle<int> canvas, float scaleFactor)
        : dir (std::move (directory)), name (std::move (documentName)), scale (scaleFactor)
    {
        dir.createDirectory();
        document->setProperty ("name", name);
        document->setProperty ("width", juce::roundToInt ((float) canvas.getWidth() * scale));
        document->setProperty ("height", juce::roundToInt ((float) canvas.getHeight() * scale));
    }

    void add (const juce::String& group, const juce::String& layerName, juce::Rectangle<int> bounds,
              const std::function<void (juce::Graphics&)>& paint, bool visible = true)
    {
        const auto area = bounds.expanded (2);
        juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) area.getWidth() * scale),
                           juce::roundToInt ((float) area.getHeight() * scale), true);
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY())
                                .scaled (scale));
            paint (g);
        }

        const auto file = dir.getChildFile (name + "_" + juce::String (layers.size()).paddedLeft ('0', 4) + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);

        auto* layer = new juce::DynamicObject();
        layer->setProperty ("group", group);
        layer->setProperty ("name", layerName);
        layer->setProperty ("file", file.getFileName());
        layer->setProperty ("left", juce::roundToInt ((float) area.getX() * scale));
        layer->setProperty ("top", juce::roundToInt ((float) area.getY() * scale));
        layer->setProperty ("visible", visible);
        layers.add (juce::var (layer));
    }

    bool finish()
    {
        document->setProperty ("layers", layers);
        return dir.getChildFile (name + ".json").replaceWithText (juce::JSON::toString (juce::var (document.get())));
    }

private:
    juce::File dir;
    juce::String name;
    float scale;
    juce::DynamicObject::Ptr document { new juce::DynamicObject() };
    juce::Array<juce::var> layers;
};

/** Draws a component (and its children) at its position inside the editor. */
void paintAt (juce::Graphics& g, juce::Component& editor, juce::Component& component)
{
    juce::Graphics::ScopedSaveState save (g);
    g.setOrigin (editor.getLocalPoint (&component, juce::Point<int>()));
    component.paintEntireComponent (g, false);
}

void addKnobLayers (LayerWriter& out, const juce::String& group, juce::Rectangle<float> bounds,
                    float startAngle, float endAngle, float pos, bool bipolar, int ticks = 0)
{
    const auto k     = LookAndFeel::knobGeometry (bounds);
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto area  = bounds.getSmallestIntegerContainer();

    out.add (group, "Scale", area, [&] (auto& g) { LookAndFeel::drawKnobTrack (g, k, startAngle, endAngle, bipolar); });
    if (ticks > 1)
        out.add (group, "Position Marks", area, [&] (auto& g) { LookAndFeel::drawKnobTicks (g, k, startAngle, endAngle, ticks); });
    out.add (group, "Knob", area, [&] (auto& g) { LookAndFeel::drawKnobCap (g, k, angle); });
    out.add (group, "Pointer", area, [&] (auto& g) { LookAndFeel::drawKnobPointer (g, k, angle); });
}

void exportEditor (NegativeAtomiumEditor& editor, const juce::File& dir, float scale)
{
    const auto canvas = editor.getLocalBounds();
    LayerWriter out (dir, "NegativeAtomium-GUI", canvas, scale);

    out.add ("Background", "Background", canvas, [&] (auto& g) { NegativeAtomiumEditor::paintBackground (g, canvas); });
    out.add ("Header", "Logo", { 0, 0, 80, 56 }, [] (auto& g) { NegativeAtomiumEditor::paintLogo (g); });
    out.add ("Header", "Title + Subtitle", { 0, 0, canvas.getWidth(), 56 },
             [&] (auto& g) { NegativeAtomiumEditor::paintHeaderText (g, canvas.getWidth()); });

    for (auto* section : editor.getSections())
    {
        const auto group  = "Sections/" + section->getTitle().replace ("/", "-");
        const auto bounds = section->getBounds();
        auto inSection    = [&] (auto paint) { return [&, paint] (juce::Graphics& g) { g.setOrigin (bounds.getPosition()); paint (g); }; };

        out.add (group, "Panel", bounds, inSection ([&] (auto& g) { section->paintPanel (g); }));
        out.add (group, "Title", bounds, inSection ([&] (auto& g) { section->paintTitle (g); }));
        out.add (group, "Note", bounds, inSection ([&] (auto& g) { section->paintNote (g); }));

        for (auto* cell : section->getCells())
        {
            if (auto* knob = dynamic_cast<Knob*> (cell))
            {
                auto& slider     = knob->getSlider();
                auto& label      = knob->getLabel();
                const auto knobGroup = group + "/" + label.getText();
                const auto rotary    = slider.getRotaryParameters();
                const auto sliderArea = editor.getLocalArea (&slider, slider.getLocalBounds()).toFloat();

                addKnobLayers (out, knobGroup, sliderArea, rotary.startAngleRadians, rotary.endAngleRadians,
                               (float) slider.valueToProportionOfLength (slider.getValue()),
                               slider.getProperties()["bipolar"], slider.getProperties()["ticks"]);

                const auto labelArea = editor.getLocalArea (&label, label.getLocalBounds());
                out.add (knobGroup, "Label", labelArea, [&] (auto& g)
                {
                    g.setColour (label.findColour (juce::Label::textColourId));
                    g.setFont (label.getFont());
                    g.drawFittedText (label.getText(), labelArea, label.getJustificationType(), 1);
                });
            }
            else if (auto* stack = dynamic_cast<SwitchStack*> (cell))
            {
                for (auto* item : stack->getItems())
                {
                    const auto area = editor.getLocalArea (item, item->getLocalBounds());

                    if (auto* toggle = dynamic_cast<juce::ToggleButton*> (item))
                    {
                        const auto text = toggle->getButtonText();
                        const bool wasOn = toggle->getToggleState();

                        for (const bool on : { false, true })
                        {
                            toggle->setToggleState (on, juce::dontSendNotification);
                            out.add (group + "/" + text, on ? "On" : "Off", area,
                                     [&] (auto& g) { paintAt (g, editor, *toggle); }, on == wasOn);
                        }

                        toggle->setToggleState (wasOn, juce::dontSendNotification);
                    }
                    else if (auto* combo = dynamic_cast<juce::ComboBox*> (item))
                    {
                        out.add (group, "Choice " + combo->getText(), area, [&] (auto& g) { paintAt (g, editor, *combo); });
                    }
                }
            }
        }
    }

    out.finish();
}

/** Large single-knob template to design one knob and reuse it everywhere. */
void exportKnobTemplate (const juce::File& dir)
{
    const juce::Rectangle<int> canvas (0, 0, 80, 80);
    LayerWriter out (dir, "Knob-Template", canvas, 8.0f);

    juce::Slider reference;
    reference.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    const auto rotary = reference.getRotaryParameters();
    const auto bounds = canvas.toFloat().reduced (2.0f);

    out.add ("Background", "Panel colour", canvas, [&] (auto& g) { g.fillAll (colours::panel); });
    addKnobLayers (out, "Knob (pointer at 12 o'clock)", bounds, rotary.startAngleRadians, rotary.endAngleRadians, 0.5f, false);
    out.finish();
}

/** Switch states and the choice box at a large scale. */
void exportSwitchTemplate (const juce::File& dir)
{
    const juce::Rectangle<int> canvas (0, 0, 120, 110);
    LayerWriter out (dir, "Switch-Template", canvas, 4.0f);
    LookAndFeel lnf;

    juce::ToggleButton toggle ("RANGE");
    toggle.setLookAndFeel (&lnf);
    toggle.setBounds (10, 10, 100, 24);

    juce::ComboBox combo;
    combo.setLookAndFeel (&lnf);
    combo.addItemList ({ "8'", "16'", "32'" }, 1);
    combo.setSelectedId (1, juce::dontSendNotification);
    combo.setBounds (10, 76, 100, 24);

    auto at = [] (juce::Component& c) { return [&c] (juce::Graphics& g)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOrigin (c.getPosition());
        c.paintEntireComponent (g, false);
    }; };

    out.add ("Background", "Panel colour", canvas, [&] (auto& g) { g.fillAll (colours::panel); });

    toggle.setToggleState (false, juce::dontSendNotification);
    out.add ("Toggle", "Off", toggle.getBounds(), at (toggle));

    toggle.setTopLeftPosition (10, 43);
    toggle.setToggleState (true, juce::dontSendNotification);
    out.add ("Toggle", "On", toggle.getBounds(), at (toggle));

    out.add ("Choice box", "Choice box", combo.getBounds(), at (combo));
    out.finish();

    toggle.setLookAndFeel (nullptr);
    combo.setLookAndFeel (nullptr);
}

} // namespace

bool exportSkin (const juce::File& dir, float scale)
{
    NegativeAtomiumProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    auto* naEditor = dynamic_cast<NegativeAtomiumEditor*> (editor.get());
    if (naEditor == nullptr)
        return false;

    exportEditor (*naEditor, dir, scale);
    exportKnobTemplate (dir);
    exportSwitchTemplate (dir);
    return true;
}
