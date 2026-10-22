#pragma once
#include "Common.h"
#include "SoundBank.h"
#include "Voice.h"

//==============================================================================
// Hilo que decodifica el sample elegido sin tocar el hilo de audio.
class SampleLoader : public juce::Thread
{
public:
    SampleLoader(VoiceShared& s, std::atomic<float>& secondsOut);
    ~SampleLoader() override { stopThread(3000); }

    void request(int index) { requested = index; notify(); }
    void loadNow(int index) { load(index); }
    void run() override;

private:
    void load(int index);

    VoiceShared& shared;
    std::atomic<float>& seconds;
    std::atomic<int> requested { -1 };
    juce::AudioFormatManager formats;
    juce::Array<SampleData::Ptr> graveyard;
};

//==============================================================================
class XR1Processor : public juce::AudioProcessor
{
public:
    XR1Processor();
    ~XR1Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Trance Module XR-1"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> loadedSeconds { 0.0f };
    std::atomic<int>   activeVoices { 0 };

private:
    static constexpr int kNumVoices = 16;

    VoiceShared shared;
    SampleLoader loader;
    juce::Synthesiser synth;

    std::atomic<float> *soundParam{}, *delayTime{}, *delayFb{}, *delayMix{},
                       *reverbMix{}, *reverbSize{}, *volume{};
    int lastSound = -1;

    // Efectos
    juce::Reverb reverb;
    std::vector<float> delayBuf[2];
    int delayWrite = 0;
    float delaySmoothed = 0.0f;
    juce::SmoothedValue<float> gainSmooth;
    double currentSR = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XR1Processor)
};
