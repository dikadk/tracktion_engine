/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if JUCE_WINDOWS && TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
 #pragma comment (lib, "libelastiquePro.lib")
 #pragma comment (lib, "libelastiqueEfficient.lib")
 #pragma comment (lib, "libelastiqueSOLOIST.lib")
 #pragma comment (lib, "libResample.lib")
 #pragma comment (lib, "libzplVecLib.lib")
#endif

#if JUCE_WINDOWS && TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE_IPP
 #pragma comment (lib, "ippcore_l.lib")
 #pragma comment (lib, "ipps_l.lib")
 #pragma comment (lib, "ippvm_l.lib")
#endif

#if JUCE_WINDOWS
 #define NOMINMAX
#endif

namespace tracktion { inline namespace engine
{

TimeStretcher::ElastiqueProOptions::ElastiqueProOptions (const juce::String& string)
{
    if (string.isEmpty())
        return;

    if (string.startsWith ("1/"))
    {
        juce::StringArray tokens;
        tokens.addTokens (string, "/", {});

        if (tokens.size() == 5)
        {
            midSideStereo        = tokens[1].getIntValue() != 0;
            syncTimeStrPitchShft = tokens[2].getIntValue() != 0;
            preserveFormants     = tokens[3].getIntValue() != 0;
            envelopeOrder        = tokens[4].getIntValue();
            return;
        }
    }

    jassertfalse; //unknown string format
}

juce::String TimeStretcher::ElastiqueProOptions::toString() const
{
    // version / midside / sync / preserve / order
    return juce::String::formatted ("1/%d/%d/%d/%d",
                                    midSideStereo        ? 1 : 0,
                                    syncTimeStrPitchShft ? 1 : 0,
                                    preserveFormants     ? 1 : 0,
                                    envelopeOrder);
}

bool TimeStretcher::ElastiqueProOptions::operator== (const ElastiqueProOptions& other) const
{
    return midSideStereo        == other.midSideStereo
        && syncTimeStrPitchShft == other.syncTimeStrPitchShft
        && preserveFormants     == other.preserveFormants
        && envelopeOrder        == other.envelopeOrder;
}

bool TimeStretcher::ElastiqueProOptions::operator!= (const ElastiqueProOptions& other) const
{
    return ! operator== (other);
}

//==============================================================================
struct TimeStretcher::Stretcher
{
    virtual ~Stretcher() {}

    virtual bool isOk() const = 0;
    virtual void reset() = 0;
    virtual bool setSpeedAndPitch (float speedRatio, float semitonesUp) = 0;
    virtual int getFramesNeeded() const = 0;
    virtual int getMaxFramesNeeded() const = 0;
    virtual int processData (const float* const* inChannels, int numSamples, float* const* outChannels) = 0;
    virtual int flush (float* const* outChannels) = 0;
};

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE

struct ElastiqueStretcher  : public TimeStretcher::Stretcher
{
    ElastiqueStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels,
                        TimeStretcher::Mode mode, TimeStretcher::ElastiqueProOptions options, float minFactor)
        : elastiqueMode (mode),
          elastiqueProOptions (options),
          samplesPerOutputBuffer (samplesPerBlock)
    {
        CRASH_TRACER
        jassert (sourceSampleRate > 0.0);
        int res = CElastiqueProV3If::CreateInstance (elastique, samplesPerBlock, numChannels, (float) sourceSampleRate,
                                                     getElastiqueMode (mode), minFactor);
        jassert (res == 0); juce::ignoreUnused (res);

        if (elastique == nullptr)
        {
            jassertfalse;
            TRACKTION_LOG_ERROR ("Cannot create Elastique");
        }
        else
        {
            // This must be called first, before any other functions and can not
            // be called again
            maxFramesNeeded = elastique->GetMaxFramesNeeded();

            if (mode == TimeStretcher::elastiquePro)
                elastique->SetStereoInputMode (elastiqueProOptions.midSideStereo ? CElastiqueProV3If::kMSMode : CElastiqueProV3If::kPlainStereoMode);
        }
    }

    ~ElastiqueStretcher() override
    {
        if (elastique != nullptr)
            CElastiqueProV3If::DestroyInstance (elastique);
    }

    bool isOk() const override      { return elastique != nullptr; }
    void reset() override           { elastique->Reset(); }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        float pitch = juce::jlimit (0.25f, 4.0f, Pitch::semitonesToRatio (semitonesUp));
        bool sync  = (elastiqueMode == TimeStretcher::elastiquePro) ? elastiqueProOptions.syncTimeStrPitchShft : false;
        int r = elastique->SetStretchPitchQFactor (speedRatio, pitch, sync);
        jassert (r == 0); juce::ignoreUnused (r);

        if (elastiqueMode == TimeStretcher::elastiquePro && elastiqueProOptions.preserveFormants)
        {
            elastique->SetEnvelopeFactor (pitch);
            elastique->SetEnvelopeOrder (elastiqueProOptions.envelopeOrder);
        }

        return r == 0;
    }

    int getFramesNeeded() const override
    {
        const int framesNeeded = elastique->GetFramesNeeded();
        jassert (framesNeeded <= maxFramesNeeded);
        return framesNeeded;
    }

    int getMaxFramesNeeded() const override
    {
        return maxFramesNeeded;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        int err = elastique->ProcessData ((float**) inChannels, numSamples, (float**) outChannels);
        jassert (err == 0); juce::ignoreUnused (err);
        return samplesPerOutputBuffer;
    }

    int flush (float* const* outChannels) override
    {
        return elastique->FlushBuffer ((float**) outChannels);
    }

private:
    CElastiqueProV3If* elastique = nullptr;
    TimeStretcher::Mode elastiqueMode;
    TimeStretcher::ElastiqueProOptions elastiqueProOptions;
    const int samplesPerOutputBuffer;
    int maxFramesNeeded;

