#include "LookAndFeel.h"

namespace na::ui
{

namespace
{
    constexpr int scaleMarks = 21;      // printed scale around every knob
    constexpr int starLobes = 7;        // lobes of the star knob

    juce::Rectangle<float> circle (juce::Point<float> centre, float radius)
    {
        return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    }

    float markAngle (int mark, float startAngle, float endAngle)
    {
        return startAngle + (float) mark / (float) (scaleMarks - 1) * (endAngle - startAngle);
    }

    /** Fine grain of the powder coat, tiled over the panel. */
    struct PanelTexture
    {
        PanelTexture() : image (juce::Image::ARGB, 160, 160, true)
        {
            juce::Random random (0x4e41);
            for (int y = 0; y < image.getHeight(); ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                {
                    const auto grain = random.nextFloat();
                    const auto colour = grain > 0.5f ? juce::Colours::white.withAlpha ((grain - 0.5f) * 0.075f)
                                                     : juce::Colours::black.withAlpha ((0.5f - grain) * 0.11f);
                    image.setPixelAt (x, y, colour);
                }
        }

        juce::Image image;
    };

    /** Soft round shadow a part casts on the panel (light from above). */
    void dropShadow (juce::Graphics& g, juce::Point<float> centre, float radius, float depth)
    {
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.11f));
            g.fillEllipse (circle (centre.translated (0.0f, depth), radius + (float) i * radius * 0.05f));
        }
    }
}

// ── user folder ─────────────────────────────────────────────────────────────
juce::File userDataFolder()
{
#if JUCE_MAC
    // JUCE's userApplicationDataDirectory is ~/Library on macOS, not Application Support.
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library").getChildFile ("Application Support").getChildFile ("NegativeAtomium");
#elif JUCE_LINUX || JUCE_BSD
    const auto xdg = juce::SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const auto base = juce::File::isAbsolutePath (xdg)
                    ? juce::File (xdg)
                    : juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".local").getChildFile ("share");
    return base.getChildFile ("NegativeAtomium");
#else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("NegativeAtomium");
#endif
}

juce::Array<juce::File> legacyUserDataFolders()
{
    juce::Array<juce::File> folders;
#if JUCE_MAC || JUCE_LINUX || JUCE_BSD
    // ~/Library/NegativeAtomium (macOS) or ~/.config/NegativeAtomium (Linux)
    const auto legacy = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("NegativeAtomium");
    if (legacy != userDataFolder())
        folders.add (legacy);
#endif
    return folders;
}

// ── LookAndFeel ─────────────────────────────────────────────────────────────
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::background);
    setColour (juce::ComboBox::textColourId, colours::accent);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::background);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accentDim);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::TooltipWindow::backgroundColourId, colours::background);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
}

// ── panel parts ─────────────────────────────────────────────────────────────
void LookAndFeel::drawPanelSurface (juce::Graphics& g, juce::Rectangle<float> area)
{
    // anthracite powder coat: a faint top-down light plus a fine grain
    g.setGradientFill (juce::ColourGradient (colours::panel.brighter (0.06f), area.getX(), area.getY(),
                                             colours::panel.darker (0.12f), area.getX(), area.getBottom(), false));
    g.fillRect (area);

    juce::SharedResourcePointer<PanelTexture> texture;
    g.setTiledImageFill (texture->image, 0, 0, 1.0f);
    g.fillRect (area);
}

void LookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius, float slotAngle)
{
    dropShadow (g, centre, radius, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8b9096), centre.x - radius, centre.y - radius,
                                             juce::Colour (0xff2a2d31), centre.x + radius, centre.y + radius, false));
    g.fillEllipse (circle (centre, radius));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (circle (centre, radius), 0.8f);

    // cross-head slot
    for (const auto angle : { slotAngle, slotAngle + juce::MathConstants<float>::halfPi })
    {
        const auto a = centre.getPointOnCircumference (radius * 0.62f, angle);
        const auto b = centre.getPointOnCircumference (radius * 0.62f, angle + juce::MathConstants<float>::pi);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawLine ({ a, b }, std::max (1.0f, radius * 0.28f));
    }
}

