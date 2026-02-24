#pragma once

#include "PluginProcessor.h"

class __PLUGIN_CLASS__AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit __PLUGIN_CLASS__AudioProcessorEditor(__PLUGIN_CLASS__AudioProcessor&);
    ~__PLUGIN_CLASS__AudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    __PLUGIN_CLASS__AudioProcessor& processor;

    juce::Slider roomSizeSlider;
    juce::Slider mixSlider;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> roomSizeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(__PLUGIN_CLASS__AudioProcessorEditor)
};
