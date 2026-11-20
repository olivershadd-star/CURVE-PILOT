#pragma once
#include <JuceHeader.h>
#include "LoopRecorder.h"
#include <array>
#include <cmath>
#include <stdexcept>

// One instrument call per device buffer.
class Engine final:public juce::AudioProcessor {
public:
    static constexpr int laneCount = 12;
    static constexpr int pointCount = 257;
    struct Lane
    {
        std::array<float, pointCount> points{};
        int target = -1;
        bool enabled = false;
        double bars = 4;
        float minimum = 0;
        float maximum = 1;
    };

    void setLane(int index, const Lane& lane)
    {
        const juce::SpinLock::ScopedLockType lock(curveLock);
        pending[index] = lane;
        ++revision;
    }

    std::atomic<bool> playing{false}, restart{false};
    std::atomic<double> tempo{120};
    LoopRecorder recorder;

    Engine():AudioProcessor(BusesProperties().withOutput("Stereo",juce::AudioChannelSet::stereo(),true)){}
    // Only replace the instrument after removing the device audio callback.
    void replace(std::unique_ptr<juce::AudioPluginInstance> next){
        if (!next)
        {
            instrument.reset();
            return;
        }
        next->disableNonMainBuses();
        auto layout = next->getBusesLayout();
        for (auto& bus : layout.inputBuses)
            bus = juce::AudioChannelSet::disabled();
        if (layout.outputBuses.isEmpty())
            throw std::runtime_error("Le VST ne possede pas de sortie audio.");
        layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
        if (!next->setBusesLayout(layout))
            throw std::runtime_error("Ce prototype exige une sortie stereo sans entree audio.");
        instrument = std::move(next);
        calls = 0;
        samples = 0;
        beat = 0;
    }
    juce::AudioPluginInstance* get()const{return instrument.get();}
    void prepareToPlay(double rate,int size)override{sampleRate=rate;if(instrument){instrument->setPlayHead(&transport);instrument->setRateAndBufferSizeDetails(rate,size);instrument->prepareToPlay(rate,size);}}
    void releaseResources()override{recorder.interrupt();if(instrument)instrument->releaseResources();}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{
        juce::ScopedNoDenormals noDenormals;
        if(!instrument){buffer.clear();midi.clear();return;}
        const auto startRecording = recorder.start(sampleRate);
        if (restart.exchange(false) || startRecording)
        {
            beat = 0;
            transportSamples = 0;
        }

        if (const juce::SpinLock::ScopedTryLockType lock(curveLock); lock.isLocked() && revision != appliedRevision)
        {
            active = pending;
            appliedRevision = revision;
        }

        const auto recording = recorder.getState() == LoopRecorder::State::recording;
        const auto running = playing.load() || recording;
        const auto bpm = recording ? recorder.getTempo() : tempo.load();
        transport.position.setBpm(bpm);
        transport.position.setTimeSignature(juce::AudioPlayHead::TimeSignature{4, 4});
        transport.position.setPpqPosition(beat);
        transport.position.setTimeInSamples(transportSamples);
        transport.position.setIsPlaying(running);

        if (running)
        {
            const auto& parameters = instrument->getParameters();
            for (size_t i = 0; i < active.size(); ++i)
            {
                const auto& lane = active[i];
                if (!lane.enabled || lane.target < 0 || lane.target >= parameters.size())
                    continue;
                const auto cycles = beat / (4 * lane.bars);
                const auto point = (cycles - std::floor(cycles)) * (pointCount - 1);
                const auto left = juce::jmin(static_cast<int>(point), pointCount - 2);
                const auto value = juce::jmap(static_cast<float>(point - left),
                                             lane.points[left], lane.points[left + 1]);
                const auto scaled = juce::jmap(value, lane.minimum, lane.maximum);
                auto* parameter = parameters[lane.target];
                if (std::abs(parameter->getValue() - scaled) > 0.00001f)
                    parameter->setValue(scaled);
            }
            beat += buffer.getNumSamples() * bpm / (60 * sampleRate);
            transportSamples += buffer.getNumSamples();
        }

        buffer.clear();
        instrument->processBlock(buffer,midi);
        recorder.capture(buffer);
        calls.fetch_add(1,std::memory_order_relaxed);samples.fetch_add((uint64_t)buffer.getNumSamples(),std::memory_order_relaxed);
    }
    std::atomic<uint64_t> calls{0},samples{0};
    const juce::String getName()const override{return "Curve Pilot autonome prototype 0.2";}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return instrument?instrument->getTailLengthSeconds():0;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
private:
    struct Transport final : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override { return position; }
        PositionInfo position;
    };

    Transport transport;
    juce::SpinLock curveLock;
    std::array<Lane, laneCount> pending{}, active{};
    unsigned revision = 0, appliedRevision = 0;
    double sampleRate = 44100, beat = 0;
    int64_t transportSamples = 0;
    std::unique_ptr<juce::AudioPluginInstance> instrument;
};
