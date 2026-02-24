#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <array>
#include <cmath>

namespace
{
float clamp01(const float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

int msToSamples(const float ms, const double sampleRate, const int maxSamples)
{
    return juce::jlimit(1, maxSamples - 1, static_cast<int>(ms * 0.001 * sampleRate));
}

float divisionToQuarterNotes(const int divisionIndex)
{
    static constexpr std::array<float, 12> quarterNotes {
        4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 1.5f, 0.75f, 0.375f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f
    };

    const auto safeIndex = juce::jlimit(0, static_cast<int>(quarterNotes.size()) - 1, divisionIndex);
    return quarterNotes[static_cast<size_t>(safeIndex)];
}

float bpmSyncedMs(const double bpm, const int divisionIndex, const float fallbackMs)
{
    if (bpm <= 0.0)
        return fallbackMs;

    const auto quarterMs = static_cast<float>(60000.0 / bpm);
    return quarterMs * divisionToQuarterNotes(divisionIndex);
}

float grainWindow(const int samplesLeft, const int totalSamples)
{
    const auto safeTotal = juce::jmax(1, totalSamples);
    const auto progress = 1.0f - static_cast<float>(samplesLeft) / static_cast<float>(safeTotal);
    return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * progress);
}
} // namespace

PHASEUSDelayAudioProcessor::PHASEUSDelayAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void PHASEUSDelayAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const auto delayBufferSize = static_cast<int>(maxDelaySeconds * sampleRate) + samplesPerBlock + 1;
    delayBuffer.setSize(2, delayBufferSize);
    delayBuffer.clear();

    writePosition = 0;
    grainSamplesUntilNext = 0;
    grainSamplesLeft = 0;
    grainCurrentLengthSamples = 1;
    currentGrainDelaySamples = juce::jmax(1, static_cast<int>(0.2 * sampleRate));
    grainPanToRight = false;
    grainWetSmoothedL = 0.0f;
    grainWetSmoothedR = 0.0f;
}

void PHASEUSDelayAudioProcessor::releaseResources()
{
}