    static CElastiqueProV3If::ElastiqueMode_t getElastiqueMode (TimeStretcher::Mode mode)
    {
        switch (mode)
        {
            case TimeStretcher::elastiquePro:               return CElastiqueProV3If::kV3Pro;
            case TimeStretcher::elastiqueEfficient:         return CElastiqueProV3If::kV3Eff;
            case TimeStretcher::elastiqueMobile:            return CElastiqueProV3If::kV3mobile;
            case TimeStretcher::elastiqueMonophonic:        return CElastiqueProV3If::kV3Monophonic;
            case TimeStretcher::elastiqueDirectPro:         [[ fallthrough ]];
            case TimeStretcher::elastiqueDirectEfficient:   [[ fallthrough ]];
            case TimeStretcher::elastiqueDirectMobile:      [[ fallthrough ]];
            case TimeStretcher::disabled:                   [[ fallthrough ]];
            case TimeStretcher::elastiqueTransient:         [[ fallthrough ]];
            case TimeStretcher::elastiqueTonal:             [[ fallthrough ]];
            case TimeStretcher::soundtouchNormal:           [[ fallthrough ]];
            case TimeStretcher::soundtouchBetter:           [[ fallthrough ]];
            case TimeStretcher::melodyne:                   [[ fallthrough ]];
            case TimeStretcher::rubberbandMelodic:          [[ fallthrough ]];
            case TimeStretcher::rubberbandPercussive:       [[ fallthrough ]];
            default:
                jassertfalse;
                return CElastiqueProV3If::kV3Pro;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElastiqueStretcher)
};


struct ElastiqueDirectStretcher : public TimeStretcher::Stretcher
{
    ElastiqueDirectStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels_,
                              TimeStretcher::Mode mode, TimeStretcher::ElastiqueProOptions options, float minFactor)
        : elastiqueMode (mode),
          elastiqueProOptions (options),
          samplesPerOutputBuffer (samplesPerBlock),
          numChannels (numChannels_),
          outputFifo (numChannels, samplesPerBlock)
    {
        CRASH_TRACER
        jassert (sourceSampleRate > 0.0);
        [[maybe_unused]] int res = CElastiqueProV3DirectIf::CreateInstance (elastique, numChannels, (float) sourceSampleRate,
                                                                            getElastiqueMode (mode), minFactor);
        jassert (res == 0);

        if (elastique == nullptr)
        {
            jassertfalse;
            TRACKTION_LOG_ERROR ("Cannot create Elastique Direct");
        }
        else
        {
            // This must be called first, before any other functions and can not
            // be called again
            maxFramesNeeded = elastique->GetMaxFramesNeeded();
            outputFifo.setSize (numChannels, maxFramesNeeded * 2);

            if (mode == TimeStretcher::elastiquePro)
                elastique->SetStereoInputMode (elastiqueProOptions.midSideStereo ? CElastiqueProV3DirectIf::kMSMode
                                                                                 : CElastiqueProV3DirectIf::kPlainStereoMode);
        }
    }

    ~ElastiqueDirectStretcher() override
    {
        if (elastique != nullptr)
            CElastiqueProV3DirectIf::DestroyInstance (elastique);
    }

    bool isOk() const override
    {
        return elastique != nullptr;
    }

    void reset() override
    {
        hasBeenReset = true;
        elastique->Reset();
        outputFifo.reset();

        newSpeedAndPitchPending = true;
    }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        newSpeedRatio = speedRatio;
        newSemitonesUp = semitonesUp;
        newSpeedAndPitchPending = true;
        tryToSetNewSpeedAndPitch();

        return true;
    }

    int getFramesNeeded() const override
    {
        tryToSetNewSpeedAndPitch();

        if (outputFifo.getNumReady() > samplesPerOutputBuffer)
            return 0;

        const int framesNeeded = hasBeenReset ? elastique->GetPreFramesNeeded()
                                              : elastique->GetFramesNeeded();
        jassert (framesNeeded <= maxFramesNeeded);

        return framesNeeded;
    }

    int getMaxFramesNeeded() const override
    {
        return maxFramesNeeded;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        using namespace choc::buffer;

        if (numSamples > 0)
        {
            const auto numFrames = static_cast<FrameCount> (numSamples);
            const auto inputView = createChannelArrayView (inChannels,
                                                           static_cast<ChannelCount> (numChannels),
                                                           numFrames);

            if (std::exchange (hasBeenReset, false))
            {
                assert (numFrames == static_cast<FrameCount> (elastique->GetPreFramesNeeded()));
                processPreFrames (inputView);
            }
            else
            {
                assert (numSamples == elastique->GetFramesNeeded());
                processFrames (inputView);
            }
        }
        else
        {
            // This should only happen if there are enough frames in the fifo to return
            assert (outputFifo.getNumReady() >= samplesPerOutputBuffer);
        }

        // If enough samples are ready, output these now
        if (const int numReady = outputFifo.getNumReady(); numReady > 0)
        {
            const int numToReturn = std::min (numReady, samplesPerOutputBuffer);
            juce::AudioBuffer<float> outputView (outChannels, numChannels, numToReturn);

            if (! outputFifo.read (outputView, 0))
            {
                assert (false);
                return 0;
            }

            return outputView.getNumSamples();
        }

        jassertfalse;
        return 0;
    }

    int flush (float* const* outChannels) override
    {
        {
            AudioScratchBuffer scratchBuffer (numChannels, maxFramesNeeded);
            const int numFramesReturned = elastique->FlushBuffer ((float **) scratchBuffer.buffer.getArrayOfWritePointers());
            assert (numFramesReturned <= scratchBuffer.buffer.getNumSamples());
            assert (outputFifo.getFreeSpace() >= numFramesReturned);
            outputFifo.write (scratchBuffer.buffer, 0, numFramesReturned);
        }

        // If enough samples are ready, output these now
        if (const int numReady = outputFifo.getNumReady(); numReady > 0)
        {
            const int numToReturn = std::min (numReady, samplesPerOutputBuffer);
            juce::AudioBuffer<float> outputView (outChannels, numChannels, numToReturn);
            [[maybe_unused]] const bool success = outputFifo.read (outputView, 0);
            jassert (success);

            return outputView.getNumSamples();
        }

        return 0;
    }

