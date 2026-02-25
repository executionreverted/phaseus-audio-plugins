#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
constexpr int simpleMode = 0;
constexpr int pingMode = 1;
constexpr int grainMode = 2;

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

juce::StringArray syncDivisions()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4D", "1/8D", "1/16D", "1/4T", "1/8T", "1/16T" };
}

float normalizedLogFreq(const float hz, const float minHz, const float maxHz)
{
    const auto safeHz = juce::jlimit(minHz, maxHz, hz);
    return (std::log(safeHz) - std::log(minHz)) / (std::log(maxHz) - std::log(minHz));
}

float freqFromNormalized(const float norm, const float minHz, const float maxHz)
{
    const auto clamped = juce::jlimit(0.0f, 1.0f, norm);
    return std::exp(std::log(minHz) + clamped * (std::log(maxHz) - std::log(minHz)));
}

int divisionIndexFromKnobValue(const juce::Slider& slider, const double value)
{
    const auto range = slider.getMaximum() - slider.getMinimum();
    if (range <= 0.0)
        return 0;

    const auto norm = (value - slider.getMinimum()) / range;
    return juce::jlimit(0, syncDivisions().size() - 1, static_cast<int>(std::round(norm * (syncDivisions().size() - 1))));
}

double knobValueFromDivisionIndex(const juce::Slider& slider, const int divisionIndex)
{
    const auto clamped = juce::jlimit(0, syncDivisions().size() - 1, divisionIndex);
    const auto norm = static_cast<double>(clamped) / static_cast<double>(juce::jmax(1, syncDivisions().size() - 1));
    return slider.getMinimum() + norm * (slider.getMaximum() - slider.getMinimum());
}
}

class PHASEUSDelayAudioProcessorEditor::FilterResponseView final : public juce::Component
{
public:
    std::function<int()> getFilterType;
    std::function<float()> getCutoff;
    std::function<float()> getQ;
    std::function<float()> getCombMs;
    std::function<float()> getCombFeedback;
    std::function<bool()> getDarkMode;
    std::function<void(float, float)> onDragPoint;

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        const auto dark = getDarkMode != nullptr ? getDarkMode() : true;
        const auto line = dark ? juce::Colour::fromRGB(70, 72, 78) : juce::Colour::fromRGB(180, 180, 182);
        const auto accent = juce::Colour::fromRGB(80, 190, 240);
        const auto bg = dark ? juce::Colour::fromRGB(10, 12, 16) : juce::Colour::fromRGB(248, 248, 248);

        g.setColour(bg);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(line.withAlpha(0.45f));
        g.drawRoundedRectangle(r, 6.0f, 1.0f);

        for (int i = 1; i < 4; ++i)
        {
            const auto x = juce::jmap(static_cast<float>(i), 0.0f, 4.0f, r.getX(), r.getRight());
            const auto y = juce::jmap(static_cast<float>(i), 0.0f, 4.0f, r.getY(), r.getBottom());
            g.drawVerticalLine(static_cast<int>(x), r.getY(), r.getBottom());
            g.drawHorizontalLine(static_cast<int>(y), r.getX(), r.getRight());
        }

        juce::Path response;
        const auto type = getFilterType != nullptr ? getFilterType() : 0;
        const auto cutoff = getCutoff != nullptr ? getCutoff() : 1000.0f;
        const auto q = getQ != nullptr ? juce::jlimit(0.1f, 8.0f, getQ()) : 0.7f;
        const auto combMs = getCombMs != nullptr ? juce::jlimit(1.0f, 100.0f, getCombMs()) : 15.0f;
        const auto combFb = getCombFeedback != nullptr ? juce::jlimit(0.0f, 0.97f, getCombFeedback()) : 0.3f;
        const auto xNorm = normalizedLogFreq(cutoff, 20.0f, 18000.0f);
        const auto xCut = juce::jmap(xNorm, r.getX(), r.getRight());
        const auto qNorm = juce::jlimit(0.0f, 1.0f, (q - 0.1f) / 7.9f);
        const auto yQ = juce::jmap(1.0f - qNorm, r.getY() + 8.0f, r.getBottom() - 8.0f);

        response.startNewSubPath(r.getX(), r.getBottom() - 12.0f);

        for (int i = 1; i <= 120; ++i)
        {
            const auto x = juce::jmap(static_cast<float>(i), 0.0f, 120.0f, r.getX(), r.getRight());
            const auto t = (x - r.getX()) / r.getWidth();
            float y = r.getBottom() - 12.0f;

            if (type == 1) // LP
            {
                const auto d = juce::jmax(0.0f, (t - xNorm) * (10.0f + q * 2.0f));
                y = (r.getY() + 12.0f) + (r.getHeight() - 24.0f) * juce::jlimit(0.0f, 1.0f, d);
            }
            else if (type == 2) // HP
            {
                const auto d = juce::jmax(0.0f, (xNorm - t) * (10.0f + q * 2.0f));
                y = (r.getY() + 12.0f) + (r.getHeight() - 24.0f) * juce::jlimit(0.0f, 1.0f, d);
            }
            else if (type == 3) // Notch
            {
                const auto dist = std::abs(t - xNorm);
                const auto width = 0.03f + (1.0f - qNorm) * 0.22f;
                const auto notch = juce::jlimit(0.0f, 1.0f, 1.0f - dist / width);
                y = (r.getY() + 8.0f) + notch * (r.getHeight() * 0.6f);
            }
            else if (type == 4) // Comb
            {
                const auto density = juce::jmap(combMs, 1.0f, 100.0f, 22.0f, 3.5f);
                const auto phase = t * juce::MathConstants<float>::twoPi * density;
                const auto amp = 0.12f + combFb * 0.45f;
                y = r.getCentreY() - std::sin(phase) * r.getHeight() * amp;
            }
            else // Off
            {
                y = r.getCentreY();
            }

            response.lineTo(x, y);
        }

        g.setColour(accent.withAlpha(0.9f));
        g.strokePath(response, juce::PathStrokeType(2.0f));

        if (type != 0)
        {
            const auto handleY = type == 4 ? juce::jmap(1.0f - combFb, r.getY() + 12.0f, r.getBottom() - 12.0f) : yQ;
            g.setColour(accent);
            g.fillEllipse(xCut - 5.0f, handleY - 5.0f, 10.0f, 10.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        mouseDrag(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (onDragPoint == nullptr)
            return;

        auto r = getLocalBounds().toFloat().reduced(4.0f);
        const auto nx = juce::jlimit(0.0f, 1.0f, (event.position.x - r.getX()) / juce::jmax(1.0f, r.getWidth()));
        const auto ny = juce::jlimit(0.0f, 1.0f, (event.position.y - r.getY()) / juce::jmax(1.0f, r.getHeight()));
        onDragPoint(nx, ny);
    }
};

PHASEUSDelayAudioProcessorEditor::PHASEUSDelayAudioProcessorEditor(PHASEUSDelayAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    for (auto* tab : { &simpleTab, &pingTab, &grainTab })
    {
        addAndMakeVisible(*tab);
        tab->setClickingTogglesState(false);
        tab->setLookAndFeel(&squareButtonLookAndFeel());
    }

    simpleTab.onClick = [this] { setModeFromTab(simpleMode); };
    pingTab.onClick = [this] { setModeFromTab(pingMode); };
    grainTab.onClick = [this] { setModeFromTab(grainMode); };

    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
    outputGainSlider.setTextBoxIsEditable(true);
    outputGainSlider.setTextValueSuffix(" dB");
    outputGainSlider.setDoubleClickReturnValue(true, 0.0);
    outputGainSlider.textFromValueFunction = [](const double v) { return juce::String(v, 1) + " dB"; };
    outputGainSlider.valueFromTextFunction = [](const juce::String& t) { return t.upToFirstOccurrenceOf("dB", false, false).getDoubleValue(); };
    addAndMakeVisible(outputGainSlider);
    setupLabel(outputGainLabel, "Output");

    setupToggle(loFiToggle, "LoFi");
    presetBar = std::make_unique<phaseus::PresetBar>(audioProcessor.apvts, audioProcessor.getName());
    addAndMakeVisible(*presetBar);
    setupKnob(loFiAmountSlider, " %", 0.35);
    loFiAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    loFiAmountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    loFiAmountSlider.textFromValueFunction = [](const double v) { return juce::String(v * 100.0, 1) + " %"; };
    loFiAmountSlider.valueFromTextFunction = [](const juce::String& t) { return juce::jlimit(0.0, 1.0, t.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0); };
    setupLabel(loFiAmountLabel, "LoFi Amount");
    setupToggle(reverseToggle, "Reverse");
    setupKnob(reverseMixSlider, " %", 0.0);
    setupKnob(reverseStartSlider, " ms", 24.0);
    setupKnob(reverseEndSlider, " ms", 8.0);
    reverseMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    reverseMixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    reverseStartSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    reverseStartSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    reverseEndSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    reverseEndSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    reverseMixSlider.textFromValueFunction = [](const double v) { return juce::String(v * 100.0, 1) + " %"; };
    reverseMixSlider.valueFromTextFunction = [](const juce::String& t) { return juce::jlimit(0.0, 1.0, t.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0); };
    setupLabel(reverseMixLabel, "Reverse Mix");
    setupLabel(reverseStartLabel, "Start Offset");
    setupLabel(reverseEndLabel, "End Offset");

    setupToggle(duckingToggle, "Ducking");
    setupKnob(duckingAmountSlider, " %", 0.45);
    setupKnob(duckingAttackSlider, " ms", 16.0);
    setupKnob(duckingReleaseSlider, " ms", 220.0);
    for (auto* slider : { &duckingAmountSlider, &duckingAttackSlider, &duckingReleaseSlider })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    }
    duckingAmountSlider.textFromValueFunction = [](const double v) { return juce::String(v * 100.0, 1) + " %"; };
    duckingAmountSlider.valueFromTextFunction = [](const juce::String& t) { return juce::jlimit(0.0, 1.0, t.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0); };
    setupLabel(duckingAmountLabel, "Duck Amount");
    setupLabel(duckingAttackLabel, "Duck Attack");
    setupLabel(duckingReleaseLabel, "Duck Release");

    setupToggle(diffusionToggle, "Diffusion");
    setupKnob(diffusionAmountSlider, " %", 0.30);
    setupKnob(diffusionSizeSlider, " ms", 24.0);
    for (auto* slider : { &diffusionAmountSlider, &diffusionSizeSlider })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    }
    diffusionAmountSlider.textFromValueFunction = [](const double v) { return juce::String(v * 100.0, 1) + " %"; };
    diffusionAmountSlider.valueFromTextFunction = [](const juce::String& t) { return juce::jlimit(0.0, 1.0, t.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0); };
    setupLabel(diffusionAmountLabel, "Diff Amount");
    setupLabel(diffusionSizeLabel, "Diff Size");

