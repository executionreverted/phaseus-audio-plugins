#include "PluginEditor.h"

PHASEUSDelayAudioProcessorEditor::PHASEUSDelayAudioProcessorEditor(PHASEUSDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    modeBox.addItem("Simple", 1);
    modeBox.addItem("PingPong", 2);
    modeBox.addItem("Grain", 3);
    addAndMakeVisible(modeBox);

    darkModeToggle.setButtonText("Dark");
    darkModeToggle.setToggleState(darkModeEnabled, juce::dontSendNotification);
    darkModeToggle.onClick = [this]
    {
        darkModeEnabled = darkModeToggle.getToggleState();
        applyTheme();
        repaint();
    };
    addAndMakeVisible(darkModeToggle);

    setupLabel(modeLabel, "Delay Mode");

    setupKnob(wetDrySlider, " %", 0.35);
    setupLabel(wetDryLabel, "Wet/Dry");

    setupKnob(simpleTimeSlider, " ms", 420.0);
    setupKnob(simpleFeedbackSlider, " %", 0.35);
    setupToggle(simpleSyncToggle);
    simpleSyncToggle.setButtonText("Tempo Sync");
    simpleSyncDivisionBox.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" }, 1);
    addAndMakeVisible(simpleSyncDivisionBox);
    setupLabel(simpleTimeLabel, "Time");
    setupLabel(simpleFeedbackLabel, "Feedback");
    setupLabel(simpleSyncDivisionLabel, "Division");

    setupKnob(pingTimeLeftSlider, " ms", 330.0);
    setupKnob(pingTimeRightSlider, " ms", 500.0);
    setupKnob(pingFeedbackLeftSlider, " %", 0.40);
    setupKnob(pingFeedbackRightSlider, " %", 0.40);
    setupToggle(pingSyncToggle);
    pingSyncToggle.setButtonText("Tempo Sync");
    pingSyncDivisionLeftBox.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" }, 1);
    pingSyncDivisionRightBox.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" }, 1);
    addAndMakeVisible(pingSyncDivisionLeftBox);
    addAndMakeVisible(pingSyncDivisionRightBox);
    setupToggle(pingLinkTimesToggle);
    setupToggle(pingLinkFeedbackToggle);
    pingLinkTimesToggle.setButtonText("Sync L/R Time");
    pingLinkFeedbackToggle.setButtonText("Sync L/R Feedback");
    setupLabel(pingTimeLeftLabel, "Time Left");
    setupLabel(pingTimeRightLabel, "Time Right");
    setupLabel(pingTimeLinkLabel, "⛓");
    pingTimeLinkLabel.setJustificationType(juce::Justification::centred);
    setupLabel(pingFeedbackLeftLabel, "Feedback Left");
    setupLabel(pingFeedbackRightLabel, "Feedback Right");
    setupLabel(pingFeedbackLinkLabel, "⛓");
    pingFeedbackLinkLabel.setJustificationType(juce::Justification::centred);
    setupLabel(pingSyncDivisionLeftLabel, "Div Left");
    setupLabel(pingSyncDivisionRightLabel, "Div Right");

    setupKnob(grainBaseTimeSlider, " ms", 300.0);
    setupKnob(grainSizeSlider, " ms", 60.0);
    setupKnob(grainRateSlider, " Hz", 6.0);
    setupKnob(grainAmountSlider, " %", 0.55);
    setupKnob(grainFeedbackSlider, " %", 0.35);
    setupToggle(grainPingPongToggle);
    grainPingPongToggle.setButtonText("PingPong Grain Output");
    setupLabel(grainBaseTimeLabel, "Base Time");
    setupLabel(grainSizeLabel, "Grain Size");
    setupLabel(grainRateLabel, "Rate");
    setupLabel(grainAmountLabel, "Amount / Pan");
    setupLabel(grainFeedbackLabel, "Feedback");

    modeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::mode, modeBox);
    wetDryAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::wetDry, wetDrySlider);

    simpleTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleTimeMs, simpleTimeSlider);
    simpleFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleFeedback, simpleFeedbackSlider);
    simpleSyncAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleSync, simpleSyncToggle);
    simpleSyncDivisionAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleSyncDivision, simpleSyncDivisionBox);

    pingTimeLeftAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingTimeLeftMs, pingTimeLeftSlider);
    pingTimeRightAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingTimeRightMs, pingTimeRightSlider);
    pingFeedbackLeftAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingFeedbackLeft, pingFeedbackLeftSlider);
    pingFeedbackRightAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingFeedbackRight, pingFeedbackRightSlider);
    pingSyncAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingSync, pingSyncToggle);
    pingSyncDivisionLeftAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingSyncDivisionLeft, pingSyncDivisionLeftBox);
    pingSyncDivisionRightAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingSyncDivisionRight, pingSyncDivisionRightBox);
    pingLinkTimesAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingLinkTimes, pingLinkTimesToggle);
    pingLinkFeedbackAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingLinkFeedback, pingLinkFeedbackToggle);

    grainBaseTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainBaseTimeMs, grainBaseTimeSlider);
    grainSizeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainSizeMs, grainSizeSlider);
    grainRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainRateHz, grainRateSlider);
    grainAmountAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainAmount, grainAmountSlider);
    grainFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainFeedback, grainFeedbackSlider);
    grainPingPongAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainPingPong, grainPingPongToggle);

    modeBox.onChange = [this]
    {
        updateModeVisibility();
    };
    pingTimeLeftSlider.onValueChange = [this] { syncPingPongKnobsFromUI(); };
    pingTimeRightSlider.onValueChange = [this] { syncPingPongKnobsFromUI(); };
    pingFeedbackLeftSlider.onValueChange = [this] { syncPingPongKnobsFromUI(); };
    pingFeedbackRightSlider.onValueChange = [this] { syncPingPongKnobsFromUI(); };

    simpleSyncToggle.onClick = [this]
    {
        updateModeVisibility();
    };
    pingSyncToggle.onClick = [this]
    {
        updateModeVisibility();
    };
    pingLinkTimesToggle.onClick = [this]
    {
        syncPingPongKnobsFromUI();
        updateModeVisibility();
    };
    pingLinkFeedbackToggle.onClick = [this]
    {
        syncPingPongKnobsFromUI();
        updateModeVisibility();
    };

    setSize(1040, 620);
    applyTheme();
    syncPingPongKnobsFromUI();
    updateModeVisibility();
}

void PHASEUSDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto topColour = darkModeEnabled ? juce::Colour::fromRGB(9, 9, 10) : juce::Colour::fromRGB(228, 228, 228);
    const auto bottomColour = darkModeEnabled ? juce::Colour::fromRGB(24, 24, 26) : juce::Colour::fromRGB(250, 250, 250);

    juce::ColourGradient background(topColour, 0.0f, 0.0f, bottomColour, 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(background);
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(108);

    g.setColour(darkModeEnabled ? juce::Colour::fromRGB(255, 255, 255) : juce::Colour::fromRGB(18, 18, 18));
    g.setFont(juce::FontOptions(34.0f));
    g.drawFittedText("PHASEUS Delay", header.removeFromLeft(getWidth() - 180).reduced(20, 24), juce::Justification::centredLeft, 1);

    g.setColour(darkModeEnabled ? juce::Colours::white.withAlpha(0.08f) : juce::Colours::black.withAlpha(0.08f));
    g.fillRect(18, 106, getWidth() - 36, 1);
}

void PHASEUSDelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(18);

    auto header = bounds.removeFromTop(108);
    darkModeToggle.setBounds(header.removeFromRight(120).withSizeKeepingCentre(96, 28));

    auto controlBar = bounds.removeFromTop(60);
    modeLabel.setBounds(controlBar.removeFromLeft(120));
    modeBox.setBounds(controlBar.removeFromLeft(220).reduced(6, 8));

    auto body = bounds.reduced(0, 12);
    auto rightPanel = body.removeFromRight(240);
    auto content = body;

    wetDryLabel.setBounds(rightPanel.removeFromTop(24));
    wetDrySlider.setBounds(rightPanel.removeFromTop(220).reduced(12));

    constexpr int knobW = 160;
    constexpr int knobH = 140;
    constexpr int labelH = 20;

    simpleTimeLabel.setBounds(content.getX() + 16, content.getY() + 24, knobW, labelH);
    simpleTimeSlider.setBounds(content.getX() + 8, content.getY() + 44, knobW, knobH);
    simpleFeedbackLabel.setBounds(content.getX() + 212, content.getY() + 24, knobW, labelH);
    simpleFeedbackSlider.setBounds(content.getX() + 204, content.getY() + 44, knobW, knobH);
    simpleSyncToggle.setBounds(content.getX() + 20, content.getY() + 198, 150, 26);
    simpleSyncDivisionLabel.setBounds(content.getX() + 20, content.getY() + 232, 120, 22);
    simpleSyncDivisionBox.setBounds(content.getX() + 16, content.getY() + 256, 170, 28);

    pingTimeLeftLabel.setBounds(content.getX() + 10, content.getY() + 10, knobW, labelH);
    pingTimeLeftSlider.setBounds(content.getX() + 2, content.getY() + 30, knobW, knobH);
    pingTimeRightLabel.setBounds(content.getX() + 192, content.getY() + 10, knobW, labelH);
    pingTimeRightSlider.setBounds(content.getX() + 184, content.getY() + 30, knobW, knobH);
    pingTimeLinkLabel.setBounds(content.getX() + 168, content.getY() + 84, 16, 20);
    pingFeedbackLeftLabel.setBounds(content.getX() + 374, content.getY() + 10, knobW, labelH);
    pingFeedbackLeftSlider.setBounds(content.getX() + 366, content.getY() + 30, knobW, knobH);
    pingFeedbackRightLabel.setBounds(content.getX() + 556, content.getY() + 10, knobW, labelH);
    pingFeedbackRightSlider.setBounds(content.getX() + 548, content.getY() + 30, knobW, knobH);
    pingFeedbackLinkLabel.setBounds(content.getX() + 532, content.getY() + 84, 16, 20);
    pingSyncToggle.setBounds(content.getX() + 24, content.getY() + 194, 140, 26);
    pingSyncDivisionLeftLabel.setBounds(content.getX() + 14, content.getY() + 228, 90, 22);
    pingSyncDivisionLeftBox.setBounds(content.getX() + 8, content.getY() + 252, 150, 28);
    pingSyncDivisionRightLabel.setBounds(content.getX() + 188, content.getY() + 228, 94, 22);
    pingSyncDivisionRightBox.setBounds(content.getX() + 184, content.getY() + 252, 150, 28);
    pingLinkTimesToggle.setBounds(content.getX() + 528, content.getY() + 194, 150, 26);
    pingLinkFeedbackToggle.setBounds(content.getX() + 682, content.getY() + 194, 180, 26);

    grainBaseTimeLabel.setBounds(content.getX() + 10, content.getY() + 10, knobW, labelH);
    grainBaseTimeSlider.setBounds(content.getX() + 2, content.getY() + 30, knobW, knobH);
    grainSizeLabel.setBounds(content.getX() + 192, content.getY() + 10, knobW, labelH);
    grainSizeSlider.setBounds(content.getX() + 184, content.getY() + 30, knobW, knobH);
    grainRateLabel.setBounds(content.getX() + 374, content.getY() + 10, knobW, labelH);
    grainRateSlider.setBounds(content.getX() + 366, content.getY() + 30, knobW, knobH);
    grainAmountLabel.setBounds(content.getX() + 556, content.getY() + 10, knobW, labelH);
    grainAmountSlider.setBounds(content.getX() + 548, content.getY() + 30, knobW, knobH);
    grainFeedbackLabel.setBounds(content.getX() + 190, content.getY() + 186, knobW, labelH);
    grainFeedbackSlider.setBounds(content.getX() + 182, content.getY() + 206, knobW, knobH);
    grainPingPongToggle.setBounds(content.getX() + 24, content.getY() + 214, 280, 26);
}

