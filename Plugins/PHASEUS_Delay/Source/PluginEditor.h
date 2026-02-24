#pragma once

#include "PluginProcessor.h"

class PHASEUSDelayAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer,
                                               private juce::ScrollBar::Listener
{
public:
    explicit PHASEUSDelayAudioProcessorEditor(PHASEUSDelayAudioProcessor&);
    ~PHASEUSDelayAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    class FilterResponseView;

    void setupKnob(juce::Slider& slider, const juce::String& suffix, double defaultValue);
    void setupLabel(juce::Label& label, const juce::String& text, bool centred = false);
    void setupToggle(juce::ToggleButton& button, const juce::String& text);
    void setupFilterTab(juce::TextButton& button, const juce::String& text);
    void setupSyncTimeTextFunctions();
    void updateSyncTimeKnobState();
    void pushSyncDivisionFromKnob(juce::Slider& knob, juce::ComboBox& divisionBox);

    void applyTheme();
    void updateModeVisibility();
    void updateTabState();
    void updateFilterTabState();
    void setFilterTypeFromTab(int filterTypeIndex);
    void setModeFromTab(int modeIndex);
    void syncPingPongKnobsFromUI();

    void timerCallback() override;
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    PHASEUSDelayAudioProcessor& audioProcessor;
    bool syncUpdateInProgress = false;
    bool darkModeEnabled = true;
    int lastUiMode = -1;
    bool lastSimpleSyncState = false;
    bool lastPingSyncState = false;
    bool lastPingLinkTimeState = false;
    bool lastPingLinkFeedbackState = false;
    bool syncTimeUpdateInProgress = false;
    int rightPanelScrollOffset = 0;
    int rightPanelContentHeight = 0;
    juce::Rectangle<int> rightPanelViewportBounds;

    juce::TextButton simpleTab { "Simple" };
    juce::TextButton pingTab { "PingPong" };
    juce::TextButton grainTab { "Grain" };

    juce::ToggleButton darkModeToggle;
    juce::ToggleButton loFiToggle;
    juce::ScrollBar rightPanelScrollBar { false };

    juce::Slider wetDrySlider;
    juce::Label wetDryLabel;

    juce::ToggleButton simpleSyncToggle;
    juce::ComboBox simpleSyncDivisionBox;
    juce::Label simpleSyncDivisionLabel;
    juce::Slider simpleTimeRandomSlider;
    juce::Slider simpleFeedbackRandomSlider;
    juce::Label simpleTimeRandomLabel;
    juce::Label simpleFeedbackRandomLabel;

    juce::ToggleButton pingSyncToggle;
    juce::ComboBox pingSyncDivisionLeftBox;
    juce::ComboBox pingSyncDivisionRightBox;
    juce::ToggleButton pingLinkTimesToggle;
    juce::ToggleButton pingLinkFeedbackToggle;
    juce::Label pingSyncDivisionLeftLabel;
    juce::Label pingSyncDivisionRightLabel;
    juce::Slider pingTimeLeftRandomSlider;
    juce::Slider pingTimeRightRandomSlider;
    juce::Slider pingFeedbackLeftRandomSlider;
    juce::Slider pingFeedbackRightRandomSlider;
    juce::Label pingTimeLeftRandomLabel;
    juce::Label pingTimeRightRandomLabel;
    juce::Label pingFeedbackLeftRandomLabel;
    juce::Label pingFeedbackRightRandomLabel;

    juce::ToggleButton grainPingPongToggle;
    juce::Slider grainPingPongPanSlider;
    juce::Label grainPingPongPanLabel;
    juce::TextButton filterOffTab { "Off" };
    juce::TextButton filterLpTab { "LP" };
    juce::TextButton filterHpTab { "HP" };
    juce::TextButton filterNotchTab { "Notch" };
    juce::TextButton filterCombTab { "Comb" };
    std::unique_ptr<FilterResponseView> filterResponseView;
    juce::ComboBox filterTypeBox;
    juce::Label filterTypeLabel;
    juce::Slider filterCutoffSlider;
    juce::Slider filterQSlider;
    juce::Slider filterMixSlider;
    juce::Slider filterCombMsSlider;
    juce::Slider filterCombFeedbackSlider;
    juce::Label filterCutoffLabel;
    juce::Label filterQLabel;
    juce::Label filterMixLabel;
    juce::Label filterCombMsLabel;
    juce::Label filterCombFeedbackLabel;

    juce::Slider simpleTimeSlider;
    juce::Slider simpleFeedbackSlider;
    juce::Label simpleTimeLabel;
    juce::Label simpleFeedbackLabel;

    juce::Slider pingTimeLeftSlider;
    juce::Slider pingTimeRightSlider;
    juce::Slider pingFeedbackLeftSlider;
    juce::Slider pingFeedbackRightSlider;
    juce::Label pingTimeLeftLabel;
    juce::Label pingTimeRightLabel;
    juce::Label pingFeedbackLeftLabel;
    juce::Label pingFeedbackRightLabel;
    juce::Label pingTimeLinkLabel;
    juce::Label pingFeedbackLinkLabel;

    juce::Slider grainBaseTimeSlider;
    juce::Slider grainSizeSlider;
    juce::Slider grainRateSlider;
    juce::Slider grainPitchSlider;
    juce::Slider grainAmountSlider;
    juce::Slider grainFeedbackSlider;

    juce::Slider grainBaseTimeRandomSlider;
    juce::Slider grainSizeRandomSlider;
    juce::Slider grainRateRandomSlider;
    juce::Slider grainPitchRandomSlider;
    juce::Slider grainAmountRandomSlider;
    juce::Slider grainFeedbackRandomSlider;

    juce::Label grainBaseTimeLabel;
    juce::Label grainSizeLabel;
    juce::Label grainRateLabel;
    juce::Label grainPitchLabel;
    juce::Label grainAmountLabel;
    juce::Label grainFeedbackLabel;

    juce::Label grainBaseTimeRandomLabel;
    juce::Label grainSizeRandomLabel;
    juce::Label grainRateRandomLabel;
    juce::Label grainPitchRandomLabel;
    juce::Label grainAmountRandomLabel;
    juce::Label grainFeedbackRandomLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> wetDryAttachment;
    std::unique_ptr<ButtonAttachment> loFiAttachment;

    std::unique_ptr<SliderAttachment> simpleTimeAttachment;
    std::unique_ptr<SliderAttachment> simpleFeedbackAttachment;
    std::unique_ptr<SliderAttachment> simpleTimeRandomAttachment;
    std::unique_ptr<SliderAttachment> simpleFeedbackRandomAttachment;
    std::unique_ptr<ButtonAttachment> simpleSyncAttachment;
    std::unique_ptr<ComboBoxAttachment> simpleSyncDivisionAttachment;

    std::unique_ptr<SliderAttachment> pingTimeLeftAttachment;
    std::unique_ptr<SliderAttachment> pingTimeRightAttachment;
    std::unique_ptr<SliderAttachment> pingFeedbackLeftAttachment;
    std::unique_ptr<SliderAttachment> pingFeedbackRightAttachment;
    std::unique_ptr<SliderAttachment> pingTimeLeftRandomAttachment;
    std::unique_ptr<SliderAttachment> pingTimeRightRandomAttachment;
    std::unique_ptr<SliderAttachment> pingFeedbackLeftRandomAttachment;
    std::unique_ptr<SliderAttachment> pingFeedbackRightRandomAttachment;
    std::unique_ptr<ButtonAttachment> pingSyncAttachment;
    std::unique_ptr<ComboBoxAttachment> pingSyncDivisionLeftAttachment;
    std::unique_ptr<ComboBoxAttachment> pingSyncDivisionRightAttachment;
    std::unique_ptr<ButtonAttachment> pingLinkTimesAttachment;
    std::unique_ptr<ButtonAttachment> pingLinkFeedbackAttachment;

    std::unique_ptr<SliderAttachment> grainBaseTimeAttachment;
    std::unique_ptr<SliderAttachment> grainSizeAttachment;
    std::unique_ptr<SliderAttachment> grainRateAttachment;
    std::unique_ptr<SliderAttachment> grainPitchAttachment;
    std::unique_ptr<SliderAttachment> grainAmountAttachment;
    std::unique_ptr<SliderAttachment> grainFeedbackAttachment;

    std::unique_ptr<SliderAttachment> grainBaseTimeRandomAttachment;
    std::unique_ptr<SliderAttachment> grainSizeRandomAttachment;
    std::unique_ptr<SliderAttachment> grainRateRandomAttachment;
    std::unique_ptr<SliderAttachment> grainPitchRandomAttachment;
    std::unique_ptr<SliderAttachment> grainAmountRandomAttachment;
    std::unique_ptr<SliderAttachment> grainFeedbackRandomAttachment;

    std::unique_ptr<ButtonAttachment> grainPingPongAttachment;
    std::unique_ptr<SliderAttachment> grainPingPongPanAttachment;
    std::unique_ptr<ComboBoxAttachment> filterTypeAttachment;
    std::unique_ptr<SliderAttachment> filterCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterQAttachment;
    std::unique_ptr<SliderAttachment> filterMixAttachment;
    std::unique_ptr<SliderAttachment> filterCombMsAttachment;
    std::unique_ptr<SliderAttachment> filterCombFeedbackAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PHASEUSDelayAudioProcessorEditor)
};