    rightPanelScrollBar.addListener(this);
    rightPanelScrollBar.setAutoHide(false);
    addAndMakeVisible(rightPanelScrollBar);

    int bgBytes = 0;
    if (const auto* bgData = BinaryData::getNamedResource("dayidelay_jpg", bgBytes))
        backgroundImage = juce::ImageFileFormat::loadFrom(bgData, static_cast<size_t>(bgBytes));

    int chainBytes = 0;
    if (const auto* chainData = BinaryData::getNamedResource("chain_png", chainBytes))
    {
        chainImage = juce::ImageFileFormat::loadFrom(chainData, static_cast<size_t>(chainBytes));

        if (chainImage.isValid())
        {
            auto whiteChain = chainImage.convertedToFormat(juce::Image::PixelFormat::ARGB);

            for (int y = 0; y < whiteChain.getHeight(); ++y)
            {
                for (int x = 0; x < whiteChain.getWidth(); ++x)
                {
                    auto px = whiteChain.getPixelAt(x, y);
                    if (px.getAlpha() > 0)
                        whiteChain.setPixelAt(x, y, juce::Colour::fromRGBA(255, 255, 255, px.getAlpha()));
                }
            }

            chainImage = std::move(whiteChain);
        }
    }

    setupKnob(wetDrySlider, " %", 0.35);
    setupLabel(wetDryLabel, "Wet / Dry");

    setupToggle(simpleSyncToggle, "Tempo Sync");
    simpleSyncDivisionBox.addItemList(syncDivisions(), 1);
    addAndMakeVisible(simpleSyncDivisionBox);
    setupLabel(simpleSyncDivisionLabel, "Division");
    setupKnob(simpleTimeRandomSlider, " %", 0.0);
    setupKnob(simpleFeedbackRandomSlider, " %", 0.0);
    setupLabel(simpleTimeRandomLabel, "Time Random");
    setupLabel(simpleFeedbackRandomLabel, "Feedback Random");

    setupToggle(pingSyncToggle, "Tempo Sync");
    pingSyncDivisionLeftBox.addItemList(syncDivisions(), 1);
    pingSyncDivisionRightBox.addItemList(syncDivisions(), 1);
    addAndMakeVisible(pingSyncDivisionLeftBox);
    addAndMakeVisible(pingSyncDivisionRightBox);
    setupToggle(pingLinkTimesToggle, "Link Time L/R");
    setupToggle(pingLinkFeedbackToggle, "Link Feedback L/R");
    setupLabel(pingSyncDivisionLeftLabel, "Div L");
    setupLabel(pingSyncDivisionRightLabel, "Div R");
    setupKnob(pingTimeLeftRandomSlider, " %", 0.0);
    setupKnob(pingTimeRightRandomSlider, " %", 0.0);
    setupKnob(pingFeedbackLeftRandomSlider, " %", 0.0);
    setupKnob(pingFeedbackRightRandomSlider, " %", 0.0);
    setupLabel(pingTimeLeftRandomLabel, "Time L Rand");
    setupLabel(pingTimeRightRandomLabel, "Time R Rand");
    setupLabel(pingFeedbackLeftRandomLabel, "Fb L Rand");
    setupLabel(pingFeedbackRightRandomLabel, "Fb R Rand");

