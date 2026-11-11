#pragma once
#include <JuceHeader.h>
#include <stdexcept>

// One instrument, one call per device buffer. No curves, FX or sub-blocks.
class Engine final:public juce::AudioProcessor {
public:
    Engine():AudioProcessor(BusesProperties().withOutput("Stereo",juce::AudioChannelSet::stereo(),true)){}
    // Only replace the instrument after removing the device audio callback.
    void replace(std::unique_ptr<juce::AudioPluginInstance> next){
        instrument=std::move(next);calls=0;samples=0;
        if(!instrument)return;
        instrument->disableNonMainBuses();auto layout=instrument->getBusesLayout();
        for(auto& bus:layout.inputBuses)bus=juce::AudioChannelSet::disabled();
        if(layout.outputBuses.isEmpty())throw std::runtime_error("Le VST ne possede pas de sortie audio.");
        layout.outputBuses.set(0,juce::AudioChannelSet::stereo());
        if(!instrument->setBusesLayout(layout))throw std::runtime_error("Ce prototype exige une sortie stereo sans entree audio.");
    }
    juce::AudioPluginInstance* get()const{return instrument.get();}
    void prepareToPlay(double rate,int size)override{if(instrument){instrument->setRateAndBufferSizeDetails(rate,size);instrument->prepareToPlay(rate,size);}}
    void releaseResources()override{if(instrument)instrument->releaseResources();}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{
        juce::ScopedNoDenormals noDenormals;
        if(!instrument){buffer.clear();midi.clear();return;}
        buffer.clear();instrument->processBlock(buffer,midi);
        calls.fetch_add(1,std::memory_order_relaxed);samples.fetch_add((uint64_t)buffer.getNumSamples(),std::memory_order_relaxed);
    }
    std::atomic<uint64_t> calls{0},samples{0};
    const juce::String getName()const override{return "Curve Pilot autonome prototype 0.1";}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
    double getTailLengthSeconds()const override{return instrument?instrument->getTailLengthSeconds():0;}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
private:
    std::unique_ptr<juce::AudioPluginInstance> instrument;
};
