/*
  ==============================================================================

    DrumSampler.cpp
    Created: 27 Dec 2020 6:31:28pm
    Author:  home

  ==============================================================================
*/

#include "DrumSampler.h"
#include "DrumSamplerVoice.h"

//==============================================================================
DrumSamplerSound::DrumSamplerSound(const juce::String &name, juce::AudioFormatReader &source,
                                                const juce::BigInteger &midiNotes, int midiNoteForNormalPitch,
                                                double attackTimeSecs, double releaseTimeSecs,
                                                double maxSampleLengthSeconds)
        : SamplerSound(name, source, midiNotes, midiNoteForNormalPitch, attackTimeSecs, releaseTimeSecs,
                       maxSampleLengthSeconds),
          sourceSampleRate(source.sampleRate), length(source.lengthInSamples), midiRootNote(midiNoteForNormalPitch) {
}

DrumSamplerSound::~DrumSamplerSound() {}

DrumSampler::DrumSampler() {
    formatManager.registerBasicFormats();

    setup();
}

DrumSampler::~DrumSampler() {

}

void DrumSampler::setup() {
    int numVoices = 9; //Todo make configurable
    for (int i = 0; i < numVoices; i++) {
        auto *voice = new DrumSamplerVoice(i);
        switch (i) {
            case KICK:
                voice->setMidiNote(36);
                break;
            case SNARE:
                voice->setMidiNote(38);
                break;
            case CLAP:
                voice->setMidiNote(39);
                break;
            case TOM:
                voice->setMidiNote(41);
                break;
            case CLHAT:
                voice->setMidiNote(42);
                break;
            case PERC:
                voice->setMidiNote(43);
                break;
            case HIHAT:
                voice->setMidiNote(46);
                break;
            case CRASH:
                voice->setMidiNote(49);
                break;
            case RIDE:
                voice->setMidiNote(51);
                break;
            default:
                break;
        }

        addVoice(voice);
    }
    juce::File packDirectory("/Users/home/JuceProjects/MusicApp/Source/Resources/Media/Samples/Sequencer/TR-808");
    int i = 0;
    for (auto &f: packDirectory.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false)) {
        if (i >= 9)
            return;
        addDrumSound(f.getFileName(), f.getFullPathName(), static_cast<Instrument>(i));
        i++;
    }

}

void DrumSampler::setCurrentPlaybackSampleRate(double sampleRate) {
    juce::Synthesiser::setCurrentPlaybackSampleRate(sampleRate);
}

void DrumSampler::noteOn(int midiChannel, int midiNoteNumber, float velocity) {
    const juce::ScopedLock lc(lock);

    for (int i = 0; i < getNumSounds(); i++) {
        auto *sound = getSound(i).get();
        if (sound->appliesToNote(midiNoteNumber) && sound->appliesToChannel(midiChannel)) {

            for (int j = 0; j < getNumVoices(); j++) {
                if (auto *voice = dynamic_cast<DrumSamplerVoice *>(getVoice(j))) {
                    if (voice->canPlayDrumSound(midiNoteNumber) && voice->getCurrentlyPlayingSound()) {
                        stopVoice(voice, 0.0f, false);
                    }
                }
            }

            //TODO Handle HI Hat and CL Hat chocking
            for (int j = 0; j < getNumVoices(); j++) {
                if (auto *voice = dynamic_cast<DrumSamplerVoice *>(getVoice(j))) {
                    if (voice->canPlayDrumSound(midiNoteNumber) && !voice->getCurrentlyPlayingSound()) {
                        /*if(isChokeGroupActive && (midiNoteNumber == 42 || midiNoteNumber == 46))
                        {
                            switch(midiNoteNumber)
                            {
                                case 42:
                                    for(int i = 0; i < getNumVoices(); i++)
                                    {
                                        if(auto* voice = dynamic_cast<DrumSamplerVoice*>(getVoice(i)))
                                        {
                                            if(voice->canPlayDrumSound(46))
                                            {
                                                stopVoice(voice, 0.0f, false);
                                                break;
                                            }
                                        }
                                    }
                                case 46:
                                    for (int i = 0; i < getNumVoices(); i++)
                                    {
                                        if (auto* voice = dynamic_cast<DrumSamplerVoice*>(getVoice(i)))
                                        {
                                            if (voice->canPlayDrumSound(42))
                                            {
                                                stopVoice(voice, 0.0f, false);
                                                break;
                                            }
                                        }
                                    }
                                default:
                                    break;
                            }*/
                    }
                }
            }
        }
    }
}

void DrumSampler::addDrumSound(juce::String name, juce::String filename, Instrument instrument) {
    int midiNote;
    if (instrument == Instrument::KICK) {
        midiNote = 36;
    } else if (instrument == Instrument::SNARE) {
        midiNote = 38;
    } else if (instrument == Instrument::CLAP) {
        midiNote = 39;
    } else if (instrument == Instrument::TOM) {
        midiNote = 41;
    } else if (instrument == Instrument::CLHAT) {
        midiNote = 42;
    } else if (instrument == Instrument::PERC) {
        midiNote = 43;
    } else if (instrument == Instrument::HIHAT) {
        midiNote = 46;
    } else if (instrument == Instrument::CRASH) {
        midiNote = 49;
    } else if (instrument == Instrument::RIDE) {
        midiNote = 51;
    } else {
        midiNote = -1; //todo replace with incoming value for midiNote aka default
    }

    juce::File file(filename);
    std::unique_ptr<juce::AudioFormatReader> fileReader(formatManager.createReaderFor(file));

    juce::BigInteger note;
    note.setBit(midiNote);


    addSound(new DrumSamplerSound(name, *fileReader, note, midiNote, 0.0f, 10.f, 10.f));
    DBG(filename << "midiNote: " << midiNote);
}