    setupToggle(grainPingPongToggle, "PingPong Output");
    setupKnob(grainPingPongPanSlider, " %", 0.8);
    grainPingPongPanSlider.textFromValueFunction = [](const double v) { return juce::String(v * 100.0, 1) + " %"; };
    grainPingPongPanSlider.valueFromTextFunction = [](const juce::String& t) { return juce::jlimit(0.0, 1.0, t.upToFirstOccurrenceOf("%", false, false).getDoubleValue() / 100.0); };
    grainPingPongPanSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    grainPingPongPanSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 18);
    setupLabel(grainPingPongPanLabel, "PingPong Pan");
    filterSlot1Tab.setClickingTogglesState(false);
    filterSlot2Tab.setClickingTogglesState(false);
    filterSlot1Tab.setLookAndFeel(&squareButtonLookAndFeel());
    filterSlot2Tab.setLookAndFeel(&squareButtonLookAndFeel());
    addAndMakeVisible(filterSlot1Tab);
    addAndMakeVisible(filterSlot2Tab);
    setupToggle(filterEnableToggle, "Enabled");
    filterInputBox.addItemList({ "Dry", "Wet", "Filter1" }, 1);
    addAndMakeVisible(filterInputBox);
    filterRouteBox.addItemList({ "Out", "Filter2" }, 1);
    addAndMakeVisible(filterRouteBox);
    setupLabel(filterInputLabel, "Input");
    setupLabel(filterRouteLabel, "Route");

    filterTypeBox.addItemList({ "Off", "LowPass", "HighPass", "Notch", "Comb" }, 1);
    filterTypeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    addAndMakeVisible(filterTypeBox);
    filterTypeBox.setVisible(false);
    setupFilterTab(filterOffTab, "Off");
    setupFilterTab(filterLpTab, "LP");
    setupFilterTab(filterHpTab, "HP");
    setupFilterTab(filterNotchTab, "Notch");
    setupFilterTab(filterCombTab, "Comb");
    for (auto* tab : { &filterOffTab, &filterLpTab, &filterHpTab, &filterNotchTab, &filterCombTab })
        tab->setLookAndFeel(&squareButtonLookAndFeel());
    filterResponseView = std::make_unique<FilterResponseView>();
    addAndMakeVisible(*filterResponseView);
    setupLabel(filterTypeLabel, "Filter");
    setupKnob(filterCutoffSlider, " Hz", 1800.0);
    setupKnob(filterQSlider, "", 0.8);
    setupKnob(filterMixSlider, " %", 0.35);
    setupKnob(filterCombMsSlider, " ms", 12.0);
    setupKnob(filterCombFeedbackSlider, " %", 0.45);

    for (auto* slider : { &filterCutoffSlider, &filterQSlider, &filterMixSlider, &filterCombMsSlider, &filterCombFeedbackSlider })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 18);
    }

    setupLabel(filterCutoffLabel, "Cutoff");
    setupLabel(filterQLabel, "Q");
    setupLabel(filterMixLabel, "Mix");
    setupLabel(filterCombMsLabel, "Comb Time");
    setupLabel(filterCombFeedbackLabel, "Comb FB");

    setupKnob(simpleTimeSlider, " ms", 420.0);
    setupKnob(simpleFeedbackSlider, " %", 0.35);
    setupLabel(simpleTimeLabel, "Time");
    setupLabel(simpleFeedbackLabel, "Feedback");

    setupKnob(pingTimeLeftSlider, " ms", 330.0);
    setupKnob(pingTimeRightSlider, " ms", 500.0);
    setupKnob(pingFeedbackLeftSlider, " %", 0.40);
    setupKnob(pingFeedbackRightSlider, " %", 0.40);
    setupLabel(pingTimeLeftLabel, "Time Left");
    setupLabel(pingTimeRightLabel, "Time Right");
    setupLabel(pingFeedbackLeftLabel, "Feedback Left");
    setupLabel(pingFeedbackRightLabel, "Feedback Right");
    setupLabel(pingTimeLinkLabel, "", true);
    setupLabel(pingFeedbackLinkLabel, "", true);

    setupKnob(grainBaseTimeSlider, " ms", 300.0);
    setupKnob(grainSizeSlider, " ms", 60.0);
    setupKnob(grainRateSlider, " Hz", 6.0);
    setupKnob(grainPitchSlider, " st", 0.0);
    setupKnob(grainAmountSlider, " %", 0.55);
    setupKnob(grainFeedbackSlider, " %", 0.35);

    setupKnob(grainBaseTimeRandomSlider, " %", 0.0);
    setupKnob(grainSizeRandomSlider, " %", 0.0);
    setupKnob(grainRateRandomSlider, " %", 0.0);
    setupKnob(grainPitchRandomSlider, " st", 0.0);
    setupKnob(grainAmountRandomSlider, " %", 0.0);
    setupKnob(grainFeedbackRandomSlider, " %", 0.0);

    setupLabel(grainBaseTimeLabel, "Base Time");
    setupLabel(grainSizeLabel, "Size");
    setupLabel(grainRateLabel, "Rate");
    setupLabel(grainPitchLabel, "Pitch");
    setupLabel(grainAmountLabel, "Amount");
    setupLabel(grainFeedbackLabel, "Feedback");

    setupLabel(grainBaseTimeRandomLabel, "Random");
    setupLabel(grainSizeRandomLabel, "Random");
    setupLabel(grainRateRandomLabel, "Random");
    setupLabel(grainPitchRandomLabel, "Random");
    setupLabel(grainAmountRandomLabel, "Random");
    setupLabel(grainFeedbackRandomLabel, "Random");

    wetDryAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::wetDry, wetDrySlider);
    outputGainAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::outputGainDb, outputGainSlider);
    loFiAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::loFiMode, loFiToggle);
    loFiAmountAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::loFiAmount, loFiAmountSlider);
    reverseEnableAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::reverseEnable, reverseToggle);
    reverseMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::reverseMix, reverseMixSlider);
    reverseStartAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::reverseStartOffsetMs, reverseStartSlider);
    reverseEndAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::reverseEndOffsetMs, reverseEndSlider);
    duckingEnableAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::duckingEnable, duckingToggle);
    duckingAmountAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::duckingAmount, duckingAmountSlider);
    duckingAttackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::duckingAttackMs, duckingAttackSlider);
    duckingReleaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::duckingReleaseMs, duckingReleaseSlider);
    diffusionEnableAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::diffusionEnable, diffusionToggle);
    diffusionAmountAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::diffusionAmount, diffusionAmountSlider);
    diffusionSizeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::diffusionSizeMs, diffusionSizeSlider);

    simpleTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleTimeMs, simpleTimeSlider);
    simpleFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleFeedback, simpleFeedbackSlider);
    simpleTimeRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleTimeRandomPct, simpleTimeRandomSlider);
    simpleFeedbackRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleFeedbackRandomPct, simpleFeedbackRandomSlider);
    simpleSyncAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleSync, simpleSyncToggle);
    simpleSyncDivisionAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::simpleSyncDivision, simpleSyncDivisionBox);

    pingTimeLeftAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingTimeLeftMs, pingTimeLeftSlider);
    pingTimeRightAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingTimeRightMs, pingTimeRightSlider);
    pingFeedbackLeftAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingFeedbackLeft, pingFeedbackLeftSlider);
    pingFeedbackRightAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingFeedbackRight, pingFeedbackRightSlider);
    pingTimeLeftRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingTimeLeftRandomPct, pingTimeLeftRandomSlider);
    pingTimeRightRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingTimeRightRandomPct, pingTimeRightRandomSlider);
    pingFeedbackLeftRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingFeedbackLeftRandomPct, pingFeedbackLeftRandomSlider);
    pingFeedbackRightRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingFeedbackRightRandomPct, pingFeedbackRightRandomSlider);
    pingSyncAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingSync, pingSyncToggle);
    pingSyncDivisionLeftAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingSyncDivisionLeft, pingSyncDivisionLeftBox);
    pingSyncDivisionRightAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingSyncDivisionRight, pingSyncDivisionRightBox);
    pingLinkTimesAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingLinkTimes, pingLinkTimesToggle);
    pingLinkFeedbackAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::pingLinkFeedback, pingLinkFeedbackToggle);

    grainBaseTimeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainBaseTimeMs, grainBaseTimeSlider);
    grainSizeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainSizeMs, grainSizeSlider);
    grainRateAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainRateHz, grainRateSlider);
    grainPitchAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainPitchSemitones, grainPitchSlider);
    grainAmountAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainAmount, grainAmountSlider);
    grainFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainFeedback, grainFeedbackSlider);

    grainBaseTimeRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainBaseTimeRandomPct, grainBaseTimeRandomSlider);
    grainSizeRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainSizeRandomPct, grainSizeRandomSlider);
    grainRateRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainRateRandomPct, grainRateRandomSlider);
    grainPitchRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainPitchRandomPct, grainPitchRandomSlider);
    grainAmountRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainAmountRandomPct, grainAmountRandomSlider);
    grainFeedbackRandomAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainFeedbackRandomPct, grainFeedbackRandomSlider);

    grainPingPongAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainPingPong, grainPingPongToggle);
    grainPingPongPanAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::grainPingPongPan, grainPingPongPanSlider);
    bindFilterSlotAttachments(1);

    filterOffTab.onClick = [this] { setFilterTypeFromTab(0); };
    filterLpTab.onClick = [this] { setFilterTypeFromTab(1); };
    filterHpTab.onClick = [this] { setFilterTypeFromTab(2); };
    filterNotchTab.onClick = [this] { setFilterTypeFromTab(3); };
    filterCombTab.onClick = [this] { setFilterTypeFromTab(4); };
    filterSlot1Tab.onClick = [this] { setActiveFilterSlot(1); };
    filterSlot2Tab.onClick = [this] { setActiveFilterSlot(2); };

    filterTypeBox.onChange = [this]
    {
        updateFilterTabState();
        updateModeVisibility();
    };
    simpleSyncDivisionBox.onChange = [this] { updateSyncTimeKnobState(); };
    pingSyncDivisionLeftBox.onChange = [this] { updateSyncTimeKnobState(); };
    pingSyncDivisionRightBox.onChange = [this] { updateSyncTimeKnobState(); };

    filterResponseView->getFilterType = [this]
    {
        return filterTypeBox.getSelectedItemIndex();
    };
    filterResponseView->getCutoff = [this]
    {
        return static_cast<float>(filterCutoffSlider.getValue());
    };
    filterResponseView->getQ = [this]
    {
        return static_cast<float>(filterQSlider.getValue());
    };
    filterResponseView->getCombMs = [this]
    {
        return static_cast<float>(filterCombMsSlider.getValue());
    };
    filterResponseView->getCombFeedback = [this]
    {
        return static_cast<float>(filterCombFeedbackSlider.getValue());
    };
    filterResponseView->getDarkMode = [] { return true; };
    filterResponseView->onDragPoint = [this](const float nx, const float ny)
    {
        const auto type = filterTypeBox.getSelectedItemIndex();
        if (type == 4)
        {
            const auto combMs = juce::jmap(nx, 1.0f, 100.0f);
            const auto combFb = juce::jlimit(0.0f, 0.97f, 1.0f - ny);
            filterCombMsSlider.setValue(combMs, juce::sendNotificationSync);
            filterCombFeedbackSlider.setValue(combFb, juce::sendNotificationSync);
        }
        else if (type != 0)
        {
            const auto cutoff = freqFromNormalized(nx, 20.0f, 18000.0f);
            const auto q = juce::jmap(1.0f - ny, 0.1f, 8.0f);
            filterCutoffSlider.setValue(cutoff, juce::sendNotificationSync);
            filterQSlider.setValue(q, juce::sendNotificationSync);
        }
    };

    filterCutoffSlider.onValueChange = [this] { filterResponseView->repaint(); };
    filterQSlider.onValueChange = [this] { filterResponseView->repaint(); };
    filterCombMsSlider.onValueChange = [this] { filterResponseView->repaint(); };
    filterCombFeedbackSlider.onValueChange = [this] { filterResponseView->repaint(); };

    pingTimeLeftSlider.onValueChange = [this]
    {
        if (pingSyncToggle.getToggleState())
        {
            pushSyncDivisionFromKnob(pingTimeLeftSlider, pingSyncDivisionLeftBox);
            if (pingLinkTimesToggle.getToggleState())
                updateSyncTimeKnobState();
        }
        syncPingPongKnobsFromUI();
    };
    pingTimeRightSlider.onValueChange = [this]
    {
        if (pingSyncToggle.getToggleState() && !pingLinkTimesToggle.getToggleState())
            pushSyncDivisionFromKnob(pingTimeRightSlider, pingSyncDivisionRightBox);
    };
    pingFeedbackLeftSlider.onValueChange = [this] { syncPingPongKnobsFromUI(); };
    simpleTimeSlider.onValueChange = [this]
    {
        if (simpleSyncToggle.getToggleState())
            pushSyncDivisionFromKnob(simpleTimeSlider, simpleSyncDivisionBox);
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

    simpleSyncToggle.onClick = [this]
    {
        updateSyncTimeKnobState();
        updateModeVisibility();
    };
    pingSyncToggle.onClick = [this]
    {
        updateSyncTimeKnobState();
        updateModeVisibility();
    };
    loFiToggle.onClick = [this] { updateModeVisibility(); };
    reverseToggle.onClick = [this] { updateModeVisibility(); };
    duckingToggle.onClick = [this] { updateModeVisibility(); };
    diffusionToggle.onClick = [this] { updateModeVisibility(); };
    grainPingPongToggle.onClick = [this] { updateModeVisibility(); };

    setResizable(true, true);
    setResizeLimits(900, 560, 1800, 1100);
    setSize(1120, 660);
    setupSyncTimeTextFunctions();
    applyTheme();
    syncPingPongKnobsFromUI();
    updateSyncTimeKnobState();
    updateTabState();
    updateModeVisibility();
    startTimerHz(10);
}

void PHASEUSDelayAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(6, 6, 7));

    if (backgroundImage.isValid())
    {
        g.setOpacity(0.31f);
        g.drawImageWithin(backgroundImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::fillDestination);
        g.setOpacity(1.0f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillRect(getLocalBounds());

    auto header = getLocalBounds().removeFromTop(56);
    const auto titleColour = juce::Colours::white;

    g.setColour(titleColour);
    g.setFont(juce::FontOptions(28.0f));
    const int titleWidth = juce::jlimit(220, 420, getWidth() / 3);
    g.drawText("Phaseus Dayi Delay", header.removeFromLeft(titleWidth).reduced(18, 8), juce::Justification::centredLeft);

    g.setColour(titleColour.withAlpha(0.15f));
    g.fillRect(18, 54, getWidth() - 36, 1);

    if (chainImage.isValid())
    {
        auto drawChain = [&](const juce::Label& target)
        {
            if (!target.isVisible())
                return;

            auto r = target.getBounds().reduced(1);
            g.setOpacity(0.90f);
            g.drawImageWithin(chainImage, r.getX(), r.getY(), r.getWidth(), r.getHeight(), juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        };

        drawChain(pingTimeLinkLabel);
        drawChain(pingFeedbackLinkLabel);
    }
}

void PHASEUSDelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    auto header = bounds.removeFromTop(44);
    const auto fullHeader = header;
    auto rightHeader = header.removeFromRight(300);
    auto outArea = rightHeader.removeFromRight(184);
    outputGainLabel.setBounds(outArea.removeFromLeft(56));
    outputGainSlider.setBounds(outArea.reduced(0, 2));
    loFiToggle.setBounds(rightHeader.removeFromRight(92).withSizeKeepingCentre(80, 24));
    if (presetBar != nullptr)
    {
        const int presetW = juce::jlimit(300, 520, getWidth() / 3);
        auto presetArea = juce::Rectangle<int>(0, 0, presetW, 28).withCentre(fullHeader.getCentre());
        const int leftLimit = fullHeader.getX() + juce::jlimit(170, 300, getWidth() / 5);
        const int rightLimit = outArea.getX() - 8;
        presetArea.setX(juce::jlimit(leftLimit, juce::jmax(leftLimit, rightLimit - presetArea.getWidth()), presetArea.getX()));
        presetBar->setBounds(presetArea);
    }

    auto tabs = bounds.removeFromTop(46);
    const int tabW = juce::jlimit(98, 170, tabs.getWidth() / 5);
    simpleTab.setBounds(tabs.removeFromLeft(tabW).reduced(4, 6));
    pingTab.setBounds(tabs.removeFromLeft(tabW).reduced(4, 6));
    grainTab.setBounds(tabs.removeFromLeft(tabW).reduced(4, 6));

    auto content = bounds.reduced(2, 8);
    const int rightW = juce::jlimit(260, 350, static_cast<int>(content.getWidth() * 0.30f));
    auto rightPanel = content.removeFromRight(rightW).reduced(8);
    rightPanelScrollBar.setBounds(rightPanel.removeFromRight(16));
    rightPanelViewportBounds = rightPanel;
    const auto currentMaxScroll = juce::jmax(0, rightPanelContentHeight - rightPanelViewportBounds.getHeight());
    rightPanelScrollOffset = juce::jlimit(0, currentMaxScroll, rightPanelScrollOffset);
    auto leftPanel = content.reduced(10, 8);
    const int filterPanelH = juce::jlimit(190, 250, leftPanel.getHeight() / 3);
    auto filterPanel = leftPanel.removeFromBottom(filterPanelH).reduced(4, 4);
    auto leftTopPanel = leftPanel;
    const auto currentMode = audioProcessor.getCurrentModeIndex();
    constexpr int labelH = 16;
    constexpr int textH = 18;
    constexpr int wetKnob = 63; // header hero knob

    // Fixed standard sizes (autoscale removed).
    constexpr int mainKnob = 82;
    constexpr int miniKnob = 44;

    auto placeMain = [&](juce::Label& label, juce::Slider& slider, const juce::Rectangle<int>& area)
    {
        label.setBounds(area.getX(), area.getY(), area.getWidth(), labelH);
        slider.setBounds(area.withTrimmedTop(labelH).withHeight(mainKnob + textH));
    };

    auto placeMini = [&](juce::Label& label, juce::Slider& slider, const juce::Rectangle<int>& area)
    {
        label.setBounds(area.getX(), area.getY(), area.getWidth(), labelH);
        slider.setBounds(area.withTrimmedTop(labelH).withHeight(miniKnob + textH));
    };

    auto layoutSimpleLeft = [&]()
    {
        const int mainBlockH = labelH + mainKnob + textH;
        const int miniBlockH = labelH + miniKnob + textH;
        const int pairBlockH = juce::jmax(mainBlockH, miniBlockH);
        auto row = leftTopPanel.removeFromTop(pairBlockH + 18);
        const int colW = juce::jmax(mainKnob + miniKnob + 24, row.getWidth() / 2);
        auto leftCol = row.removeFromLeft(colW).reduced(12, 0);
        auto rightCol = row.removeFromLeft(colW).reduced(12, 0);

        auto placePair = [&](juce::Label& mainLabel, juce::Slider& mainSlider, juce::Label& randLabel, juce::Slider& randSlider, juce::Rectangle<int> col)
        {
            const int gap = 10;
            const int totalW = mainKnob + gap + miniKnob;
            const int startX = col.getX() + juce::jmax(0, (col.getWidth() - totalW) / 2);
            const int y = col.getY();

            placeMain(mainLabel, mainSlider, { startX, y, mainKnob, mainBlockH });
            placeMini(randLabel, randSlider, { startX + mainKnob + gap, y, miniKnob, miniBlockH });
        };

        placePair(simpleTimeLabel, simpleTimeSlider, simpleTimeRandomLabel, simpleTimeRandomSlider, leftCol);
        placePair(simpleFeedbackLabel, simpleFeedbackSlider, simpleFeedbackRandomLabel, simpleFeedbackRandomSlider, rightCol);
    };

    auto layoutPingLeft = [&]()
    {
        const int mainBlockH = labelH + mainKnob + textH;
        const int miniBlockH = labelH + miniKnob + textH;
        const int blockH = juce::jmax(mainBlockH, miniBlockH) + 14;
        auto topRow = leftTopPanel.removeFromTop(blockH);
        auto bottomRow = leftTopPanel.removeFromTop(blockH);
        const int colW = juce::jmax(mainKnob + miniKnob + 24, leftTopPanel.getWidth() / 2);

        auto placePair = [&](juce::Label& mainLabel, juce::Slider& mainSlider, juce::Label& randLabel, juce::Slider& randSlider, juce::Rectangle<int> col)
        {
            const int gap = 10;
            const int totalW = mainKnob + gap + miniKnob;
            const int startX = col.getX() + juce::jmax(0, (col.getWidth() - totalW) / 2);
            const int y = col.getY();

            placeMain(mainLabel, mainSlider, { startX, y, mainKnob, mainBlockH });
            placeMini(randLabel, randSlider, { startX + mainKnob + gap, y, miniKnob, miniBlockH });
        };

        auto tl = topRow.removeFromLeft(colW).reduced(12, 0);
        auto tr = topRow.removeFromLeft(colW).reduced(12, 0);
        placePair(pingTimeLeftLabel, pingTimeLeftSlider, pingTimeLeftRandomLabel, pingTimeLeftRandomSlider, tl);
        placePair(pingTimeRightLabel, pingTimeRightSlider, pingTimeRightRandomLabel, pingTimeRightRandomSlider, tr);
        pingTimeLinkLabel.setBounds((tl.getRight() + tr.getX()) / 2 - 10, tl.getY() + labelH + 28, 20, 20);

        auto bl = bottomRow.removeFromLeft(colW).reduced(12, 0);
        auto br = bottomRow.removeFromLeft(colW).reduced(12, 0);
        placePair(pingFeedbackLeftLabel, pingFeedbackLeftSlider, pingFeedbackLeftRandomLabel, pingFeedbackLeftRandomSlider, bl);
        placePair(pingFeedbackRightLabel, pingFeedbackRightSlider, pingFeedbackRightRandomLabel, pingFeedbackRightRandomSlider, br);
        pingFeedbackLinkLabel.setBounds((bl.getRight() + br.getX()) / 2 - 10, bl.getY() + labelH + 28, 20, 20);
    };

    auto layoutGrainLeft = [&]()
    {
        auto area = leftTopPanel;
        constexpr int cols = 3;
        constexpr int rows = 2;
        const int colW = juce::jmax(mainKnob + 24, area.getWidth() / cols);
        const int rowH = juce::jmax(1, area.getHeight() / rows);

        auto placeGrainCell = [&](int index, juce::Label& mainLabel, juce::Slider& mainSlider, juce::Label& randLabel, juce::Slider& randSlider)
        {
            const int col = index % cols;
            const int row = index / cols;
            auto cell = juce::Rectangle<int>(area.getX() + col * colW, area.getY() + row * rowH, colW, rowH).reduced(8, 2);

            const int gap = 8;
            const int totalW = mainKnob + gap + miniKnob;
            const int mainX = cell.getX() + juce::jmax(0, (cell.getWidth() - totalW) / 2);
            const int miniX = mainX + mainKnob + gap;

            placeMain(mainLabel, mainSlider, { mainX, cell.getY(), mainKnob, labelH + mainKnob + textH });
            placeMini(randLabel, randSlider, { miniX, cell.getY(), miniKnob, labelH + miniKnob + textH });
        };

        placeGrainCell(0, grainBaseTimeLabel, grainBaseTimeSlider, grainBaseTimeRandomLabel, grainBaseTimeRandomSlider);
        placeGrainCell(1, grainSizeLabel, grainSizeSlider, grainSizeRandomLabel, grainSizeRandomSlider);
        placeGrainCell(2, grainRateLabel, grainRateSlider, grainRateRandomLabel, grainRateRandomSlider);
        placeGrainCell(3, grainPitchLabel, grainPitchSlider, grainPitchRandomLabel, grainPitchRandomSlider);
        placeGrainCell(4, grainAmountLabel, grainAmountSlider, grainAmountRandomLabel, grainAmountRandomSlider);
        placeGrainCell(5, grainFeedbackLabel, grainFeedbackSlider, grainFeedbackRandomLabel, grainFeedbackRandomSlider);
    };

    auto layoutSimpleRight = [&](int& y)
    {
        const int x = rightPanelViewportBounds.getX();
        const int w = rightPanelViewportBounds.getWidth();

        simpleSyncToggle.setBounds(x, y, w, 24);
        y += 24;
        if (simpleSyncDivisionBox.isVisible())
        {
            simpleSyncDivisionLabel.setBounds(x, y, w, labelH);
            y += labelH;
            simpleSyncDivisionBox.setBounds(x, y, w, 24);
            y += 30;
        }

    };

    auto layoutPingRight = [&](int& y)
    {
        const int x = rightPanelViewportBounds.getX();
        const int w = rightPanelViewportBounds.getWidth();

        pingSyncToggle.setBounds(x, y, w, 24);
        y += 24;
        pingLinkTimesToggle.setBounds(x, y, w, 24);
        y += 24;
        pingLinkFeedbackToggle.setBounds(x, y, w, 24);
        y += 28;

        if (pingSyncDivisionLeftBox.isVisible())
        {
            pingSyncDivisionLeftLabel.setBounds(x, y, w, labelH);
            y += labelH;
            pingSyncDivisionLeftBox.setBounds(x, y, w, 24);
            y += 28;
        }
        if (pingSyncDivisionRightBox.isVisible())
        {
            pingSyncDivisionRightLabel.setBounds(x, y, w, labelH);
            y += labelH;
            pingSyncDivisionRightBox.setBounds(x, y, w, 24);
            y += 28;
        }

    };

    auto layoutGrainRight = [&](int& y)
    {
        const int x = rightPanelViewportBounds.getX();
        const int w = rightPanelViewportBounds.getWidth();
        grainPingPongToggle.setBounds(x, y, w, 24);
        y += 24;
        if (grainPingPongPanSlider.isVisible())
        {
            y += 4;
            auto row = juce::Rectangle<int>(x, y, w, 22);
            y += 24;
            grainPingPongPanLabel.setBounds(row.removeFromLeft(96));
            grainPingPongPanSlider.setBounds(row);
        }
    };

    auto layoutFilterLeft = [&]()
    {
        const int x = filterPanel.getX();
        const int w = filterPanel.getWidth();
        int y = filterPanel.getY();
        filterTypeLabel.setBounds(x, y, w, labelH);
        y += labelH;
        auto slotRow = juce::Rectangle<int>(x, y, w, 24);
        y += 26;
        const int slotW = juce::jmax(72, (slotRow.getWidth() - 6) / 2);
        filterSlot1Tab.setBounds(slotRow.removeFromLeft(slotW));
        slotRow.removeFromLeft(6);
        filterSlot2Tab.setBounds(slotRow.removeFromLeft(slotW));
        filterEnableToggle.setBounds(x, y, 112, 22);
        y += 24;

        auto tabRow = juce::Rectangle<int>(x, y, w, 22);
        y += 22;
        const int filterTabW = juce::jmax(40, (tabRow.getWidth() - 8) / 5);
        filterOffTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterLpTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterHpTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterNotchTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterCombTab.setBounds(tabRow.removeFromLeft(filterTabW));

        y += 2;
        filterResponseView->setBounds(x, y, w, 62);
        y += 66;

        if (filterCutoffSlider.isVisible())
        {
            auto rowCutoff = juce::Rectangle<int>(x, y, w, 20);
            y += 18;
            filterCutoffLabel.setBounds(rowCutoff.removeFromLeft(52));
            filterCutoffSlider.setBounds(rowCutoff);

            auto rowQ = juce::Rectangle<int>(x, y, w, 18);
            y += 18;
            filterQLabel.setBounds(rowQ.removeFromLeft(52));
            filterQSlider.setBounds(rowQ);
        }

        auto rowMix = juce::Rectangle<int>(x, y, w, 18);
        y += 18;
        filterMixLabel.setBounds(rowMix.removeFromLeft(52));
        filterMixSlider.setBounds(rowMix);

        if (filterCombMsSlider.isVisible())
        {
            auto rowCombMs = juce::Rectangle<int>(x, y, w, 18);
            y += 18;
            filterCombMsLabel.setBounds(rowCombMs.removeFromLeft(80));
            filterCombMsSlider.setBounds(rowCombMs);

            auto rowCombFb = juce::Rectangle<int>(x, y, w, 18);
            y += 18;
            filterCombFeedbackLabel.setBounds(rowCombFb.removeFromLeft(80));
            filterCombFeedbackSlider.setBounds(rowCombFb);
        }
    };

    const int rightStartY = rightPanelViewportBounds.getY();
    int rightY = rightStartY - rightPanelScrollOffset;
    wetDryLabel.setBounds(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), labelH);
    rightY += labelH;
    const int wetSliderW = 96;
    const int wetX = rightPanelViewportBounds.getX() + (rightPanelViewportBounds.getWidth() - wetSliderW) / 2;
    wetDrySlider.setBounds(wetX, rightY, wetSliderW, wetKnob + textH);
    rightY += wetKnob + textH + 10;

    if (loFiAmountSlider.isVisible())
    {
        auto rowLoFi = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        loFiAmountLabel.setBounds(rowLoFi.removeFromLeft(88));
        loFiAmountSlider.setBounds(rowLoFi);
        rightY += 24;
    }

    reverseToggle.setBounds(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 24);
    rightY += 24;
    if (reverseMixSlider.isVisible())
    {
        auto rowReverseMix = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        reverseMixLabel.setBounds(rowReverseMix.removeFromLeft(88));
        reverseMixSlider.setBounds(rowReverseMix);
        rightY += 20;

        auto rowReverseStart = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        reverseStartLabel.setBounds(rowReverseStart.removeFromLeft(88));
        reverseStartSlider.setBounds(rowReverseStart);
        rightY += 20;

        auto rowReverseEnd = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        reverseEndLabel.setBounds(rowReverseEnd.removeFromLeft(88));
        reverseEndSlider.setBounds(rowReverseEnd);
        rightY += 24;
    }

    duckingToggle.setBounds(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 24);
    rightY += 24;
    if (duckingAmountSlider.isVisible())
    {
        auto rowDuckAmt = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        duckingAmountLabel.setBounds(rowDuckAmt.removeFromLeft(88));
        duckingAmountSlider.setBounds(rowDuckAmt);
        rightY += 20;

        auto rowDuckAtk = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        duckingAttackLabel.setBounds(rowDuckAtk.removeFromLeft(88));
        duckingAttackSlider.setBounds(rowDuckAtk);
        rightY += 20;

        auto rowDuckRel = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        duckingReleaseLabel.setBounds(rowDuckRel.removeFromLeft(88));
        duckingReleaseSlider.setBounds(rowDuckRel);
        rightY += 24;
    }

    diffusionToggle.setBounds(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 24);
    rightY += 24;
    if (diffusionAmountSlider.isVisible())
    {
        auto rowDiffAmt = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        diffusionAmountLabel.setBounds(rowDiffAmt.removeFromLeft(88));
        diffusionAmountSlider.setBounds(rowDiffAmt);
        rightY += 20;

        auto rowDiffSize = juce::Rectangle<int>(rightPanelViewportBounds.getX(), rightY, rightPanelViewportBounds.getWidth(), 20);
        diffusionSizeLabel.setBounds(rowDiffSize.removeFromLeft(88));
        diffusionSizeSlider.setBounds(rowDiffSize);
        rightY += 24;
    }

    if (currentMode == simpleMode)
    {
        layoutSimpleLeft();
        layoutSimpleRight(rightY);
    }
    else if (currentMode == pingMode)
    {
        layoutPingLeft();
        layoutPingRight(rightY);
    }
    else
    {
        layoutGrainLeft();
        layoutGrainRight(rightY);
    }

    layoutFilterLeft();

    rightPanelContentHeight = rightY - (rightStartY - rightPanelScrollOffset);
    const auto visibleHeight = rightPanelViewportBounds.getHeight();
    const auto maxScroll = juce::jmax(0, rightPanelContentHeight - visibleHeight);
    rightPanelScrollOffset = juce::jlimit(0, maxScroll, rightPanelScrollOffset);

    rightPanelScrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(visibleHeight, rightPanelContentHeight)));
    rightPanelScrollBar.setCurrentRange(static_cast<double>(rightPanelScrollOffset), static_cast<double>(visibleHeight));
    rightPanelScrollBar.setSingleStepSize(24.0);
    rightPanelScrollBar.setVisible(true);
    rightPanelScrollBar.setEnabled(maxScroll > 0);
}