bool PHASEUSDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void PHASEUSDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() <= 0 || totalInputChannels <= 0)
        return;

    const auto mode = getCurrentModeIndex();
    const auto wetDryValue = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::wetDry)->load());

    auto simpleTimeMs = apvts.getRawParameterValue(PhaseusDelayParams::simpleTimeMs)->load();
    const auto simpleFeedback = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::simpleFeedback)->load());

    auto pingTimeLeftMs = apvts.getRawParameterValue(PhaseusDelayParams::pingTimeLeftMs)->load();
    auto pingTimeRightMs = apvts.getRawParameterValue(PhaseusDelayParams::pingTimeRightMs)->load();
    auto pingFeedbackLeft = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::pingFeedbackLeft)->load());
    auto pingFeedbackRight = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::pingFeedbackRight)->load());

    const auto pingLinkTimes = apvts.getRawParameterValue(PhaseusDelayParams::pingLinkTimes)->load() > 0.5f;
    const auto pingLinkFeedback = apvts.getRawParameterValue(PhaseusDelayParams::pingLinkFeedback)->load() > 0.5f;

    if (pingLinkTimes)
        pingTimeRightMs = pingTimeLeftMs;

    if (pingLinkFeedback)
        pingFeedbackRight = pingFeedbackLeft;

    const auto simpleSyncEnabled = apvts.getRawParameterValue(PhaseusDelayParams::simpleSync)->load() > 0.5f;
    const auto pingSyncEnabled = apvts.getRawParameterValue(PhaseusDelayParams::pingSync)->load() > 0.5f;
    const auto simpleSyncDivision = static_cast<int>(apvts.getRawParameterValue(PhaseusDelayParams::simpleSyncDivision)->load());
    auto pingSyncDivisionLeft = static_cast<int>(apvts.getRawParameterValue(PhaseusDelayParams::pingSyncDivisionLeft)->load());
    auto pingSyncDivisionRight = static_cast<int>(apvts.getRawParameterValue(PhaseusDelayParams::pingSyncDivisionRight)->load());

    if (pingLinkTimes)
        pingSyncDivisionRight = pingSyncDivisionLeft;

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;
    }

    if (simpleSyncEnabled)
        simpleTimeMs = bpmSyncedMs(bpm, simpleSyncDivision, simpleTimeMs);

    if (pingSyncEnabled)
    {
        pingTimeLeftMs = bpmSyncedMs(bpm, pingSyncDivisionLeft, pingTimeLeftMs);
        pingTimeRightMs = bpmSyncedMs(bpm, pingSyncDivisionRight, pingTimeRightMs);
    }

    const auto grainBaseTimeMs = apvts.getRawParameterValue(PhaseusDelayParams::grainBaseTimeMs)->load();
    const auto grainSizeMs = apvts.getRawParameterValue(PhaseusDelayParams::grainSizeMs)->load();
    const auto grainRateHz = juce::jmax(0.1f, apvts.getRawParameterValue(PhaseusDelayParams::grainRateHz)->load());
    const auto grainAmount = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::grainAmount)->load());
    const auto grainFeedback = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::grainFeedback)->load());
    const auto grainPingPong = apvts.getRawParameterValue(PhaseusDelayParams::grainPingPong)->load() > 0.5f;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    auto* delayLeft = delayBuffer.getWritePointer(0);
    auto* delayRight = delayBuffer.getWritePointer(1);

    const auto delayBufferSize = delayBuffer.getNumSamples();
    const auto simpleDelaySamples = msToSamples(simpleTimeMs, currentSampleRate, delayBufferSize);
    const auto pingDelayLeftSamples = msToSamples(pingTimeLeftMs, currentSampleRate, delayBufferSize);
    const auto pingDelayRightSamples = msToSamples(pingTimeRightMs, currentSampleRate, delayBufferSize);

    const auto grainBaseDelaySamples = msToSamples(grainBaseTimeMs, currentSampleRate, delayBufferSize);
    const auto grainSizeSamples = juce::jmax(1, msToSamples(grainSizeMs, currentSampleRate, delayBufferSize));
    const auto grainIntervalSamples = juce::jmax(1, static_cast<int>(currentSampleRate / grainRateHz));
    const auto grainGapSamples = juce::jmax(0, grainIntervalSamples - grainSizeSamples);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto inL = left[sample];
        const auto inR = right != nullptr ? right[sample] : inL;

        float wetL = 0.0f;
        float wetR = 0.0f;
        float writeL = inL;
        float writeR = inR;

        auto readDelayed = [&](const int channel, const int delaySamples) -> float
        {
            auto readPosition = writePosition - delaySamples;

            while (readPosition < 0)
                readPosition += delayBufferSize;

            return channel == 0 ? delayLeft[readPosition] : delayRight[readPosition];
        };

        if (mode == 0)
        {
            const auto delayedL = readDelayed(0, simpleDelaySamples);
            const auto delayedR = readDelayed(1, simpleDelaySamples);

            wetL = delayedL;
            wetR = delayedR;

            writeL = inL + delayedL * simpleFeedback;
            writeR = inR + delayedR * simpleFeedback;

            grainWetSmoothedL = wetL;
            grainWetSmoothedR = wetR;
        }
        else if (mode == 1)
        {
            const auto delayedL = readDelayed(0, pingDelayLeftSamples);
            const auto delayedR = readDelayed(1, pingDelayRightSamples);

            wetL = delayedL;
            wetR = delayedR;

            writeL = inL + delayedR * pingFeedbackLeft;
            writeR = inR + delayedL * pingFeedbackRight;

            grainWetSmoothedL = wetL;
            grainWetSmoothedR = wetR;
        }
        else
        {
            if (grainSamplesLeft <= 0 && grainSamplesUntilNext > 0)
                --grainSamplesUntilNext;

            if (grainSamplesLeft <= 0 && grainSamplesUntilNext <= 0)
            {
                grainPanToRight = !grainPanToRight;

                const auto spread = static_cast<int>(grainAmount * static_cast<float>(grainBaseDelaySamples) * 0.8f);
                const auto randomOffset = spread > 0 ? random.nextInt(spread * 2 + 1) - spread : 0;

                currentGrainDelaySamples = juce::jlimit(1, delayBufferSize - 1, grainBaseDelaySamples + randomOffset);
                grainCurrentLengthSamples = grainSizeSamples;
                grainSamplesLeft = grainCurrentLengthSamples;
                grainSamplesUntilNext = grainGapSamples;
            }

            auto delayedMono = 0.0f;
            auto grainWetMono = 0.0f;

            if (grainSamplesLeft > 0)
            {
                delayedMono = 0.5f * (readDelayed(0, currentGrainDelaySamples) + readDelayed(1, currentGrainDelaySamples));
                const auto envelope = grainWindow(grainSamplesLeft, grainCurrentLengthSamples);
                grainWetMono = delayedMono * envelope;
                --grainSamplesLeft;
            }

            if (grainPingPong)
            {
                const auto oppositeGain = 1.0f - grainAmount;

                if (grainPanToRight)
                {
                    wetL = grainWetMono * oppositeGain;
                    wetR = grainWetMono;
                }
                else
                {
                    wetL = grainWetMono;
                    wetR = grainWetMono * oppositeGain;
                }
            }
            else
            {
                wetL = grainWetMono;
                wetR = grainWetMono;
            }

            constexpr auto smoothing = 0.12f;
            grainWetSmoothedL += smoothing * (wetL - grainWetSmoothedL);
            grainWetSmoothedR += smoothing * (wetR - grainWetSmoothedR);
            wetL = grainWetSmoothedL;
            wetR = grainWetSmoothedR;

            const auto monoIn = 0.5f * (inL + inR);
            const auto monoWrite = juce::jlimit(-2.0f, 2.0f, monoIn + delayedMono * grainFeedback);
            writeL = monoWrite;
            writeR = monoWrite;
        }

        left[sample] = inL * (1.0f - wetDryValue) + wetL * wetDryValue;

        if (right != nullptr)
            right[sample] = inR * (1.0f - wetDryValue) + wetR * wetDryValue;

        delayLeft[writePosition] = writeL;
        delayRight[writePosition] = writeR;

        if (++writePosition >= delayBufferSize)
            writePosition = 0;
    }
}

