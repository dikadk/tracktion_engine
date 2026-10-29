/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wsign-conversion"
 #pragma clang diagnostic ignored "-Wunused-parameter"
 #pragma clang diagnostic ignored "-Wshadow"
 #pragma clang diagnostic ignored "-Wmacro-redefined"
 #pragma clang diagnostic ignored "-Wconversion"
 #pragma clang diagnostic ignored "-Wunused"
 #if __has_warning("-Wzero-as-null-pointer-constant")
  #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
 #endif
 #pragma clang diagnostic ignored "-Wextra-semi"
#endif

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
 #if ! __clang__
  #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
 #endif
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wpedantic"
 #pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#if TRACKTION_ENABLE_SOUNDTOUCH_BPM
 #include "../3rd_party/soundtouch/include/BPMDetect.h"
#endif

#ifdef __clang__
 #pragma clang diagnostic pop
#endif

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

/**
    Uses the SoundTouch BPMDetect class to guess the tempo of some audio.
*/
class TempoDetect
{
public:
    TempoDetect (int numChannels_, double sampleRate)
        : numChannels (numChannels_)
         #if TRACKTION_ENABLE_SOUNDTOUCH_BPM
          , bpmDetect (numChannels, (int) sampleRate)
         #endif
    {
       #if ! TRACKTION_ENABLE_SOUNDTOUCH_BPM
        juce::ignoreUnused (sampleRate);
       #endif
    }

    /** Processes an entire AudioFormatReader returning the tempo for it.
        @returns the tempo in BPM for the block.
    */
    float processReader (juce::AudioFormatReader& reader)
    {
       #if ! TRACKTION_ENABLE_SOUNDTOUCH_BPM
        juce::ignoreUnused (reader);
        return finishAndDetect();
       #else
        jassert ((int) reader.numChannels == numChannels);

        const SampleCount blockSize = 65536;
        const bool useRightChan = numChannels > 1;

        // can't use an AudioScratchBuffer yet
        juce::AudioBuffer<float> buffer (numChannels, blockSize);

        SampleCount numLeft = reader.lengthInSamples;
        int startSample = 0;

        while (numLeft > 0)
        {
            auto numThisTime = (int) std::min (numLeft, blockSize);
            reader.read (&buffer, 0, numThisTime, startSample, true, useRightChan);
            processSection (buffer, numThisTime);

            startSample += numThisTime;
            numLeft -= numThisTime;
        }

        return finishAndDetect();
       #endif
    }

    /** Processes a block of audio returning the tempo for it.
        @returns the tempo in BPM for the block.
    */
    float processAndDetect (const float** const inputSamples, int numSamples)
    {
        processSection (inputSamples, numSamples);

        return finishAndDetect();
    }

    /** Returns the last BPM detected. */
    float getBpm() const                            { return  bpm; }

    bool isBpmSensible() const                      { return getSensibleRange().contains (bpm); }
    static juce::Range<float> getSensibleRange()    { return { 29, 200 }; }

    //==============================================================================
    /** Processes a non-interleaved buffer section.  */
    void processSection (juce::AudioBuffer<float>& buffer, int numSamplesToProcess)
    {
        processSection (buffer.getArrayOfReadPointers(), numSamplesToProcess);
    }

    /** Completes the detection process and returns the BPM. */
    float finishAndDetect()
    {
       #if TRACKTION_ENABLE_SOUNDTOUCH_BPM
        return bpm = bpmDetect.getBpm();
       #else
        return bpm = 0.0f;
       #endif
    }

    void processSection (const float* const* inputSamples, int numSamples)
    {
       #if TRACKTION_ENABLE_SOUNDTOUCH_BPM
        AudioScratchBuffer scratch (1, numChannels * numSamples);
        float* interleaved = scratch.buffer.getWritePointer (0);

        using Format = juce::AudioData::Format<juce::AudioData::Float32, juce::AudioData::NativeEndian>;
        juce::AudioData::interleaveSamples (juce::AudioData::NonInterleavedSource<Format> { inputSamples,   numChannels },
                                            juce::AudioData::InterleavedDest<Format>      { interleaved,    numChannels },
                                            numSamples);

        bpmDetect.inputSamples (interleaved, numSamples);
       #else
        juce::ignoreUnused (inputSamples, numSamples);
       #endif
    }

private:
    //==============================================================================
    int numChannels;
   #if TRACKTION_ENABLE_SOUNDTOUCH_BPM
    soundtouch::BPMDetect bpmDetect;
   #endif
    float bpm = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoDetect)
};

}} // namespace tracktion { inline namespace engine