private:
    CElastiqueProV3DirectIf* elastique = nullptr;
    TimeStretcher::Mode elastiqueMode;
    TimeStretcher::ElastiqueProOptions elastiqueProOptions;
    const int samplesPerOutputBuffer, numChannels;
    int maxFramesNeeded = 0;
    AudioFifo outputFifo;
    bool hasBeenReset = true;
    float newSpeedRatio, newSemitonesUp;
    mutable bool newSpeedAndPitchPending = false;

    static CElastiqueProV3DirectIf::ElastiqueVersion_t getElastiqueMode (TimeStretcher::Mode mode)
    {
        switch (mode)
        {
            case TimeStretcher::elastiqueDirectPro:         return CElastiqueProV3DirectIf::kV3Pro;
            case TimeStretcher::elastiqueDirectEfficient:   return CElastiqueProV3DirectIf::kV3Eff;
            case TimeStretcher::elastiqueDirectMobile:      return CElastiqueProV3DirectIf::kV3mobile;
            case TimeStretcher::elastiquePro:               [[ fallthrough ]];
            case TimeStretcher::elastiqueEfficient:         [[ fallthrough ]];
            case TimeStretcher::elastiqueMobile:            [[ fallthrough ]];
            case TimeStretcher::elastiqueMonophonic:        [[ fallthrough ]];
            case TimeStretcher::disabled:                   [[ fallthrough ]];
            case TimeStretcher::elastiqueTransient:         [[ fallthrough ]];
            case TimeStretcher::elastiqueTonal:             [[ fallthrough ]];
            case TimeStretcher::soundtouchNormal:           [[ fallthrough ]];
            case TimeStretcher::soundtouchBetter:           [[ fallthrough ]];
            case TimeStretcher::melodyne:                   [[ fallthrough ]];
            case TimeStretcher::rubberbandMelodic:          [[ fallthrough ]];
            case TimeStretcher::rubberbandPercussive:       [[ fallthrough ]];
            default:
                jassertfalse;
                return CElastiqueProV3DirectIf::kV3Pro;
        }
    }

    void tryToSetNewSpeedAndPitch() const
    {
        if (! newSpeedAndPitchPending)
            return;

        float pitch = juce::jlimit (0.25f, 4.0f, Pitch::semitonesToRatio (newSemitonesUp));
        const bool sync = elastiqueMode == TimeStretcher::elastiqueDirectPro ? elastiqueProOptions.syncTimeStrPitchShft : false;
        const int res = elastique->SetStretchPitchQFactor (newSpeedRatio, pitch, sync);
        newSpeedAndPitchPending = res != 0;

        if (elastiqueMode == TimeStretcher::elastiqueDirectPro && elastiqueProOptions.preserveFormants)
        {
            elastique->SetEnvelopeFactor (pitch);
            elastique->SetEnvelopeOrder (elastiqueProOptions.envelopeOrder);
        }
    }

    int processPreFrames (const choc::buffer::ChannelArrayView<const float>& inFrames)
    {
        const int numInputFrames = static_cast<int> (inFrames.size.numFrames);
        assert (numInputFrames == elastique->GetPreFramesNeeded());

        AudioScratchBuffer scratchBuffer (numChannels, maxFramesNeeded);
        const int numPreProcessFrames = elastique->PreProcessData ((float **) inFrames.data.channels, numInputFrames,
                                                                   (float **) scratchBuffer.buffer.getArrayOfWritePointers(),
                                                                   samplesPerOutputBuffer >= 512 ? CElastiqueProV3DirectIf::kFastStartup
                                                                                                 : CElastiqueProV3DirectIf::kNormalStartup);

        assert (numPreProcessFrames <= scratchBuffer.buffer.getNumSamples());
        assert (outputFifo.getFreeSpace() >= numPreProcessFrames);
        outputFifo.write (scratchBuffer.buffer, 0, numPreProcessFrames);

        return numPreProcessFrames;
    }

    int processFrames (const choc::buffer::ChannelArrayView<const float>& inFrames)
    {
        const int numInputFrames = static_cast<int> (inFrames.size.numFrames);

        if (numInputFrames == 0)
            return 0;

        {
            assert (numInputFrames == elastique->GetFramesNeeded());
            [[maybe_unused]] const int err = elastique->ProcessData ((float **) inFrames.data.channels, numInputFrames);
            assert (err == 0);
        }

        for (int numProcessCalls = elastique->GetNumOfProcessCalls(); --numProcessCalls >= 0;)
        {
            [[maybe_unused]] const int err = elastique->ProcessData();
            assert (err == 0);
        }

        // N.B. GetFramesProcessed always returns a positive number until new data is supplied
        const int numFramesProcessed = elastique->GetFramesProcessed();
        AudioScratchBuffer scratchBuffer (numChannels, numFramesProcessed);
        const int numFramesReturned = elastique->GetProcessedData ((float **) scratchBuffer.buffer.getArrayOfWritePointers());
        assert (numFramesProcessed == numFramesReturned);
        assert (numFramesReturned <= scratchBuffer.buffer.getNumSamples());
        assert (outputFifo.getFreeSpace() >= numFramesReturned);
        outputFifo.write (scratchBuffer.buffer, 0, numFramesReturned);

        return numFramesReturned;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElastiqueDirectStretcher)
};


#endif

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH

#include "../3rd_party/soundtouch/include/SoundTouch.h"