void PHASEUSDelayAudioProcessorEditor::setupKnob(juce::Slider& slider, const juce::String& suffix, const double defaultValue)
{
    slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setTextBoxIsEditable(true);
    slider.setTextValueSuffix(suffix);
    slider.setDoubleClickReturnValue(true, defaultValue);
    addAndMakeVisible(slider);
}

void PHASEUSDelayAudioProcessorEditor::setupLabel(juce::Label& label, const juce::String& text, const bool centred)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(centred ? juce::Justification::centred : juce::Justification::centredLeft);
    addAndMakeVisible(label);
}

void PHASEUSDelayAudioProcessorEditor::setupToggle(juce::ToggleButton& button, const juce::String& text)
{
    button.setButtonText(text);
    addAndMakeVisible(button);
}

void PHASEUSDelayAudioProcessorEditor::setupFilterTab(juce::TextButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.setClickingTogglesState(false);
    addAndMakeVisible(button);
}

void PHASEUSDelayAudioProcessorEditor::setupSyncTimeTextFunctions()
{
    simpleTimeSlider.textFromValueFunction = [this](const double value)
    {
        if (simpleSyncToggle.getToggleState())
        {
            const auto idx = divisionIndexFromKnobValue(simpleTimeSlider, value);
            return syncDivisions()[idx];
        }

        return juce::String(value, 2) + " ms";
    };

    pingTimeLeftSlider.textFromValueFunction = [this](const double value)
    {
        if (pingSyncToggle.getToggleState())
        {
            const auto idx = divisionIndexFromKnobValue(pingTimeLeftSlider, value);
            return syncDivisions()[idx];
        }

        return juce::String(value, 2) + " ms";
    };

    pingTimeRightSlider.textFromValueFunction = [this](const double value)
    {
        if (pingSyncToggle.getToggleState())
        {
            const auto idx = divisionIndexFromKnobValue(pingTimeRightSlider, value);
            return syncDivisions()[idx];
        }

        return juce::String(value, 2) + " ms";
    };
}

