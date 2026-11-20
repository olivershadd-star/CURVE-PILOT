#pragma once
#include <JuceHeader.h>
#if JUCE_WINDOWS
#include <juce_core/native/juce_BasicNativeHeaders.h>
#endif
// A separate child peer clips native VST3 HWNDs at the viewport boundary on Windows.
// A plain JUCE Viewport only clips JUCE painting; it cannot clip foreign HWNDs.
class DeviceArea final : public juce::Component, private juce::ComponentListener, private juce::Timer {
    struct Root final : juce::Component {
        juce::Viewport viewport;
        Root(){setOpaque(true);addAndMakeVisible(viewport);viewport.setScrollBarsShown(true,true);viewport.setScrollBarThickness(16);}
        void resized()override{viewport.setBounds(getLocalBounds());}
        void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff101723));}
    } root;
    juce::Component content;
    std::unique_ptr<juce::AudioProcessorEditor> ownedEditor;
    juce::Component::SafePointer<juce::AudioProcessorEditor> current;
    void* parentHandle=nullptr;bool refreshing=false;juce::Rectangle<int> nativeBounds;
public:
    DeviceArea(){
        root.viewport.setViewedComponent(&content,false);
        #if ! JUCE_WINDOWS
        addAndMakeVisible(root);
        #endif
        startTimerHz(2);
    }
    ~DeviceArea()override{stopTimer();release();root.viewport.setViewedComponent(nullptr,false);root.removeFromDesktop();}
    void release(){
        if(current){current->setVisible(false);current->removeComponentListener(this);content.removeChildComponent(current.getComponent());}
        current=nullptr;ownedEditor.reset();content.setSize(1,1);root.setVisible(false);repaint();
    }
    void showInstrument(juce::AudioPluginInstance& instrument)
    {
        release();
        ownedEditor.reset(instrument.createEditorIfNeeded());
        if (!ownedEditor)
            ownedEditor = std::make_unique<juce::GenericAudioProcessorEditor>(instrument);
        current = ownedEditor.get();
        content.addAndMakeVisible(*ownedEditor);
        current->addComponentListener(this);
        refreshGeometry();
        root.viewport.setViewPosition(0, 0);
        repaint();
    }
    void refreshGeometry(){
        if(refreshing)return;
        const juce::ScopedValueSetter<bool> guard(refreshing,true);
        if(current){
            // The wrapper, unlike an editor's transform, has real scaled bounds for scrollbars.
            current->setTopLeftPosition(0,0);
            auto bounds=current->getBoundsInParent();
            content.setSize(juce::jmax(1,bounds.getRight()),juce::jmax(1,bounds.getBottom()));
        }
        root.viewport.resized();syncPeer();
    }
    void resetScroll(){root.viewport.setViewPosition(0,0);}
    void resized()override{syncPeer();refreshGeometry();}
    void visibilityChanged()override{syncPeer();}
    void parentHierarchyChanged()override{syncPeer();}
    void componentMovedOrResized(juce::Component&,bool,bool)override{refreshGeometry();}
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff101723));if(!current){g.setColour(juce::Colour(0xff91a2be));g.drawFittedText("Charger un instrument VST3",getLocalBounds().reduced(20),juce::Justification::centred,3);}
    }
private:
    void syncPeer(){
        #if JUCE_WINDOWS
        auto* peer=getPeer();void* handle=peer?peer->getNativeHandle():nullptr;
        if(handle!=parentHandle){root.setVisible(false);root.removeFromDesktop();parentHandle=handle;nativeBounds={};if(handle)root.addToDesktop(0,handle);}
        if(peer&&root.getPeer()){
            auto area=(peer->getAreaCoveredBy(*this).toFloat()*peer->getPlatformScaleFactor()).getSmallestIntegerContainer();
            root.setSize(getWidth(),getHeight());
            if(area!=nativeBounds){nativeBounds=area;SetWindowPos(static_cast<HWND>(root.getPeer()->getNativeHandle()),nullptr,area.getX(),area.getY(),area.getWidth(),area.getHeight(),SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);}
        }
        #else
        root.setBounds(getLocalBounds());
        #endif
        root.setVisible(isShowing()&&current!=nullptr&&getWidth()>0&&getHeight()>0);
    }
    void timerCallback()override{if(isShowing() && current) syncPeer();}
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceArea)
};
