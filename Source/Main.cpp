#include "CurveEditor.h"
#include "DeviceArea.h"
using namespace juce;

class Main final:public Component,private Timer {
public:
    Main():keyboard(keys,MidiKeyboardComponent::horizontalKeyboard){
        formats.addDefaultFormats();
        for(auto* c:std::initializer_list<Component*>{&title,&status,&metrics,&load,&show,&settings,&panic,&keyboard,&editor,&play,&reset,&record,&discard,&bpm,&bars,&bpmLabel,&barsLabel})addAndMakeVisible(c);
        title.setText("CURVE PILOT  /  AUTONOME 0.2",dontSendNotification);title.setColour(Label::textColourId,Colour(0xff55d7bd));
        addChildComponent(instrumentArea);
        load.onClick = [this] { chooseInstrument(); };
        show.onClick = [this]
        {
            editor.setVisible(!editor.isVisible());
            instrumentArea.setVisible(!editor.isVisible());
            show.setButtonText(editor.isVisible() ? "Instrument" : "Courbes");
        };
        bpmLabel.setText("Tempo", dontSendNotification);
        barsLabel.setText("Mesures WAV", dontSendNotification);
        bpm.setRange(40, 240, 1);
        bpm.setValue(120);
        bars.setRange(1, 32, 1);
        bars.setValue(4);
        for (auto* slider : {&bpm, &bars})
            slider->setSliderStyle(Slider::IncDecButtons);
        bpm.onValueChange = [this] { engine.tempo.store(bpm.getValue()); };
        play.onClick = [this]
        {
            const bool running = !engine.playing.load();
            engine.playing.store(running);
            play.setButtonText(running ? "Pause" : "Lecture");
        };
        reset.onClick = [this] { engine.restart.store(true); };
        record.onClick = [this] { recordLoop(); };
        discard.onClick = [this] { engine.recorder.discard(); refreshRecording(); };
        refreshRecording();
        settings.onClick=[this]{openSettings();};panic.onClick=[this]{for(int channel=1;channel<=16;++channel){auto message=MidiMessage::allSoundOff(channel);message.setTimeStamp(Time::getMillisecondCounterHiRes()*.001);player.getMidiMessageCollector().addMessageToQueue(message);}};
        player.setProcessor(&engine);
        const auto error=devices.initialise(0,2,nullptr,true);
        devices.addAudioCallback(&player);attached=true;
        if(error.isEmpty()){status.setText("Choisir un instrument VST3. Audio / MIDI pour activer le clavier USB.",dontSendNotification);}
        else status.setText("Audio a configurer : "+error,dontSendNotification);
        devices.addMidiInputDeviceCallback({},&player);keys.addListener(&player.getMidiMessageCollector());
        setSize(980,620);startTimerHz(2);
    }
    ~Main()override{stopTimer();chooser.reset();if(deviceDialog)delete deviceDialog.getComponent();deviceDialog=nullptr;keys.removeListener(&player.getMidiMessageCollector());devices.removeMidiInputDeviceCallback({},&player);stopAudio();player.setProcessor(nullptr);instrumentArea.release();}
    void paint(Graphics& g)override{g.fillAll(Colour(0xff101521));}
    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        title.setBounds(area.removeFromTop(28));
        auto deviceBar = area.removeFromTop(36);
        load.setBounds(deviceBar.removeFromLeft(220).reduced(3));
        show.setBounds(deviceBar.removeFromLeft(112).reduced(3));
        settings.setBounds(deviceBar.removeFromLeft(114).reduced(3));
        panic.setBounds(deviceBar.removeFromLeft(120).reduced(3));
        status.setBounds(area.removeFromTop(34));
        metrics.setBounds(area.removeFromTop(24));
        auto transport = area.removeFromTop(36);
        play.setBounds(transport.removeFromLeft(78).reduced(3));
        reset.setBounds(transport.removeFromLeft(74).reduced(3));
        bpmLabel.setBounds(transport.removeFromLeft(48));
        bpm.setBounds(transport.removeFromLeft(92).reduced(3));
        barsLabel.setBounds(transport.removeFromLeft(94));
        bars.setBounds(transport.removeFromLeft(82).reduced(3));
        record.setBounds(transport.removeFromLeft(135).reduced(3));
        discard.setBounds(transport.removeFromLeft(85).reduced(3));
        keyboard.setBounds(area.removeFromBottom(65).reduced(3));
        editor.setBounds(area.reduced(3));
        instrumentArea.setBounds(area.reduced(3));
    }