void LookAndFeel::drawSwitchKey (juce::Graphics& g, juce::Rectangle<float> key, bool on, bool highlighted)
{
    // recessed bezel the rubber key sits in
    const auto bezel = key.expanded (2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (bezel.translated (0.0f, 0.8f), 4.0f, 1.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (bezel, 4.0f);

    if (on)
    {
        // backlight glowing through the key and around it
        g.setColour (colours::accent.withAlpha (0.25f));
        g.fillRoundedRectangle (bezel.expanded (2.0f), 6.0f);
        g.setGradientFill (juce::ColourGradient (colours::accent.brighter (0.55f), key.getCentreX(), key.getCentreY() - key.getHeight() * 0.1f,
                                                 colours::accent.darker (0.35f), key.getX(), key.getBottom(), true));
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9aa0a6), key.getX(), key.getY(),
                                                 juce::Colour (0xff4c5157), key.getX(), key.getBottom(), false));
    }
    g.fillRoundedRectangle (key, 3.0f);

    // soft top edge and darker bottom edge of the rubber
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : (highlighted ? 0.45f : 0.32f)));
    g.drawLine (key.getX() + 2.5f, key.getY() + 1.2f, key.getRight() - 2.5f, key.getY() + 1.2f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine (key.getX() + 2.5f, key.getBottom() - 1.0f, key.getRight() - 2.5f, key.getBottom() - 1.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (key, 3.0f, 0.8f);
}

void LookAndFeel::drawLed (juce::Graphics& g, juce::Point<float> centre, float diameter, bool on)
{
    const auto r = diameter * 0.5f;

    // black bezel
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillEllipse (circle (centre.translated (0.0f, 0.7f), r + 2.0f));
    g.setColour (colours::background);
    g.fillEllipse (circle (centre, r + 2.0f));

    if (on)
    {
        g.setColour (colours::accent.withAlpha (0.22f));
        g.fillEllipse (circle (centre, r + 5.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff3d6), centre.x, centre.y - r * 0.2f,
                                                 colours::accent.darker (0.2f), centre.x, centre.y + r, true));
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (colours::accentDim.darker (0.2f), centre.x, centre.y - r * 0.3f,
                                                 colours::accentDim.darker (0.85f), centre.x, centre.y + r, true));
    }
    g.fillEllipse (circle (centre, r));

    // dome highlight
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.28f));
    g.fillEllipse (circle (centre.translated (-r * 0.28f, -r * 0.32f), r * 0.3f));
}

void LookAndFeel::drawPatchSocket (juce::Graphics& g, juce::Rectangle<float> bounds, bool active)
{
    const auto c = bounds.getCentre();
    const auto r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // hex nut
    juce::Path nut;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = c.getPointOnCircumference (r, juce::MathConstants<float>::pi / 6.0f + (float) i * juce::MathConstants<float>::pi / 3.0f);
        i == 0 ? nut.startNewSubPath (p) : nut.lineTo (p);
    }
    nut.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (nut, juce::AffineTransform::translation (0.0f, 1.6f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4e7ea), c.x - r, c.y - r,
                                             juce::Colour (0xff6e747a), c.x + r, c.y + r, false));
    g.fillPath (nut);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));

    // threaded sleeve and the dark hole
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5d6268), c.x, c.y - r,
                                             juce::Colour (0xffc9cdd1), c.x, c.y + r, false));
    g.fillEllipse (circle (c, r * 0.66f));
    g.setColour (juce::Colour (0xff050506));
    g.fillEllipse (circle (c, r * 0.44f));
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (circle (c, r * 0.44f), 0.7f);

    if (active)
    {
        g.setColour (colours::accent);
        g.drawEllipse (circle (c, r + 2.0f), 1.6f);
    }
}

// ── knobs ───────────────────────────────────────────────────────────────────
void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto geometry = knobGeometry (juce::Rectangle<int> (x, y, width, height).toFloat());
    const auto angle    = startAngle + sliderPos * (endAngle - startAngle);

    drawKnobTrack (g, geometry, startAngle, endAngle, slider.getProperties()["bipolar"]);
    drawKnobTicks (g, geometry, startAngle, endAngle, slider.getProperties()["ticks"]);
    drawKnobCap (g, geometry, angle);
    drawKnobPointer (g, geometry, angle);
}

