#pragma once

#include <JuceHeader.h>

namespace PhaseusDelayParams
{
inline constexpr const char* mode = "mode";
inline constexpr const char* wetDry = "wetDry";

inline constexpr const char* simpleTimeMs = "simpleTimeMs";
inline constexpr const char* simpleFeedback = "simpleFeedback";
inline constexpr const char* simpleSync = "simpleSync";
inline constexpr const char* simpleSyncDivision = "simpleSyncDivision";

inline constexpr const char* pingTimeLeftMs = "pingTimeLeftMs";
inline constexpr const char* pingTimeRightMs = "pingTimeRightMs";
inline constexpr const char* pingFeedbackLeft = "pingFeedbackLeft";
inline constexpr const char* pingFeedbackRight = "pingFeedbackRight";
inline constexpr const char* pingLinkTimes = "pingLinkTimes";
inline constexpr const char* pingLinkFeedback = "pingLinkFeedback";
inline constexpr const char* pingSync = "pingSync";
inline constexpr const char* pingSyncDivisionLeft = "pingSyncDivisionLeft";
inline constexpr const char* pingSyncDivisionRight = "pingSyncDivisionRight";

inline constexpr const char* grainBaseTimeMs = "grainBaseTimeMs";
inline constexpr const char* grainSizeMs = "grainSizeMs";
inline constexpr const char* grainRateHz = "grainRateHz";
inline constexpr const char* grainAmount = "grainAmount";
inline constexpr const char* grainFeedback = "grainFeedback";
inline constexpr const char* grainPingPong = "grainPingPong";
}

class PHASEUSDelayAudioProcessor final : public juce::AudioProcessor
{
public:
    PHASEUSDelayAudioProcessor();
    ~PHASEUSDelayAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int getCurrentModeIndex() const;

private:
    static constexpr double maxDelaySeconds = 4.0;

    juce::AudioBuffer<float> delayBuffer;
    int writePosition = 0;
    double currentSampleRate = 44100.0;

    int grainSamplesUntilNext = 0;
    int grainSamplesLeft = 0;
    int grainCurrentLengthSamples = 1;
    int currentGrainDelaySamples = 1;
    bool grainPanToRight = false;
    float grainWetSmoothedL = 0.0f;
    float grainWetSmoothedR = 0.0f;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PHASEUSDelayAudioProcessor)
};