struct SoundTouchStretcher  : public TimeStretcher::Stretcher,
                              private soundtouch::SoundTouch
{
    SoundTouchStretcher (double sourceSampleRate, int samplesPerBlock,
                         int chans, bool betterQuality)
        : numChannels (chans), samplesPerOutputBuffer (samplesPerBlock)
    {
        CRASH_TRACER
        setChannels (static_cast<unsigned int> (numChannels));
        setSampleRate (static_cast<unsigned int> (sourceSampleRate));

        if (betterQuality)
        {
            setSetting (SETTING_USE_AA_FILTER, 1);
            setSetting (SETTING_AA_FILTER_LENGTH, 64);
            setSetting (SETTING_USE_QUICKSEEK, 0);
            setSetting (SETTING_SEQUENCE_MS, 60);
            setSetting (SETTING_SEEKWINDOW_MS, 25);
        }
    }

    bool isOk() const override      { return true; }
    void reset() override           { clear(); }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        setTempo (1.0f / speedRatio);
        setPitchSemiTones (semitonesUp);
        inputOutputSampleRatio = getInputOutputSampleRatio();

        return true;
    }

    int getFramesNeeded() const override
    {
        const int numAvailable = (int) numSamples();
        const int numRequiredForOneBlock = juce::roundToInt (samplesPerOutputBuffer * inputOutputSampleRatio);

        return std::max (0, numRequiredForOneBlock - numAvailable);
    }

    int getMaxFramesNeeded() const override
    {
        // This was derived by experimentation
        return 8192;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        jassert (numSamples <= getFramesNeeded());
        writeInput (inChannels, numSamples);

        const int numAvailable = (int) soundtouch::SoundTouch::numSamples();
        jassert (numAvailable >= 0);

        const int numToRead = std::min (numAvailable, samplesPerOutputBuffer);

        if (numToRead > 0)
            return readOutput (outChannels, 0, numToRead);

        return 0;
    }

    int flush (float* const* outChannels) override
    {
        CRASH_TRACER
        if (! hasDoneFinalBlock)
        {
            soundtouch::SoundTouch::flush();
            hasDoneFinalBlock = true;
        }

        const int numAvailable = (int) numSamples();
        const int numToRead = std::min (numAvailable, samplesPerOutputBuffer);

        return readOutput (outChannels, 0, numToRead);
    }

private:
    int numChannels = 0, samplesPerOutputBuffer = 0;
    bool hasDoneFinalBlock = false;
    double inputOutputSampleRatio = 1.0;

    int readOutput (float* const* outChannels, int offset, int numNeeded)
    {
        float* interleaved = ptrBegin();
        auto num = std::min (numNeeded, (int) numSamples());

        for (int chan = 0; chan < numChannels; ++chan)
        {
            const float* src = interleaved + chan;
            float* dest = outChannels[chan] + offset;

            for (int i = 0; i < num; ++i)
            {
                dest[i] = *src;
                src += numChannels;
            }
        }

        receiveSamples (static_cast<unsigned int> (num));
        return num;
    }

    void writeInput (const float* const* inChannels, int numSamples)
    {
        if (numChannels > 1)
        {
            AudioScratchBuffer scratch (1, numSamples * numChannels);
            float* interleaved = scratch.buffer.getWritePointer(0);

            for (int chan = 0; chan < numChannels; ++chan)
            {
                float* dest = interleaved + chan;
                const float* src = inChannels[chan];

                for (int i = 0; i < numSamples; ++i)
                {
                    *dest = src[i];
                    dest += numChannels;
                }
            }

            putSamples (interleaved, static_cast<unsigned int> (numSamples));
        }
        else
        {
            putSamples (inChannels[0], static_cast<unsigned int> (numSamples));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundTouchStretcher)
};
#endif

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wextra-semi"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wsign-compare"
 #pragma GCC diagnostic ignored "-Wcast-align"
 #if __clang__
  #pragma GCC diagnostic ignored "-Wdeprecated-copy-with-user-provided-dtor"
 #else
  #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
 #endif
 #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wpedantic"
 #pragma GCC diagnostic ignored "-Wunknown-pragmas"
 #pragma GCC diagnostic ignored "-Wswitch-enum"
 #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

#if JUCE_WINDOWS
 #pragma warning (push)
 #pragma warning (disable: 4456)
#endif

#define WIN32_LEAN_AND_MEAN 1
#define Point CarbonDummyPointName
#define Component CarbonDummyCompName

}} // namespace tracktion { inline namespace engine
#if TRACKTION_BUILD_RUBBERBAND
 #if __has_include(<rubberband/single/RubberBandSingle.cpp>)
  #include <rubberband/single/RubberBandSingle.cpp>
 #elif __has_include("../3rd_party/rubberband/single/RubberBandSingle.cpp")
  #include "../3rd_party/rubberband/single/RubberBandSingle.cpp"
 #else
  #error "TRACKTION_BUILD_RUBBERBAND enabled but not found in the search path!"
 #endif
#else
 #if __has_include(<rubberband/rubberband/RubberBandStretcher.h>)
  #include <rubberband/rubberband/RubberBandStretcher.h>
 #elif __has_include("../3rd_party/rubberband/rubberband/RubberBandStretcher.h")
  #include "../3rd_party/rubberband/rubberband/RubberBandStretcher.h"
 #else
  #error "TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND enabled but not found in the search path!"
 #endif
#endif
namespace tracktion { inline namespace engine {

#undef WIN32_LEAN_AND_MEAN
#undef Point
#undef Component

#if JUCE_WINDOWS
 #pragma warning (pop)
#endif

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

struct RubberBandStretcher  : public TimeStretcher::Stretcher
{
    static int getOptionFlags (bool percussive)
    {
        if (percussive)
            return RubberBand::RubberBandStretcher::OptionProcessRealTime
                | RubberBand::RubberBandStretcher::OptionPitchHighConsistency
                | RubberBand::RubberBandStretcher::PercussiveOptions;

        return RubberBand::RubberBandStretcher::OptionProcessRealTime
            | RubberBand::RubberBandStretcher::OptionPitchHighConsistency
            | RubberBand::RubberBandStretcher::OptionWindowShort;
    }

    RubberBandStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels, bool percussive)
        : rubberBandStretcher ((size_t) sourceSampleRate, (size_t) numChannels,
                               getOptionFlags (percussive)),
          samplesPerOutputBuffer (samplesPerBlock)
    {
    }

    bool isOk() const override
    {
        return true;
    }

    void reset() override
    {
        rubberBandStretcher.reset();
    }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        const float pitch = juce::jlimit (0.25f, 4.0f, tracktion::engine::Pitch::semitonesToRatio (semitonesUp));

        rubberBandStretcher.setPitchScale (pitch);
        rubberBandStretcher.setTimeRatio (speedRatio);

        if (numSamplesToDrop == -1)
        {
            // This is the first speed and pitch change so set up the padding and dropping
            numSamplesToDrop = int (rubberBandStretcher.getStartDelay());
            int numSamplesToPad = static_cast<size_t> (rubberBandStretcher.getPreferredStartPad());

            if (numSamplesToPad > 0)
            {
                AudioScratchBuffer scratch (int (rubberBandStretcher.getChannelCount()), samplesPerOutputBuffer);
                scratch.buffer.clear();

                while (numSamplesToPad > 0)
                {
                    const int numThisTime = std::min (numSamplesToPad, samplesPerOutputBuffer);
                    rubberBandStretcher.process (scratch.buffer.getArrayOfReadPointers(), numThisTime, false);
                    numSamplesToPad -= numThisTime;
                }
            }

            jassert (numSamplesToPad == 0);
        }

        const bool r = rubberBandStretcher.getPitchScale() == pitch
                    && rubberBandStretcher.getTimeRatio() == speedRatio;
        jassert (r);
        juce::ignoreUnused (r);

        return r;
    }

    int getFramesNeeded() const override
    {
        // This number of frames required for the first block of output samples was found by
        // trial and error because getSamplesRequired() doesn't seem to return enough for the
        // first block.
        // The x6 was found to be ok to process at time ratios of 0.5 - 2.0
        const int numFramesRequired = static_cast<int> (rubberBandStretcher.getSamplesRequired());
        return numSamplesToDrop > 0 ? std::min (numFramesRequired * 6, getMaxFramesNeeded())
                                    : numFramesRequired;
    }

    int getMaxFramesNeeded() const override
    {
        return maxFramesNeeded;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        jassert (numSamples <= getFramesNeeded());
        rubberBandStretcher.process (inChannels, (size_t) numSamples, false);

        // Once there is output, read this in to the output buffer
        int numAvailable = rubberBandStretcher.available();
        jassert (numAvailable >= 0);

        if (numSamplesToDrop > 0)
        {
            auto numToDropThisTime = std::min (numSamplesToDrop, std::min (numAvailable, samplesPerOutputBuffer));
            rubberBandStretcher.retrieve (outChannels, (size_t) numToDropThisTime);
            numSamplesToDrop -= numToDropThisTime;
            jassert (numSamplesToDrop >= 0);

            numAvailable -= numToDropThisTime;
        }

        if (numAvailable > 0)
            return (int) rubberBandStretcher.retrieve (outChannels, (size_t) std::min (numAvailable, samplesPerOutputBuffer));

        return 0;
    }

    int flush (float* const* outChannels) override
    {
        // Empty the FIFO in to the stretcher and mark the last block as final
        if (! hasDoneFinalBlock)
        {
            float* inChannels[32] = { nullptr };
            rubberBandStretcher.process (inChannels, 0, true);
            hasDoneFinalBlock = true;
        }

        // Then get the rest of the data out of the stretcher
        const int numAvailable = rubberBandStretcher.available();
        const int numThisBlock = std::min (numAvailable, samplesPerOutputBuffer);

        if (numThisBlock > 0)
            return (int) rubberBandStretcher.retrieve (outChannels, (size_t) numThisBlock);

        return 0;
    }

private:
    RubberBand::RubberBandStretcher rubberBandStretcher;
    const int maxFramesNeeded = 8192;
    const int samplesPerOutputBuffer = 0;
    int numSamplesToDrop = -1;
    bool hasDoneFinalBlock = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RubberBandStretcher)
};

#endif // TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH

// Adapted for the pre-latency-query TimeStretcher API. All storage is prepared
// in the constructor. The caller must serialise reset/parameter/process calls,
// as for the other backends. Ratios are output duration / input duration.
struct SignalsmithStretcher final : public TimeStretcher::Stretcher
{
    SignalsmithStretcher (double sampleRate, int blockSize, int channels,
                          TimeStretcher::Mode mode)
        : numChannels (channels), samplesPerOutputBuffer (blockSize)
    {
        // Bound arithmetic and reject invalid configurations before allocating.
        if (! std::isfinite (sampleRate) || sampleRate < 1000.0 || sampleRate > 768000.0
            || channels <= 0 || channels > 64 || blockSize <= 0 || blockSize > 1048576)
            return;

        // The old API's seek/drain contract requires completed spectra. Split
        // computation leaves synthesis pending across calls, but upstream flush
        // reads the live STFT output rather than the split output cursor. With
        // this adapter that loses short transients and shifts EOF impulses.
        // Use complete-spectrum processing for both realtime and offline calls;
        // this changes CPU scheduling, not the allocation/blocking contract.
        if (mode == TimeStretcher::signalsmithCheaper)
            stretch.presetCheaper (channels, static_cast<float> (sampleRate), false);
        else
            stretch.presetDefault (channels, static_cast<float> (sampleRate), false);

        inputLatency = stretch.inputLatency();
        outputLatency = stretch.outputLatency();
        maxFramesNeeded = inputLatency + static_cast<int> (std::ceil ((blockSize + outputLatency) / minSpeed)) + 1;
        const int scratchSize = std::max (blockSize + outputLatency,
                                          static_cast<int> (std::ceil (inputLatency * maxSpeed)) + outputLatency)
                              + static_cast<int> (maxSpeed) + 2;
        priming.setSize (channels, inputLatency);
        output.setSize (channels, scratchSize);
        inputPointers.resize (static_cast<size_t> (channels));
        outputPointers.resize (static_cast<size_t> (channels));
        valid = true;
        reset();
    }

