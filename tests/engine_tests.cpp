#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../Source/Engine.h"
#include <cassert>
#include <iostream>
struct Mock final:juce::AudioPluginInstance {
    int calls=0,lastSize=0,notes=0,noteOffset=-1,prepares=0,releases=0;double rate=0;int maximum=0;
    Mock(){setPlayConfigDetails(0,2,48000,512);}
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
    std::cout<<"PASS: one instrument call per complete buffer (64..1024), unchanged MIDI offset, stereo audio, prepare/release, silence when unloaded\n";
}