private:
    AudioDeviceManager devices;AudioProcessorPlayer player;Engine engine;
    MidiKeyboardState keys;MidiKeyboardComponent keyboard;
    AudioPluginFormatManager formats;
    CurveEditor editor{engine};
    DeviceArea instrumentArea;
    std::unique_ptr<FileChooser> chooser, wavChooser;
    Component::SafePointer<DialogWindow> deviceDialog;
    Label title, status, metrics, bpmLabel, barsLabel;
    TextButton load{"Charger un instrument VST3"}, show{"Instrument"}, settings{"Audio / MIDI"},
        panic{"Couper les notes"}, play{"Lecture"}, reset{"Début"}, record{"Capturer la boucle"}, discard{"Effacer"};
    Slider bpm, bars;

    void refreshRecording()
    {
        using State = LoopRecorder::State;
        const auto state = engine.recorder.getState();
        const bool busy = state == State::armed || state == State::recording;
        discard.setEnabled(!saving && (state == State::complete || state == State::interrupted));
        record.setEnabled(!saving && (state == State::complete
            || (state == State::idle && engine.get() && devices.getCurrentAudioDevice() && !loading)));
        record.setButtonText(state == State::complete ? "Enregistrer WAV" :
                             state == State::interrupted ? "Capture interrompue" :
                             busy ? "Capture en cours" : "Capturer la boucle");
        reset.setEnabled(!busy);
        play.setEnabled(!busy);
        load.setEnabled(!busy && !loading);
        settings.setEnabled(!busy);
        bars.setEnabled(!busy);
        bpm.setEnabled(!busy);
    }

    void recordLoop()
    {
        using State = LoopRecorder::State;
        if (engine.recorder.getState() == State::complete)
        {
            wavChooser = std::make_unique<FileChooser>("Enregistrer la boucle", File::getSpecialLocation(File::userMusicDirectory)
                                                       .getChildFile("Curve Pilot.wav"), "*.wav");
            saving = true;
            refreshRecording();
            Component::SafePointer<Main> safe(this);
            wavChooser->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                                  | FileBrowserComponent::warnAboutOverwriting, [safe](const FileChooser& file)
            {
                if (!safe)
                    return;
                safe->saving = false;
                const auto destination = file.getResult();
                if (destination != File{})
                {
                    const auto ok = safe->engine.recorder.save(destination);
                    safe->status.setText(ok ? "Boucle WAV enregistrée." : "Impossible d'écrire le WAV.",
                                         dontSendNotification);
                }
                safe->refreshRecording();
            });
            return;
        }

        if (auto* device = devices.getCurrentAudioDevice(); engine.get() && device)
        {
            if (engine.recorder.arm(device->getCurrentSampleRate(), static_cast<int>(bars.getValue()), bpm.getValue()))
            {
                engine.playing.store(false);
                play.setButtonText("Lecture");
                status.setText("Capture depuis le début. Joue les notes ou utilise l'arpégiateur du VST3.",
                               dontSendNotification);
            }
            else
                status.setText("Capture indisponible à cette durée et fréquence audio.", dontSendNotification);
        }
        refreshRecording();
    }

    bool attached=false,loading=false,saving=false;
    void stopAudio(){if(attached){devices.removeAudioCallback(&player);attached=false;}}
    void startAudio(){if(!attached){devices.addAudioCallback(&player);attached=true;}}
    void openSettings(){
        if(deviceDialog){deviceDialog->setVisible(true);deviceDialog->toFront(true);return;}
        DialogWindow::LaunchOptions o;o.dialogTitle="Sortie audio et entrees MIDI";o.dialogBackgroundColour=Colour(0xff1b2435);o.useNativeTitleBar=true;o.resizable=true;
        auto* panel=new AudioDeviceSelectorComponent(devices,0,0,2,2,true,false,true,false);panel->setSize(620,520);o.content.setOwned(panel);o.componentToCentreAround=this;deviceDialog=o.launchAsync();
    }
    void chooseInstrument(){
        if(loading || !load.isEnabled())return;
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
        refreshRecording();
        stopAudio();double rate=44100;int block=512;
        if(auto* device=devices.getCurrentAudioDevice()){rate=device->getCurrentSampleRate();block=device->getCurrentBufferSizeSamples();}
        Component::SafePointer<Main> safe(this);
        formats.createPluginInstanceAsync(desc,rate,block,[safe](std::unique_ptr<AudioPluginInstance> instance,const String& error){
            if(!safe)return;
            safe->loading=false;safe->load.setEnabled(true);
            if (!instance)
            {
                safe->status.setText("Chargement impossible : " + error, dontSendNotification);
            }
            else
            {
                safe->instrumentArea.release();
                safe->editor.detach();
                try
                {
                    safe->engine.replace(std::move(instance));
                    safe->editor.instrumentChanged();
                    safe->instrumentArea.showInstrument(*safe->engine.get());
                    safe->status.setText("Instrument : " + safe->engine.get()->getName(), dontSendNotification);
                }
                catch (const std::exception& e)
                {
                    safe->instrumentArea.release();
                    safe->editor.detach();
                    safe->engine.replace(nullptr);
                    safe->editor.instrumentChanged();
                    safe->status.setText("Configuration impossible : " + String::fromUTF8(e.what()), dontSendNotification);
                }
            }
            safe->startAudio();
            safe->refreshRecording();
        });
    }
    void timerCallback()override{
        refreshRecording();
        String text="Charge audio : "+String(devices.getCpuUsage()*100,1)+" %   |   Decrochages : "+String(devices.getXRunCount());
        if(auto* d=devices.getCurrentAudioDevice())text+="  |  "+d->getName()+"  /  "+String(d->getCurrentSampleRate(),0)+" Hz  /  "+String(d->getCurrentBufferSizeSamples())+" echantillons";
        else text+="  |  Aucune sortie audio active";
        metrics.setText(text,dontSendNotification);
    }
};

class App final:public JUCEApplication {
    class Window final:public DocumentWindow {
    public:
        Window():DocumentWindow("Curve Pilot autonome - Prototype 0.2",Colour(0xff101521),DocumentWindow::allButtons){setUsingNativeTitleBar(true);setContentOwned(new Main(),true);setResizable(true,false);setResizeLimits(760,420,3200,2000);centreWithSize(getWidth(),getHeight());setVisible(true);}
        void closeButtonPressed()override{JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
public:
    const String getApplicationName()override{return "Curve Pilot autonome";}const String getApplicationVersion()override{return "0.2.0";}
    bool moreThanOneInstanceAllowed()override{return false;}
    void initialise(const String&)override{window=std::make_unique<Window>();}
    void shutdown()override{window.reset();}
};
START_JUCE_APPLICATION(App)
