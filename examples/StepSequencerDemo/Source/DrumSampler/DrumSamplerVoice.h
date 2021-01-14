//
// Created by home on 12/27/20.
//

#ifndef MUSICAPP_DRUMSAMPLERVOICE_H
#define MUSICAPP_DRUMSAMPLERVOICE_H


#include <juce_audio_utils/juce_audio_utils.h>

class DrumSamplerVoice: public juce::SamplerVoice {
public:
    DrumSamplerVoice(int index);
    ~DrumSamplerVoice();

    void setMidiNote(int note);

    bool canPlayDrumSound(int midiNoteNumber) const;

    bool canPlaySound(juce::SynthesiserSound *sound) override;

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sound, int pitchWheel) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newValue) override;

    void controllerMoved(int controllerNumber, int newValue) override;

    void renderNextBlock(juce::AudioBuffer<float> &buffer, int startSample, int numSamples) override;

private:
    int index;
    juce::BigInteger midiNote;


    double sourceSamplePosition = 0;
};


#endif //MUSICAPP_DRUMSAMPLERVOICE_H
