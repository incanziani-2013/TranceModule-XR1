#include "PluginEditor.h"

namespace
{
    constexpr int kW = 1400, kH = 150;
    constexpr int kEar = 36;                  // orejas del rack
    constexpr int kX0 = kEar + 22;            // inicio del contenido
    constexpr int kKnobX0 = 548;              // inicio de la fila de perillas
    constexpr int kCell = 48;                 // ancho de cada perilla
    constexpr int kGap = 6;                   // espacio entre secciones
    constexpr int kKnobY = 62, kKnobSize = 46;
    constexpr int kSecY = 52;                 // linea de las etiquetas de seccion

    // columnas donde empieza una seccion nueva (para dejar un espacio)
    int colX(int col)
    {
        int gaps = 0;
        for (int start : { 4, 8, 10, 13, 15 }) if (col >= start) ++gaps;
        return kKnobX0 + col * kCell + gaps * kGap;
    }
}

XR1Editor::XR1Editor(XR1Processor& p) : juce::AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);

    struct Spec { const char* id; const char* text; int row; int col; };
    const Spec specs[] = {
        { "attack", "ATK", 0, 0 }, { "decay", "DEC", 0, 1 }, { "sustain", "SUS", 0, 2 }, { "release", "REL", 0, 3 },
        { "cutoff", "CUT", 0, 4 }, { "res", "RES", 0, 5 }, { "fltEnv", "ENV", 0, 6 }, { "fltDecay", "E.DEC", 0, 7 },
        { "tune", "TUNE", 0, 8 }, { "fine", "FINE", 0, 9 },
        { "delayTime", "TIME", 0, 10 }, { "delayFb", "FDBK", 0, 11 }, { "delayMix", "MIX", 0, 12 },
        { "reverbSize", "SIZE", 0, 13 }, { "reverbMix", "MIX", 0, 14 },
        { "volume", "VOL", 0, 15 } };

    for (auto& s : specs)
    {
        auto k = std::make_unique<Knob>();
        k->row = s.row; k->col = s.col;
        k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k->slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        k->slider.setPopupDisplayEnabled(true, true, this);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, s.id, k->slider);
        if (auto* par = proc.apvts.getParameter(s.id))
            k->slider.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
        k->label.setText(s.text, juce::dontSendNotification);
        k->label.setJustificationType(juce::Justification::centred);
        k->label.setFont(XR1LookAndFeel::bold(11.0f));
        k->label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(k->slider);
        addAndMakeVisible(k->label);
        knobs.push_back(std::move(k));
    }

    sections = { { "AMP ENV", 0, 0, 3 }, { "FILTER", 0, 4, 7 }, { "PITCH", 0, 8, 9 },
                 { "DELAY", 0, 10, 12 }, { "REVERB", 0, 13, 14 }, { "OUT", 0, 15, 15 } };

    lcd.onClick = [this] { showSoundMenu(); };
    addAndMakeVisible(lcd);

    prevBtn.onClick = [this] { stepSound(-1); };
    nextBtn.onClick = [this] { stepSound(+1); };
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);

    addAndMakeVisible(loopBtn);
    loopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "loop", loopBtn);

    fltType.addItemList({ "LOWPASS", "HIGHPASS", "BANDPASS", "OFF" }, 1);
    addAndMakeVisible(fltType);
    fltAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "fltType", fltType);

    setSize(kW, kH);   // al final: resized() necesita que ya existan todos los controles
    timerCallback();
    startTimerHz(15);
}

