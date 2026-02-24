#include "PhaseusPresetBar.h"

namespace phaseus
{
namespace
{
constexpr auto selectedPresetProperty = "phaseusSelectedPreset";

class SquareButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto colour = backgroundColour;
        if (shouldDrawButtonAsDown)
            colour = colour.brighter(0.08f);
        else if (shouldDrawButtonAsHighlighted)
            colour = colour.brighter(0.04f);

        g.setColour(colour);
        g.fillRect(button.getLocalBounds().toFloat());
    }
};

SquareButtonLookAndFeel& squareButtonLookAndFeel()
{
    static SquareButtonLookAndFeel laf;
    return laf;
}
}

PresetBar::PresetBar(juce::AudioProcessorValueTreeState& stateToUse, const juce::String& pluginNameToUse)
    : apvts(stateToUse), pluginName(pluginNameToUse)
{
    addAndMakeVisible(presetList);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(initButton);
    saveButton.setLookAndFeel(&squareButtonLookAndFeel());
    initButton.setLookAndFeel(&squareButtonLookAndFeel());

    presetList.onChange = [this] { loadSelectedPreset(); };
    saveButton.onClick = [this] { savePresetWithDialog(); };
    initButton.onClick = [this] { resetToInit(); };

    for (auto* param : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            defaultParameterValues.emplace_back(ranged, ranged->getDefaultValue());

    refreshPresetList(getSelectedPresetProperty(), false);
    captureReferenceState();
    updateDisplayedPresetText();
    startTimerHz(5);
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    initButton.setBounds(area.removeFromRight(52).reduced(2));
    saveButton.setBounds(area.removeFromRight(56).reduced(2));
    presetList.setBounds(area.reduced(2));
}

void PresetBar::applyTheme(const bool darkModeEnabled)
{
    const auto bg = darkModeEnabled ? juce::Colour::fromRGB(16, 16, 17) : juce::Colour::fromRGB(245, 245, 245);
    const auto fg = darkModeEnabled ? juce::Colour::fromRGB(243, 243, 243) : juce::Colour::fromRGB(24, 24, 24);

    presetList.setColour(juce::ComboBox::backgroundColourId, bg);
    presetList.setColour(juce::ComboBox::textColourId, fg);
    presetList.setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.25f));

    for (auto* button : { &saveButton, &initButton })
    {
        button->setColour(juce::TextButton::buttonColourId, bg.withAlpha(0.80f));
        button->setColour(juce::TextButton::textColourOffId, fg);
    }
}

void PresetBar::refreshPresetList(const juce::String& preferredSelection, const bool loadPreferred)
{
    const auto presetDir = getPresetDirectory();
    presetDir.createDirectory();

    presetFiles = presetDir.findChildFiles(juce::File::findFiles, false, "*.xml");
    presetFiles.sort();

    ignorePresetChange = true;
    const auto previouslySelected = preferredSelection.isNotEmpty() ? preferredSelection : getSelectedPresetProperty();
    presetList.clear(juce::dontSendNotification);
    presetList.addItem("Init", 1);

    int selectedId = 1;
    for (int i = 0; i < presetFiles.size(); ++i)
    {
        const auto name = presetFiles.getReference(i).getFileNameWithoutExtension();
        presetList.addItem(name, i + 2);
        if (name == previouslySelected)
            selectedId = i + 2;
    }

    presetList.setSelectedId(selectedId, juce::dontSendNotification);
    selectedPresetName = selectedId == 1 ? "Init" : presetList.getText();
    ignorePresetChange = false;

    if (loadPreferred && selectedId > 1)
        loadSelectedPreset();

    updateDisplayedPresetText();
}

void PresetBar::loadSelectedPreset()
{
    if (ignorePresetChange)
        return;

    const auto id = presetList.getSelectedId();
    if (id <= 0)
        return;

    if (id == 1)
    {
        resetToInit();
        return;
    }

    const auto presetIndex = id - 2;
    if (presetIndex < 0 || presetIndex >= presetFiles.size())
        return;

    const auto file = presetFiles.getReference(presetIndex);
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return;

    apvts.replaceState(state);
    selectedPresetName = file.getFileNameWithoutExtension();
    setSelectedPresetProperty(selectedPresetName);
    captureReferenceState();
    presetDirty = false;
    updateDisplayedPresetText();
}