LookAndFeel::KnobGeometry LookAndFeel::knobGeometry (juce::Rectangle<float> bounds)
{
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
    const auto mark   = std::max (3.0f, radius * 0.12f);
    return { bounds.getCentre(), radius, radius - mark - 2.0f, mark };
}

void LookAndFeel::drawKnobTrack (juce::Graphics& g, const KnobGeometry& k, float startAngle, float endAngle, bool bipolar)
{
    // printed scale: longer marks at the ends, every fifth and (bipolar) the centre
    g.setColour (colours::print.withAlpha (0.8f));
    for (int i = 0; i < scaleMarks; ++i)
    {
        const bool major = i % 5 == 0 || (bipolar && i == scaleMarks / 2);
        const auto angle = markAngle (i, startAngle, endAngle);
        const auto inner = k.arcRadius - (major ? k.stroke : k.stroke * 0.55f);
        g.drawLine ({ k.centre.getPointOnCircumference (inner, angle),
                      k.centre.getPointOnCircumference (k.arcRadius, angle) }, major ? 1.3f : 0.8f);
    }
}

void LookAndFeel::drawKnobTicks (juce::Graphics& g, const KnobGeometry& k, float startAngle, float endAngle,
                                 int numTicks)
{
    if (numTicks < 2)
        return;

    // waveform positions: printed dots just outside the scale
    g.setColour (colours::print);
    for (int i = 0; i < numTicks; ++i)
    {
        const auto angle = startAngle + (float) i / (float) (numTicks - 1) * (endAngle - startAngle);
        g.fillEllipse (circle (k.centre.getPointOnCircumference (k.arcRadius - k.stroke * 0.5f, angle),
                               std::max (1.5f, k.stroke * 0.38f)));
    }
}

void LookAndFeel::drawKnobCap (juce::Graphics& g, const KnobGeometry& k, float angle)
{
    const auto c = k.centre;
    const auto r = k.capRadius;
    const auto pi = juce::MathConstants<float>::pi;

    // Star knob as on machine tools: rounded points with broad, shallow concave
    // scallops between them. The body turns with the value; the light does not.
    juce::Path star;
    constexpr int points = 252;
    for (int i = 0; i < points; ++i)
    {
        const auto a = (float) i * 2.0f * pi / (float) points;
        const auto lobe = 0.5f + 0.5f * std::cos ((float) starLobes * a);
        const auto p = c.getPointOnCircumference (r * (0.84f + 0.16f * std::pow (lobe, 1.6f)), a + angle);
        i == 0 ? star.startNewSubPath (p) : star.lineTo (p);
    }
    star.closeSubPath();

    // soft shadow on the panel and a dark contact edge
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillPath (star, juce::AffineTransform::scale (1.0f + (float) i * 0.025f, 1.0f + (float) i * 0.025f, c.x, c.y)
                              .translated (0.0f, r * 0.05f * (float) i));
    }
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillPath (star, juce::AffineTransform::translation (0.0f, r * 0.04f));

    // side wall of the moulded body, seen a little from above
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232326), c.x, c.y - r,
                                             juce::Colour (0xff020202), c.x, c.y + r, false));
    g.fillPath (star);

    // the domed top, lifted towards the viewer
    const auto lift = juce::AffineTransform::scale (0.93f, 0.93f, c.x, c.y).translated (0.0f, -r * 0.04f);
    juce::Path top (star);
    top.applyTransform (lift);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (top);

        // glossy black plastic: dark, with the dome lighter towards the light
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3f), c.x - r * 0.2f, c.y - r * 0.35f,
                                                 juce::Colour (0xff060607), c.x + r * 0.55f, c.y + r * 0.75f, true));
        g.fillPath (top);

        // reflection of a window above: a broad soft band along the upper edge ...
        const auto band = juce::Rectangle<float> (r * 1.3f, r * 0.62f).withCentre ({ c.x - r * 0.08f, c.y - r * 0.55f });
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.34f), band.getCentreX(), band.getY(),
                                                 juce::Colours::transparentWhite, band.getCentreX(), band.getBottom(), false));
        g.fillEllipse (band);

        // ... and a small sharp highlight on the dome
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (r * 0.30f, r * 0.13f).withCentre ({ c.x - r * 0.30f, c.y - r * 0.40f }));

        // the lower part of the dome turns away from the light
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c.x, c.y,
                                                 juce::Colours::black.withAlpha (0.45f), c.x, c.y + r, false));
        g.fillPath (top);
    }

    // glossy rim where the top rolls over into the side wall
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), c.x, c.y - r,
                                             juce::Colours::transparentWhite, c.x, c.y + r * 0.2f, false));
    g.strokePath (top, juce::PathStrokeType (std::max (0.8f, r * 0.035f)));
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.strokePath (star, juce::PathStrokeType (0.9f));
}

