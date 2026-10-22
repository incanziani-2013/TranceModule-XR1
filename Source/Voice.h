#pragma once
#include "Common.h"
#include <cmath>

//==============================================================================
// Un sample ya decodificado (44.1 kHz estereo) + version lista para loop.
struct SampleData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    juce::AudioBuffer<float> oneShot, loop;
    double sampleRate = 44100.0;
    int index = -1;

    // Loop con crossfade de 20 ms entre el final y el principio.
    void buildLoop()
    {
        const int len = oneShot.getNumSamples();
        const int xf = juce::jmax(1, juce::jmin((int) (0.02 * sampleRate), len / 4));
        const int loopLen = juce::jmax(1, len - xf);
        loop.setSize(2, loopLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* dst = loop.getWritePointer(ch);
            const auto* src = oneShot.getReadPointer(ch);
            juce::FloatVectorOperations::copy(dst, src, loopLen);
            for (int i = 0; i < xf && loopLen + i < len; ++i)
            {
                const float t = (float) i / (float) xf;
                dst[i] = src[i] * std::sin(t * juce::MathConstants<float>::halfPi)
                       + src[loopLen + i] * std::cos(t * juce::MathConstants<float>::halfPi);
            }
        }
    }
};

//==============================================================================
// Parametros y sample actual, compartidos por todas las voces.
struct VoiceShared
{
    std::atomic<float> *attack{}, *decay{}, *sustain{}, *release{};
    std::atomic<float> *cutoff{}, *res{}, *fltType{}, *fltEnv{}, *fltDecay{};
    std::atomic<float> *loop{}, *tune{}, *fine{};

    juce::SpinLock sampleLock;
    SampleData::Ptr current;

    SampleData::Ptr getCurrent()
    {
        const juce::SpinLock::ScopedTryLockType l(sampleLock);
        return l.isLocked() ? current : SampleData::Ptr();
    }
};

struct XRSound : public juce::SynthesiserSound
{
    bool appliesToNote(int) override    { return true; }
    bool appliesToChannel(int) override { return true; }
};

//==============================================================================
class XRVoice : public juce::SynthesiserVoice
{
public:
    explicit XRVoice(VoiceShared& s) : shared(s) {}

    using juce::SynthesiserVoice::renderNextBlock;

    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<XRSound*>(s) != nullptr; }

    void setCurrentPlaybackSampleRate(double sr) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(sr);
        if (sr > 0.0)
        {
            ampEnv.setSampleRate(sr);
            fltEnvelope.setSampleRate(sr);
            filter.prepare({ sr, 32u, 2u });
        }
    }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        data = shared.getCurrent();
        if (data == nullptr) { clearCurrentNote(); return; }

        looping   = shared.loop->load() > 0.5f;
        baseSemis = (double) (midiNote - 60) + shared.tune->load() + shared.fine->load() / 100.0;
        bendSemis = ((double) pitchWheel - 8192.0) / 8192.0 * 2.0;
        velGain   = std::pow(juce::jlimit(0.0f, 1.0f, velocity), 0.7f);
        pos = 0.0;

        ampEnv.setParameters({ shared.attack->load(), shared.decay->load(),
                               shared.sustain->load(), shared.release->load() });
        fltEnvelope.setParameters({ 0.001f, shared.fltDecay->load(), 0.0f, 0.1f });
        ampEnv.noteOn();
        fltEnvelope.noteOn();
        filter.reset();
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff && data != nullptr)
        {
            ampEnv.noteOff();
            fltEnvelope.noteOff();
        }
        else
        {
            data = nullptr;
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int v) override { bendSemis = ((double) v - 8192.0) / 8192.0 * 2.0; }
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (data == nullptr) return;

        const bool wrap = looping;
        const auto& src = wrap ? data->loop : data->oneShot;
        const int len = src.getNumSamples();
        const float* sl = src.getReadPointer(0);
        const float* sr = src.getReadPointer(1);
        const double srRatio = data->sampleRate / getSampleRate();
        const double endFade = 256.0;

        float* outL = out.getWritePointer(0, startSample);
        float* outR = out.getNumChannels() > 1 ? out.getWritePointer(1, startSample) : nullptr;

        const int mode = (int) shared.fltType->load(); // 0 LP, 1 HP, 2 BP, 3 Off
        const float baseCut = shared.cutoff->load();
        const float envAmt  = shared.fltEnv->load();
        const float q       = 0.7f + shared.res->load() * shared.res->load() * 11.0f;

        int done = 0;
        while (done < numSamples)
        {
            const int n = juce::jmin(32, numSamples - done);
            const double ratio = std::pow(2.0, (baseSemis + bendSemis) / 12.0) * srRatio;

            float tl[32], tr[32];
            float fe = 0.0f;
            bool ended = false;

            for (int i = 0; i < n; ++i)
            {
                const float env = ampEnv.getNextSample();
                fe = fltEnvelope.getNextSample();

                float l = 0.0f, r = 0.0f, endGain = 1.0f;
                if (! wrap && pos >= (double) (len - 1)) ended = true;

                if (! ended)
                {
                    const int i1 = (int) pos;
                    const float fr = (float) (pos - (double) i1);
                    l = hermite(sl, len, i1, fr, wrap);
                    r = hermite(sr, len, i1, fr, wrap);
                    pos += ratio;
                    if (wrap) { while (pos >= (double) len) pos -= (double) len; }
                    else
                    {
                        const double rem = (double) len - pos;
                        if (rem < endFade) endGain = (float) juce::jmax(0.0, rem / endFade);
                    }
                }

                const float g = env * velGain * endGain;
                tl[i] = l * g;
                tr[i] = r * g;
            }

            if (mode != 3)
            {
                static constexpr juce::dsp::StateVariableTPTFilterType types[] = {
                    juce::dsp::StateVariableTPTFilterType::lowpass,
                    juce::dsp::StateVariableTPTFilterType::highpass,
                    juce::dsp::StateVariableTPTFilterType::bandpass };
                filter.setType(types[juce::jlimit(0, 2, mode)]);
                filter.setResonance(q);
                const float hz = baseCut * std::pow(2.0f, envAmt * fe * 5.0f);
                filter.setCutoffFrequency(juce::jlimit(20.0f, (float) (0.45 * getSampleRate()), hz));
                float* chans[2] = { tl, tr };
                juce::dsp::AudioBlock<float> blk(chans, 2, (size_t) n);
                filter.process(juce::dsp::ProcessContextReplacing<float>(blk));
            }

            for (int i = 0; i < n; ++i)
            {
                outL[done + i] += tl[i];
                if (outR != nullptr) outR[done + i] += tr[i];
            }
            done += n;

            if (ended || ! ampEnv.isActive())
            {
                data = nullptr;
                clearCurrentNote();
                break;
            }
        }
    }

private:
    static inline float hermite(const float* d, int len, int i1, float x, bool wrap)
    {
        auto g = [&](int i) -> float
        {
            if (wrap) { i %= len; if (i < 0) i += len; return d[i]; }
            return (i < 0 || i >= len) ? 0.0f : d[i];
        };
        const float y0 = g(i1 - 1), y1 = g(i1), y2 = g(i1 + 1), y3 = g(i1 + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * x + c2) * x + c1) * x + y1;
    }

    VoiceShared& shared;
    SampleData::Ptr data;
    juce::ADSR ampEnv, fltEnvelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    double pos = 0.0, baseSemis = 0.0, bendSemis = 0.0;
    float velGain = 1.0f;
    bool looping = false;
};