void PresetBar::savePresetWithDialog()
{
    auto initialName = presetList.getText();
    if (initialName.isEmpty() || initialName == "No Presets")
        initialName = "Preset";

    auto* dialog = new juce::AlertWindow("Save Preset", "Preset name:", juce::AlertWindow::NoIcon);
    const auto bg = juce::Colour::fromRGB(12, 12, 14);
    const auto fg = juce::Colour::fromRGB(240, 240, 240);
    const auto outline = fg.withAlpha(0.28f);
    const auto accent = juce::Colour::fromRGB(80, 190, 240);

    dialog->setColour(juce::AlertWindow::backgroundColourId, bg);
    dialog->setColour(juce::AlertWindow::textColourId, fg);
    dialog->setColour(juce::TextEditor::backgroundColourId, bg.brighter(0.08f));
    dialog->setColour(juce::TextEditor::textColourId, fg);
    dialog->setColour(juce::TextEditor::outlineColourId, outline);
    dialog->setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.35f));
    dialog->setColour(juce::TextButton::buttonColourId, bg.brighter(0.12f));
    dialog->setColour(juce::TextButton::textColourOffId, fg);

    dialog->addTextEditor("name", initialName, "Name");
    dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->setLookAndFeel(&squareButtonLookAndFeel());
    constexpr int dialogW = 360;
    constexpr int dialogH = 150;
    if (auto* top = getTopLevelComponent())
        dialog->centreAroundComponent(top, dialogW, dialogH);
    else
        dialog->centreWithSize(dialogW, dialogH);

    juce::Component::SafePointer<PresetBar> safeThis(this);
    dialog->enterModalState(
        true,
        juce::ModalCallbackFunction::create([safeThis, dialog](int result)
        {
            dialog->setLookAndFeel(nullptr);
            std::unique_ptr<juce::AlertWindow> cleanup(dialog);
            if (result != 1 || safeThis == nullptr)
                return;

            const auto typedName = dialog->getTextEditorContents("name");
            safeThis->savePresetWithName(typedName);
        }),
        true);
}

void PresetBar::savePresetWithName(const juce::String& name)
{
    auto safeName = sanitisePresetName(name);
    if (safeName.isEmpty() || safeName == "No_Presets")
        safeName = "Preset";

    const auto presetDir = getPresetDirectory();
    presetDir.createDirectory();
    const auto presetFile = presetDir.getChildFile(safeName + ".xml");

    if (const auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml != nullptr)
        {
            if (presetFile.replaceWithText(xml->toString()))
                refreshPresetList(safeName, true);
            else
                juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Preset Save Failed", "Could not write preset file.");
        }
    }
}

void PresetBar::resetToInit()
{
    ignorePresetChange = true;
    presetList.setSelectedId(1, juce::dontSendNotification);
    ignorePresetChange = false;

    for (const auto& [param, defaultValue] : defaultParameterValues)
    {
        if (param == nullptr)
            continue;

        param->beginChangeGesture();
        param->setValueNotifyingHost(defaultValue);
        param->endChangeGesture();
    }

    selectedPresetName = "Init";
    setSelectedPresetProperty(selectedPresetName);
    captureReferenceState();
    presetDirty = false;
    updateDisplayedPresetText();
}

void PresetBar::setSelectedPresetProperty(const juce::String& name)
{
    apvts.state.setProperty(selectedPresetProperty, name, nullptr);
}

juce::String PresetBar::getSelectedPresetProperty() const
{
    if (!apvts.state.hasProperty(selectedPresetProperty))
        return "Init";

    return apvts.state.getProperty(selectedPresetProperty).toString();
}

juce::String PresetBar::buildCurrentStateSignature() const
{
    if (const auto state = apvts.copyState(); state.isValid())
        if (std::unique_ptr<juce::XmlElement> xml(state.createXml()); xml != nullptr)
            return xml->toString();

    return {};
}

void PresetBar::captureReferenceState()
{
    referenceStateSignature = buildCurrentStateSignature();
}

void PresetBar::updateDisplayedPresetText()
{
    auto text = selectedPresetName.isNotEmpty() ? selectedPresetName : "Init";
    if (presetDirty)
        text << "*";

    ignorePresetChange = true;
    presetList.setText(text, juce::dontSendNotification);
    ignorePresetChange = false;
}

void PresetBar::timerCallback()
{
    if (ignorePresetChange)
        return;

    const auto currentSig = buildCurrentStateSignature();
    const auto nowDirty = referenceStateSignature.isNotEmpty() && currentSig != referenceStateSignature;
    if (nowDirty != presetDirty)
    {
        presetDirty = nowDirty;
        updateDisplayedPresetText();
    }
}

juce::File PresetBar::getPresetDirectory() const
{
    auto safePluginName = pluginName;
    safePluginName = safePluginName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ");
    safePluginName = safePluginName.trim();
    if (safePluginName.isEmpty())
        safePluginName = "Plugin";
    safePluginName = safePluginName.replaceCharacter(' ', '_');

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Phaseus")
        .getChildFile(safePluginName)
        .getChildFile("Presets");
}

juce::String PresetBar::sanitisePresetName(const juce::String& input)
{
    auto result = input;
    result = result.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ");
    result = result.trim();
    while (result.contains("  "))
        result = result.replace("  ", " ");
    result = result.replaceCharacter(' ', '_');
    return result;
}
} // namespace phaseus
