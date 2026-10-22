#pragma once
#include "Common.h"

namespace XRColours
{
    const juce::Colour yellowTop   { 0xffffd84a };
    const juce::Colour yellowMid   { 0xfff5c400 };
    const juce::Colour yellowBot   { 0xffd9a200 };
    const juce::Colour ink         { 0xff161616 };
    const juce::Colour earMetal    { 0xff2a2a2a };
    const juce::Colour lcdLight    { 0xffb9d84e };
    const juce::Colour lcdDark     { 0xff8db02c };
    const juce::Colour lcdText     { 0xff16240a };
    const juce::Colour ledOn       { 0xff3dff5a };
}

class XR1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    XR1LookAndFeel()
    {
        using namespace XRColours;
        setColour(juce::ComboBox::backgroundColourId, ink);
        setColour(juce::ComboBox::textColourId, yellowMid);
        setColour(juce::ComboBox::arrowColourId, yellowMid);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::PopupMenu::backgroundColourId, ink);
        setColour(juce::PopupMenu::textColourId, yellowMid);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, yellowMid);
        setColour(juce::PopupMenu::highlightedTextColourId, ink);
        setColour(juce::TextButton::buttonColourId, ink);
        setColour(juce::TextButton::textColourOffId, yellowMid);
        setColour(juce::TextButton::textColourOnId, yellowMid);
        setColour(juce::BubbleComponent::backgroundColourId, ink);
        setColour(juce::TooltipWindow::textColourId, yellowMid);
        setColour(juce::Label::textColourId, ink);
    }

    static juce::Font bold(float h) { return juce::Font(juce::FontOptions(h, juce::Font::bold)); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                          float startAngle, float endAngle, juce::Slider&) override
    {
        using namespace XRColours;
        const auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(2.0f);
        const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = startAngle + pos * (endAngle - startAngle);

        // marcas alrededor
        g.setColour(ink.withAlpha(0.75f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const float s = std::sin(a), co = -std::cos(a);
            const float r1 = r * 0.84f, r2 = r * (i % 5 == 0 ? 1.0f : 0.94f);
            g.drawLine(c.x + s * r1, c.y + co * r1, c.x + s * r2, c.y + co * r2, i % 5 == 0 ? 2.0f : 1.2f);
        }

        const float kr = r * 0.72f;
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(c.x - kr + 1.5f, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);

        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5a5a5a), c.x - kr * 0.4f, c.y - kr * 0.5f,
                                               juce::Colour(0xff0c0c0c), c.x + kr * 0.8f, c.y + kr * 0.9f, true));
        g.fillEllipse(c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour(juce::Colours::black);
        g.drawEllipse(c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawEllipse(c.x - kr + 3, c.y - kr + 3, kr * 2.0f - 6, kr * 2.0f - 6, 1.0f);

        const float s = std::sin(ang), co = -std::cos(ang);
        g.setColour(yellowMid);
        g.drawLine(c.x + s * kr * 0.25f, c.y + co * kr * 0.25f, c.x + s * kr * 0.88f, c.y + co * kr * 0.88f, 3.2f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& bt, const juce::Colour&, bool hi, bool down) override
    {
        auto b = bt.getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(XRColours::ink.brighter(down ? 0.35f : hi ? 0.15f : 0.0f));
        g.fillRoundedRectangle(b, 5.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override { return bold(14.0f); }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& bt, bool hi, bool) override
    {
        using namespace XRColours;
        auto b = bt.getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(ink.brighter(hi ? 0.15f : 0.0f));
        g.fillRoundedRectangle(b, 5.0f);

        const float d = 11.0f;
        const auto led = juce::Rectangle<float>(b.getX() + 12.0f, b.getCentreY() - d / 2, d, d);
        g.setColour(bt.getToggleState() ? ledOn : juce::Colour(0xff2f4a2f));
        g.fillEllipse(led);
        if (bt.getToggleState())
        {
            g.setColour(ledOn.withAlpha(0.3f));
            g.fillEllipse(led.expanded(3.0f));
        }
        g.setColour(yellowMid);
        g.setFont(bold(14.0f));
        g.drawText(bt.getButtonText(), b.withTrimmedLeft(30.0f), juce::Justification::centredLeft);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return bold(14.0f); }
    juce::Font getPopupMenuFont() override { return bold(15.0f); }
};
