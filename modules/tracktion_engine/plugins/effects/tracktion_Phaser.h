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

class PhaserPlugin  : public Plugin
{
public:
    PhaserPlugin (PluginCreationInfo);
    ~PhaserPlugin() override;

    //==============================================================================
    static const char* getPluginName()                      { return NEEDS_TRANS("Phaser"); }
    static const char* xmlTypeName;

    juce::String getName() const override                   { return TRANS("Phaser"); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    int getNumOutputChannelsGivenInputs (int numInputChannels) override  { return juce::jmin (numInputChannels, 2); }
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> depth, rate, feedbackGain;

    /** [Mrpbm patch: plugin signal tap] The all-pass sweep at the end of the last rendered block:
        the stages' break frequency in Hz, and the LFO's place in its up-down cycle, [0, 1] with
        0 at the bottom of the sweep and 0.5 at the top. Any thread. */
    float getSweepHz() const noexcept       { return sweepHz.load (std::memory_order_relaxed); }
    float getLfoPhase() const noexcept      { return lfoPhase.load (std::memory_order_relaxed); }

private:
    //==============================================================================
    double filterVals[2][8];
    double sweep = 0.0, sweepFactor = 0.0, minSweep = 0.0;
    std::atomic<float> sweepHz { 0.0f }, lfoPhase { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaserPlugin)
};

}} // namespace tracktion { inline namespace engine
