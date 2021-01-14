//
// Created by home on 12/27/20.
//

#ifndef MUSICAPP_DRUMSAMPLERPLUGIN_H
#define MUSICAPP_DRUMSAMPLERPLUGIN_H

#include "JuceHeader.h"
#include "DrumSampler.h"

namespace te = tracktion_engine;

class DrumSamplerPlugin : public te::Plugin,
                          private juce::AsyncUpdater {
public:
    DrumSamplerPlugin(te::PluginCreationInfo);
    ~DrumSamplerPlugin() override;

    int getNumSounds() const;
    juce::String getSoundName (int index) const;
    void setSoundName (int index, const juce::String& name);
    //te::AudioFile getSoundFile (int index) const;
    juce::String getSoundMedia (int index) const;
    int getKeyNote (int index) const;
    int getMinKey (int index) const;
    int getMaxKey (int index) const;
    float getSoundGainDb (int index) const;
    float getSoundPan (int index) const;
    bool isSoundOpenEnded (int index) const;
    double getSoundStartTime (int index) const;
    double getSoundLength (int index) const;
    void setSoundExcerpt (int index, double start, double length);

    // returns an error
    juce::String addSound (const juce::String& sourcePathOrProjectID, const juce::String& name,
                           double startTime, double length, float gainDb);


    static const char* getPluginName()                  { return NEEDS_TRANS("DrumSampler"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS("SamplerADSR"); };
    juce::String getPluginType() override               { return xmlTypeName; };
    juce::String getShortName (int) override            { return "DrumSampler"; }
    juce::String getSelectableDescription() override    { return TRANS("DrumSampler"); }

    int getNumOutputChannelsGivenInputs(int numInputChannels) override { return juce::jmin(numInputChannels, 2); }
    bool producesAudioWhenNoAudioInput() override { return true; }

    bool takesAudioInput() override { return false; }
    bool takesMidiInput() override { return true; }
    bool isSynth() override { return true; }
    bool needsConstantBufferSize() override { return false; }

    void initialise(const te::PlaybackInitialisationInfo &info) override;
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext &context) override;

private:

    DrumSampler sampler;

    juce::MidiBuffer midiBuffer;
    te::ActiveNoteList activeNotes;

    void handleAsyncUpdate() override;
    void prepareIncomingMidiMessages(te::MidiMessageArray incoming, int numSamples, bool isPlaying);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSamplerPlugin)
};


#endif //MUSICAPP_DRUMSAMPLERPLUGIN_H
