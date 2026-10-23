#include "PluginEditor.h"
#include "Parameters.h"

#include <algorithm>

namespace
{
juce::File userPresetFolder()
{
    return na::ui::userDataFolder().getChildFile ("Presets");
}

juce::Array<juce::File> findPresetFiles()
{
    // Create the folder so it is there to drop presets into.
    userPresetFolder().createDirectory();

    juce::Array<juce::File> folders { userPresetFolder() };
    for (const auto& legacy : na::ui::legacyUserDataFolders())
        folders.add (legacy.getChildFile ("Presets"));

    juce::Array<juce::File> files;
    for (const auto& folder : folders)
        if (folder.isDirectory())
            folder.findChildFiles (files, juce::File::findFiles, false, "*.napreset");

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

juce::String presetDisplayName (const juce::File& file)
{
    return file.getFileNameWithoutExtension();
}
}

namespace na::ui
{

// ── Knob ────────────────────────────────────────────────────────────────────
Knob::Knob (Apvts& s, const char* paramId, const juce::String& labelName, bool bipolar)
    : state (s), name (labelName)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);

    label.setText (labelName, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, colours::text);
    label.getProperties().set ("naColour", "text");
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<Apvts::SliderAttachment> (state, paramId, slider);

    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

Knob& Knob::showsShape (const char* blendParamId, na::ShapeSet set)
{
    slider.getProperties().set ("ticks", 5);

    auto* blend = state.getRawParameterValue (blendParamId);
    setLabelSource ([this, blend, set]
    {
        return na::describeShape ((float) slider.getValue(), blend->load() >= 0.5f, set, true);
    });
    return *this;
}

Knob& Knob::showsLfoShape (const char* blendParamId)
{
    slider.getProperties().set ("ticks", 5);

    auto* blend = state.getRawParameterValue (blendParamId);
    std::array<std::atomic<float>*, 5> slots {};
    for (int position = 0; position < 5; ++position)
        slots[(size_t) position] = state.getRawParameterValue (na::id::lfoSlot (position));

    setLabelSource ([this, blend, slots]
    {
        std::array<int, 5> order {};
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = juce::roundToInt (slots[i]->load());
        return na::describeShape ((float) slider.getValue(), blend->load() >= 0.5f,
                                  na::lfoNamesForSlots (order, true), true);
    });
    return *this;
}

Knob& Knob::showsDivisionWhenSynced (const char* syncParamId)
{
    auto* sync = state.getRawParameterValue (syncParamId);
    setLabelSource ([this, sync]
    {
        if (sync->load() < 0.5f)
            return name;

        const auto position = (float) slider.valueToProportionOfLength (slider.getValue());
        return na::lfoDivisionNames()[na::lfoDivisionIndex (position)];
    });
    return *this;
}

Knob& Knob::setKnobSize (int diameter)
{
    knobSize = diameter;
    resized();
    return *this;
}

void Knob::setLabelSource (std::function<juce::String()> source)
{
    labelText = std::move (source);
    timerCallback();
    startTimerHz (15);
}

void Knob::timerCallback()
{
    if (labelText == nullptr)
        return;

    const auto text = labelText();
    if (text != label.getText())
        label.setText (text, juce::dontSendNotification);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    const int size = std::max (0, std::min ({ area.getWidth(), area.getHeight() - 15, knobSize }));
    // knob and name sit together in the middle of the cell
    auto block = area.withSizeKeepingCentre (area.getWidth(), size + 15);
    slider.setBounds (block.removeFromTop (size).withSizeKeepingCentre (size, size));
    label.setBounds (block);
}

// ── LedSelector ─────────────────────────────────────────────────────────────
LedSelector::LedSelector (Apvts& state, const char* paramId, bool isVertical, bool withLabels,
                          std::vector<int> optionOrder, juce::StringArray optionLabels)
    : order (std::move (optionOrder)), labels (std::move (optionLabels)), vertical (isVertical), showLabels (withLabels)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId));
    jassert (param != nullptr);

    if (order.empty())
        for (int i = 0; i < param->choices.size(); ++i)
            order.push_back (i);
    if (labels.isEmpty())
        for (const auto index : order)
            labels.add (param->choices[index]);

    // The glow of a lit LED reaches past the selector's edge, so let it paint there
    // and refresh that margin in the parent too.
    setPaintingIsUnclipped (true);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float value)
    {
        selected = juce::roundToInt (value);
        if (auto* parent = getParentComponent())
            parent->repaint (getBounds().expanded (10));
        else
            repaint();
    });
    attachment->sendInitialUpdate();
}

juce::Rectangle<float> LedSelector::slot (int position) const
{
    const auto count = (float) order.size();
    const auto bounds = getLocalBounds().toFloat();
    return vertical ? bounds.withHeight (bounds.getHeight() / count).translated (0.0f, bounds.getHeight() / count * (float) position)
                    : bounds.withWidth (bounds.getWidth() / count).translated (bounds.getWidth() / count * (float) position, 0.0f);
}

void LedSelector::paint (juce::Graphics& g)
{
    constexpr float led = 8.0f;
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));

    for (int position = 0; position < (int) order.size(); ++position)
    {
        auto area = slot (position);
        const bool on = order[(size_t) position] == selected;

        const auto ledArea = vertical ? area.removeFromLeft (led + 6.0f) : area.removeFromTop (led + 6.0f);
        LookAndFeel::drawLed (g, ledArea.getCentre(), led, on);

        if (showLabels)
        {
            g.setColour (colours::text);
            g.drawFittedText (labels[position], area.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }
}

// ── StepButton ──────────────────────────────────────────────────────────────
StepButton::StepButton (Apvts& state, const char* paramId, juce::String keyName, std::vector<int> optionOrder)
    : order (std::move (optionOrder)), name (std::move (keyName))
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId));
    jassert (param != nullptr);

    if (order.empty())
        for (int i = 0; i < param->choices.size(); ++i)
            order.push_back (i);

    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float value) { selected = juce::roundToInt (value); });
    attachment->sendInitialUpdate();
}