void PHASEUSDelayAudioProcessorEditor::updateSyncTimeKnobState()
{
    if (syncTimeUpdateInProgress)
        return;

    syncTimeUpdateInProgress = true;

    if (simpleSyncToggle.getToggleState())
        simpleTimeSlider.setValue(knobValueFromDivisionIndex(simpleTimeSlider, simpleSyncDivisionBox.getSelectedItemIndex()), juce::dontSendNotification);

    if (pingSyncToggle.getToggleState())
    {
        pingTimeLeftSlider.setValue(knobValueFromDivisionIndex(pingTimeLeftSlider, pingSyncDivisionLeftBox.getSelectedItemIndex()), juce::dontSendNotification);

        if (pingLinkTimesToggle.getToggleState())
        {
            pingSyncDivisionRightBox.setSelectedItemIndex(pingSyncDivisionLeftBox.getSelectedItemIndex(), juce::sendNotificationSync);
            pingTimeRightSlider.setValue(knobValueFromDivisionIndex(pingTimeRightSlider, pingSyncDivisionLeftBox.getSelectedItemIndex()), juce::dontSendNotification);
        }
        else
        {
            pingTimeRightSlider.setValue(knobValueFromDivisionIndex(pingTimeRightSlider, pingSyncDivisionRightBox.getSelectedItemIndex()), juce::dontSendNotification);
        }
    }

    simpleTimeSlider.updateText();
    pingTimeLeftSlider.updateText();
    pingTimeRightSlider.updateText();

    syncTimeUpdateInProgress = false;
}

void PHASEUSDelayAudioProcessorEditor::pushSyncDivisionFromKnob(juce::Slider& knob, juce::ComboBox& divisionBox)
{
    if (syncTimeUpdateInProgress)
        return;

    syncTimeUpdateInProgress = true;
    const auto divisionIndex = divisionIndexFromKnobValue(knob, knob.getValue());
    divisionBox.setSelectedItemIndex(divisionIndex, juce::sendNotificationSync);
    knob.setValue(knobValueFromDivisionIndex(knob, divisionIndex), juce::dontSendNotification);
    knob.updateText();
    syncTimeUpdateInProgress = false;
}

void PHASEUSDelayAudioProcessorEditor::bindFilterSlotAttachments(const int slotIndex)
{
    activeFilterSlot = juce::jlimit(1, 2, slotIndex);

    filterTypeAttachment.reset();
    filterEnableAttachment.reset();
    filterInputAttachment.reset();
    filterRouteAttachment.reset();
    filterCutoffAttachment.reset();
    filterQAttachment.reset();
    filterMixAttachment.reset();
    filterCombMsAttachment.reset();
    filterCombFeedbackAttachment.reset();

    if (activeFilterSlot == 1)
    {
        filterInputBox.clear(juce::dontSendNotification);
        filterInputBox.addItemList({ "Dry", "Wet" }, 1);
        filterTypeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterType, filterTypeBox);
        filterEnableAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter1Enable, filterEnableToggle);
        filterInputAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter1Input, filterInputBox);
        filterRouteAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter1Route, filterRouteBox);
        filterCutoffAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterCutoffHz, filterCutoffSlider);
        filterQAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterQ, filterQSlider);
        filterMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterMix, filterMixSlider);
        filterCombMsAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterCombMs, filterCombMsSlider);
        filterCombFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterCombFeedback, filterCombFeedbackSlider);
    }
    else
    {
        filterInputBox.clear(juce::dontSendNotification);
        filterInputBox.addItemList({ "Dry", "Wet", "Filter1" }, 1);
        filterTypeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2Type, filterTypeBox);
        filterEnableAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2Enable, filterEnableToggle);
        filterInputAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2Input, filterInputBox);
        filterCutoffAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2CutoffHz, filterCutoffSlider);
        filterQAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2Q, filterQSlider);
        filterMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2Mix, filterMixSlider);
        filterCombMsAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2CombMs, filterCombMsSlider);
        filterCombFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filter2CombFeedback, filterCombFeedbackSlider);
    }

    updateFilterTabState();
}

void PHASEUSDelayAudioProcessorEditor::setActiveFilterSlot(const int slotIndex)
{
    bindFilterSlotAttachments(slotIndex);
    updateModeVisibility();
}

