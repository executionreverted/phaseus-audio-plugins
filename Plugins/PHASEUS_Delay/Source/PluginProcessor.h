#pragma once

#include <JuceHeader.h>
#include <array>

namespace PhaseusDelayParams
{
inline constexpr const char* mode = "mode";
inline constexpr const char* wetDry = "wetDry";
inline constexpr const char* outputGainDb = "outputGainDb";
inline constexpr const char* loFiMode = "loFiMode";
inline constexpr const char* loFiAmount = "loFiAmount";
inline constexpr const char* reverseEnable = "reverseEnable";
inline constexpr const char* reverseMix = "reverseMix";
inline constexpr const char* reverseStartOffsetMs = "reverseStartOffsetMs";
inline constexpr const char* reverseEndOffsetMs = "reverseEndOffsetMs";
inline constexpr const char* duckingEnable = "duckingEnable";
inline constexpr const char* duckingAmount = "duckingAmount";
inline constexpr const char* duckingAttackMs = "duckingAttackMs";
inline constexpr const char* duckingReleaseMs = "duckingReleaseMs";
inline constexpr const char* duckingDetectorHpHz = "duckingDetectorHpHz";
inline constexpr const char* duckingDetectorLpHz = "duckingDetectorLpHz";
inline constexpr const char* diffusionEnable = "diffusionEnable";
inline constexpr const char* diffusionAmount = "diffusionAmount";
inline constexpr const char* diffusionSizeMs = "diffusionSizeMs";
inline constexpr const char* wetWidth = "wetWidth";

inline constexpr const char* simpleTimeMs = "simpleTimeMs";
inline constexpr const char* simpleFeedback = "simpleFeedback";
inline constexpr const char* simpleSync = "simpleSync";
inline constexpr const char* simpleSyncDivision = "simpleSyncDivision";
inline constexpr const char* simpleTimeRandomPct = "simpleTimeRandomPct";
inline constexpr const char* simpleFeedbackRandomPct = "simpleFeedbackRandomPct";

inline constexpr const char* pingTimeLeftMs = "pingTimeLeftMs";
inline constexpr const char* pingTimeRightMs = "pingTimeRightMs";
inline constexpr const char* pingFeedbackLeft = "pingFeedbackLeft";
inline constexpr const char* pingFeedbackRight = "pingFeedbackRight";
inline constexpr const char* pingLinkTimes = "pingLinkTimes";
inline constexpr const char* pingLinkFeedback = "pingLinkFeedback";
inline constexpr const char* pingSync = "pingSync";
inline constexpr const char* pingSyncDivisionLeft = "pingSyncDivisionLeft";
inline constexpr const char* pingSyncDivisionRight = "pingSyncDivisionRight";
inline constexpr const char* pingTimeLeftRandomPct = "pingTimeLeftRandomPct";
inline constexpr const char* pingTimeRightRandomPct = "pingTimeRightRandomPct";
inline constexpr const char* pingFeedbackLeftRandomPct = "pingFeedbackLeftRandomPct";
inline constexpr const char* pingFeedbackRightRandomPct = "pingFeedbackRightRandomPct";

inline constexpr const char* grainBaseTimeMs = "grainBaseTimeMs";
inline constexpr const char* grainSizeMs = "grainSizeMs";
inline constexpr const char* grainRateHz = "grainRateHz";
inline constexpr const char* grainPitchSemitones = "grainPitchSemitones";
inline constexpr const char* grainAmount = "grainAmount";
inline constexpr const char* grainFeedback = "grainFeedback";
inline constexpr const char* grainPingPong = "grainPingPong";
inline constexpr const char* grainPingPongPan = "grainPingPongPan";
inline constexpr const char* grainBaseTimeRandomPct = "grainBaseTimeRandomPct";
inline constexpr const char* grainSizeRandomPct = "grainSizeRandomPct";
inline constexpr const char* grainRateRandomPct = "grainRateRandomPct";
inline constexpr const char* grainPitchRandomPct = "grainPitchRandomPct";
inline constexpr const char* grainAmountRandomPct = "grainAmountRandomPct";
inline constexpr const char* grainFeedbackRandomPct = "grainFeedbackRandomPct";

inline constexpr const char* filterType = "filterType";
inline constexpr const char* filterCutoffHz = "filterCutoffHz";
inline constexpr const char* filterQ = "filterQ";
inline constexpr const char* filterMix = "filterMix";
inline constexpr const char* filterCombMs = "filterCombMs";
inline constexpr const char* filterCombFeedback = "filterCombFeedback";
inline constexpr const char* filter1Enable = "filter1Enable";
inline constexpr const char* filter2Enable = "filter2Enable";
inline constexpr const char* filter1Input = "filter1Input";
inline constexpr const char* filter2Input = "filter2Input";
inline constexpr const char* filter1Route = "filter1Route";
inline constexpr const char* filter2Type = "filter2Type";
inline constexpr const char* filter2CutoffHz = "filter2CutoffHz";
inline constexpr const char* filter2Q = "filter2Q";
inline constexpr const char* filter2Mix = "filter2Mix";
inline constexpr const char* filter2CombMs = "filter2CombMs";
inline constexpr const char* filter2CombFeedback = "filter2CombFeedback";
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
    static constexpr int maxGrainVoices = 8;

    struct GrainVoice
    {
        bool active = false;
        int samplesLeft = 0;
        int totalSamples = 1;
        float readPosition = 0.0f;
        float pitchRatio = 1.0f;
        float amount = 0.0f;
        float feedback = 0.0f;
        float pan = 0.0f;
    };

    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> reverseBuffer;
    int writePosition = 0;
    double currentSampleRate = 44100.0;

    int grainSamplesUntilNext = 0;
    int currentGrainDelaySamples = 1;
    std::array<GrainVoice, maxGrainVoices> grainVoices;
    int nextGrainVoiceIndex = 0;
    bool grainPanToRight = false;
    float grainWetSmoothedL = 0.0f;
    float grainWetSmoothedR = 0.0f;
    juce::Random random;

    juce::IIRFilter filterL;
    juce::IIRFilter filterR;
    juce::IIRFilter filter2L;
    juce::IIRFilter filter2R;
    juce::AudioBuffer<float> combBuffer;
    juce::AudioBuffer<float> combBuffer2;
    int combWritePosition = 0;
    int combWritePosition2 = 0;
    juce::AudioBuffer<float> diffusionBufferA;
    juce::AudioBuffer<float> diffusionBufferB;
    int diffusionWriteA = 0;
    int diffusionWriteB = 0;
    int loFiDownsampleCounter = 0;
    float loFiHeldWetL = 0.0f;
    float loFiHeldWetR = 0.0f;

    int simpleTimeRandomCounter = 0;
    int pingTimeLeftRandomCounter = 0;
    int pingTimeRightRandomCounter = 0;
    int simpleFeedbackRandomCounter = 0;
    int pingFeedbackLeftRandomCounter = 0;
    int pingFeedbackRightRandomCounter = 0;
    float simpleHeldRandomTimeMs = 420.0f;
    float pingHeldRandomTimeLeftMs = 330.0f;
    float pingHeldRandomTimeRightMs = 500.0f;
    float simpleHeldRandomFeedback = 0.35f;
    float pingHeldRandomFeedbackLeft = 0.40f;
    float pingHeldRandomFeedbackRight = 0.40f;
    float duckEnvelope = 0.0f;
    float duckDetectorHpState = 0.0f;
    float duckDetectorLpState = 0.0f;
    int reverseSamplesRemaining = 0;
    int reverseLengthSamples = 1;
    int reverseAnchor = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PHASEUSDelayAudioProcessor)
};
