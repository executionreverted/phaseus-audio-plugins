#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
float clamp01(const float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float clampPct(const float value)
{
    return juce::jlimit(0.0f, 100.0f, value);
}

float quantizeToBitDepth(const float value, const int bits)
{
    const auto levels = static_cast<float>((1 << juce::jmax(1, bits)) - 1);
    const auto mapped = juce::jlimit(-1.0f, 1.0f, value) * 0.5f + 0.5f;
    const auto quantized = std::round(mapped * levels) / levels;
    return quantized * 2.0f - 1.0f;
}

int msToSamples(const float ms, const double sampleRate, const int maxSamples)
{
    return juce::jlimit(1, maxSamples - 1, static_cast<int>(ms * 0.001 * sampleRate));
}

float applyPercentRandom(const float baseValue, const float randomPct, juce::Random& random)
{
    const auto spread = std::abs(baseValue) * clampPct(randomPct) * 0.01f;
    if (spread <= 0.0f)
        return baseValue;

    return baseValue + random.nextFloat() * spread * 2.0f - spread;
}

float applyDirectedIntervalRandom(const float baseValue, const float intervalSemitones, juce::Random& random)
{
    if (intervalSemitones == 0.0f)
        return baseValue;

    return baseValue + intervalSemitones * random.nextFloat();
}

float semitoneToRatio(const float semitones)
{
    return std::pow(2.0f, semitones / 12.0f);
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
    reverseBuffer.setSize(2, delayBufferSize);
    reverseBuffer.clear();

    writePosition = 0;
    grainSamplesUntilNext = 0;
    grainSamplesLeft = 0;
    grainCurrentLengthSamples = 1;
    currentGrainDelaySamples = juce::jmax(1, static_cast<int>(0.2 * sampleRate));
    currentGrainReadPosition = 0.0f;
    currentGrainPitchRatio = 1.0f;
    currentGrainFeedback = 0.35f;
    currentGrainAmount = 0.5f;
    grainPanToRight = false;
    grainWetSmoothedL = 0.0f;
    grainWetSmoothedR = 0.0f;

    filterL.reset();
    filterR.reset();
    combBuffer.setSize(2, delayBufferSize);
    combBuffer.clear();
    combWritePosition = 0;
    const auto diffusionBufferSize = juce::jmax(32, static_cast<int>(0.12 * sampleRate) + samplesPerBlock + 1);
    diffusionBufferA.setSize(2, diffusionBufferSize);
    diffusionBufferB.setSize(2, diffusionBufferSize);
    diffusionBufferA.clear();
    diffusionBufferB.clear();
    diffusionWriteA = 0;
    diffusionWriteB = 0;
    loFiDownsampleCounter = 0;
    loFiHeldWetL = 0.0f;
    loFiHeldWetR = 0.0f;
    simpleTimeRandomCounter = 0;
    pingTimeLeftRandomCounter = 0;
    pingTimeRightRandomCounter = 0;
    simpleFeedbackRandomCounter = 0;
    pingFeedbackLeftRandomCounter = 0;
    pingFeedbackRightRandomCounter = 0;
    simpleHeldRandomTimeMs = 420.0f;
    pingHeldRandomTimeLeftMs = 330.0f;
    pingHeldRandomTimeRightMs = 500.0f;
    simpleHeldRandomFeedback = 0.35f;
    pingHeldRandomFeedbackLeft = 0.40f;
    pingHeldRandomFeedbackRight = 0.40f;
    duckEnvelope = 0.0f;
    reverseSamplesRemaining = 0;
    reverseLengthSamples = 1;
    reverseAnchor = 0;
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
    const auto outputGainLinear = juce::Decibels::decibelsToGain(apvts.getRawParameterValue(PhaseusDelayParams::outputGainDb)->load());
    const auto loFiEnabled = apvts.getRawParameterValue(PhaseusDelayParams::loFiMode)->load() > 0.5f;
    const auto loFiAmount = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::loFiAmount)->load());
    const auto reverseEnabled = apvts.getRawParameterValue(PhaseusDelayParams::reverseEnable)->load() > 0.5f;
    const auto reverseMix = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::reverseMix)->load());
    const auto reverseStartOffsetMs = juce::jmax(0.0f, apvts.getRawParameterValue(PhaseusDelayParams::reverseStartOffsetMs)->load());
    const auto reverseEndOffsetMs = juce::jmax(0.0f, apvts.getRawParameterValue(PhaseusDelayParams::reverseEndOffsetMs)->load());
    const auto duckingEnabled = apvts.getRawParameterValue(PhaseusDelayParams::duckingEnable)->load() > 0.5f;
    const auto duckingAmount = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::duckingAmount)->load());
    const auto duckingAttackMs = juce::jmax(1.0f, apvts.getRawParameterValue(PhaseusDelayParams::duckingAttackMs)->load());
    const auto duckingReleaseMs = juce::jmax(5.0f, apvts.getRawParameterValue(PhaseusDelayParams::duckingReleaseMs)->load());
    const auto diffusionEnabled = apvts.getRawParameterValue(PhaseusDelayParams::diffusionEnable)->load() > 0.5f;
    const auto diffusionAmount = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::diffusionAmount)->load());
    const auto diffusionSizeMs = juce::jmax(2.0f, apvts.getRawParameterValue(PhaseusDelayParams::diffusionSizeMs)->load());

    auto simpleTimeMs = apvts.getRawParameterValue(PhaseusDelayParams::simpleTimeMs)->load();
    const auto simpleFeedback = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::simpleFeedback)->load());
    const auto simpleTimeRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::simpleTimeRandomPct)->load();
    const auto simpleFeedbackRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::simpleFeedbackRandomPct)->load();

    auto pingTimeLeftMs = apvts.getRawParameterValue(PhaseusDelayParams::pingTimeLeftMs)->load();
    auto pingTimeRightMs = apvts.getRawParameterValue(PhaseusDelayParams::pingTimeRightMs)->load();
    auto pingFeedbackLeft = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::pingFeedbackLeft)->load());
    auto pingFeedbackRight = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::pingFeedbackRight)->load());
    const auto pingTimeLeftRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::pingTimeLeftRandomPct)->load();
    const auto pingTimeRightRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::pingTimeRightRandomPct)->load();
    const auto pingFeedbackLeftRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::pingFeedbackLeftRandomPct)->load();
    const auto pingFeedbackRightRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::pingFeedbackRightRandomPct)->load();

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

    simpleTimeMs = juce::jmax(1.0f, simpleTimeMs);
    auto simpleFeedbackBlock = simpleFeedback;

    pingTimeLeftMs = juce::jmax(1.0f, pingTimeLeftMs);
    pingTimeRightMs = juce::jmax(1.0f, pingTimeRightMs);

    auto pingFeedbackLeftBlock = pingFeedbackLeft;
    auto pingFeedbackRightBlock = pingFeedbackRight;

    const auto grainBaseTimeMs = apvts.getRawParameterValue(PhaseusDelayParams::grainBaseTimeMs)->load();
    const auto grainSizeMs = apvts.getRawParameterValue(PhaseusDelayParams::grainSizeMs)->load();
    const auto grainRateHz = juce::jmax(0.1f, apvts.getRawParameterValue(PhaseusDelayParams::grainRateHz)->load());
    const auto grainPitchSemitones = apvts.getRawParameterValue(PhaseusDelayParams::grainPitchSemitones)->load();
    const auto grainAmount = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::grainAmount)->load());
    const auto grainFeedback = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::grainFeedback)->load());
    const auto grainPingPong = apvts.getRawParameterValue(PhaseusDelayParams::grainPingPong)->load() > 0.5f;
    const auto grainPingPongPan = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::grainPingPongPan)->load());
    const auto grainBaseTimeRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::grainBaseTimeRandomPct)->load();
    const auto grainSizeRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::grainSizeRandomPct)->load();
    const auto grainRateRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::grainRateRandomPct)->load();
    const auto grainPitchRandomInterval = apvts.getRawParameterValue(PhaseusDelayParams::grainPitchRandomPct)->load();
    const auto grainAmountRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::grainAmountRandomPct)->load();
    const auto grainFeedbackRandomPct = apvts.getRawParameterValue(PhaseusDelayParams::grainFeedbackRandomPct)->load();

    const auto filterType = static_cast<int>(apvts.getRawParameterValue(PhaseusDelayParams::filterType)->load());
    const auto filterCutoffHz = apvts.getRawParameterValue(PhaseusDelayParams::filterCutoffHz)->load();
    const auto filterQ = apvts.getRawParameterValue(PhaseusDelayParams::filterQ)->load();
    const auto filterMix = clamp01(apvts.getRawParameterValue(PhaseusDelayParams::filterMix)->load());
    const auto filterCombMs = apvts.getRawParameterValue(PhaseusDelayParams::filterCombMs)->load();
    const auto filterCombFeedback = juce::jlimit(0.0f, 0.97f, apvts.getRawParameterValue(PhaseusDelayParams::filterCombFeedback)->load());

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    auto* delayLeft = delayBuffer.getWritePointer(0);
    auto* delayRight = delayBuffer.getWritePointer(1);
    auto* reverseLeft = reverseBuffer.getWritePointer(0);
    auto* reverseRight = reverseBuffer.getWritePointer(1);
    auto* combLeft = combBuffer.getWritePointer(0);
    auto* combRight = combBuffer.getWritePointer(1);
    auto* diffusionALeft = diffusionBufferA.getWritePointer(0);
    auto* diffusionARight = diffusionBufferA.getWritePointer(1);
    auto* diffusionBLeft = diffusionBufferB.getWritePointer(0);
    auto* diffusionBRight = diffusionBufferB.getWritePointer(1);

    const auto delayBufferSize = delayBuffer.getNumSamples();
    const auto baseSimpleDelaySamples = msToSamples(simpleTimeMs, currentSampleRate, delayBufferSize);
    const auto basePingDelayLeftSamples = msToSamples(pingTimeLeftMs, currentSampleRate, delayBufferSize);
    const auto basePingDelayRightSamples = msToSamples(pingTimeRightMs, currentSampleRate, delayBufferSize);

    const auto combDelaySamples = msToSamples(filterCombMs, currentSampleRate, combBuffer.getNumSamples());
    const auto diffusionDelayA = msToSamples(diffusionSizeMs, currentSampleRate, diffusionBufferA.getNumSamples());
    const auto diffusionDelayAL = juce::jlimit(1, diffusionBufferA.getNumSamples() - 1, diffusionDelayA);
    const auto diffusionDelayAR = juce::jlimit(1, diffusionBufferA.getNumSamples() - 1, diffusionDelayA + 11);
    const auto diffusionDelayB = msToSamples(diffusionSizeMs * 0.63f + 3.0f, currentSampleRate, diffusionBufferB.getNumSamples());
    const auto diffusionDelayBL = juce::jlimit(1, diffusionBufferB.getNumSamples() - 1, diffusionDelayB);
    const auto diffusionDelayBR = juce::jlimit(1, diffusionBufferB.getNumSamples() - 1, diffusionDelayB + 17);
    const auto diffusionFeedback = juce::jmap(diffusionAmount, 0.16f, 0.76f);

    if (filterType == 1)
    {
        filterL.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, filterCutoffHz, filterQ));
        filterR.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, filterCutoffHz, filterQ));
    }
    else if (filterType == 2)
    {
        filterL.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, filterCutoffHz, filterQ));
        filterR.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, filterCutoffHz, filterQ));
    }
    else if (filterType == 3)
    {
        filterL.setCoefficients(juce::IIRCoefficients::makeNotchFilter(currentSampleRate, filterCutoffHz, filterQ));
        filterR.setCoefficients(juce::IIRCoefficients::makeNotchFilter(currentSampleRate, filterCutoffHz, filterQ));
    }

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

        auto readMonoInterpolated = [&](float readPosition) -> float
        {
            while (readPosition < 0.0f)
                readPosition += static_cast<float>(delayBufferSize);

            while (readPosition >= static_cast<float>(delayBufferSize))
                readPosition -= static_cast<float>(delayBufferSize);

            const auto indexA = static_cast<int>(readPosition);
            const auto indexB = (indexA + 1) % delayBufferSize;
            const auto frac = readPosition - static_cast<float>(indexA);

            const auto monoA = 0.5f * (delayLeft[indexA] + delayRight[indexA]);
            const auto monoB = 0.5f * (delayLeft[indexB] + delayRight[indexB]);
            return monoA + (monoB - monoA) * frac;
        };

        auto currentSimpleDelaySamples = baseSimpleDelaySamples;
        auto currentPingDelayLeftSamples = basePingDelayLeftSamples;
        auto currentPingDelayRightSamples = basePingDelayRightSamples;

        if (mode == 0)
        {
            if (simpleTimeRandomPct > 0.0f)
            {
                if (simpleTimeRandomCounter <= 0)
                {
                    simpleHeldRandomTimeMs = juce::jmax(1.0f, applyPercentRandom(simpleTimeMs, simpleTimeRandomPct, random));
                    simpleTimeRandomCounter = msToSamples(simpleHeldRandomTimeMs, currentSampleRate, delayBufferSize);
                }
                --simpleTimeRandomCounter;
                currentSimpleDelaySamples = msToSamples(simpleHeldRandomTimeMs, currentSampleRate, delayBufferSize);
            }
            else
            {
                simpleHeldRandomTimeMs = simpleTimeMs;
                simpleTimeRandomCounter = baseSimpleDelaySamples;
            }

            if (simpleFeedbackRandomPct > 0.0f)
            {
                if (simpleFeedbackRandomCounter <= 0)
                {
                    simpleHeldRandomFeedback = juce::jlimit(0.0f, 0.97f, applyPercentRandom(simpleFeedback, simpleFeedbackRandomPct, random));
                    simpleFeedbackRandomCounter = currentSimpleDelaySamples;
                }
                --simpleFeedbackRandomCounter;
                simpleFeedbackBlock = simpleHeldRandomFeedback;
            }
            else
            {
                simpleHeldRandomFeedback = simpleFeedback;
                simpleFeedbackRandomCounter = currentSimpleDelaySamples;
                simpleFeedbackBlock = simpleFeedback;
            }

            const auto delayedL = readDelayed(0, currentSimpleDelaySamples);
            const auto delayedR = readDelayed(1, currentSimpleDelaySamples);

            wetL = delayedL;
            wetR = delayedR;

            writeL = inL + delayedL * simpleFeedbackBlock;
            writeR = inR + delayedR * simpleFeedbackBlock;

            grainWetSmoothedL = wetL;
            grainWetSmoothedR = wetR;
        }
        else if (mode == 1)
        {
            if (pingTimeLeftRandomPct > 0.0f)
            {
                if (pingTimeLeftRandomCounter <= 0)
                {
                    pingHeldRandomTimeLeftMs = juce::jmax(1.0f, applyPercentRandom(pingTimeLeftMs, pingTimeLeftRandomPct, random));
                    pingTimeLeftRandomCounter = msToSamples(pingHeldRandomTimeLeftMs, currentSampleRate, delayBufferSize);
                }
                --pingTimeLeftRandomCounter;
                currentPingDelayLeftSamples = msToSamples(pingHeldRandomTimeLeftMs, currentSampleRate, delayBufferSize);
            }
            else
            {
                pingHeldRandomTimeLeftMs = pingTimeLeftMs;
                pingTimeLeftRandomCounter = basePingDelayLeftSamples;
            }

            if (pingLinkTimes)
            {
                pingHeldRandomTimeRightMs = pingHeldRandomTimeLeftMs;
                pingTimeRightRandomCounter = pingTimeLeftRandomCounter;
                currentPingDelayRightSamples = currentPingDelayLeftSamples;
            }
            else if (pingTimeRightRandomPct > 0.0f)
            {
                if (pingTimeRightRandomCounter <= 0)
                {
                    pingHeldRandomTimeRightMs = juce::jmax(1.0f, applyPercentRandom(pingTimeRightMs, pingTimeRightRandomPct, random));
                    pingTimeRightRandomCounter = msToSamples(pingHeldRandomTimeRightMs, currentSampleRate, delayBufferSize);
                }
                --pingTimeRightRandomCounter;
                currentPingDelayRightSamples = msToSamples(pingHeldRandomTimeRightMs, currentSampleRate, delayBufferSize);
            }
            else
            {
                pingHeldRandomTimeRightMs = pingTimeRightMs;
                pingTimeRightRandomCounter = basePingDelayRightSamples;
            }

            if (pingFeedbackLeftRandomPct > 0.0f)
            {
                if (pingFeedbackLeftRandomCounter <= 0)
                {
                    pingHeldRandomFeedbackLeft = juce::jlimit(0.0f, 0.97f, applyPercentRandom(pingFeedbackLeft, pingFeedbackLeftRandomPct, random));
                    pingFeedbackLeftRandomCounter = currentPingDelayLeftSamples;
                }
                --pingFeedbackLeftRandomCounter;
                pingFeedbackLeftBlock = pingHeldRandomFeedbackLeft;
            }
            else
            {
                pingHeldRandomFeedbackLeft = pingFeedbackLeft;
                pingFeedbackLeftRandomCounter = currentPingDelayLeftSamples;
                pingFeedbackLeftBlock = pingFeedbackLeft;
            }

            if (pingLinkFeedback)
            {
                pingHeldRandomFeedbackRight = pingHeldRandomFeedbackLeft;
                pingFeedbackRightRandomCounter = pingFeedbackLeftRandomCounter;
                pingFeedbackRightBlock = pingFeedbackLeftBlock;
            }
            else if (pingFeedbackRightRandomPct > 0.0f)
            {
                if (pingFeedbackRightRandomCounter <= 0)
                {
                    pingHeldRandomFeedbackRight = juce::jlimit(0.0f, 0.97f, applyPercentRandom(pingFeedbackRight, pingFeedbackRightRandomPct, random));
                    pingFeedbackRightRandomCounter = currentPingDelayRightSamples;
                }
                --pingFeedbackRightRandomCounter;
                pingFeedbackRightBlock = pingHeldRandomFeedbackRight;
            }
            else
            {
                pingHeldRandomFeedbackRight = pingFeedbackRight;
                pingFeedbackRightRandomCounter = currentPingDelayRightSamples;
                pingFeedbackRightBlock = pingFeedbackRight;
            }

            const auto delayedL = readDelayed(0, currentPingDelayLeftSamples);
            const auto delayedR = readDelayed(1, currentPingDelayRightSamples);

            wetL = delayedL;
            wetR = delayedR;

            writeL = inL + delayedR * pingFeedbackLeftBlock;
            writeR = inR + delayedL * pingFeedbackRightBlock;

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

                const auto randomBaseMs = juce::jmax(1.0f, applyPercentRandom(grainBaseTimeMs, grainBaseTimeRandomPct, random));
                const auto randomSizeMs = juce::jmax(1.0f, applyPercentRandom(grainSizeMs, grainSizeRandomPct, random));
                const auto randomRateHz = juce::jmax(0.1f, applyPercentRandom(grainRateHz, grainRateRandomPct, random));
                const auto randomPitchSemitones = juce::jlimit(-24.0f, 24.0f, applyDirectedIntervalRandom(grainPitchSemitones, grainPitchRandomInterval, random));
                const auto randomAmount = clamp01(applyPercentRandom(grainAmount, grainAmountRandomPct, random));
                const auto randomFeedback = juce::jlimit(0.0f, 0.97f, applyPercentRandom(grainFeedback, grainFeedbackRandomPct, random));

                currentGrainDelaySamples = msToSamples(randomBaseMs, currentSampleRate, delayBufferSize);
                grainCurrentLengthSamples = juce::jmax(1, msToSamples(randomSizeMs, currentSampleRate, delayBufferSize));
                grainSamplesLeft = grainCurrentLengthSamples;

                const auto intervalSamples = juce::jmax(1, static_cast<int>(currentSampleRate / randomRateHz));
                grainSamplesUntilNext = juce::jmax(0, intervalSamples - grainCurrentLengthSamples);
                currentGrainAmount = randomAmount;
                currentGrainFeedback = randomFeedback;
                currentGrainPitchRatio = semitoneToRatio(randomPitchSemitones);

                currentGrainReadPosition = static_cast<float>(writePosition - currentGrainDelaySamples);
                while (currentGrainReadPosition < 0.0f)
                    currentGrainReadPosition += static_cast<float>(delayBufferSize);
            }

            auto delayedMono = 0.0f;
            auto grainWetMono = 0.0f;

            if (grainSamplesLeft > 0)
            {
                delayedMono = readMonoInterpolated(currentGrainReadPosition);
                const auto envelope = grainWindow(grainSamplesLeft, grainCurrentLengthSamples);
                grainWetMono = delayedMono * envelope * currentGrainAmount;
                --grainSamplesLeft;

                currentGrainReadPosition += currentGrainPitchRatio;
                if (currentGrainReadPosition >= static_cast<float>(delayBufferSize))
                    currentGrainReadPosition -= static_cast<float>(delayBufferSize);
            }

            if (grainPingPong)
            {
                const auto pan = grainPanToRight ? grainPingPongPan : -grainPingPongPan;
                const auto gainL = std::sqrt(0.5f * (1.0f - pan));
                const auto gainR = std::sqrt(0.5f * (1.0f + pan));
                wetL = grainWetMono * gainL;
                wetR = grainWetMono * gainR;
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
            const auto fbScale = (0.25f + 0.75f * currentGrainAmount);
            auto feedbackSignal = delayedMono * currentGrainFeedback * fbScale;
            if (std::abs(feedbackSignal) < 1.0e-6f)
                feedbackSignal = 0.0f;

            auto monoWrite = monoIn + feedbackSignal;
            if (std::abs(monoWrite) < 1.0e-6f)
                monoWrite = 0.0f;
            monoWrite = juce::jlimit(-2.0f, 2.0f, monoWrite);
            writeL = monoWrite;
            writeR = monoWrite;
        }

        auto wetProcL = wetL;
        auto wetProcR = wetR;

        if (reverseEnabled && reverseMix > 0.0f)
        {
            int reverseTargetLength = baseSimpleDelaySamples;
            if (mode == 1)
                reverseTargetLength = (basePingDelayLeftSamples + basePingDelayRightSamples) / 2;
            else if (mode == 2)
                reverseTargetLength = juce::jmax(1, currentGrainDelaySamples);

            reverseTargetLength = juce::jlimit(1, delayBufferSize - 1, reverseTargetLength);

            const auto reverseStartOffsetSamplesRaw = msToSamples(reverseStartOffsetMs, currentSampleRate, delayBufferSize);
            const auto reverseEndOffsetSamplesRaw = msToSamples(reverseEndOffsetMs, currentSampleRate, delayBufferSize);
            const auto reverseStartOffsetSamples = juce::jlimit(0, reverseTargetLength - 1, reverseStartOffsetSamplesRaw);
            const auto reverseEndOffsetSamples = juce::jlimit(0, reverseTargetLength - 1, reverseEndOffsetSamplesRaw);
            const auto reverseUsableLength = juce::jmax(1, reverseTargetLength - reverseStartOffsetSamples - reverseEndOffsetSamples);

            if (reverseSamplesRemaining <= 0 || std::abs(reverseUsableLength - reverseLengthSamples) > 48)
            {
                reverseLengthSamples = reverseUsableLength;
                reverseSamplesRemaining = reverseLengthSamples;
                reverseAnchor = writePosition - reverseStartOffsetSamples;
                while (reverseAnchor < 0)
                    reverseAnchor += delayBufferSize;
                reverseAnchor %= delayBufferSize;
            }

            const auto reverseIndex = reverseLengthSamples - reverseSamplesRemaining;
            auto reverseRead = reverseAnchor - reverseIndex;
            while (reverseRead < 0)
                reverseRead += delayBufferSize;
            reverseRead %= delayBufferSize;

            const auto reverseL = reverseLeft[reverseRead];
            const auto reverseR = reverseRight[reverseRead];

            const auto fadeSamples = juce::jmax(8, juce::jmin(128, reverseLengthSamples / 8));
            float reverseEnv = 1.0f;
            if (reverseIndex < fadeSamples)
                reverseEnv = static_cast<float>(reverseIndex) / static_cast<float>(fadeSamples);
            else if (reverseLengthSamples - reverseIndex < fadeSamples)
                reverseEnv = static_cast<float>(reverseLengthSamples - reverseIndex) / static_cast<float>(fadeSamples);
            reverseEnv = juce::jlimit(0.0f, 1.0f, reverseEnv);

            wetProcL = wetProcL * (1.0f - reverseMix) + reverseL * reverseMix * reverseEnv;
            wetProcR = wetProcR * (1.0f - reverseMix) + reverseR * reverseMix * reverseEnv;

            --reverseSamplesRemaining;
        }

        if (diffusionEnabled && diffusionAmount > 0.0f)
        {
            auto processAllpass = [](const float x, float* data, const int size, const int write, const int delay, const float g) -> float
            {
                auto read = write - delay;
                while (read < 0)
                    read += size;
                read %= size;

                const auto delayed = data[read];
                const auto y = -g * x + delayed;
                data[write] = x + g * delayed;
                return y;
            };

            wetProcL = processAllpass(wetProcL, diffusionALeft, diffusionBufferA.getNumSamples(), diffusionWriteA, diffusionDelayAL, diffusionFeedback);
            wetProcR = processAllpass(wetProcR, diffusionARight, diffusionBufferA.getNumSamples(), diffusionWriteA, diffusionDelayAR, diffusionFeedback);
            wetProcL = processAllpass(wetProcL, diffusionBLeft, diffusionBufferB.getNumSamples(), diffusionWriteB, diffusionDelayBL, diffusionFeedback);
            wetProcR = processAllpass(wetProcR, diffusionBRight, diffusionBufferB.getNumSamples(), diffusionWriteB, diffusionDelayBR, diffusionFeedback);
        }

        if (duckingEnabled && duckingAmount > 0.0f)
        {
            const auto detector = juce::jlimit(0.0f, 1.0f, std::max(std::abs(inL), std::abs(inR)));
            const auto attackCoeff = std::exp(-1.0f / (duckingAttackMs * 0.001f * static_cast<float>(currentSampleRate)));
            const auto releaseCoeff = std::exp(-1.0f / (duckingReleaseMs * 0.001f * static_cast<float>(currentSampleRate)));

            if (detector > duckEnvelope)
                duckEnvelope = detector + attackCoeff * (duckEnvelope - detector);
            else
                duckEnvelope = detector + releaseCoeff * (duckEnvelope - detector);

            const auto duckGain = juce::jlimit(0.0f, 1.0f, 1.0f - duckingAmount * juce::jlimit(0.0f, 1.0f, duckEnvelope * 1.4f));
            wetProcL *= duckGain;
            wetProcR *= duckGain;
        }

        if (loFiEnabled)
        {
            const auto downsampleFactor = juce::jmax(1, static_cast<int>(std::round(currentSampleRate / 12000.0)));
            constexpr int loFiBits = 8;

            if (loFiDownsampleCounter <= 0)
            {
                loFiHeldWetL = quantizeToBitDepth(wetProcL, loFiBits);
                loFiHeldWetR = quantizeToBitDepth(wetProcR, loFiBits);
                loFiDownsampleCounter = downsampleFactor;
            }

            wetProcL = wetProcL * (1.0f - loFiAmount) + loFiHeldWetL * loFiAmount;
            wetProcR = wetProcR * (1.0f - loFiAmount) + loFiHeldWetR * loFiAmount;
            --loFiDownsampleCounter;
        }

        if (filterType == 1 || filterType == 2 || filterType == 3)
        {
            const auto fL = filterL.processSingleSampleRaw(wetProcL);
            const auto fR = filterR.processSingleSampleRaw(wetProcR);
            wetProcL = wetProcL * (1.0f - filterMix) + fL * filterMix;
            wetProcR = wetProcR * (1.0f - filterMix) + fR * filterMix;
        }
        else if (filterType == 4)
        {
            auto combRead = combWritePosition - combDelaySamples;
            while (combRead < 0)
                combRead += combBuffer.getNumSamples();

            const auto delayedL = combLeft[combRead];
            const auto delayedR = combRight[combRead];
            combLeft[combWritePosition] = wetProcL + delayedL * filterCombFeedback;
            combRight[combWritePosition] = wetProcR + delayedR * filterCombFeedback;

            const auto combL = wetProcL + delayedL * 0.7f;
            const auto combR = wetProcR + delayedR * 0.7f;
            wetProcL = wetProcL * (1.0f - filterMix) + combL * filterMix;
            wetProcR = wetProcR * (1.0f - filterMix) + combR * filterMix;
        }

        auto outL = inL * (1.0f - wetDryValue) + wetProcL * wetDryValue;
        auto outR = inR * (1.0f - wetDryValue) + wetProcR * wetDryValue;
        outL *= outputGainLinear;
        outR *= outputGainLinear;

        left[sample] = outL;

        if (right != nullptr)
            right[sample] = outR;

        reverseLeft[writePosition] = wetL;
        reverseRight[writePosition] = wetR;
        delayLeft[writePosition] = writeL;
        delayRight[writePosition] = writeR;

        if (++writePosition >= delayBufferSize)
            writePosition = 0;

        if (++combWritePosition >= combBuffer.getNumSamples())
            combWritePosition = 0;

        if (++diffusionWriteA >= diffusionBufferA.getNumSamples())
            diffusionWriteA = 0;

        if (++diffusionWriteB >= diffusionBufferB.getNumSamples())
            diffusionWriteB = 0;
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::outputGainDb, "Output Gain", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::loFiMode, "LoFi Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::loFiAmount, "LoFi Amount", 0.0f, 1.0f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::reverseEnable, "Reverse Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::reverseMix, "Reverse Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::reverseStartOffsetMs, "Reverse Start Offset", 0.0f, 250.0f, 24.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::reverseEndOffsetMs, "Reverse End Offset", 0.0f, 250.0f, 8.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::duckingEnable, "Ducking Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::duckingAmount, "Ducking Amount", 0.0f, 1.0f, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::duckingAttackMs, "Ducking Attack", 1.0f, 150.0f, 16.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::duckingReleaseMs, "Ducking Release", 20.0f, 800.0f, 220.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::diffusionEnable, "Diffusion Enable", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::diffusionAmount, "Diffusion Amount", 0.0f, 1.0f, 0.30f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::diffusionSizeMs, "Diffusion Size", 2.0f, 80.0f, 24.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::simpleTimeMs, "Simple Time", 1.0f, 2000.0f, 420.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::simpleFeedback, "Simple Feedback", 0.0f, 0.97f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::simpleTimeRandomPct, "Simple Time Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::simpleFeedbackRandomPct, "Simple Feedback Random", 0.0f, 100.0f, 0.0f));
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingTimeLeftRandomPct, "Ping Time Left Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingTimeRightRandomPct, "Ping Time Right Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingFeedbackLeftRandomPct, "Ping Feedback Left Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::pingFeedbackRightRandomPct, "Ping Feedback Right Random", 0.0f, 100.0f, 0.0f));
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainPitchSemitones, "Grain Pitch", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainAmount, "Grain Amount", 0.0f, 1.0f, 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainFeedback, "Grain Feedback", 0.0f, 0.97f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainBaseTimeRandomPct, "Grain Base Time Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainSizeRandomPct, "Grain Size Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainRateRandomPct, "Grain Rate Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainPitchRandomPct, "Grain Pitch Random Interval", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainAmountRandomPct, "Grain Amount Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainFeedbackRandomPct, "Grain Feedback Random", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(PhaseusDelayParams::grainPingPong, "Grain PingPong", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::grainPingPongPan, "Grain PingPong Pan", 0.0f, 1.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PhaseusDelayParams::filterType,
        "Filter Type",
        juce::StringArray { "Off", "LowPass", "HighPass", "Notch", "Comb" },
        0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PhaseusDelayParams::filterCutoffHz,
        "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.3f),
        1800.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::filterQ, "Filter Q", 0.1f, 10.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::filterMix, "Filter Mix", 0.0f, 1.0f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::filterCombMs, "Filter Comb Time", 1.0f, 60.0f, 12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(PhaseusDelayParams::filterCombFeedback, "Filter Comb Feedback", 0.0f, 0.97f, 0.45f));

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
