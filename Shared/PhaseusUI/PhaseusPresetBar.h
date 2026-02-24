#pragma once

#include <JuceHeader.h>

namespace phaseus
{
class PresetBar final : public juce::Component,
                            private juce::Timer
{
public:
    PresetBar(juce::AudioProcessorValueTreeState& stateToUse, const juce::String& pluginNameToUse);

    void resized() override;
    void applyTheme(bool darkModeEnabled);

private:
    void refreshPresetList(const juce::String& preferredSelection = {}, bool loadPreferred = false);
    void loadSelectedPreset();
    void savePresetWithDialog();
    void savePresetWithName(const juce::String& name);
    void resetToInit();
    void setSelectedPresetProperty(const juce::String& name);
    juce::String getSelectedPresetProperty() const;
    juce::String buildCurrentStateSignature() const;
    void captureReferenceState();
    void updateDisplayedPresetText();
    void timerCallback() override;
    juce::File getPresetDirectory() const;
    static juce::String sanitisePresetName(const juce::String& input);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String pluginName;
    juce::ComboBox presetList;
    juce::TextButton saveButton { "Save" };
    juce::TextButton initButton { "Init" };
    juce::Array<juce::File> presetFiles;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> defaultParameterValues;
    bool ignorePresetChange = false;
    juce::String selectedPresetName { "Init" };
    juce::String referenceStateSignature;
    bool presetDirty = false;
};
} // namespace phaseus
