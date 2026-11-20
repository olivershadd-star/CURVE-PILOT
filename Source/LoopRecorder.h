#pragma once

#include <JuceHeader.h>
#include <cmath>

class LoopRecorder {
public:
    enum class State { idle, armed, recording, complete, interrupted };

    bool arm(double rate, int bars, double bpm)
    {
        if (state.load() != State::idle || rate <= 0 || bars < 1 || bars > 32
            || bpm < 40 || bpm > 240)
            return false;

        const auto frames = rate * bars * 240.0 / bpm;
        if (frames < 1 || frames > 16000000)
            return false;

        audio.setSize(2, static_cast<int>(std::llround(frames)));
        sampleRate = rate;
        tempo = bpm;
        position = 0;
        state.store(State::armed);
        return true;
    }

    bool start(double rate) noexcept
    {
        if (state.load() != State::armed)
            return false;
        if (rate != sampleRate)
        {
            state.store(State::interrupted);
            return false;
        }
        state.store(State::recording);
        return true;
    }

    void capture(const juce::AudioBuffer<float>& source) noexcept
    {
        if (state.load() != State::recording)
            return;

        const auto count = juce::jmin(source.getNumSamples(), audio.getNumSamples() - position);
        for (int channel = 0; channel < 2; ++channel)
            audio.copyFrom(channel, position, source, channel, 0, count);

        position += count;
        if (position == audio.getNumSamples())
            state.store(State::complete);
    }

    void interrupt() noexcept
    {
        const auto current = state.load();
        if (current == State::armed || current == State::recording)
            state.store(State::interrupted);
    }

    bool save(const juce::File& destination) const
    {
        if (state.load() != State::complete)
            return false;

        juce::TemporaryFile temporary(destination);
        std::unique_ptr<juce::OutputStream> stream = temporary.getFile().createOutputStream();
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            format.createWriterFor(stream.get(), sampleRate, 2, 24, {}, 0));
        if (writer == nullptr)
            return false;

        stream.release();
        if (!writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()) || !writer->flush())
            return false;
        writer.reset();
        return temporary.overwriteTargetFileWithTemporary();
    }

    void discard()
    {
        const auto current = state.load();
        if (current == State::complete || current == State::interrupted)
        {
            audio.setSize(0, 0);
            state.store(State::idle);
        }
    }

    State getState() const noexcept { return state.load(); }
    double getTempo() const noexcept { return tempo; }

private:
    std::atomic<State> state{State::idle};
    juce::AudioBuffer<float> audio;
    double sampleRate = 0;
    double tempo = 120;
    int position = 0;
};
