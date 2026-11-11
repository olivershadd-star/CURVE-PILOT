#include "Engine.h"
using namespace juce;

// The plugin gets its own native window: the OS clips native VST controls.
// There is no transform, hidden editor, or nested scaling system in this test.
class InstrumentWindow final:public DocumentWindow {
public:
    explicit InstrumentWindow(AudioPluginInstance& plugin):DocumentWindow(plugin.getName(),Colour(0xff151c29),DocumentWindow::closeButton|DocumentWindow::minimiseButton){
        setUsingNativeTitleBar(true);
        auto* editor=plugin.createEditorIfNeeded();if(!editor)editor=new GenericAudioProcessorEditor(plugin);
        setContentOwned(editor,true);setResizable(editor->isResizable(),false);
        if(auto* display=Desktop::getInstance().getDisplays().getPrimaryDisplay()){
            auto available=display->userArea.reduced(20);
            setTopLeftPosition(available.getX()+jmax(0,(available.getWidth()-getWidth())/2),available.getY());
        }
        setVisible(true);
    }
    void closeButtonPressed()override{setVisible(false);}
};

class Main final:public Component,private Timer {
public:
    Main():keyboard(keys,MidiKeyboardComponent::horizontalKeyboard){
        formats.addDefaultFormats();
        for(auto* c:std::initializer_list<Component*>{&title,&status,&metrics,&load,&show,&settings,&panic,&keyboard})addAndMakeVisible(c);
        title.setText("CURVE PILOT  /  AUTONOME 0.1",dontSendNotification);title.setColour(Label::textColourId,Colour(0xff55d7bd));
        load.onClick=[this]{chooseInstrument();};show.onClick=[this]{if(window){window->setVisible(true);window->toFront(true);}};
        settings.onClick=[this]{openSettings();};panic.onClick=[this]{for(int channel=1;channel<=16;++channel){auto message=MidiMessage::allSoundOff(channel);message.setTimeStamp(Time::getMillisecondCounterHiRes()*.001);player.getMidiMessageCollector().addMessageToQueue(message);}};
        player.setProcessor(&engine);
        const auto error=devices.initialise(0,2,nullptr,true);
        devices.addAudioCallback(&player);attached=true;
        if(error.isEmpty()){status.setText("Choisir un instrument VST3. Audio / MIDI pour activer le clavier USB.",dontSendNotification);}
        else status.setText("Audio a configurer : "+error,dontSendNotification);
        devices.addMidiInputDeviceCallback({},&player);keys.addListener(&player.getMidiMessageCollector());
        setSize(900,285);startTimerHz(2);
    }
    ~Main()override{stopTimer();chooser.reset();if(deviceDialog)delete deviceDialog.getComponent();deviceDialog=nullptr;keys.removeListener(&player.getMidiMessageCollector());devices.removeMidiInputDeviceCallback({},&player);stopAudio();player.setProcessor(nullptr);window.reset();}
    void paint(Graphics& g)override{g.fillAll(Colour(0xff101521));}
    void resized()override{auto r=getLocalBounds().reduced(16);title.setBounds(r.removeFromTop(32));auto bar=r.removeFromTop(40);load.setBounds(bar.removeFromLeft(230).reduced(3));show.setBounds(bar.removeFromLeft(170).reduced(3));settings.setBounds(bar.removeFromLeft(160).reduced(3));panic.setBounds(bar.removeFromLeft(120).reduced(3));status.setBounds(r.removeFromTop(46));metrics.setBounds(r.removeFromTop(42));keyboard.setBounds(r.reduced(0,5));}
private:
    AudioDeviceManager devices;AudioProcessorPlayer player;Engine engine;
    MidiKeyboardState keys;MidiKeyboardComponent keyboard;
    AudioPluginFormatManager formats;std::unique_ptr<InstrumentWindow> window;
    std::unique_ptr<FileChooser> chooser;Component::SafePointer<DialogWindow> deviceDialog;
    Label title,status,metrics;TextButton load{"Charger un instrument VST3"},show{"Afficher l'instrument"},settings{"Audio / MIDI"},panic{"Couper les notes"};
    bool attached=false,loading=false;
    void stopAudio(){if(attached){devices.removeAudioCallback(&player);attached=false;}}
    void startAudio(){if(!attached){devices.addAudioCallback(&player);attached=true;}}
    void openSettings(){
        if(deviceDialog){deviceDialog->setVisible(true);deviceDialog->toFront(true);return;}
        DialogWindow::LaunchOptions o;o.dialogTitle="Sortie audio et entrees MIDI";o.dialogBackgroundColour=Colour(0xff1b2435);o.useNativeTitleBar=true;o.resizable=true;
        auto* panel=new AudioDeviceSelectorComponent(devices,0,0,2,2,true,false,true,false);panel->setSize(620,520);o.content.setOwned(panel);o.componentToCentreAround=this;deviceDialog=o.launchAsync();
    }
    void chooseInstrument(){
        if(loading)return;
        auto folder=File::getSpecialLocation(File::globalApplicationsDirectory).getChildFile("Common Files/VST3");
        chooser=std::make_unique<FileChooser>("Choisir le VST3 complet",folder,"*.vst3",true);
        Component::SafePointer<Main> safe(this);
        chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::canSelectDirectories,[safe](const FileChooser& c){if(safe){auto file=c.getResult();if(file!=File{})safe->loadFile(file);}});
    }
    void loadFile(const File& file){
        if(!file.hasFileExtension("vst3")){status.setText("Selectionner un fichier ou dossier .vst3 complet.",dontSendNotification);return;}
        OwnedArray<PluginDescription> found;
        for(int i=0;i<formats.getNumFormats();++i)formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
        if(found.isEmpty()){status.setText("Aucun VST3 reconnu.",dontSendNotification);return;}
        const auto desc=*found[0];if(!desc.isInstrument){status.setText("Ce premier prototype attend un instrument, pas un effet.",dontSendNotification);return;}
        loading=true;load.setEnabled(false);status.setText("Chargement de "+desc.name+"...",dontSendNotification);
        // Stop processing while constructing/replacing a plugin, not on normal UI refresh.
        stopAudio();double rate=44100;int block=512;
        if(auto* device=devices.getCurrentAudioDevice()){rate=device->getCurrentSampleRate();block=device->getCurrentBufferSizeSamples();}
        Component::SafePointer<Main> safe(this);
        formats.createPluginInstanceAsync(desc,rate,block,[safe](std::unique_ptr<AudioPluginInstance> instance,const String& error){
            if(!safe)return;
            safe->loading=false;safe->load.setEnabled(true);
            if(!instance){safe->status.setText("Chargement impossible : "+error,dontSendNotification);safe->startAudio();return;}
            safe->window.reset();
            try{safe->engine.replace(std::move(instance));safe->startAudio();safe->window=std::make_unique<InstrumentWindow>(*safe->engine.get());safe->status.setText("Instrument : "+safe->engine.get()->getName()+". Tampons complets, aucun effet ni automation.",dontSendNotification);}
            catch(const std::exception& e){safe->stopAudio();safe->engine.replace(nullptr);safe->startAudio();safe->status.setText("Configuration impossible : "+String::fromUTF8(e.what()),dontSendNotification);}
        });
    }
    void timerCallback()override{
        String text="Charge audio : "+String(devices.getCpuUsage()*100,1)+" %   |   Decrochages : "+String(devices.getXRunCount());
        if(auto* d=devices.getCurrentAudioDevice())text+="\n"+d->getName()+"  /  "+String(d->getCurrentSampleRate(),0)+" Hz  /  "+String(d->getCurrentBufferSizeSamples())+" echantillons";
        else text+="\nAucune sortie audio active";
        metrics.setText(text,dontSendNotification);
    }
};

class App final:public JUCEApplication {
    class Window final:public DocumentWindow {
    public:
        Window():DocumentWindow("Curve Pilot autonome - Prototype 0.1",Colour(0xff101521),DocumentWindow::allButtons){setUsingNativeTitleBar(true);setContentOwned(new Main(),true);setResizable(true,false);setResizeLimits(740,285,2000,700);centreWithSize(getWidth(),getHeight());setVisible(true);}
        void closeButtonPressed()override{JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
public:
    const String getApplicationName()override{return "Curve Pilot autonome";}const String getApplicationVersion()override{return "0.1.0";}
    bool moreThanOneInstanceAllowed()override{return false;}
    void initialise(const String&)override{window=std::make_unique<Window>();}
    void shutdown()override{window.reset();}
};
START_JUCE_APPLICATION(App)
