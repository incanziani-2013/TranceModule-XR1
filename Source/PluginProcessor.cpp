#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SampleLoader::SampleLoader(VoiceShared& s, std::atomic<float>& secondsOut)
    : juce::Thread("XR-1 sample loader"), shared(s), seconds(secondsOut)
{
    formats.registerBasicFormats();
}

void SampleLoader::run()
{
    while (! threadShouldExit())
    {
        const int idx = requested.exchange(-1);
        if (idx >= 0) load(idx);

        // liberar samples viejos fuera del hilo de audio
        for (int i = graveyard.size(); --i >= 0;)
            if (graveyard.getReference(i)->getReferenceCount() == 1)
                graveyard.remove(i);

        wait(250);
    }
}

void SampleLoader::load(int index)
{
    const auto& bank = getSoundBank();
    if (! juce::isPositiveAndBelow(index, (int) bank.size())) return;
    const auto& e = bank[(size_t) index];

    std::unique_ptr<juce::AudioFormatReader> reader(
        formats.createReaderFor(std::make_unique<juce::MemoryInputStream>(e.data, (size_t) e.size, false)));
    if (reader == nullptr || reader->lengthInSamples <= 0) return;

    const int len = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> tmp((int) juce::jmax(2u, reader->numChannels), len);
    reader->read(&tmp, 0, len, 0, true, true);
    if (reader->numChannels == 1) tmp.copyFrom(1, 0, tmp, 0, 0, len);

    SampleData::Ptr sd = new SampleData();
    sd->index = index;
    sd->sampleRate = reader->sampleRate;
    sd->oneShot.setSize(2, len);
    sd->oneShot.copyFrom(0, 0, tmp, 0, 0, len);
    sd->oneShot.copyFrom(1, 0, tmp, 1, 0, len);
    sd->buildLoop();

    SampleData::Ptr old;
    {
        const juce::SpinLock::ScopedLockType l(shared.sampleLock);
        old = shared.current;
        shared.current = sd;
    }
    if (old != nullptr) graveyard.add(old);
    seconds = (float) ((double) len / reader->sampleRate);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout XR1Processor::createLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    StringArray names;
    for (auto& e : getSoundBank()) names.add(e.choiceName);
    if (names.isEmpty()) names.add("(sin samples)");

    auto secs = [](float v, int) { return v < 1.0f ? String(roundToInt(v * 1000.0f)) + " ms" : String(v, 2) + " s"; };
    auto hz   = [](float v, int) { return v < 1000.0f ? String(roundToInt(v)) + " Hz" : String(v / 1000.0f, 2) + " kHz"; };
    auto pct  = [](float v, int) { return String(roundToInt(v * 100.0f)) + " %"; };
    auto semi = [](float v, int) { return String(roundToInt(v)) + " st"; };
    auto cent = [](float v, int) { return String(roundToInt(v)) + " ct"; };
    auto db   = [](float v, int) { return String(v, 1) + " dB"; };
    auto ms   = [](float v, int) { return String(roundToInt(v)) + " ms"; };
    auto bip  = [](float v, int) { return (v > 0 ? "+" : "") + String(roundToInt(v * 100.0f)) + " %"; };
    auto num  = [](float v, int) { return String(v, 2); };

    auto F = [&](const char* id, const char* name, NormalisableRange<float> r, float def, auto fn)
    { p.push_back(std::make_unique<AudioParameterFloat>(ParameterID { id, 1 }, name, r, def, Attr().withStringFromValueFunction(fn))); };

    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { "sound", 1 }, "Sound", names, 0));

    F("attack",  "Attack",  { 0.001f, 5.0f, 0.0f, 0.3f }, 0.005f, secs);
    F("decay",   "Decay",   { 0.001f, 5.0f, 0.0f, 0.3f }, 0.5f,   secs);
    F("sustain", "Sustain", { 0.0f, 1.0f }, 1.0f, pct);
    F("release", "Release", { 0.001f, 8.0f, 0.0f, 0.3f }, 0.25f,  secs);

    F("cutoff",   "Cutoff",     { 20.0f, 20000.0f, 0.0f, 0.3f }, 20000.0f, hz);
    F("res",      "Resonance",  { 0.0f, 1.0f }, 0.1f, num);
    F("fltEnv",   "Filter Env", { -1.0f, 1.0f }, 0.0f, bip);
    F("fltDecay", "Filter Decay", { 0.01f, 4.0f, 0.0f, 0.4f }, 0.4f, secs);
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID { "fltType", 1 }, "Filter Type",
                                                       StringArray { "LP", "HP", "BP", "OFF" }, 0));

    p.push_back(std::make_unique<AudioParameterBool>(ParameterID { "loop", 1 }, "Loop", false));
    F("tune", "Tune", { -24.0f, 24.0f, 1.0f }, 0.0f, semi);
    F("fine", "Fine", { -100.0f, 100.0f, 1.0f }, 0.0f, cent);

    F("delayTime", "Delay Time",     { 10.0f, 1000.0f, 0.0f, 0.5f }, 321.0f, ms);
    F("delayFb",   "Delay Feedback", { 0.0f, 0.95f }, 0.4f, pct);
    F("delayMix",  "Delay Mix",      { 0.0f, 1.0f }, 0.0f, pct);
    F("reverbSize","Reverb Size",    { 0.0f, 1.0f }, 0.6f, pct);
    F("reverbMix", "Reverb Mix",     { 0.0f, 1.0f }, 0.15f, pct);

    F("volume", "Volume", { -36.0f, 6.0f, 0.0f, 1.6f }, -3.0f, db);

    return { p.begin(), p.end() };
}