    bool isOk() const override { return valid; }

    void reset() override
    {
        if (! valid)
            return;

        stretch.reset();
        priming.clear();
        output.clear();
        primingFrames = bufferedFrames = 0;
        outputToDrop = outputLatency;
        inputFrames = targetFrames = emittedFrames = 0;
        targetFraction = renderFraction = 0.0;
        primed = flushing = false;
        // Signalsmith reset preserves transpose; speedRatio is also preserved.
    }

    bool setSpeedAndPitch (float ratio, float semitones) override
    {
        if (! valid || flushing || ! std::isfinite (ratio) || ! std::isfinite (semitones)
            || ratio < minSpeed || ratio > maxSpeed || semitones < -48.0f || semitones > 48.0f)
            return false;

        speedRatio = ratio;
        stretch.setTransposeSemitones (semitones);
        return true;
    }

    int getFramesNeeded() const override
    {
        if (! valid || flushing)
            return 0;

        const int prefill = primed ? 0 : inputLatency - primingFrames;
        const double wanted = samplesPerOutputBuffer + outputToDrop - bufferedFrames - renderFraction;
        return prefill + std::max (0, static_cast<int> (std::ceil (wanted / speedRatio)));
    }

    int getMaxFramesNeeded() const override { return maxFramesNeeded; }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        if (! valid || flushing || numSamples < 0 || numSamples > getFramesNeeded()
            || ! validChannels (outChannels) || (numSamples > 0 && ! validChannels (inChannels)))
            return 0;

        const juce::ScopedNoDenormals noDenormals;
        inputFrames += numSamples;
        const double target = targetFraction + numSamples * speedRatio;
        const auto wholeTarget = static_cast<int64_t> (std::floor (target));
        targetFrames += wholeTarget;
        targetFraction = target - static_cast<double> (wholeTarget);

        int consumed = 0;
        if (! primed)
        {
            consumed = std::min (numSamples, inputLatency - primingFrames);
            for (int c = 0; c < numChannels; ++c)
                if (consumed > 0)
                    priming.copyFrom (c, primingFrames, inChannels[c], consumed);
            primingFrames += consumed;

            if (primingFrames < inputLatency)
                return 0;

            prime();
        }

        const int remaining = numSamples - consumed;
        if (remaining > 0)
        {
            const double rendered = renderFraction + remaining * speedRatio;
            const int toRender = static_cast<int> (std::floor (rendered));
            renderFraction = rendered - toRender;
            for (int c = 0; c < numChannels; ++c)
                inputPointers[static_cast<size_t> (c)] = inChannels[c] + consumed;
            pointOutputAt (bufferedFrames);
            stretch.process (inputPointers.data(), remaining, outputPointers.data(), toRender);
            bufferedFrames += toRender;
        }

        discardLatency();
        return readOutput (outChannels);
    }

    int flush (float* const* outChannels) override
    {
        if (! valid || ! validChannels (outChannels))
            return 0;

        const juce::ScopedNoDenormals noDenormals;
        if (! flushing)
        {
            flushing = true;
            if (inputFrames == 0)
                return 0;

            // Short sources are padded only for analysis; the returned duration
            // is still round(real input * ratio), with no leading latency.
            if (! primed)
                prime();

            targetFrames += targetFraction >= 0.5 ? 1 : 0;
            const int remaining = static_cast<int> (targetFrames - emittedFrames);
            const int tailFrames = std::max (0, remaining - bufferedFrames + outputToDrop - outputLatency);
            pointOutputAt (bufferedFrames);
            stretch.process (Silence{}, static_cast<int> (std::min<int64_t> (inputFrames, inputLatency)),
                             outputPointers.data(), tailFrames);
            bufferedFrames += tailFrames;

            // Upstream flush resets its synthesis state. Call it exactly once,
            // for the complete output latency, then drain in caller-sized chunks.
            pointOutputAt (bufferedFrames);
            stretch.flush (outputPointers.data(), outputLatency);
            bufferedFrames += outputLatency;
            discardLatency();
            bufferedFrames = std::min (bufferedFrames, remaining);
        }

        return readOutput (outChannels);
    }

private:
    struct Silence
    {
        struct Channel { float operator[] (int) const { return 0.0f; } };
        Channel operator[] (int) const { return {}; }
    };

    template<typename Sample>
    bool validChannels (Sample* const* pointers) const
    {
        if (pointers == nullptr)
            return false;
        for (int c = 0; c < numChannels; ++c)
            if (pointers[c] == nullptr)
                return false;
        return true;
    }

    void prime()
    {
        stretch.seek (priming.getArrayOfReadPointers(), inputLatency, 1.0 / speedRatio);
        primed = true;
    }

    void pointOutputAt (int offset)
    {
        for (int c = 0; c < numChannels; ++c)
            outputPointers[static_cast<size_t> (c)] = output.getWritePointer (c, offset);
    }

    void removeOutput (int count)
    {
        bufferedFrames -= count;
        for (int c = 0; c < numChannels; ++c)
            std::memmove (output.getWritePointer (c), output.getReadPointer (c, count),
                          static_cast<size_t> (bufferedFrames) * sizeof (float));
    }

    void discardLatency()
    {
        const int count = std::min (outputToDrop, bufferedFrames);
        outputToDrop -= count;
        removeOutput (count);
    }

    int readOutput (float* const* outChannels)
    {
        const int count = std::min (samplesPerOutputBuffer, bufferedFrames);
        for (int c = 0; c < numChannels; ++c)
            juce::FloatVectorOperations::copy (outChannels[c], output.getReadPointer (c), count);
        emittedFrames += count;
        removeOutput (count);
        return count;
    }

    static constexpr double minSpeed = 0.125, maxSpeed = 8.0;
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> priming, output;
    std::vector<const float*> inputPointers;
    std::vector<float*> outputPointers;
    const int numChannels, samplesPerOutputBuffer;
    int inputLatency = 0, outputLatency = 0, maxFramesNeeded = 0;
    int primingFrames = 0, bufferedFrames = 0, outputToDrop = 0;
    int64_t inputFrames = 0, targetFrames = 0, emittedFrames = 0;
    double speedRatio = 1.0, targetFraction = 0.0, renderFraction = 0.0;
    bool valid = false, primed = false, flushing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalsmithStretcher)
};