void StepButton::paint (juce::Graphics& g)
{
    constexpr float size = 16.0f;
    const auto key = juce::Rectangle<float> (size, size).withCentre ({ (float) getWidth() * 0.5f, 3.0f + size * 0.5f });
    // lit while held down, like a momentary key with a backlight
    LookAndFeel::drawSwitchKey (g, key, pressed, isMouseOver());

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawFittedText (name, getLocalBounds().withTop ((int) key.getBottom() + 4), juce::Justification::centredTop, 1, 0.7f);
}

void StepButton::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    repaint();

    // step on to the next option in panel order, wrapping round
    const auto current = std::find (order.begin(), order.end(), selected);
    const auto next = current == order.end() || std::next (current) == order.end() ? order.front() : *std::next (current);
    attachment->setValueAsCompleteGesture ((float) next);
}

void StepButton::mouseUp (const juce::MouseEvent&)
{
    pressed = false;
    repaint();
}

void LedSelector::mouseDown (const juce::MouseEvent& event)
{
    for (int position = 0; position < (int) order.size(); ++position)
        if (slot (position).contains (event.position))
            attachment->setValueAsCompleteGesture ((float) order[(size_t) position]);
}

// ── SwitchStack ─────────────────────────────────────────────────────────────
SwitchStack& SwitchStack::toggle (const char* paramId, const juce::String& text)
{
    auto* button = items.add (std::make_unique<juce::ToggleButton> (text));
    addAndMakeVisible (button);
    buttonAttachments.add (std::make_unique<Apvts::ButtonAttachment> (state, paramId,
                                                                       *static_cast<juce::ToggleButton*> (button)));
    return *this;
}

SwitchStack& SwitchStack::choice (const char* paramId)
{
    auto* combo = new juce::ComboBox();
    items.add (combo);
    addAndMakeVisible (combo);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        combo->addItemList (p->choices, 1);

    comboAttachments.add (std::make_unique<Apvts::ComboBoxAttachment> (state, paramId, *combo));
    return *this;
}

SwitchStack& SwitchStack::caption (const juce::String& text)
{
    auto* label = new juce::Label ({}, text);
    label->setJustificationType (juce::Justification::centred);
    label->setFont (juce::FontOptions (10.5f, juce::Font::bold));
    label->setColour (juce::Label::textColourId, colours::textDim);
    label->getProperties().set ("naColour", "dim");
    items.add (label);
    addAndMakeVisible (label);
    return *this;
}

// ── SettingsPanel ───────────────────────────────────────────────────────────
SettingsPanel::SettingsPanel (Apvts& s)
{
    namespace id = na::id;

    auto& keyboard = section ("KEYBOARD");
    keyboard.add<SwitchStack> (s).caption ("PRIORITY").choice (id::notePriority);
    keyboard.add<Knob> (s, id::bendRange, "BEND RANGE");
    keyboard.add<Knob> (s, id::keySplit, "KEY SPLIT");
    keyboard.add<SwitchStack> (s).toggle (id::envRetrigger, "ENV RETRIG").toggle (id::pedalMode, "PEDAL")
                                 .toggle (id::muteOutOfRange, "MUTE C1-C7 OUT");

    auto& oscillators = section ("OSCILLATORS");
    oscillators.add<Knob> (s, id::autoglide1, "AUTOGLIDE 1", true);
    oscillators.add<Knob> (s, id::autoglide2, "AUTOGLIDE 2", true);
    oscillators.add<Knob> (s, id::drift, "DRIFT");
    oscillators.add<SwitchStack> (s).caption ("TUNE BYPASS").toggle (id::tuneBypass1, "OSC 1").toggle (id::tuneBypass2, "OSC 2");

    auto& lfo = section ("LFO");
    lfo.add<Knob> (s, id::lfoDepth, "DEPTH");
    lfo.add<Knob> (s, id::lfoKeyRate, "KEY RATE");
    lfo.add<SwitchStack> (s).toggle (id::lfoOneShot, "ONE SHOT").toggle (id::lfoRetrigger, "RETRIG");

    auto& modulation = section ("VCF MOD  \xc2\xb7  ASSIGN");
    modulation.add<SwitchStack> (s).caption ("VCF MOD").choice (id::vcfModSource);
    modulation.add<Knob> (s, id::vcfModAmount, "AMOUNT");
    modulation.add<SwitchStack> (s).caption ("ASSIGN OUT").choice (id::assignSource);

    auto& lfoOrder = section ("LFO SHAPE ORDER  \xc2\xb7  START PHASE");
    for (int position = 0; position < 5; ++position)
        lfoOrder.add<SwitchStack> (s).caption ("POSITION " + juce::String (position + 1))
                                     .choice (id::lfoSlot (position)).choice (id::lfoPhase (position));
}

Section& SettingsPanel::section (const char* utf8Title)
{
    auto* sec = sections.add (std::make_unique<Section> (juce::String (juce::CharPointer_UTF8 (utf8Title))));
    addAndMakeVisible (sec);
    return *sec;
}

