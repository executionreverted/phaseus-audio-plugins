#include "PluginEditor.h"

namespace
{
void setupSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
}
}

__PLUGIN_CLASS__AudioProcessorEditor::__PLUGIN_CLASS__AudioProcessorEditor(__PLUGIN_CLASS__AudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setupSlider(roomSizeSlider);
    setupSlider(mixSlider);

    addAndMakeVisible(roomSizeSlider);
    addAndMakeVisible(mixSlider);

    roomSizeAttachment = std::make_unique<SliderAttachment>(processor.apvts, "roomSize", roomSizeSlider);
    mixAttachment = std::make_unique<SliderAttachment>(processor.apvts, "mix", mixSlider);

    setSize(320, 180);
}

void __PLUGIN_CLASS__AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("__PLUGIN_NAME__", 0, 8, getWidth(), 24, juce::Justification::centred, 1);
}

void __PLUGIN_CLASS__AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(28);

    auto left = area.removeFromLeft(area.getWidth() / 2);
    roomSizeSlider.setBounds(left.reduced(8));
    mixSlider.setBounds(area.reduced(8));
}