void PHASEUSDelayAudioProcessorEditor::setupKnob(juce::Slider& slider, const juce::String& suffix, const double defaultValue)
{
    slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 20);
    slider.setTextBoxIsEditable(true);
    slider.setTextValueSuffix(suffix);
    slider.setDoubleClickReturnValue(true, defaultValue);
    addAndMakeVisible(slider);
}

void PHASEUSDelayAudioProcessorEditor::setupToggle(juce::ToggleButton& toggle)
{
    addAndMakeVisible(toggle);
}

void PHASEUSDelayAudioProcessorEditor::setupLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);
}

void PHASEUSDelayAudioProcessorEditor::updateModeVisibility()
{
    const auto mode = audioProcessor.getCurrentModeIndex();

    const auto showSimple = mode == 0;
    const auto showSimpleSync = showSimple && simpleSyncToggle.getToggleState();
    const auto showSimpleMs = showSimple && !simpleSyncToggle.getToggleState();
    simpleTimeLabel.setVisible(showSimpleMs);
    simpleTimeSlider.setVisible(showSimpleMs);
    simpleFeedbackLabel.setVisible(showSimple);
    simpleFeedbackSlider.setVisible(showSimple);
    simpleSyncToggle.setVisible(showSimple);
    simpleSyncDivisionLabel.setVisible(showSimpleSync);
    simpleSyncDivisionBox.setVisible(showSimpleSync);

    const auto showPingPong = mode == 1;
    const auto showPingSync = showPingPong && pingSyncToggle.getToggleState();
    const auto showPingMs = showPingPong && !pingSyncToggle.getToggleState();
    pingTimeLeftLabel.setVisible(showPingMs);
    pingTimeLeftSlider.setVisible(showPingMs);
    pingTimeRightLabel.setVisible(showPingMs);
    pingTimeRightSlider.setVisible(showPingMs);
    pingTimeLinkLabel.setVisible(showPingPong);
    pingFeedbackLeftLabel.setVisible(showPingPong);
    pingFeedbackLeftSlider.setVisible(showPingPong);
    pingFeedbackRightLabel.setVisible(showPingPong);
    pingFeedbackRightSlider.setVisible(showPingPong);
    pingFeedbackLinkLabel.setVisible(showPingPong);
    pingSyncToggle.setVisible(showPingPong);
    const auto showPingRightDivision = showPingSync && !pingLinkTimesToggle.getToggleState();
    pingSyncDivisionLeftLabel.setVisible(showPingSync);
    pingSyncDivisionLeftBox.setVisible(showPingSync);
    pingSyncDivisionRightLabel.setVisible(showPingRightDivision);
    pingSyncDivisionRightBox.setVisible(showPingRightDivision);
    pingLinkTimesToggle.setVisible(showPingPong);
    pingLinkFeedbackToggle.setVisible(showPingPong);
    pingTimeRightSlider.setEnabled(!pingLinkTimesToggle.getToggleState() && !pingSyncToggle.getToggleState());
    pingFeedbackRightSlider.setEnabled(!pingLinkFeedbackToggle.getToggleState());

    const auto showGrain = mode == 2;
    grainBaseTimeLabel.setVisible(showGrain);
    grainBaseTimeSlider.setVisible(showGrain);
    grainSizeLabel.setVisible(showGrain);
    grainSizeSlider.setVisible(showGrain);
    grainRateLabel.setVisible(showGrain);
    grainRateSlider.setVisible(showGrain);
    grainAmountLabel.setVisible(showGrain);
    grainAmountSlider.setVisible(showGrain);
    grainFeedbackLabel.setVisible(showGrain);
    grainFeedbackSlider.setVisible(showGrain);
    grainPingPongToggle.setVisible(showGrain);

    repaint();
}