//==============================================================================
XR1Processor::XR1Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "XR1_STATE", createLayout()),
      loader(shared, loadedSeconds)
{
    auto g = [this](const char* id) { return apvts.getRawParameterValue(id); };
    shared.attack = g("attack");   shared.decay = g("decay");
    shared.sustain = g("sustain"); shared.release = g("release");
    shared.cutoff = g("cutoff");   shared.res = g("res");
    shared.fltType = g("fltType"); shared.fltEnv = g("fltEnv");
    shared.fltDecay = g("fltDecay");
    shared.loop = g("loop");       shared.tune = g("tune"); shared.fine = g("fine");

    soundParam = g("sound");
    delayTime = g("delayTime"); delayFb = g("delayFb"); delayMix = g("delayMix");
    reverbMix = g("reverbMix"); reverbSize = g("reverbSize");
    volume = g("volume");

    synth.addSound(new XRSound());
    for (int i = 0; i < kNumVoices; ++i) synth.addVoice(new XRVoice(shared));

    lastSound = (int) soundParam->load();
    loader.loadNow(lastSound);   // primer sonido listo antes de abrir el editor
    loader.startThread();
}

XR1Processor::~XR1Processor()
{
    loader.stopThread(3000);
    synth.clearVoices();
}

bool XR1Processor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void XR1Processor::prepareToPlay(double sr, int)
{
    currentSR = sr;
    synth.setCurrentPlaybackSampleRate(sr);

    const size_t size = (size_t) (sr * 1.2) + 8;
    for (auto& b : delayBuf) b.assign(size, 0.0f);
    delayWrite = 0;
    delaySmoothed = (float) (delayTime->load() * 0.001 * sr);

    reverb.setSampleRate(sr);
    reverb.reset();
    gainSmooth.reset(sr, 0.03);
    gainSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(volume->load()));
}

void XR1Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // cambio de sonido -> decodificar en el hilo auxiliar
    const int snd = (int) soundParam->load();
    if (snd != lastSound) { lastSound = snd; loader.request(snd); }

    synth.renderNextBlock(buffer, midi, 0, n);

    int active = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice(i)->isVoiceActive()) ++active;
    activeVoices = active;

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

    // --- Delay ping-pong ---
    const float mix = delayMix->load();
    const float fb  = delayFb->load();
    const float target = (float) (delayTime->load() * 0.001 * currentSR);
    const int size = (int) delayBuf[0].size();
    if (size > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            delaySmoothed += (target - delaySmoothed) * 0.0008f;
            float rp = (float) delayWrite - delaySmoothed;
            while (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp % size;
            const int i1 = (i0 + 1) % size;
            const float fr = rp - std::floor(rp);
            const float wl = delayBuf[0][(size_t) i0] * (1.0f - fr) + delayBuf[0][(size_t) i1] * fr;
            const float wr = delayBuf[1][(size_t) i0] * (1.0f - fr) + delayBuf[1][(size_t) i1] * fr;
            const float inL = L[i], inR = R[i];
            delayBuf[0][(size_t) delayWrite] = inL + fb * wr;
            delayBuf[1][(size_t) delayWrite] = inR + fb * wl;
            delayWrite = (delayWrite + 1) % size;
            L[i] = inL + mix * wl;
            R[i] = inR + mix * wr;
        }
    }

    // --- Reverb ---
    juce::Reverb::Parameters rp;
    rp.roomSize = reverbSize->load();
    rp.damping = 0.45f;
    rp.wetLevel = reverbMix->load() * 0.5f;
    rp.dryLevel = 1.0f;
    rp.width = 1.0f;
    reverb.setParameters(rp);
    reverb.processStereo(L, R, n);

    // --- Volumen + limitador suave ---
    gainSmooth.setTargetValue(juce::Decibels::decibelsToGain(volume->load()));
    auto soft = [](float x)
    {
        const float a = std::abs(x);
        if (a <= 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f);
        return x < 0.0f ? -y : y;
    };
    for (int i = 0; i < n; ++i)
    {
        const float gn = gainSmooth.getNextValue();
        L[i] = soft(L[i] * gn);
        R[i] = soft(R[i] * gn);
    }
}

juce::AudioProcessorEditor* XR1Processor::createEditor() { return new XR1Editor(*this); }

void XR1Processor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void XR1Processor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XR1Processor(); }