#endif // TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH

//==============================================================================
TimeStretcher::TimeStretcher() {}
TimeStretcher::~TimeStretcher() {}

static juce::String getMelodyne()                   { return "Melodyne"; }
static juce::String getElastiquePro()               { return "Elastique (" + TRANS("Pro") + ")"; }
static juce::String getElastiqueEfficeint()         { return "Elastique (" + TRANS("Efficient") + ")"; }
static juce::String getElastiqueMobile()            { return "Elastique (" + TRANS("Mobile") + ")"; }
static juce::String getElastiqueMono()              { return "Elastique (" + TRANS("Monophonic") + ")"; }
static juce::String getSoundTouchNormal()           { return "SoundTouch (" + TRANS("Normal") + ")"; }
static juce::String getSoundTouchBetter()           { return "SoundTouch (" + TRANS("Better") + ")"; }
static juce::String getRubberBandMelodic()          { return "RubberBand (" + TRANS("Melodic") + ")"; }
static juce::String getRubberBandPercussive()       { return "RubberBand (" + TRANS("Percussive") + ")"; }
static juce::String getElastiqueDirectPro()         { return "Elastique Direct (" + TRANS("Pro") + ")"; }
static juce::String getElastiqueDirectEfficeint()   { return "Elastique Direct (" + TRANS("Efficient") + ")"; }
static juce::String getElastiqueDirectMobile()      { return "Elastique Direct (" + TRANS("Mobile") + ")"; }
static juce::String getSignalsmithDefault()          { return "Signalsmith (" + TRANS("Default") + ")"; }
static juce::String getSignalsmithCheaper()          { return "Signalsmith (" + TRANS("Cheaper") + ")"; }

TimeStretcher::Mode TimeStretcher::checkModeIsAvailable (Mode m)
{
    switch (m)
    {
        case elastiqueTransient:
        case elastiqueTonal:
       #if ! TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
       #endif
       #if ! TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
        case elastiqueDirectPro:
        case elastiqueDirectEfficient:
        case elastiqueDirectMobile:
       #endif
       #if ! TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        case rubberbandMelodic:
        case rubberbandPercussive:
       #endif
       #if ! TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        case signalsmithDefault:
        case signalsmithCheaper:
       #endif
            return defaultMode;
       #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        case signalsmithDefault:
        case signalsmithCheaper:
            return m;
       #endif
       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
            return m;
       #endif
       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
        case elastiqueDirectPro:
        case elastiqueDirectEfficient:
        case elastiqueDirectMobile:
            return m;
       #endif
       #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        case rubberbandMelodic:
        case rubberbandPercussive:
            return m;
       #endif
        case disabled:
        case melodyne:
        default:
            return m;
    }
}

juce::StringArray TimeStretcher::getPossibleModes (Engine& e, bool excludeMelodyne)
{
    juce::StringArray s;

   #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
    s.add (getSignalsmithDefault());
    s.add (getSignalsmithCheaper());
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
    s.add (getElastiquePro());
    s.add (getElastiqueEfficeint());
    s.add (getElastiqueMobile());
    s.add (getElastiqueMono());
    s.add (getElastiqueDirectPro());
    s.add (getElastiqueDirectEfficeint());
    s.add (getElastiqueDirectMobile());
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    s.add (getSoundTouchNormal());
    s.add (getSoundTouchBetter());
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
    s.add (getRubberBandMelodic());
    s.add (getRubberBandPercussive());
   #endif

   #if TRACKTION_ENABLE_ARA
    if (! excludeMelodyne && e.getPluginManager().getARACompatiblePlugDescriptions().size() > 0)
        s.add (getMelodyne());
   #else
    juce::ignoreUnused (e, excludeMelodyne);
   #endif

    return s;
}

TimeStretcher::Mode TimeStretcher::getModeFromName (Engine& e, const juce::String& name)
{
   #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
    if (name == getSignalsmithDefault())         return signalsmithDefault;
    if (name == getSignalsmithCheaper())         return signalsmithCheaper;
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
    if (name == getElastiquePro())              return elastiquePro;
    if (name == getElastiqueEfficeint())        return elastiqueEfficient;
    if (name == getElastiqueMobile())           return elastiqueMobile;
    if (name == getElastiqueMono())             return elastiqueMonophonic;
    if (name == getElastiqueDirectPro())        return elastiqueDirectPro;
    if (name == getElastiqueDirectEfficeint())  return elastiqueDirectEfficient;
    if (name == getElastiqueDirectMobile())     return elastiqueDirectMobile;
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
     if (name == getRubberBandMelodic())    return rubberbandMelodic;
     if (name == getRubberBandPercussive()) return rubberbandPercussive;
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    if (name == getSoundTouchNormal())      return soundtouchNormal;
    if (name == getSoundTouchBetter())      return soundtouchBetter;
   #endif

   #if TRACKTION_ENABLE_ARA
    if (name == getMelodyne())              return melodyne;
   #endif

    return getPossibleModes (e, false).contains (name) ? defaultMode
                                                       : disabled;
}

