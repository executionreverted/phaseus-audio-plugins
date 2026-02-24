#pragma once

#include "PluginProcessor.h"

class PHASEUSDelayAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PHASEUSDelayAudioProcessorEditor(PHASEUSDelayAudioProcessor&);
    ~PHASEUSDelayAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob(juce::Slider& slider, const juce::String& suffix, double defaultValue);
    void setupToggle(juce::ToggleButton& toggle);
    void setupLabel(juce::Label& label, const juce::String& text);
    void updateModeVisibility();
    void applyTheme();
    void syncPingPongKnobsFromUI();

    PHASEUSDelayAudioProcessor& audioProcessor;
    bool syncUpdateInProgress = false;

    bool darkModeEnabled = true;
    juce::ToggleButton darkModeToggle;

    juce::ComboBox modeBox;
    juce::Label modeLabel;

    juce::Slider wetDrySlider;
    juce::Label wetDryLabel;

    juce::Slider simpleTimeSlider;
    juce::Slider simpleFeedbackSlider;
    juce::ToggleButton simpleSyncToggle;
    juce::ComboBox simpleSyncDivisionBox;
    juce::Label simpleTimeLabel;
    juce::Label simpleFeedbackLabel;
    juce::Label simpleSyncDivisionLabel;

    juce::Slider pingTimeLeftSlider;
    juce::Slider pingTimeRightSlider;
    juce::Slider pingFeedbackLeftSlider;
    juce::Slider pingFeedbackRightSlider;
    juce::ToggleButton pingSyncToggle;
    juce::ComboBox pingSyncDivisionLeftBox;
    juce::ComboBox pingSyncDivisionRightBox;
    juce::ToggleButton pingLinkTimesToggle;
    juce::ToggleButton pingLinkFeedbackToggle;
    juce::Label pingTimeLeftLabel;
    juce::Label pingTimeRightLabel;
    juce::Label pingTimeLinkLabel;
    juce::Label pingFeedbackLeftLabel;
    juce::Label pingFeedbackRightLabel;
    juce::Label pingFeedbackLinkLabel;
    juce::Label pingSyncDivisionLeftLabel;
    juce::Label pingSyncDivisionRightLabel;

    juce::Slider grainBaseTimeSlider;
    juce::Slider grainSizeSlider;
    juce::Slider grainRateSlider;
    juce::Slider grainAmountSlider;
    juce::Slider grainFeedbackSlider;
    juce::ToggleButton grainPingPongToggle;
    juce::Label grainBaseTimeLabel;
    juce::Label grainSizeLabel;
    juce::Label grainRateLabel;
    juce::Label grainAmountLabel;
    juce::Label grainFeedbackLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    std::unique_ptr<SliderAttachment> wetDryAttachment;

    std::unique_ptr<SliderAttachment> simpleTimeAttachment;
    std::unique_ptr<SliderAttachment> simpleFeedbackAttachment;
    std::unique_ptr<ButtonAttachment> simpleSyncAttachment;
    std::unique_ptr<ComboBoxAttachment> simpleSyncDivisionAttachment;

    std::unique_ptr<SliderAttachment> pingTimeLeftAttachment;
    std::unique_ptr<SliderAttachment> pingTimeRightAttachment;
    std::unique_ptr<SliderAttachment> pingFeedbackLeftAttachment;
    std::unique_ptr<SliderAttachment> pingFeedbackRightAttachment;
    std::unique_ptr<ButtonAttachment> pingSyncAttachment;
    std::unique_ptr<ComboBoxAttachment> pingSyncDivisionLeftAttachment;
    std::unique_ptr<ComboBoxAttachment> pingSyncDivisionRightAttachment;
    std::unique_ptr<ButtonAttachment> pingLinkTimesAttachment;
    std::unique_ptr<ButtonAttachment> pingLinkFeedbackAttachment;

    std::unique_ptr<SliderAttachment> grainBaseTimeAttachment;
    std::unique_ptr<SliderAttachment> grainSizeAttachment;
    std::unique_ptr<SliderAttachment> grainRateAttachment;
    std::unique_ptr<SliderAttachment> grainAmountAttachment;
    std::unique_ptr<SliderAttachment> grainFeedbackAttachment;
    std::unique_ptr<ButtonAttachment> grainPingPongAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PHASEUSDelayAudioProcessorEditor)
};
