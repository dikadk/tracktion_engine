//
// Created by home on 12/27/20.
//

#include "DrumSamplerPlugin.h"
#include "DrumSamplerVoice.h"

const char *DrumSamplerPlugin::xmlTypeName = "DrumSampler";

DrumSamplerPlugin::DrumSamplerPlugin(te::PluginCreationInfo info) : Plugin(info){

    //todo iterate over sounds and setup their parameters
}

DrumSamplerPlugin::~DrumSamplerPlugin() {

}

void DrumSamplerPlugin::initialise(const te::PlaybackInitialisationInfo &info) {
    sampler.setCurrentPlaybackSampleRate(info.sampleRate);
    for (int i = 0; i < sampler.getNumVoices(); i++) {
        if (auto *voice = dynamic_cast<DrumSamplerVoice *>(sampler.getVoice(i))) {
            //voice->getEnvelope().setSampleRate(info.sampleRate);
            //voice->getFilterLeft().setSampleRate(sampleRate);
            //voice->getFilterRight().setSampleRate(sampleRate);
        }
    }
}

void DrumSamplerPlugin::deinitialise() {

}

void DrumSamplerPlugin::applyToBuffer(const te::PluginRenderContext &fc) {
    if (fc.destBuffer != nullptr) {
        juce::ScopedNoDenormals noDenormals;

        te::clearChannels(*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

        midiBuffer.clear();

        if (fc.bufferForMidiMessages != nullptr)
            prepareIncomingMidiMessages(*fc.bufferForMidiMessages, fc.bufferNumSamples, fc.isPlaying);

        sampler.renderNextBlock(*fc.destBuffer, midiBuffer, fc.bufferStartSample,
                                fc.bufferNumSamples);
    }
}

void DrumSamplerPlugin::handleAsyncUpdate() {

}

void DrumSamplerPlugin::prepareIncomingMidiMessages(te::MidiMessageArray incoming, int numSamples, bool isPlaying) {
    if (incoming.isAllNotesOff) {
        juce::uint32 eventsSentOnChannel = 0;

        activeNotes.iterate([&eventsSentOnChannel, this, isPlaying](int chan, int noteNumber) {
            midiBuffer.addEvent(juce::MidiMessage::noteOff(chan, noteNumber), 0);

            if ((eventsSentOnChannel & (1 << chan)) == 0) {
                eventsSentOnChannel |= (1 << chan);

                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(chan, 66 /* sustain pedal off */, 0), 0);
                midiBuffer.addEvent(juce::MidiMessage::controllerEvent(chan, 64 /* hold pedal off */, 0), 0);

                // NB: Some buggy plugins seem to fail to respond to note-ons if they are preceded
                // by an all-notes-off, so avoid this if just dragging the cursor around while playing.
                if (!isPlaying)
                    midiBuffer.addEvent(juce::MidiMessage::allNotesOff(chan), 0);
            }
        });

        activeNotes.reset();
    }

    for (auto &m : incoming) {
        if (m.isNoteOn()) {
            if (activeNotes.isNoteActive(m.getChannel(), m.getNoteNumber()))
                continue;

            activeNotes.startNote(m.getChannel(), m.getNoteNumber());
        } else if (m.isNoteOff()) {
            activeNotes.clearNote(m.getChannel(), m.getNoteNumber());
        }

        auto sample = juce::jlimit(0, numSamples - 1, (int) (m.getTimeStamp() * sampleRate));
        midiBuffer.addEvent(m, sample);
    }

#if 0
    if (! incoming.isEmpty())
    {
        const uint8* midiData;
        int numBytes, midiEventPos;

        DBG ("----------");

        for (MidiBuffer::Iterator iter (midiBuffer); iter.getNextEvent (midiData, numBytes, midiEventPos);)
            DBG (String::toHexString (midiData, numBytes) << "   " << midiEventPos);
    }
#endif

    incoming.clear();
}

int DrumSamplerPlugin::getNumSounds() const {
    return 0;
}

juce::String DrumSamplerPlugin::getSoundName(int index) const {
    return juce::String();
}

void DrumSamplerPlugin::setSoundName(int index, const juce::String &name) {

}


juce::String DrumSamplerPlugin::getSoundMedia(int index) const {
    return juce::String();
}

int DrumSamplerPlugin::getKeyNote(int index) const {
    return 0;
}

int DrumSamplerPlugin::getMinKey(int index) const {
    return 0;
}

int DrumSamplerPlugin::getMaxKey(int index) const {
    return 0;
}

float DrumSamplerPlugin::getSoundGainDb(int index) const {
    return 0;
}

float DrumSamplerPlugin::getSoundPan(int index) const {
    return 0;
}

bool DrumSamplerPlugin::isSoundOpenEnded(int index) const {
    return false;
}

double DrumSamplerPlugin::getSoundStartTime(int index) const {
    return 0;
}

double DrumSamplerPlugin::getSoundLength(int index) const {
    return 0;
}

void DrumSamplerPlugin::setSoundExcerpt(int index, double start, double length) {

}

juce::String
DrumSamplerPlugin::addSound(const juce::String &sourcePathOrProjectID, const juce::String &name, double startTime,
                            double length, float gainDb) {
    return juce::String();
}