void PHASEUSDelayAudioProcessorEditor::applyTheme()
{
    const auto bg = juce::Colour::fromRGB(10, 10, 12);
    const auto fg = juce::Colour::fromRGB(243, 243, 243);
    const auto accent = juce::Colour::fromRGB(230, 230, 230);

    for (auto* tab : { &simpleTab, &pingTab, &grainTab })
    {
        tab->setColour(juce::TextButton::buttonColourId, bg.withAlpha(0.7f));
        tab->setColour(juce::TextButton::textColourOffId, fg.withAlpha(0.85f));
        tab->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }

    for (auto* tab : { &filterOffTab, &filterLpTab, &filterHpTab, &filterNotchTab, &filterCombTab })
    {
        tab->setColour(juce::TextButton::buttonColourId, bg.withAlpha(0.78f));
        tab->setColour(juce::TextButton::textColourOffId, fg.withAlpha(0.78f));
        tab->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }

    for (auto* tab : { &filterSlot1Tab, &filterSlot2Tab })
    {
        tab->setColour(juce::TextButton::buttonColourId, bg.withAlpha(0.78f));
        tab->setColour(juce::TextButton::textColourOffId, fg.withAlpha(0.78f));
        tab->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }

    for (auto* combo : { &simpleSyncDivisionBox, &pingSyncDivisionLeftBox, &pingSyncDivisionRightBox, &filterInputBox, &filterRouteBox })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, bg);
        combo->setColour(juce::ComboBox::textColourId, fg);
        combo->setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.25f));
    }

    for (auto* toggle : { &loFiToggle, &reverseToggle, &duckingToggle, &diffusionToggle, &simpleSyncToggle, &pingSyncToggle, &pingLinkTimesToggle, &pingLinkFeedbackToggle, &grainPingPongToggle, &filterEnableToggle })
        toggle->setColour(juce::ToggleButton::textColourId, fg);

    rightPanelScrollBar.setColour(juce::ScrollBar::thumbColourId, juce::Colour::fromRGB(80, 190, 240).withAlpha(0.78f));
    rightPanelScrollBar.setColour(juce::ScrollBar::trackColourId, fg.withAlpha(0.22f));

    if (presetBar != nullptr)
        presetBar->applyTheme(true);

    for (auto* label : { &wetDryLabel, &simpleTimeLabel, &simpleFeedbackLabel, &simpleSyncDivisionLabel,
                         &outputGainLabel,
                         &simpleTimeRandomLabel, &simpleFeedbackRandomLabel,
                         &pingTimeLeftLabel, &pingTimeRightLabel, &pingTimeLinkLabel, &pingFeedbackLeftLabel,
                         &pingFeedbackRightLabel, &pingFeedbackLinkLabel, &pingSyncDivisionLeftLabel,
                         &pingSyncDivisionRightLabel, &pingTimeLeftRandomLabel, &pingTimeRightRandomLabel,
                         &pingFeedbackLeftRandomLabel, &pingFeedbackRightRandomLabel,
                         &grainBaseTimeLabel, &grainSizeLabel, &grainRateLabel,
                         &grainPitchLabel, &grainAmountLabel, &grainFeedbackLabel, &grainBaseTimeRandomLabel,
                         &grainSizeRandomLabel, &grainRateRandomLabel, &grainPitchRandomLabel,
                         &grainAmountRandomLabel, &grainFeedbackRandomLabel, &grainPingPongPanLabel, &loFiAmountLabel,
                         &reverseMixLabel, &reverseStartLabel, &reverseEndLabel, &duckingAmountLabel, &duckingAttackLabel, &duckingReleaseLabel,
                         &diffusionAmountLabel, &diffusionSizeLabel,
                         &filterTypeLabel, &filterInputLabel, &filterRouteLabel, &filterCutoffLabel,
                         &filterQLabel, &filterMixLabel, &filterCombMsLabel, &filterCombFeedbackLabel })
    {
        label->setColour(juce::Label::textColourId, fg.withAlpha(0.88f));
    }

    for (auto* slider : { &wetDrySlider, &simpleTimeSlider, &simpleFeedbackSlider, &pingTimeLeftSlider, &pingTimeRightSlider,
                          &outputGainSlider,
                          &simpleTimeRandomSlider, &simpleFeedbackRandomSlider,
                          &pingFeedbackLeftSlider, &pingFeedbackRightSlider, &grainBaseTimeSlider, &grainSizeSlider,
                          &grainRateSlider, &grainPitchSlider, &grainAmountSlider, &grainFeedbackSlider,
                          &grainPingPongPanSlider, &loFiAmountSlider,
                          &reverseMixSlider, &reverseStartSlider, &reverseEndSlider, &duckingAmountSlider, &duckingAttackSlider, &duckingReleaseSlider,
                          &diffusionAmountSlider, &diffusionSizeSlider,
                          &pingTimeLeftRandomSlider, &pingTimeRightRandomSlider, &pingFeedbackLeftRandomSlider, &pingFeedbackRightRandomSlider,
                          &grainBaseTimeRandomSlider, &grainSizeRandomSlider, &grainRateRandomSlider,
                          &grainPitchRandomSlider, &grainAmountRandomSlider, &grainFeedbackRandomSlider,
                          &filterCutoffSlider, &filterQSlider, &filterMixSlider, &filterCombMsSlider, &filterCombFeedbackSlider })
    {
        slider->setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, fg.withAlpha(0.25f));
        slider->setColour(juce::Slider::trackColourId, fg.withAlpha(0.32f));
        slider->setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(80, 190, 240));
        slider->setColour(juce::Slider::textBoxTextColourId, fg);
        slider->setColour(juce::Slider::textBoxOutlineColourId, fg.withAlpha(0.2f));
        slider->setColour(juce::Slider::textBoxBackgroundColourId, bg.withAlpha(0.92f));
    }

    if (filterResponseView != nullptr)
        filterResponseView->repaint();

    updateTabState();
    updateFilterTabState();
}

void PHASEUSDelayAudioProcessorEditor::updateModeVisibility()
{
    const auto mode = audioProcessor.getCurrentModeIndex();

    const auto showSimple = mode == simpleMode;
    const auto showSimpleSync = showSimple && simpleSyncToggle.getToggleState();
    simpleTimeLabel.setVisible(showSimple);
    simpleTimeSlider.setVisible(showSimple);
    simpleFeedbackLabel.setVisible(showSimple);
    simpleFeedbackSlider.setVisible(showSimple);

    simpleSyncToggle.setVisible(showSimple);
    simpleSyncDivisionLabel.setVisible(showSimpleSync);
    simpleSyncDivisionBox.setVisible(showSimpleSync);
    simpleTimeRandomLabel.setVisible(showSimple);
    simpleTimeRandomSlider.setVisible(showSimple);
    simpleFeedbackRandomLabel.setVisible(showSimple);
    simpleFeedbackRandomSlider.setVisible(showSimple);

    const auto showPing = mode == pingMode;
    const auto showPingSync = showPing && pingSyncToggle.getToggleState();

    pingTimeLeftLabel.setVisible(showPing);
    pingTimeLeftSlider.setVisible(showPing);
    pingTimeRightLabel.setVisible(showPing);
    pingTimeRightSlider.setVisible(showPing);
    pingTimeLinkLabel.setVisible(showPing && pingLinkTimesToggle.getToggleState());

    pingFeedbackLeftLabel.setVisible(showPing);
    pingFeedbackLeftSlider.setVisible(showPing);
    pingFeedbackRightLabel.setVisible(showPing);
    pingFeedbackRightSlider.setVisible(showPing);
    pingFeedbackLinkLabel.setVisible(showPing && pingLinkFeedbackToggle.getToggleState());

    pingSyncToggle.setVisible(showPing);
    pingLinkTimesToggle.setVisible(showPing);
    pingLinkFeedbackToggle.setVisible(showPing);

    pingSyncDivisionLeftLabel.setVisible(showPingSync);
    pingSyncDivisionLeftBox.setVisible(showPingSync);

    const auto showPingRightDiv = showPingSync && !pingLinkTimesToggle.getToggleState();
    pingSyncDivisionRightLabel.setVisible(showPingRightDiv);
    pingSyncDivisionRightBox.setVisible(showPingRightDiv);
    pingTimeLeftRandomLabel.setVisible(showPing);
    pingTimeLeftRandomSlider.setVisible(showPing);
    pingTimeRightRandomLabel.setVisible(showPing && !pingLinkTimesToggle.getToggleState());
    pingTimeRightRandomSlider.setVisible(showPing && !pingLinkTimesToggle.getToggleState());
    pingFeedbackLeftRandomLabel.setVisible(showPing);
    pingFeedbackLeftRandomSlider.setVisible(showPing);
    pingFeedbackRightRandomLabel.setVisible(showPing && !pingLinkFeedbackToggle.getToggleState());
    pingFeedbackRightRandomSlider.setVisible(showPing && !pingLinkFeedbackToggle.getToggleState());

    pingTimeRightSlider.setEnabled(!pingLinkTimesToggle.getToggleState());
    pingFeedbackRightSlider.setEnabled(!pingLinkFeedbackToggle.getToggleState());

    const auto showGrain = mode == grainMode;

    grainBaseTimeLabel.setVisible(showGrain);
    grainBaseTimeSlider.setVisible(showGrain);
    grainSizeLabel.setVisible(showGrain);
    grainSizeSlider.setVisible(showGrain);
    grainRateLabel.setVisible(showGrain);
    grainRateSlider.setVisible(showGrain);
    grainPitchLabel.setVisible(showGrain);
    grainPitchSlider.setVisible(showGrain);
    grainAmountLabel.setVisible(showGrain);
    grainAmountSlider.setVisible(showGrain);
    grainFeedbackLabel.setVisible(showGrain);
    grainFeedbackSlider.setVisible(showGrain);

    grainBaseTimeRandomLabel.setVisible(showGrain);
    grainBaseTimeRandomSlider.setVisible(showGrain);
    grainSizeRandomLabel.setVisible(showGrain);
    grainSizeRandomSlider.setVisible(showGrain);
    grainRateRandomLabel.setVisible(showGrain);
    grainRateRandomSlider.setVisible(showGrain);
    grainPitchRandomLabel.setVisible(showGrain);
    grainPitchRandomSlider.setVisible(showGrain);
    grainAmountRandomLabel.setVisible(showGrain);
    grainAmountRandomSlider.setVisible(showGrain);
    grainFeedbackRandomLabel.setVisible(showGrain);
    grainFeedbackRandomSlider.setVisible(showGrain);

    grainPingPongToggle.setVisible(showGrain);
    grainPingPongPanLabel.setVisible(showGrain && grainPingPongToggle.getToggleState());
    grainPingPongPanSlider.setVisible(showGrain && grainPingPongToggle.getToggleState());

    const auto filterActive = true;
    const auto filterType = filterTypeBox.getSelectedItemIndex();
    const auto combFilter = filterType == 4;
    filterTypeLabel.setVisible(filterActive);
    filterSlot1Tab.setVisible(filterActive);
    filterSlot2Tab.setVisible(filterActive);
    filterEnableToggle.setVisible(filterActive);
    filterInputLabel.setVisible(false);
    filterInputBox.setVisible(false);
    filterRouteLabel.setVisible(false);
    filterRouteBox.setVisible(false);
    filterTypeBox.setVisible(false);
    filterOffTab.setVisible(filterActive);
    filterLpTab.setVisible(filterActive);
    filterHpTab.setVisible(filterActive);
    filterNotchTab.setVisible(filterActive);
    filterCombTab.setVisible(filterActive);
    if (filterResponseView != nullptr)
        filterResponseView->setVisible(filterActive);

    filterCutoffLabel.setVisible(filterActive && !combFilter);
    filterCutoffSlider.setVisible(filterActive && !combFilter);
    filterQLabel.setVisible(filterActive && !combFilter);
    filterQSlider.setVisible(filterActive && !combFilter);
    filterMixLabel.setVisible(filterActive);
    filterMixSlider.setVisible(filterActive);
    filterCombMsLabel.setVisible(filterActive && combFilter);
    filterCombMsSlider.setVisible(filterActive && combFilter);
    filterCombFeedbackLabel.setVisible(filterActive && combFilter);
    filterCombFeedbackSlider.setVisible(filterActive && combFilter);

    wetDrySlider.setVisible(true);
    wetDryLabel.setVisible(true);
    loFiAmountLabel.setVisible(loFiToggle.getToggleState());
    loFiAmountSlider.setVisible(loFiToggle.getToggleState());
    reverseToggle.setVisible(true);
    reverseMixLabel.setVisible(reverseToggle.getToggleState());
    reverseMixSlider.setVisible(reverseToggle.getToggleState());
    reverseStartLabel.setVisible(reverseToggle.getToggleState());
    reverseStartSlider.setVisible(reverseToggle.getToggleState());
    reverseEndLabel.setVisible(reverseToggle.getToggleState());
    reverseEndSlider.setVisible(reverseToggle.getToggleState());
    duckingToggle.setVisible(true);
    const auto duckingExpanded = duckingToggle.getToggleState();
    duckingAmountLabel.setVisible(duckingExpanded);
    duckingAmountSlider.setVisible(duckingExpanded);
    duckingAttackLabel.setVisible(duckingExpanded);
    duckingAttackSlider.setVisible(duckingExpanded);
    duckingReleaseLabel.setVisible(duckingExpanded);
    duckingReleaseSlider.setVisible(duckingExpanded);
    diffusionToggle.setVisible(true);
    const auto diffusionExpanded = diffusionToggle.getToggleState();
    diffusionAmountLabel.setVisible(diffusionExpanded);
    diffusionAmountSlider.setVisible(diffusionExpanded);
    diffusionSizeLabel.setVisible(diffusionExpanded);
    diffusionSizeSlider.setVisible(diffusionExpanded);

    updateTabState();
    updateFilterTabState();
    updateSyncTimeKnobState();
    resized();
    repaint();
}