void LookAndFeel::drawKnobPointer (juce::Graphics& g, const KnobGeometry& k, float angle)
{
    // white index line printed on the top, towards one lobe
    const auto centre = k.centre.translated (0.0f, -k.capRadius * 0.04f);
    const auto width  = std::max (1.6f, k.capRadius * 0.09f);
    const auto inner  = centre.getPointOnCircumference (k.capRadius * 0.30f, angle);
    const auto outer  = centre.getPointOnCircumference (k.capRadius * 0.80f, angle);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine ({ inner.translated (0.0f, 0.8f), outer.translated (0.0f, 0.8f) }, width);
    g.setColour (colours::print);
    g.drawLine ({ inner, outer }, width);
}

// ── buttons, boxes, labels ──────────────────────────────────────────────────
void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    if (button.getButtonText().isEmpty())
        return;

    // a rubber push button in a recessed bezel
    const auto bounds = button.getLocalBounds().toFloat().reduced (2.5f);
    const auto active = button.getToggleState() || down;
    g.setColour (colours::background);
    g.fillRoundedRectangle (bounds.expanded (2.0f), 5.0f);
    g.setGradientFill (juce::ColourGradient (active ? colours::accent.brighter (0.2f) : juce::Colour (highlighted ? 0xff50555c : 0xff43474d),
                                             bounds.getX(), bounds.getY(),
                                             active ? colours::accent.darker (0.3f) : juce::Colour (0xff23262a),
                                             bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (bounds.getX() + 3.0f, bounds.getY() + 1.0f, bounds.getRight() - 3.0f, bounds.getY() + 1.0f, 1.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on     = button.getToggleState();
    const auto size   = std::min (bounds.getHeight() - 6.0f, 16.0f);
    const auto key    = juce::Rectangle<float> (size, size).withCentre ({ bounds.getX() + size * 0.5f + 4.0f, bounds.getCentreY() });

    drawSwitchKey (g, key, on, highlighted);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawFittedText (button.getButtonText(), bounds.withLeft (key.getRight() + 7.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.7f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                juce::ComboBox& box)
{
    // a recessed little display
    const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (bounds.translated (0.0f, 0.8f), 3.0f, 1.0f);
    g.setColour (juce::Colour (0xff070708));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawLine (bounds.getX() + 3.0f, bounds.getY() + 1.0f, bounds.getRight() - 3.0f, bounds.getY() + 1.0f, 1.5f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent : colours::outline.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 3.0f, 0.8f);

    juce::Path arrow;
    const auto cx = bounds.getRight() - 9.0f, cy = bounds.getCentreY();
    arrow.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
    g.setColour (colours::accent);
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (12.0f, juce::Font::bold);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return juce::FontOptions (13.0f);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (11.0f));
    label->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    return label;
}

void LookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    const auto role = label.getProperties()["naColour"].toString();
    if (role.isEmpty())
    {
        LookAndFeel_V4::drawLabel (g, label);
        return;
    }

    g.setColour (role == "dim" ? colours::textDim : colours::text);
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(), 1, 0.7f);
}

} // namespace na::ui
