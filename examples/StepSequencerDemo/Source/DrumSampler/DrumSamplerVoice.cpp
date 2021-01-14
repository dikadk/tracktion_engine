//
// Created by home on 12/27/20.
//

#include "DrumSamplerVoice.h"
#include "DrumSampler.h"

DrumSamplerVoice::DrumSamplerVoice(int _index) : index(_index) {

}

DrumSamplerVoice::~DrumSamplerVoice() {

}

bool DrumSamplerVoice::canPlayDrumSound(int note) const {
    return midiNote[note];
}

void DrumSamplerVoice::setMidiNote(int note) {
    midiNote.setBit(note);
}

bool DrumSamplerVoice::canPlaySound(juce::SynthesiserSound *sound) {
    return ((dynamic_cast<DrumSamplerSound *>(sound)) != nullptr);
}

void
DrumSamplerVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound *sampSound, int pitchWheel) {
    if (auto *sound = dynamic_cast<DrumSamplerSound *>(sampSound)) {
        sourceSamplePosition = 0.0;
    } else {
        jassertfalse;
    }
}

void DrumSamplerVoice::stopNote(float velocity, bool allowTailOff) {
    if (allowTailOff) {

    } else if (velocity == 0.0f) {
        clearCurrentNote();
    } else if (velocity == 1.0f) {

    }
}

void DrumSamplerVoice::pitchWheelMoved(int newValue) {
    SamplerVoice::pitchWheelMoved(newValue);
}

void DrumSamplerVoice::controllerMoved(int controllerNumber, int newValue) {
    SamplerVoice::controllerMoved(controllerNumber, newValue);
}

void DrumSamplerVoice::renderNextBlock(juce::AudioBuffer<float> &buffer, int startSample, int numSamples) {
    if (auto *sound = dynamic_cast<DrumSamplerSound *>(getCurrentlyPlayingSound().get())) {
        auto &data = *sound->getAudioData();

        auto inputLeft = data.getReadPointer(0);
        auto inputRight = data.getNumChannels() > 1 ? data.getReadPointer(1) : nullptr;

        float *outLeft = buffer.getWritePointer(0, startSample);
        float *outRight = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample) : nullptr;

        while (--numSamples >= 0) {
            int pos = sourceSamplePosition;
            float alpha = sourceSamplePosition - pos;
            auto invAlpha = 1.0f - alpha;
            float envVal = 0.0f;
            //linear interpolation
            float l = (inputLeft[pos] * invAlpha + inputLeft[pos + 1] * alpha);
            float r = (inputRight != nullptr) ? (inputRight[pos] * invAlpha + inputRight[pos + 1] * alpha) : l;

            if (inputRight != nullptr) {
                *outLeft++ += l;
                *outRight++ += r;
            } else {
                *outLeft++ += (l + r) * 0.5f;
            }
        }
    }
}