juce::AudioProcessorEditor* PHASEUSDelayAudioProcessor::createEditor()
{
    return new PHASEUSDelayAudioProcessorEditor(*this);
}

bool PHASEUSDelayAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String PHASEUSDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PHASEUSDelayAudioProcessor::acceptsMidi() const
{
    return false;
}

bool PHASEUSDelayAudioProcessor::producesMidi() const
{
    return false;
}

bool PHASEUSDelayAudioProcessor::isMidiEffect() const
{
    return false;
}

double PHASEUSDelayAudioProcessor::getTailLengthSeconds() const
{
    return maxDelaySeconds;
}

int PHASEUSDelayAudioProcessor::getNumPrograms()
{
    return 1;
}

int PHASEUSDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PHASEUSDelayAudioProcessor::setCurrentProgram(int)
{
}

const juce::String PHASEUSDelayAudioProcessor::getProgramName(int)
{
    return {};
}

void PHASEUSDelayAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void PHASEUSDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void PHASEUSDelayAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState == nullptr)
        return;

    if (!xmlState->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout PHASEUSDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PhaseusDelayParams::mode,
        "Mode",
        juce::StringArray { "Simple", "PingPong", "Grain" },
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::wetDry, "Wet/Dry", 0.0f, 1.0f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::simpleTimeMs, "Simple Time", 1.0f, 2000.0f, 420.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::simpleFeedback, "Simple Feedback", 0.0f, 0.97f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::simpleSync, "Simple Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PhaseusDelayParams::simpleSyncDivision,
        "Simple Sync Division",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" },
        2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingTimeLeftMs, "Ping Time Left", 1.0f, 2000.0f, 330.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingTimeRightMs, "Ping Time Right", 1.0f, 2000.0f, 500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingFeedbackLeft, "Ping Feedback Left", 0.0f, 0.97f, 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingFeedbackRight, "Ping Feedback Right", 0.0f, 0.97f, 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::pingLinkTimes, "Sync Left/Right Time", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::pingLinkFeedback, "Sync Left/Right Feedback", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::pingSync, "Ping Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PhaseusDelayParams::pingSyncDivisionLeft,
        "Ping Sync Division Left",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" },
        3));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PhaseusDelayParams::pingSyncDivisionRight,
        "Ping Sync Division Right",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" },
        4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainBaseTimeMs, "Grain Base Time", 1.0f, 1200.0f, 300.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainSizeMs, "Grain Size", 5.0f, 250.0f, 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainRateHz, "Grain Rate", 0.25f, 24.0f, 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainAmount, "Grain Amount", 0.0f, 1.0f, 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainFeedback, "Grain Feedback", 0.0f, 0.97f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::grainPingPong, "Grain PingPong", true));

    return { params.begin(), params.end() };
}

int PHASEUSDelayAudioProcessor::getCurrentModeIndex() const
{
    return static_cast<int>(apvts.getRawParameterValue(PhaseusDelayParams::mode)->load());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PHASEUSDelayAudioProcessor();
}
