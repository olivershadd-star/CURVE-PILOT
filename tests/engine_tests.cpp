#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../Source/Engine.h"
#include <cassert>
#include <iostream>
struct TestParameter final : juce::AudioParameterFloat {
    explicit TestParameter(int index)
        : AudioParameterFloat(juce::ParameterID{"p" + juce::String(index), 1},
                              "Test", 0.0f, 1.0f, 0.0f) {}
    void valueChanged(float) override { ++changes; }
    int changes = 0;
};

struct Mock final:juce::AudioPluginInstance {
    int calls=0,lastSize=0,notes=0,noteOffset=-1,prepares=0,releases=0;double rate=0;int maximum=0;
    Mock(){
        setPlayConfigDetails(0,2,48000,512);
        for (int i = 0; i < Engine::laneCount; ++i)
            addHostedParameter(std::make_unique<TestParameter>(i));
    }
    const juce::String getName()const override{return "Test instrument";}
    void fillInPluginDescription(juce::PluginDescription&)const override{}
    void prepareToPlay(double sr,int size)override{++prepares;rate=sr;maximum=size;}
    void releaseResources()override{++releases;}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override{++calls;lastSize=audio.getNumSamples();for(const auto event:midi)if(event.getMessage().isNoteOn()){++notes;noteOffset=event.samplePosition;}for(int c=0;c<audio.getNumChannels();++c)for(int k=0;k<audio.getNumSamples();++k)audio.setSample(c,k,.25f);}
    bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}double getTailLengthSeconds()const override{return 0;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
};
int main(){
    Engine engine;
    for(int size:{64,128,256,512,1024}){
        auto plugin=std::make_unique<Mock>();auto* observed=plugin.get();engine.replace(std::move(plugin));engine.prepareToPlay(48000,size);
        assert(observed->prepares==1&&observed->rate==48000&&observed->maximum==size);
        juce::AudioBuffer<float> audio(2,size);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),size-1);
        engine.processBlock(audio,midi);
        assert(observed->calls==1&&observed->lastSize==size&&observed->notes==1&&observed->noteOffset==size-1);
        assert(engine.calls.load()==1&&engine.samples.load()==(uint64_t)size);
        for(int k=0;k<size;++k)assert(audio.getSample(0,k)==.25f&&audio.getSample(1,k)==.25f);
        engine.releaseResources();assert(observed->releases==1);engine.replace(nullptr);
        engine.processBlock(audio,midi);assert(audio.getMagnitude(0,size)==0&&midi.isEmpty());
    }
    auto instrument = std::make_unique<Mock>();
    auto* observed = instrument.get();
    engine.replace(std::move(instrument));
    engine.prepareToPlay(48000, 512);
    for (int i = 0; i < Engine::laneCount; ++i)
    {
        Engine::Lane lane;
        lane.target = i;
        lane.enabled = true;
        lane.points.fill(.75f);
        engine.setLane(i, lane);
    }

    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    engine.processBlock(audio, midi);
    for (auto* parameter : observed->getParameters())
        assert(static_cast<TestParameter*>(parameter)->changes == 0);

    engine.playing.store(true);
    engine.processBlock(audio, midi);
    engine.processBlock(audio, midi);
    assert(observed->calls == 3 && observed->lastSize == 512);
    for (auto* parameter : observed->getParameters())
    {
        assert(parameter->getValue() == .75f);
        assert(static_cast<TestParameter*>(parameter)->changes == 1);
    }

    engine.playing.store(false);
    assert(engine.recorder.arm(48000, 1, 240));
    for (int i = 0; i < 93; ++i)
        engine.processBlock(audio, midi);
    assert(engine.recorder.getState() == LoopRecorder::State::recording);
    engine.processBlock(audio, midi);
    assert(engine.recorder.getState() == LoopRecorder::State::complete);
    assert(observed->calls == 97 && observed->lastSize == 512);

    juce::TemporaryFile output(".wav");
    assert(engine.recorder.save(output.getFile()));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(
        wav.createReaderFor(output.getFile().createInputStream().release(), true));
    assert(reader && reader->lengthInSamples == 48000 && reader->sampleRate == 48000
           && reader->numChannels == 2 && reader->bitsPerSample == 24);
    juce::AudioBuffer<float> tail(2, 1);
    assert(reader->read(&tail, 0, 1, 47999, true, true));
    assert(std::abs(tail.getSample(0, 0) - .25f) < .00001f);
    assert(std::abs(tail.getSample(1, 0) - .25f) < .00001f);

    engine.recorder.discard();
    assert(engine.recorder.arm(48000, 1, 240));
    engine.releaseResources();
    assert(engine.recorder.getState() == LoopRecorder::State::interrupted);
    std::cout<<"PASS: one instrument call per complete buffer (64..1024), unchanged MIDI offset, stereo audio, prepare/release, silence when unloaded\n";
}