void SettingsPanel::paint (juce::Graphics& g)
{
    // covers the front panel with more of the same panel
    LookAndFeel::drawPanelSurface (g, getLocalBounds().toFloat());
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText ("Options the hardware hides behind button combinations (manual chapter 5). "
                      "ONE SHOT needs KEY SYNC. RETRIG under LFO restarts it on overlapping notes. "
                      "Shape order and start phase apply per position of the LFO SHAPE knob.",
                      getLocalBounds().removeFromBottom (40).reduced (12, 0), juce::Justification::centredLeft, 2);
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (0, 4).withTrimmedBottom (44);
    const int gap = 10, rowHeight = std::min (150, (area.getHeight() - 2 * gap) / 3);
    auto top = area.removeFromTop (rowHeight);
    area.removeFromTop (gap);
    auto middle = area.removeFromTop (rowHeight);
    area.removeFromTop (gap);
    auto bottom = area.removeFromTop (rowHeight);

    sections[0]->setBounds (top.removeFromLeft ((top.getWidth() - gap) / 2));
    top.removeFromLeft (gap);
    sections[1]->setBounds (top);
    sections[2]->setBounds (middle.removeFromLeft ((middle.getWidth() - gap) / 2));
    middle.removeFromLeft (gap);
    sections[3]->setBounds (middle);
    sections[4]->setBounds (bottom.removeFromLeft (bottom.getWidth() * 3 / 4));
}

void SwitchStack::resized()
{
    constexpr int itemHeight = 24, spacing = 8;
    const int total = items.size() * itemHeight + (items.size() - 1) * spacing;
    auto area = getLocalBounds().reduced (4, 0).withSizeKeepingCentre (getWidth() - 8, total);

    for (auto* item : items)
    {
        item->setBounds (area.removeFromTop (itemHeight));
        area.removeFromTop (spacing);
    }
}

// ── Section ─────────────────────────────────────────────────────────────────
Section::Section (juce::String sectionTitle, int numRows)
    : title (std::move (sectionTitle)), rows (std::max (1, numRows))
{
    tabs.emplace_back (title, 0.5f);
}

void Section::paint (juce::Graphics& g)
{
    paintPanel (g);
    paintTitle (g);
    paintNote (g);
}

void Section::paintPanel (juce::Graphics& g) const
{
    // A printed frame; its bottom line runs through the middle of the name tabs.
    const auto frame = getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom ((float) tabHeight * 0.5f - 1.0f);
    g.setColour (signalPath ? colours::print : colours::outline);
    g.drawRoundedRectangle (frame, 6.0f, signalPath ? 1.8f : 1.0f);
}

void Section::paintTitle (juce::Graphics& g) const
{
    const juce::FontOptions font (10.5f, juce::Font::bold);
    g.setFont (font);
    for (const auto& [text, position] : tabs)
    {
        if (text.isEmpty())
            continue;

        const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 16.0f;
        const auto tab = juce::Rectangle<float> (width, (float) tabHeight)
                             .withCentre ({ (float) getWidth() * position, (float) getHeight() - (float) tabHeight * 0.5f - 0.5f });
        // audio path: solid white tab with dark lettering; modulation: outlined
        if (signalPath)
        {
            g.setColour (colours::print);
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (colours::panel.darker (0.4f));
        }
        else
        {
            g.setColour (colours::panel);
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (colours::outline);
            g.drawRoundedRectangle (tab, 3.0f, 1.0f);
            g.setColour (colours::text);
        }
        g.drawText (text, tab, juce::Justification::centred);
    }
}

void Section::paintNote (juce::Graphics& g) const
{
    if (note.isEmpty())
        return;

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText (note, getLocalBounds().withTrimmedBottom (tabHeight).reduced (10), juce::Justification::centred, 4);
}

void Section::resized()
{
    if (cells.isEmpty() || manualLayout)
        return;

    auto area = getLocalBounds().withTrimmedBottom (tabHeight + 2).reduced (6, 4);
    const int columns = (cells.size() + rows - 1) / rows;
    const int cellW   = area.getWidth() / columns;
    const int cellH   = area.getHeight() / rows;

    for (int i = 0; i < cells.size(); ++i)
    {
        const int row = i / columns, col = i % columns;
        cells[i]->setBounds (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH);
    }
}

} // namespace na::ui

// ── Patchbay ───────────────────────────────────────────────────────────────
namespace
{
    constexpr float bayHeader = 20.0f, bayGap = 10.0f, bayPadding = 2.0f, labelHeight = 10.0f;
    constexpr int inColumns = 4, outColumns = 3, bayRows = 8;
    constexpr float baseSocket = 27.0f;
}

// IN (4 columns) and OUT (3 columns) share one cell width, like the hardware.
juce::Rectangle<float> NegativeAtomiumEditor::PatchBay::panelBounds (bool outputs) const
{
    auto area = getLocalBounds().toFloat().withTrimmedTop (bayHeader);
    const auto cellWidth = (area.getWidth() - bayGap) / (float) (inColumns + outColumns);
    return outputs ? area.removeFromRight (cellWidth * (float) outColumns)
                   : area.removeFromLeft (cellWidth * (float) inColumns);
}

/** Socket diameter, limited to what fits a cell above its label. */
float NegativeAtomiumEditor::PatchBay::socketSize() const
{
    const auto panel = panelBounds (false).reduced (bayPadding, 0.0f);
    const auto cellW = panel.getWidth() / (float) inColumns;
    const auto cellH = panel.getHeight() / (float) bayRows;
    return std::min ({ baseSocket, cellW * 0.8f, cellH - labelHeight - 6.0f });
}

// Each cell holds a socket with its name underneath, centred in the cell.
NegativeAtomiumEditor::PatchBay::Port NegativeAtomiumEditor::PatchBay::port (bool output, int index) const
{
    const auto columns = output ? outColumns : inColumns;
    const auto panel = panelBounds (output).reduced (bayPadding, 0.0f);
    const auto cellW = panel.getWidth() / (float) columns;
    const auto cellH = panel.getHeight() / (float) bayRows;
    const auto cell = juce::Rectangle<float> (panel.getX() + cellW * (float) (index % columns),
                                              panel.getY() + cellH * (float) (index / columns),
                                              cellW, cellH);

    const auto socket = socketSize();
    const auto top = cell.getCentreY() - (socket + 2.0f + labelHeight) * 0.5f;
    const juce::Point<float> centre { cell.getCentreX(), top + socket * 0.5f };
    return { centre, { cell.getX(), top + socket + 2.0f, cellW, labelHeight } };
}

