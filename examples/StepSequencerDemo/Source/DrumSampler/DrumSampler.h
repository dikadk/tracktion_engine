/*
  ==============================================================================

    DrumSampler.h
    Created: 27 Dec 2020 6:31:28pm
    Author:  home

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

class DrumSampler : public juce::Synthesiser {
public:
    DrumSampler();

    ~DrumSampler();

    enum Instrument {
        KICK = 0, SNARE, CLAP, HIHAT, TOM, PERC, CLHAT, CRASH, RIDE, OTHER, NUM_INSTRUMENTS
    };

    void setup();

    void setCurrentPlaybackSampleRate(double sampleRate) override;

    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override;

    void addDrumSound(juce::String name, juce::String filename, Instrument instrument);

private:

    juce::AudioFormatManager formatManager;
    juce::StringArray instruments;

};


class DrumSamplerSound : public juce::SamplerSound {
public:
    DrumSamplerSound(const juce::String &name, juce::AudioFormatReader &source, const juce::BigInteger &midiNotes,
                     int midiNoteForNormalPitch, double attackTimeSecs, double releaseTimeSecs,
                     double maxSampleLengthSeconds);

    ~DrumSamplerSound() override;

    friend class DrumSamplerVoice;

private:
    double sourceSampleRate;
    long long length;
    int midiRootNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSamplerSound)
};