juce::String TimeStretcher::getNameOfMode (const Mode mode)
{
    switch (mode)
    {
        case elastiquePro:              return getElastiquePro();
        case elastiqueEfficient:        return getElastiqueEfficeint();
        case elastiqueMobile:           return getElastiqueMobile();
        case elastiqueMonophonic:       return getElastiqueMono();
        case elastiqueDirectPro:        return getElastiqueDirectPro();
        case elastiqueDirectEfficient:  return getElastiqueDirectEfficeint();
        case elastiqueDirectMobile:     return getElastiqueDirectMobile();
        case soundtouchNormal:          return getSoundTouchNormal();
        case soundtouchBetter:          return getSoundTouchBetter();
        case rubberbandMelodic:         return getRubberBandMelodic();
        case rubberbandPercussive:      return getRubberBandPercussive();
        case signalsmithDefault:        return getSignalsmithDefault();
        case signalsmithCheaper:        return getSignalsmithCheaper();
        case melodyne:                  return getMelodyne();
        case disabled:
        case elastiqueTransient:
        case elastiqueTonal:
        default:                        jassertfalse; break;
    }

    return {};
}

bool TimeStretcher::isMelodyne (Mode mode)
{
   #if TRACKTION_ENABLE_ARA
    return mode == melodyne;
   #else
    juce::ignoreUnused (mode);
    return false;
   #endif
}

bool TimeStretcher::isInitialised() const
{
    return stretcher != nullptr;
}

void TimeStretcher::initialise (double sourceSampleRate, int samplesPerBlock,
                                int numChannels, Mode mode, ElastiqueProOptions options, bool realtime)
{
    juce::ignoreUnused (sourceSampleRate, numChannels, mode, options, realtime);
    jassert (! isMelodyne (mode));

    samplesPerBlockRequested = samplesPerBlock;

    CRASH_TRACER
    jassert (stretcher == nullptr);

   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE || TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND || TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH || TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
    switch (mode)
    {
       #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        case signalsmithDefault:
        case signalsmithCheaper:
            stretcher = std::make_unique<SignalsmithStretcher> (sourceSampleRate, samplesPerBlock, numChannels, mode);
            break;
       #else
        case signalsmithDefault: [[fallthrough]];
        case signalsmithCheaper:
            break;
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
            stretcher = std::make_unique<ElastiqueStretcher> (sourceSampleRate, samplesPerBlock, numChannels,
                                                              mode, options, realtime ? 0.25f : 0.1f);
            break;
        case elastiqueDirectPro:
        case elastiqueDirectEfficient:
        case elastiqueDirectMobile:
            stretcher = std::make_unique<ElastiqueDirectStretcher> (sourceSampleRate, samplesPerBlock, numChannels,
                                                                    mode, options, realtime ? 0.25f : 0.1f);
            break;
       #else
        case elastiquePro:              [[fallthrough]];
        case elastiqueEfficient:        [[fallthrough]];
        case elastiqueMobile:           [[fallthrough]];
        case elastiqueMonophonic:       [[fallthrough]];
        case elastiqueDirectPro:        [[fallthrough]];
        case elastiqueDirectEfficient:  [[fallthrough]];
        case elastiqueDirectMobile:
            break;
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
            juce::ignoreUnused (options, realtime);
            stretcher = std::make_unique<SoundTouchStretcher> (sourceSampleRate, samplesPerBlock, numChannels,
                                                               mode == soundtouchBetter);
            break;
       #else
        case soundtouchNormal:      [[fallthrough]];
        case soundtouchBetter:
            break;
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        case rubberbandMelodic:
        case rubberbandPercussive:
            juce::ignoreUnused (options, realtime);
            stretcher = std::make_unique<tracktion::engine::RubberBandStretcher> (sourceSampleRate, samplesPerBlock, numChannels,
                                                                                  mode == rubberbandPercussive);
            break;
       #else
        case rubberbandMelodic:     [[fallthrough]];
        case rubberbandPercussive:
            break;
       #endif

        case disabled:              [[fallthrough]];
        case melodyne:              [[fallthrough]];
        case elastiqueTransient:    [[fallthrough]];
        case elastiqueTonal:        [[fallthrough]];
        default:
            break;
    }
   #endif

    if (stretcher != nullptr)
        if (! stretcher->isOk())
            stretcher.reset();
}

bool TimeStretcher::canProcessFor (const Mode mode)
{
    return mode != TimeStretcher::disabled;
}

void TimeStretcher::reset()
{
    if (stretcher != nullptr)
        stretcher->reset();
}

bool TimeStretcher::setSpeedAndPitch (float speedRatio, float semitonesUp)
{
    if (stretcher != nullptr)
        return stretcher->setSpeedAndPitch (speedRatio, semitonesUp);

    return false;
}

int TimeStretcher::getFramesNeeded() const
{
    if (stretcher != nullptr)
        return stretcher->getFramesNeeded();

    return 0;
}

int TimeStretcher::getMaxFramesNeeded() const
{
    if (stretcher != nullptr)
        return stretcher->getMaxFramesNeeded();

    return 0;
}

int TimeStretcher::processData (const float* const* inChannels, int numSamples, float* const* outChannels)
{
    if (stretcher != nullptr)
        return stretcher->processData (inChannels, numSamples, outChannels);

    return 0;
}

int TimeStretcher::processData (AudioFifo& inFifo, int numSamples, AudioFifo& outFifo)
{
    if (stretcher == nullptr)
        return 0;

    jassert (numSamples == stretcher->getFramesNeeded());
    AudioScratchBuffer inBuffer (inFifo.getNumChannels(), numSamples);
    AudioScratchBuffer outBuffer (outFifo.getNumChannels(), samplesPerBlockRequested);

    auto inChannels = inBuffer.buffer.getArrayOfReadPointers();
    auto outChannels = outBuffer.buffer.getArrayOfWritePointers();

    inFifo.read (inBuffer.buffer, 0, numSamples);
    const int numOutputFrames = stretcher->processData (inChannels, numSamples, outChannels);
    outFifo.write (outBuffer.buffer, 0, numOutputFrames);

    return numOutputFrames;
}

int TimeStretcher::flush (float* const* outChannels)
{
    if (stretcher != nullptr)
        return stretcher->flush (outChannels);

    return 0;
}

}} // namespace tracktion { inline namespace engine