juce::Point<float> NegativeAtomiumEditor::PatchBay::sourcePort (int index) const
{
    return port (true, index).centre;
}

juce::Point<float> NegativeAtomiumEditor::PatchBay::destinationPort (int index) const
{
    return port (false, index).centre;
}

int NegativeAtomiumEditor::PatchBay::sourceAt (juce::Point<float> position) const
{
    for (int i = 0; i < (int) na::patch::sourceCount; ++i)
        if (sourcePort (i).getDistanceFrom (position) < std::max (11.0f, socketSize() * 0.5f))
            return i;
    return -1;
}

int NegativeAtomiumEditor::PatchBay::destinationAt (juce::Point<float> position) const
{
    for (int i = 0; i < (int) na::patch::destinationCount; ++i)
        if (destinationPort (i).getDistanceFrom (position) < std::max (11.0f, socketSize() * 0.5f))
            return i;
    return -1;
}

void NegativeAtomiumEditor::PatchBay::paint (juce::Graphics& g)
{
    const auto inPanel = panelBounds (false);
    const auto outPanel = panelBounds (true);
    const auto bayPanelColour = na::ui::colours::panel.darker (0.3f);
    for (const auto& panel : { inPanel, outPanel })
    {
        g.setColour (bayPanelColour);
        g.fillRoundedRectangle (panel, 4.0f);

        // fine diagonal stripes on the recessed jack fields
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (panel.toNearestInt());
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (auto x = panel.getX() - panel.getHeight(); x < panel.getRight(); x += 12.0f)
            g.drawLine (x, panel.getY(), x + panel.getHeight(), panel.getBottom(), 0.7f);
    }
    g.setColour (na::ui::colours::outline);
    g.drawRoundedRectangle (inPanel.reduced (0.5f), 4.0f, 1.0f);
    g.drawRoundedRectangle (outPanel.reduced (0.5f), 4.0f, 1.0f);

    const auto& connections = processor.getPatchConnections();

    auto drawPort = [&] (juce::Point<float> centre, bool active)
    {
        const auto size = socketSize();
        na::ui::LookAndFeel::drawPatchSocket (g, juce::Rectangle<float> (size, size).withCentre (centre), active);
    };

    g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
    for (int i = 0; i < (int) na::patch::sourceCount; ++i)
    {
        const auto p = port (true, i);
        drawPort (p.centre, selectedSource == i);
        g.setColour (na::ui::colours::text);
        g.drawFittedText (na::patch::sourceName (static_cast<na::patch::Source> (i)),
                          p.label.toNearestInt(), juce::Justification::centred, 1, 0.65f);
    }

    for (int i = 0; i < (int) na::patch::destinationCount; ++i)
    {
        const auto p = port (false, i);
        const auto& slots = connections[(std::size_t) i];
        const bool connected = std::any_of (slots.begin(), slots.end(), [] (int source) { return source >= 0; });
        drawPort (p.centre, connected);
        g.setColour (na::ui::colours::text);
        // The pitch inputs are named after their oscillator, as on the hardware.
        const auto name = juce::String (na::patch::destinationName (static_cast<na::patch::Destination> (i))).replace (" PITCH", "");
        g.drawFittedText (name, p.label.toNearestInt(), juce::Justification::centred, 1, 0.65f);
    }

    // Cables lie on top of the panel, sockets and labels, so a cable crossing
    // another jack never looks as if it were plugged into it.
    for (int destination = 0; destination < (int) connections.size(); ++destination)
    {
        for (int slot = 0; slot < (int) na::patch::maxConnectionsPerDestination; ++slot)
        {
            const auto source = connections[(std::size_t) destination][(std::size_t) slot];
            if (source < 0 || source >= (int) na::patch::sourceCount)
                continue;

            const auto a = sourcePort (source);
            const auto b = destinationPort (destination);
            const auto span = std::abs (a.x - b.x);
            auto sag = 18.0f + std::min (54.0f, span * 0.08f)
                     + (float) ((destination + slot) % 4) * 5.0f;
            // Keep the hanging cable inside the patchbay (the curve dips ~3/4 of the sag);
            // near the bottom edge it arches upwards instead.
            const auto room = ((float) getHeight() - 8.0f - std::max (a.y, b.y)) / 0.75f;
            if (room < sag)
                sag = room >= 14.0f ? room : -sag;
            juce::Path cable;
            cable.startNewSubPath (a);
            cable.cubicTo (a.x - span * 0.34f, a.y + sag,
                           b.x + span * 0.34f, b.y + sag,
                           b.x, b.y);

            constexpr float cableScale = 1.0f;

            // Cable shadow and body give the patch a little physical depth.
            g.setColour (juce::Colour (0x90000000));
            g.strokePath (cable, juce::PathStrokeType (7.0f * cableScale, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            const auto hue = std::fmod (0.11f + (float) destination * 0.173f + (float) slot * 0.311f, 1.0f);
            const auto cableColour = juce::Colour::fromHSV (hue, 0.68f, 0.95f, 0.92f);
            g.setColour (cableColour.darker (0.28f));
            g.strokePath (cable, juce::PathStrokeType (5.0f * cableScale, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            g.setColour (cableColour.brighter (0.18f));
            g.strokePath (cable, juce::PathStrokeType (2.0f * cableScale, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        }
    }

    // Draw the plug hardware after the sockets so each cable visibly terminates
    // in a connector instead of disappearing underneath the jack artwork.
    for (int destination = 0; destination < (int) connections.size(); ++destination)
        for (int slot = 0; slot < (int) na::patch::maxConnectionsPerDestination; ++slot)
            if (const auto source = connections[(std::size_t) destination][(std::size_t) slot];
                source >= 0 && source < (int) na::patch::sourceCount)
            {
                const auto hue = std::fmod (0.11f + (float) destination * 0.173f + (float) slot * 0.311f, 1.0f);
                const auto cableColour = juce::Colour::fromHSV (hue, 0.68f, 0.95f, 0.92f);
                const auto drawPlug = [&] (juce::Point<float> port, float direction)
                {
                    // Keep the sleeve outside the jack; only the small tip
                    // enters the socket's centre hole.
                    const auto body = juce::Rectangle<float> (13.0f, 7.0f)
                                          .withCentre (port + juce::Point<float> (direction * 7.0f, 0.0f));
                    // Top-down plug face: a round metal cap sits over the
                    // socket hole, with the short cable neck behind it.
                    const auto tip = juce::Rectangle<float> (12.0f, 12.0f).withCentre (port);
                    g.setColour (juce::Colour (0x85000000));
                    g.fillRoundedRectangle (body.translated (0.0f, 2.0f), 3.0f);
                    g.setColour (juce::Colour (0xff252d33));
                    g.fillRoundedRectangle (body, 3.0f);
                    g.setColour (cableColour.brighter (0.1f));
                    g.drawRoundedRectangle (body.reduced (0.5f), 3.0f, 1.0f);
                    g.setColour (juce::Colour (0xffcbd4d4));
                    g.fillEllipse (tip);
                    g.setColour (juce::Colour (0xff56636a));
                    g.drawEllipse (tip.reduced (0.7f), 1.0f);
                    g.setColour (juce::Colour (0xff11171b));
                    g.fillEllipse (tip.reduced (3.5f));
                };
                drawPlug (sourcePort (source), -1.0f);
                drawPlug (destinationPort (destination), 1.0f);
            }

    g.setColour (na::ui::colours::textDim);
    g.setFont (juce::FontOptions (10.0f));
    // IN / OUT tags above the two fields
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    for (const auto& [panel, text] : { std::pair { inPanel, "IN" }, std::pair { outPanel, "OUT" } })
    {
        const auto tag = juce::Rectangle<float> (40.0f, 15.0f).withCentre ({ panel.getCentreX(), bayHeader * 0.5f });
        g.setColour (na::ui::colours::print);
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (na::ui::colours::panel.darker (0.4f));
        g.drawText (text, tag, juce::Justification::centred);
    }
    if (selectedSource >= 0)
    {
        g.setColour (na::ui::colours::accent);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText ("SELECT DESTINATION", juce::Rectangle<float> (inPanel.getCentreX() + 24.0f, 0.0f,
                                                         inPanel.getWidth() * 0.5f - 24.0f, bayHeader),
                    juce::Justification::centred);
    }
}

void NegativeAtomiumEditor::PatchBay::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (const auto destination = destinationAt (event.position); destination >= 0)
            processor.setPatchConnection (static_cast<na::patch::Destination> (destination), -1);
        selectedSource = -1;
        repaint();
        return;
    }

    if (const auto source = sourceAt (event.position); source >= 0)
    {
        selectedSource = source;
        repaint();
        return;
    }

    if (selectedSource >= 0)
        if (const auto destination = destinationAt (event.position); destination >= 0)
            processor.setPatchConnection (static_cast<na::patch::Destination> (destination), selectedSource);

    selectedSource = -1;
    repaint();
}

// ── Editor ──────────────────────────────────────────────────────────────────
namespace
{
    // One front panel in the hardware's arrangement, left to right:
    // oscillators | VCF | LFO over NOISE/VCA + envelopes | delay, overdrive | output,
    // S&H, slew, attenuators | patchbay.
    // A black metal frame holds the panel; the header is printed on the panel itself.
    constexpr int frame = 16;
    constexpr int margin = frame + 8, headerHeight = 48;
    constexpr int panelLeft = frame + 8, panelTop = frame + headerHeight, panelHeight = 500;
    constexpr int editorWidth  = panelLeft + 1576 + 8 + frame;
    constexpr int editorHeight = panelTop + panelHeight + 8 + frame;
}

na::ui::Section& NegativeAtomiumEditor::section (const juce::String& title, int rows)
{
    auto* s = sections.add (std::make_unique<na::ui::Section> (title, rows));
    content.addAndMakeVisible (s);
    return *s;
}

NegativeAtomiumEditor::NegativeAtomiumEditor (NegativeAtomiumProcessor& p)
    : AudioProcessorEditor (p), synthProcessor (p), patchBayView (p)
{
    using namespace na::ui;
    namespace id = na::id;
    auto& s = p.getState();

    setLookAndFeel (&lookAndFeel);
    content.setSize (editorWidth, editorHeight);
    addAndMakeVisible (content);

    aboutButton.setTooltip ("Show plugin version and build information");
    aboutButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    aboutButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    aboutButton.setColour (juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    aboutButton.setColour (juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    aboutButton.onClick = [this] { showAbout(); };
    content.addAndMakeVisible (aboutButton);

    // Cells are placed in layoutSections(), in the order they are added here.
    oscillators = &section ("OSCILLATORS");
    oscillators->setTabs ({ { "OSC 1", 0.25f }, { "OSC 2", 0.75f } });
    oscillators->setSignalPath (true);
    oscillators->add<Knob> (s, id::osc1Tune, "TUNE", true).setKnobSize (88);
    oscillators->add<Knob> (s, id::oscMix, "OSC MIX").setKnobSize (44);
    oscillators->add<Knob> (s, id::osc2Tune, "TUNE", true).setKnobSize (88);
    oscillators->add<Knob> (s, id::osc1Shape, "SHAPE").showsShape (id::osc1Blend, na::ShapeSet::oscillator).setKnobSize (60);
    oscillators->add<Knob> (s, id::osc2Shape, "SHAPE").showsShape (id::osc2Blend, na::ShapeSet::oscillator).setKnobSize (60);
    oscillators->add<SwitchStack> (s).toggle (id::osc1Blend, "BLEND");
    oscillators->add<SwitchStack> (s).toggle (id::osc2Blend, "BLEND");
    oscillators->add<SwitchStack> (s).caption ("OCTAVE");
    // 8' on top like the hardware; the labels between the two LED columns serve both.
    oscillators->add<LedSelector> (s, id::osc1Octave, true, true, std::vector<int> { 2, 1, 0, 3 },
                                   juce::StringArray { "8'", "16'", "32'", "FULL" });
    oscillators->add<LedSelector> (s, id::osc2Octave, true, false, std::vector<int> { 2, 1, 0, 3 });
    oscillators->add<Knob> (s, id::osc1Width, "WIDTH").setKnobSize (60);
    oscillators->add<Knob> (s, id::osc2Width, "WIDTH").setKnobSize (60);
    oscillators->add<SwitchStack> (s).toggle (id::oscSync, "OSC SYNC");
    oscillators->add<SwitchStack> (s).toggle (id::paraphonic, "PARAPHONIC");
    oscillators->add<StepButton> (s, id::osc1Octave, "RANGE", std::vector<int> { 2, 1, 0, 3 });
    oscillators->add<StepButton> (s, id::osc2Octave, "RANGE", std::vector<int> { 2, 1, 0, 3 });

    vcf = &section ("VCF");
    vcf->setSignalPath (true);
    vcf->add<LedSelector> (s, id::vcfMode, false, true);
    vcf->add<Knob> (s, id::vcfFreq, "FREQ").setKnobSize (66);
    vcf->add<Knob> (s, id::vcfReso, "RESO").setKnobSize (52);
    vcf->add<SwitchStack> (s).toggle (id::vcfKeyTrack, "KEY TRK");
    vcf->add<Knob> (s, id::vcfModDepth, "MOD DEPTH").setKnobSize (52);
    vcf->add<Knob> (s, id::vcfEnvDepth, "ENV DEPTH", true).setKnobSize (52);
    vcf->add<StepButton> (s, id::vcfMode, "MODE");

    lfo = &section ("LFO");
    lfo->add<SwitchStack> (s).toggle (id::lfoKeySync, "KEY SYNC").toggle (id::lfoSync, "SYNC").toggle (id::lfoBlend, "BLEND");
    lfo->add<Knob> (s, id::lfoRate, "RATE").showsDivisionWhenSynced (id::lfoSync).setKnobSize (52);
    lfo->add<Knob> (s, id::lfoShape, "SHAPE").showsLfoShape (id::lfoBlend).setKnobSize (88);

    noiseVca = &section ("NOISE / VCA");
    noiseVca->setTabs ({});
    noiseVca->setSignalPath (true);
    noiseVca->add<Knob> (s, id::noise, "NOISE").setKnobSize (52);
    noiseVca->add<Knob> (s, id::vcaBias, "VCA BIAS").setKnobSize (52);

    env1 = &section (juce::CharPointer_UTF8 ("ENVELOPE 1  \xc2\xb7  VCA"));
    env1->add<Knob> (s, id::env1Attack, "A");
    env1->add<Knob> (s, id::env1Decay, "D");
    env1->add<Knob> (s, id::env1Sustain, "S");
    env1->add<Knob> (s, id::env1Release, "R");

    env2 = &section (juce::CharPointer_UTF8 ("ENVELOPE 2  \xc2\xb7  VCF"));
    env2->add<Knob> (s, id::env2Attack, "A");
    env2->add<Knob> (s, id::env2Decay, "D");
    env2->add<Knob> (s, id::env2Sustain, "S");
    env2->add<Knob> (s, id::env2Release, "R");

    delay = &section ("DELAY");
    delay->setSignalPath (true);
    delay->add<Knob> (s, id::delayTime, "TIME");
    delay->add<Knob> (s, id::delayRepeats, "REPEATS");
    delay->add<Knob> (s, id::delayMix, "MIX");

    overdrive = &section ("OVERDRIVE");
    overdrive->setSignalPath (true);
    overdrive->add<Knob> (s, id::odDrive, "DRIVE");
    overdrive->add<Knob> (s, id::odTone, "TONE");
    overdrive->add<Knob> (s, id::odLevel, "LEVEL");

    output = &section ("OUTPUT");
    output->setSignalPath (true);
    output->add<Knob> (s, id::volume, "VOLUME").setKnobSize (60);
    output->add<SwitchStack> (s).caption ("ASSIGN").choice (id::assignSource);

    sampleHold = &section ("SAMPLE & HOLD");
    sampleHold->add<Knob> (s, id::shRate, "RATE");
    sampleHold->add<Knob> (s, id::shGlide, "GLIDE");

    slew = &section ("SLEW RATE LIMITER");
    slew->add<Knob> (s, id::slew, "SLEW");
    slew->add<Knob> (s, id::portaTime, "PORTA TIME");

    attenuators = &section ("ATTENUATORS");
    attenuators->add<Knob> (s, id::att1, "1");
    attenuators->add<Knob> (s, id::att2, "2");

    patchbay = &section ("PATCHBAY");
    patchbay->setTabs ({});
    content.addAndMakeVisible (patchBayView);

    settingsPanel = std::make_unique<na::ui::SettingsPanel> (s);
    content.addChildComponent (*settingsPanel);

    settingsButton.setClickingTogglesState (true);
    settingsButton.onClick = [this] { showSettings (settingsButton.getToggleState()); };
    content.addAndMakeVisible (settingsButton);

    presetSelector.setTextWhenNothingSelected ("PRESET");
    presetSelector.setTooltip ("Load a saved sound preset");
    presetSelector.onChange = [this] { selectPreset (presetSelector.getSelectedId()); };
    content.addAndMakeVisible (presetSelector);

    savePresetButton.setTooltip ("Save the current sound and patchbay as a preset");
    savePresetButton.onClick = [this] { savePreset(); };
    content.addAndMakeVisible (savePresetButton);
    populatePresets();

    layoutSections();

    // Read the saved size first: setting the limits below already triggers resized().
    const auto scale = std::clamp (p.getUiScale(), minScale, maxScale);
    setSize (juce::roundToInt (editorWidth * scale), juce::roundToInt (editorHeight * scale));

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (editorWidth * minScale), juce::roundToInt (editorHeight * minScale),
                     juce::roundToInt (editorWidth * maxScale), juce::roundToInt (editorHeight * maxScale));
    getConstrainer()->setFixedAspectRatio ((double) editorWidth / (double) editorHeight);
    sizeRestored = true;
}

NegativeAtomiumEditor::~NegativeAtomiumEditor()
{
    setLookAndFeel (nullptr);
}

void NegativeAtomiumEditor::paint (juce::Graphics& g)
{
    paintBackground (g, getLocalBounds());
}

void NegativeAtomiumEditor::Content::paint (juce::Graphics& g)
{
    paintBackground (g, getLocalBounds());
    paintLogo (g);
    paintHeaderText (g, getWidth());
}

void NegativeAtomiumEditor::paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
{
    using na::ui::LookAndFeel;
    const auto bounds = area.toFloat();

    // black anodised frame, slightly lit from above
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232427), 0.0f, bounds.getY(),
                                             juce::Colour (0xff060607), 0.0f, bounds.getBottom(), false));
    g.fillRect (bounds);

    // the front panel, sitting in the frame
    const auto plate = bounds.reduced ((float) frame);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (plate.expanded (1.5f), 4.0f);
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path shape;
        shape.addRoundedRectangle (plate, 3.0f);
        g.reduceClipRegion (shape);
        LookAndFeel::drawPanelSurface (g, plate);
    }
    // edge of the folded sheet: light on top, shadow at the bottom
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawLine (plate.getX() + 3.0f, plate.getY() + 0.5f, plate.getRight() - 3.0f, plate.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (plate.getX() + 3.0f, plate.getBottom() - 0.5f, plate.getRight() - 3.0f, plate.getBottom() - 0.5f, 1.0f);

    // screws in the frame
    const auto inset = (float) frame * 0.5f;
    int screw = 0;
    for (const auto x : { bounds.getX() + inset, bounds.getCentreX(), bounds.getRight() - inset })
        for (const auto y : { bounds.getY() + inset, bounds.getBottom() - inset })
            LookAndFeel::drawScrew (g, { x, y }, 4.5f, 0.35f + (float) (screw++) * 0.9f);

    // printed rule under the header
    g.setColour (na::ui::colours::outline);
    g.fillRect (juce::Rectangle<float> ((float) margin, (float) (frame + headerHeight) - 4.0f,
                                        bounds.getWidth() - 2.0f * (float) margin, 1.0f));
}

void NegativeAtomiumEditor::paintLogo (juce::Graphics& g)
{
    // atom mark printed on the panel: nucleus + three orbits
    const auto centre = juce::Point<float> ((float) margin + 18.0f, (float) frame + (float) headerHeight * 0.5f - 2.0f);
    g.setColour (na::ui::colours::print);
    for (int i = 0; i < 3; ++i)
    {
        juce::Path orbit;
        orbit.addEllipse (-16.0f, -6.0f, 32.0f, 12.0f);
        orbit.applyTransform (juce::AffineTransform::rotation ((float) i * juce::MathConstants<float>::pi / 3.0f)
                                  .translated (centre));
        g.strokePath (orbit, juce::PathStrokeType (1.4f));
    }
    g.setColour (na::ui::colours::accent);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (centre));
}

void NegativeAtomiumEditor::paintHeaderText (juce::Graphics& g, int editorWidth)
{
    using namespace na::ui;
    auto header = juce::Rectangle<int> (margin + 48, frame, editorWidth - 2 * margin - 48, headerHeight - 4);
    g.setColour (colours::print);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("NEGATIVE ATOMIUM", header.removeFromLeft (270), juce::Justification::centredLeft);

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("SEMI-MODULAR PARAPHONIC SYNTHESIZER", header, juce::Justification::centredLeft);
}

void NegativeAtomiumEditor::resized()
{
    const auto scale = (float) getWidth() / (float) editorWidth;
    content.setTransform (juce::AffineTransform::scale (scale));

    if (sizeRestored)
        synthProcessor.setUiScale (scale);
}

void NegativeAtomiumEditor::showSettings (bool shouldShow)
{
    settingsButton.setToggleState (shouldShow, juce::dontSendNotification);
    settingsPanel->setVisible (shouldShow);
    settingsButton.setButtonText (shouldShow ? "CLOSE" : "SETTINGS");
}

void NegativeAtomiumEditor::populatePresets()
{
    presetSelector.clear (juce::dontSendNotification);
    presets = findPresetFiles();
    for (int i = 0; i < presets.size(); ++i)
        presetSelector.addItem (presetDisplayName (presets.getReference (i)), i + 1);
    presetSelector.setSelectedId (0, juce::dontSendNotification);
}

void NegativeAtomiumEditor::selectPreset (int itemId)
{
    if (itemId <= 0 || itemId > presets.size())
        return;

    const auto file = presets.getReference (itemId - 1);
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (synthProcessor.applyPresetState (juce::ValueTree::fromXml (*xml)))
        {
            patchBayView.repaint();
            content.repaint();
        }
    }
}

