#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto roomSizeId = "roomSize";
constexpr auto mixId = "mix";
}

__PLUGIN_CLASS__AudioProcessor::__PLUGIN_CLASS__AudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void __PLUGIN_CLASS__AudioProcessor::prepareToPlay(double, int)
{
    reverb.reset();
}

void __PLUGIN_CLASS__AudioProcessor::releaseResources()
{
}

bool __PLUGIN_CLASS__AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void __PLUGIN_CLASS__AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    juce::Reverb::Parameters params;
    params.roomSize = *apvts.getRawParameterValue(roomSizeId);
    params.damping = 0.5f;
    params.wetLevel = *apvts.getRawParameterValue(mixId);
    params.dryLevel = 1.0f - params.wetLevel;
    params.width = 1.0f;
    params.freezeMode = 0.0f;

    reverb.setParameters(params);

    if (buffer.getNumChannels() == 1)
    {
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
        return;
    }

    reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
}

juce::AudioProcessorEditor* __PLUGIN_CLASS__AudioProcessor::createEditor()
{
    return new __PLUGIN_CLASS__AudioProcessorEditor(*this);
}

bool __PLUGIN_CLASS__AudioProcessor::hasEditor() const
{
    return true;
}

const juce::String __PLUGIN_CLASS__AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool __PLUGIN_CLASS__AudioProcessor::acceptsMidi() const
{
    return false;
}

bool __PLUGIN_CLASS__AudioProcessor::producesMidi() const
{
    return false;
}

bool __PLUGIN_CLASS__AudioProcessor::isMidiEffect() const
{
    return false;
}

double __PLUGIN_CLASS__AudioProcessor::getTailLengthSeconds() const
{
    return 2.0;
}

int __PLUGIN_CLASS__AudioProcessor::getNumPrograms()
{
    return 1;
}

int __PLUGIN_CLASS__AudioProcessor::getCurrentProgram()
{
    return 0;
}

void __PLUGIN_CLASS__AudioProcessor::setCurrentProgram(int)
{
}

const juce::String __PLUGIN_CLASS__AudioProcessor::getProgramName(int)
{
    return {};
}

void __PLUGIN_CLASS__AudioProcessor::changeProgramName(int, const juce::String&)
{
}

void __PLUGIN_CLASS__AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void __PLUGIN_CLASS__AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState == nullptr)
        return;

    if (!xmlState->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout __PLUGIN_CLASS__AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(roomSizeId, "Room Size", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(mixId, "Mix", 0.0f, 1.0f, 0.25f));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new __PLUGIN_CLASS__AudioProcessor();
}
