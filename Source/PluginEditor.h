#pragma once
#include "Common.h"
#include "LookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class LcdDisplay : public juce::Component
{
public:
    std::function<void()> onClick;

    LcdDisplay() { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

    void setLines(const juce::String& a, const juce::String& b, const juce::String& c)
    {
        if (a == l1 && b == l2 && c == l3) return;
        l1 = a; l2 = b; l3 = c;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        using namespace XRColours;
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(bounds, 9.0f);
        auto in = bounds.reduced(6.0f);
        g.setGradientFill(juce::ColourGradient(lcdLight, in.getX(), in.getY(), lcdDark, in.getX(), in.getBottom(), false));
        g.fillRoundedRectangle(in, 5.0f);

        g.setColour(juce::Colours::black.withAlpha(0.06f));
        for (float y = in.getY(); y < in.getBottom(); y += 3.0f)
            g.drawHorizontalLine((int) y, in.getX(), in.getRight());

        const auto mono = juce::Font::getDefaultMonospacedFontName();
        g.setColour(lcdText);
        auto area = in.reduced(12.0f, 5.0f);
        g.setFont(juce::Font(juce::FontOptions(mono, 13.0f, juce::Font::bold)));
        g.drawText(l1, area.removeFromTop(18.0f), juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(mono, 30.0f, juce::Font::bold)));
        g.drawText(l2, area.removeFromTop(40.0f), juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(mono, 13.0f, juce::Font::bold)));
        g.drawText(l3, area, juce::Justification::centredLeft);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (onClick != nullptr && getLocalBounds().contains(e.getPosition())) onClick();
    }

private:
    juce::String l1, l2, l3;
};

//==============================================================================
class XR1Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit XR1Editor(XR1Processor&);
    ~XR1Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        int row = 0, col = 0;
    };
    struct Section { juce::String name; int row, c0, c1; };

    void timerCallback() override;
    void stepSound(int delta);
    void setSound(int index);
    void showSoundMenu();

    XR1Processor& proc;
    XR1LookAndFeel laf;

    LcdDisplay lcd;
    juce::TextButton prevBtn { "PREV" }, nextBtn { "NEXT" };
    juce::ToggleButton loopBtn { "LOOP" };
    juce::ComboBox fltType;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fltAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XR1Editor)
};