void PHASEUSDelayAudioProcessorEditor::syncPingPongKnobsFromUI()
{
    if (syncUpdateInProgress)
        return;

    syncUpdateInProgress = true;

    if (pingLinkTimesToggle.getToggleState())
        pingTimeRightSlider.setValue(pingTimeLeftSlider.getValue(), juce::sendNotificationSync);

    if (pingLinkFeedbackToggle.getToggleState())
        pingFeedbackRightSlider.setValue(pingFeedbackLeftSlider.getValue(), juce::sendNotificationSync);

    syncUpdateInProgress = false;
}

void PHASEUSDelayAudioProcessorEditor::applyTheme()
{
    const auto bg = darkModeEnabled ? juce::Colour::fromRGB(16, 16, 16) : juce::Colour::fromRGB(244, 244, 244);
    const auto fg = darkModeEnabled ? juce::Colour::fromRGB(245, 245, 245) : juce::Colour::fromRGB(20, 20, 20);
    const auto accent = darkModeEnabled ? juce::Colour::fromRGB(232, 232, 232) : juce::Colour::fromRGB(28, 28, 28);

    modeBox.setColour(juce::ComboBox::backgroundColourId, bg);
    modeBox.setColour(juce::ComboBox::textColourId, fg);
    modeBox.setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.3f));
    simpleSyncDivisionBox.setColour(juce::ComboBox::backgroundColourId, bg);
    simpleSyncDivisionBox.setColour(juce::ComboBox::textColourId, fg);
    simpleSyncDivisionBox.setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.3f));
    pingSyncDivisionLeftBox.setColour(juce::ComboBox::backgroundColourId, bg);
    pingSyncDivisionLeftBox.setColour(juce::ComboBox::textColourId, fg);
    pingSyncDivisionLeftBox.setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.3f));
    pingSyncDivisionRightBox.setColour(juce::ComboBox::backgroundColourId, bg);
    pingSyncDivisionRightBox.setColour(juce::ComboBox::textColourId, fg);
    pingSyncDivisionRightBox.setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.3f));

    darkModeToggle.setColour(juce::ToggleButton::textColourId, fg);
    simpleSyncToggle.setColour(juce::ToggleButton::textColourId, fg);
    pingSyncToggle.setColour(juce::ToggleButton::textColourId, fg);
    pingLinkTimesToggle.setColour(juce::ToggleButton::textColourId, fg);
    pingLinkFeedbackToggle.setColour(juce::ToggleButton::textColourId, fg);
    grainPingPongToggle.setColour(juce::ToggleButton::textColourId, fg);

    for (auto* label : { &modeLabel, &wetDryLabel, &simpleTimeLabel, &simpleFeedbackLabel, &pingTimeLeftLabel, &pingTimeRightLabel,
                         &pingTimeLinkLabel,
                         &simpleSyncDivisionLabel, &pingFeedbackLeftLabel, &pingFeedbackRightLabel, &pingSyncDivisionLeftLabel,
                         &pingSyncDivisionRightLabel, &pingFeedbackLinkLabel, &grainBaseTimeLabel, &grainSizeLabel, &grainRateLabel, &grainAmountLabel,
                         &grainFeedbackLabel })
    {
        label->setColour(juce::Label::textColourId, fg.withAlpha(0.86f));
    }

    for (auto* slider : { &wetDrySlider, &simpleTimeSlider, &simpleFeedbackSlider, &pingTimeLeftSlider, &pingTimeRightSlider,
                          &pingFeedbackLeftSlider, &pingFeedbackRightSlider, &grainBaseTimeSlider, &grainSizeSlider,
                          &grainRateSlider, &grainAmountSlider, &grainFeedbackSlider })
    {
        slider->setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, fg.withAlpha(0.28f));
        slider->setColour(juce::Slider::textBoxTextColourId, fg);
        slider->setColour(juce::Slider::textBoxOutlineColourId, fg.withAlpha(0.2f));
        slider->setColour(juce::Slider::textBoxBackgroundColourId, bg.withAlpha(0.9f));
    }
}