void NegativeAtomiumEditor::savePreset()
{
    auto* window = new juce::AlertWindow ("Save preset", "Enter a name for the current sound.",
                                          juce::AlertWindow::NoIcon);
    window->addTextEditor ("name", "My Preset", "Preset name:");
    window->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result)
    {
        if (result != 1)
            return;

        auto name = window->getTextEditorContents ("name").trim();
        if (name.isEmpty())
            return;

        name = name.replaceCharacters ("/:\\\\", "---");
        auto folder = userPresetFolder();
        if (! folder.createDirectory())
            return;

        const auto file = folder.getChildFile (name).withFileExtension ("napreset");
        if (auto xml = synthProcessor.copyPresetState().createXml())
        {
            xml->setAttribute ("presetName", name);
            if (xml->writeTo (file))
                populatePresets();
        }
    }), true);
}

void NegativeAtomiumEditor::showAbout()
{
    // The plugin target supplies JucePlugin_VersionString; the standalone
    // test target intentionally does not, so keep the diagnostic UI portable.
#if defined(JucePlugin_VersionString)
    constexpr auto pluginVersion = JucePlugin_VersionString;
#else
    constexpr auto pluginVersion = "0.1.0";
#endif
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Negative Atomium",
        juce::String (JucePlugin_Name) + " v" + pluginVersion + "\n"
            + "VST3 / CLAP\n"
            + "Build: " __DATE__ " " __TIME__ + "\n\n"
            + "Presets: " + userPresetFolder().getFullPathName(),
        "OK");
}