void PHASEUSDelayAudioProcessorEditor::updateTabState()
{
    const auto mode = audioProcessor.getCurrentModeIndex();
    const auto active = juce::Colour::fromRGB(240, 240, 240);

    simpleTab.setToggleState(mode == simpleMode, juce::dontSendNotification);
    pingTab.setToggleState(mode == pingMode, juce::dontSendNotification);
    grainTab.setToggleState(mode == grainMode, juce::dontSendNotification);

    for (auto* tab : { &simpleTab, &pingTab, &grainTab })
        tab->setColour(juce::TextButton::buttonOnColourId, active);
}

void PHASEUSDelayAudioProcessorEditor::updateFilterTabState()
{
    const auto type = filterTypeBox.getSelectedItemIndex();
    const auto active = juce::Colour::fromRGB(236, 236, 236);
    const auto inactive = juce::Colour::fromRGB(24, 24, 26);

    filterOffTab.setToggleState(type == 0, juce::dontSendNotification);
    filterLpTab.setToggleState(type == 1, juce::dontSendNotification);
    filterHpTab.setToggleState(type == 2, juce::dontSendNotification);
    filterNotchTab.setToggleState(type == 3, juce::dontSendNotification);
    filterCombTab.setToggleState(type == 4, juce::dontSendNotification);

    for (auto* tab : { &filterOffTab, &filterLpTab, &filterHpTab, &filterNotchTab, &filterCombTab })
        tab->setColour(juce::TextButton::buttonOnColourId, active);

    filterSlot1Tab.setToggleState(activeFilterSlot == 1, juce::dontSendNotification);
    filterSlot2Tab.setToggleState(activeFilterSlot == 2, juce::dontSendNotification);
    filterSlot1Tab.setColour(juce::TextButton::buttonOnColourId, active);
    filterSlot2Tab.setColour(juce::TextButton::buttonOnColourId, active);
    filterSlot1Tab.setColour(juce::TextButton::buttonColourId, inactive);
    filterSlot2Tab.setColour(juce::TextButton::buttonColourId, inactive);

    if (filterResponseView != nullptr)
        filterResponseView->repaint();
}

void PHASEUSDelayAudioProcessorEditor::setFilterTypeFromTab(const int filterTypeIndex)
{
    filterTypeBox.setSelectedItemIndex(juce::jlimit(0, 4, filterTypeIndex), juce::sendNotificationSync);
}

void PHASEUSDelayAudioProcessorEditor::setModeFromTab(const int modeIndex)
{
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*>(audioProcessor.apvts.getParameter(PhaseusDelayParams::mode)))
    {
        const auto norm = static_cast<float>(modeIndex) / static_cast<float>(juce::jmax(1, modeParam->choices.size() - 1));
        modeParam->beginChangeGesture();
        modeParam->setValueNotifyingHost(norm);
        modeParam->endChangeGesture();
    }

    updateModeVisibility();
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

void PHASEUSDelayAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    auto scrollHit = rightPanelViewportBounds.withTrimmedLeft(-24);

    if (scrollHit.contains(event.getPosition()) && rightPanelScrollBar.isEnabled())
    {
        const auto delta = static_cast<int>(-wheel.deltaY * 64.0f);
        const auto maxScroll = juce::jmax(0, rightPanelContentHeight - rightPanelViewportBounds.getHeight());
        rightPanelScrollOffset = juce::jlimit(0, maxScroll, rightPanelScrollOffset + delta);
        resized();
        return;
    }

    AudioProcessorEditor::mouseWheelMove(event, wheel);
}

void PHASEUSDelayAudioProcessorEditor::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, const double newRangeStart)
{
    if (scrollBarThatHasMoved != &rightPanelScrollBar)
        return;

    const auto maxScroll = juce::jmax(0, rightPanelContentHeight - rightPanelViewportBounds.getHeight());
    rightPanelScrollOffset = juce::jlimit(0, maxScroll, static_cast<int>(std::round(newRangeStart)));
    resized();
}

void PHASEUSDelayAudioProcessorEditor::timerCallback()
{
    const auto mode = audioProcessor.getCurrentModeIndex();
    const auto simpleSync = simpleSyncToggle.getToggleState();
    const auto pingSync = pingSyncToggle.getToggleState();
    const auto pingLinkTime = pingLinkTimesToggle.getToggleState();
    const auto pingLinkFeedback = pingLinkFeedbackToggle.getToggleState();
    const auto loFiOn = loFiToggle.getToggleState();
    const auto reverseOn = reverseToggle.getToggleState();
    const auto duckingOn = duckingToggle.getToggleState();
    const auto diffusionOn = diffusionToggle.getToggleState();

    if (mode != lastUiMode
        || simpleSync != lastSimpleSyncState
        || pingSync != lastPingSyncState
        || pingLinkTime != lastPingLinkTimeState
        || pingLinkFeedback != lastPingLinkFeedbackState
        || loFiOn != lastLoFiState
        || reverseOn != lastReverseState
        || duckingOn != lastDuckingState
        || diffusionOn != lastDiffusionState)
    {
        lastUiMode = mode;
        lastSimpleSyncState = simpleSync;
        lastPingSyncState = pingSync;
        lastPingLinkTimeState = pingLinkTime;
        lastPingLinkFeedbackState = pingLinkFeedback;
        lastLoFiState = loFiOn;
        lastReverseState = reverseOn;
        lastDuckingState = duckingOn;
        lastDiffusionState = diffusionOn;
        updateModeVisibility();
        return;
    }

    updateTabState();
}