XR1Editor::~XR1Editor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void XR1Editor::paint(juce::Graphics& g)
{
    using namespace XRColours;
    const auto full = getLocalBounds().toFloat();

    // placa amarilla
    g.setGradientFill(juce::ColourGradient(yellowTop, 0.0f, 0.0f, yellowBot, 0.0f, full.getBottom(), false));
    g.fillAll();
    g.setColour(yellowMid.withAlpha(0.35f));
    for (int y = 0; y < kH; y += 4) g.drawHorizontalLine(y, (float) kEar, (float) (kW - kEar));   // acabado cepillado
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawHorizontalLine(1, (float) kEar, (float) (kW - kEar));

    // orejas del rack con tornillos
    for (int side = 0; side < 2; ++side)
    {
        const float x = side == 0 ? 0.0f : (float) (kW - kEar);
        g.setGradientFill(juce::ColourGradient(earMetal.brighter(0.2f), x, 0.0f, earMetal.darker(0.4f), x + kEar, 0.0f, false));
        g.fillRect(x, 0.0f, (float) kEar, (float) kH);
        for (float y : { 34.0f, (float) kH - 34.0f })
        {
            const float cx = x + kEar / 2.0f;
            g.setColour(juce::Colours::black);
            g.fillEllipse(cx - 9.0f, y - 9.0f, 18.0f, 18.0f);
            g.setColour(juce::Colour(0xff9a9a9a));
            g.fillEllipse(cx - 7.0f, y - 7.0f, 14.0f, 14.0f);
            g.setColour(juce::Colour(0xff3a3a3a));
            g.drawLine(cx - 5.0f, y - 2.0f, cx + 5.0f, y + 2.0f, 2.0f);
        }
    }

    // marca
    g.setColour(ink);
    g.setFont(XR1LookAndFeel::bold(20.0f));
    g.drawText("TRANCE MODULE", kX0, 6, 190, 26, juce::Justification::centredLeft);
    const juce::Rectangle<float> tag(kX0 + 168.0f, 9.0f, 56.0f, 21.0f);
    g.fillRoundedRectangle(tag, 4.0f);
    g.setColour(yellowMid);
    g.setFont(XR1LookAndFeel::bold(16.0f));
    g.drawText("XR-1", tag.toNearestInt(), juce::Justification::centred);
    g.setColour(ink);
    g.setFont(XR1LookAndFeel::bold(11.0f));
    g.drawText("16-VOICE SAMPLE MODULE", kW - kEar - 22 - 240, 10, 240, 18, juce::Justification::centredRight);

    // etiquetas de seccion con corchete
    for (auto& s : sections)
    {
        const float y = (float) kSecY;
        const float x0 = (float) colX(s.c0) + 3.0f;
        const float x1 = (float) (colX(s.c1) + kCell) - 3.0f;
        g.setColour(ink);
        g.setFont(XR1LookAndFeel::bold(11.0f));
        const int tw = (int) juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), s.name) + 10;
        const float mid = (x0 + x1) / 2.0f;
        if (tw + 12 < (int) (x1 - x0))
        {
            g.drawText(s.name, (int) mid - tw / 2, (int) y - 8, tw, 16, juce::Justification::centred);
            g.drawLine(x0, y, mid - tw / 2.0f, y, 1.3f);
            g.drawLine(mid + tw / 2.0f, y, x1, y, 1.3f);
        }
        else
        {
            g.drawText(s.name, (int) x0 - 6, (int) y - 17, (int) (x1 - x0) + 12, 14, juce::Justification::centred);
            g.drawLine(x0, y, x1, y, 1.3f);
        }
        g.drawLine(x0, y, x0, y + 5.0f, 1.3f);
        g.drawLine(x1, y, x1, y + 5.0f, 1.3f);
    }
}

void XR1Editor::resized()
{
    lcd.setBounds(kX0, 36, 304, 100);

    const int bx = kX0 + 320;
    prevBtn.setBounds(bx, 36, 78, 30);
    nextBtn.setBounds(bx + 84, 36, 78, 30);
    loopBtn.setBounds(bx, 72, 162, 30);
    fltType.setBounds(bx, 106, 162, 30);

    for (auto& k : knobs)
    {
        const int cx = colX(k->col);
        k->slider.setBounds(cx + 1, kKnobY, kKnobSize, kKnobSize);
        k->label.setBounds(cx - 2, kKnobY + kKnobSize + 1, kCell + 4, 14);
    }
}

//==============================================================================
void XR1Editor::stepSound(int delta)
{
    const int n = (int) getSoundBank().size();
    if (n == 0) return;
    const int cur = (int) proc.apvts.getRawParameterValue("sound")->load();
    setSound(((cur + delta) % n + n) % n);
}

void XR1Editor::setSound(int index)
{
    if (auto* p = proc.apvts.getParameter("sound"))
        p->setValueNotifyingHost(p->convertTo0to1((float) index));
}

void XR1Editor::showSoundMenu()
{
    const auto& bank = getSoundBank();
    const int cur = (int) proc.apvts.getRawParameterValue("sound")->load();

    juce::PopupMenu root;
    juce::String lastCat;
    juce::PopupMenu sub;
    bool subHasTick = false;
    auto flush = [&]
    {
        if (lastCat.isNotEmpty()) root.addSubMenu(lastCat.toUpperCase(), sub, true, nullptr, subHasTick);
        sub = juce::PopupMenu();
        subHasTick = false;
    };
    for (int i = 0; i < (int) bank.size(); ++i)
    {
        if (bank[(size_t) i].category != lastCat) { flush(); lastCat = bank[(size_t) i].category; }
        sub.addItem(i + 1, bank[(size_t) i].choiceName, true, i == cur);
        subHasTick = subHasTick || i == cur;
    }
    flush();

    root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&lcd).withMinimumWidth(220),
                       [safe = juce::Component::SafePointer<XR1Editor>(this)](int result)
                       { if (safe != nullptr && result > 0) safe->setSound(result - 1); });
}

void XR1Editor::timerCallback()
{
    const auto& bank = getSoundBank();
    const int idx = (int) proc.apvts.getRawParameterValue("sound")->load();
    const bool valid = juce::isPositiveAndBelow(idx, (int) bank.size());

    const float secs = proc.loadedSeconds.load();
    const bool loop = proc.apvts.getRawParameterValue("loop")->load() > 0.5f;
    const int v = proc.activeVoices.load();

    juce::String info = juce::String::formatted("%05.2fs  %s  VOICES %02d", secs, loop ? "LOOP" : "1-SHOT", v);
    lcd.setLines("TRANCE MODULE XR-1",
                 valid ? bank[(size_t) idx].lcdName : juce::String("--"),
                 valid ? info : juce::String("NO SAMPLES"));
}