void NegativeAtomiumEditor::layoutSections()
{
    using Section = na::ui::Section;
    using R = juce::Rectangle<int>;

    // Places a section on the panel; its cells either get the given bounds (in section
    // coordinates, in the order they were added) or share the width evenly.
    auto place = [] (Section* section, R bounds, std::initializer_list<R> cellBounds = {})
    {
        section->setManualLayout (true);
        section->setBounds (bounds.translated (panelLeft - 12, panelTop));
        const auto& cells = section->getCells();

        if (cellBounds.size() > 0)
        {
            int i = 0;
            for (const auto& cell : cellBounds)
                cells[i++]->setBounds (cell);
            return;
        }

        const auto area = section->getLocalBounds().withTrimmedBottom (Section::tabHeight + 2).reduced (6, 4);
        const auto vertical = area.getHeight() > area.getWidth();
        for (int i = 0; i < cells.size(); ++i)
            cells[i]->setBounds (vertical ? area.withHeight (area.getHeight() / cells.size()).translated (0, i * area.getHeight() / cells.size())
                                          : area.withWidth (area.getWidth() / cells.size()).translated (i * area.getWidth() / cells.size(), 0));
    };

    place (oscillators, { 12, 0, 380, panelHeight }, {
        { 20, 10, 130, 128 },    // TUNE 1
        { 150, 4, 80, 72 },      // OSC MIX
        { 230, 10, 130, 128 },   // TUNE 2
        { 30, 164, 110, 92 },    // SHAPE 1
        { 240, 164, 110, 92 },   // SHAPE 2
        { 36, 258, 98, 24 },     // BLEND 1
        { 246, 258, 98, 24 },    // BLEND 2
        { 150, 150, 80, 22 },    // OCTAVE
        { 159, 176, 49, 100 },   // octave LEDs OSC 1 + labels
        { 209, 176, 14, 100 },   // octave LEDs OSC 2
        { 30, 356, 110, 92 },    // WIDTH 1
        { 240, 356, 110, 92 },   // WIDTH 2
        { 136, 360, 112, 24 },   // OSC SYNC
        { 136, 400, 112, 24 },   // PARAPHONIC
        { 146, 282, 40, 36 },    // RANGE 1 (under the OSC 1 LEDs)
        { 196, 282, 40, 36 },    // RANGE 2
    });

    place (vcf, { 400, 0, 112, panelHeight }, {
        { 40, 10, 68, 32 },      // mode LEDs
        { 4, 52, 104, 96 },      // FREQ
        { 4, 160, 104, 78 },     // RESO
        { 12, 250, 92, 24 },     // KEY TRK
        { 4, 288, 104, 78 },     // MOD DEPTH
        { 4, 384, 104, 78 },     // ENV DEPTH
        { 2, 8, 40, 36 },        // MODE key, left of the LEDs
    });

    place (lfo, { 520, 0, 196, 250 }, {
        { 4, 6, 100, 92 },       // KEY SYNC, SYNC, BLEND
        { 104, 4, 88, 88 },      // RATE
        { 40, 100, 116, 128 },   // SHAPE
    });
    place (noiseVca, { 520, 258, 96, 242 });
    place (env1, { 624, 258, 376, 117 });
    place (env2, { 624, 383, 376, 117 });

    place (delay, { 724, 0, 276, 121 });
    place (overdrive, { 724, 129, 276, 121 });

    place (output, { 1008, 0, 188, 121 }, {
        { 4, 4, 90, 99 },        // VOLUME
        { 96, 20, 86, 70 },      // ASSIGN
    });
    place (sampleHold, { 1008, 129, 188, 121 });
    place (slew, { 1008, 258, 188, 117 });
    place (attenuators, { 1008, 383, 188, 117 });

    place (patchbay, { 1204, 0, 384, panelHeight });
    patchBayView.setBounds (patchbay->getBounds().reduced (7).withTrimmedBottom (4));

    const int buttonY = frame + (headerHeight - 30) / 2;
    presetSelector.setBounds (editorWidth - margin - 420, buttonY, 200, 30);
    savePresetButton.setBounds (editorWidth - margin - 210, buttonY, 90, 30);
    settingsButton.setBounds (editorWidth - margin - 110, buttonY, 110, 30);
    aboutButton.setBounds (margin, frame, 40, headerHeight);
    settingsPanel->setBounds (panelLeft, panelTop, editorWidth - panelLeft - margin, panelHeight);
}
