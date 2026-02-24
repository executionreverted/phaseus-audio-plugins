#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr int simpleMode = 0;
constexpr int pingMode = 1;
constexpr int grainMode = 2;

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
    }

    simpleTab.onClick = [this] { setModeFromTab(simpleMode); };
    pingTab.onClick = [this] { setModeFromTab(pingMode); };
    grainTab.onClick = [this] { setModeFromTab(grainMode); };

    darkModeToggle.setButtonText("Dark");
    darkModeToggle.setToggleState(true, juce::dontSendNotification);
    darkModeToggle.onClick = [this]
    {
        darkModeEnabled = darkModeToggle.getToggleState();
        applyTheme();
        repaint();
    };
    addAndMakeVisible(darkModeToggle);

    setupToggle(loFiToggle, "LoFi");
    rightPanelScrollBar.addListener(this);
    rightPanelScrollBar.setAutoHide(false);
    addAndMakeVisible(rightPanelScrollBar);

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
    filterTypeBox.addItemList({ "Off", "LowPass", "HighPass", "Notch", "Comb" }, 1);
    filterTypeBox.setSelectedItemIndex(0, juce::dontSendNotification);
    addAndMakeVisible(filterTypeBox);
    filterTypeBox.setVisible(false);
    setupFilterTab(filterOffTab, "Off");
    setupFilterTab(filterLpTab, "LP");
    setupFilterTab(filterHpTab, "HP");
    setupFilterTab(filterNotchTab, "Notch");
    setupFilterTab(filterCombTab, "Comb");
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
    setupLabel(pingTimeLinkLabel, "⛓", true);
    setupLabel(pingFeedbackLinkLabel, "⛓", true);

    setupKnob(grainBaseTimeSlider, " ms", 300.0);
    setupKnob(grainSizeSlider, " ms", 60.0);
    setupKnob(grainRateSlider, " Hz", 6.0);
    setupKnob(grainPitchSlider, " st", 0.0);
    setupKnob(grainAmountSlider, " %", 0.55);
    setupKnob(grainFeedbackSlider, " %", 0.35);

    setupKnob(grainBaseTimeRandomSlider, " %", 0.0);
    setupKnob(grainSizeRandomSlider, " %", 0.0);
    setupKnob(grainRateRandomSlider, " %", 0.0);
    setupKnob(grainPitchRandomSlider, " %", 0.0);
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
    loFiAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, PhaseusDelayParams::loFiMode, loFiToggle);

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
    filterTypeAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterType, filterTypeBox);
    filterCutoffAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterCutoffHz, filterCutoffSlider);
    filterQAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterQ, filterQSlider);
    filterMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterMix, filterMixSlider);
    filterCombMsAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterCombMs, filterCombMsSlider);
    filterCombFeedbackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, PhaseusDelayParams::filterCombFeedback, filterCombFeedbackSlider);

    filterOffTab.onClick = [this] { setFilterTypeFromTab(0); };
    filterLpTab.onClick = [this] { setFilterTypeFromTab(1); };
    filterHpTab.onClick = [this] { setFilterTypeFromTab(2); };
    filterNotchTab.onClick = [this] { setFilterTypeFromTab(3); };
    filterCombTab.onClick = [this] { setFilterTypeFromTab(4); };

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
    filterResponseView->getDarkMode = [this]
    {
        return darkModeEnabled;
    };
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
    const auto top = darkModeEnabled ? juce::Colour::fromRGB(8, 8, 9) : juce::Colour::fromRGB(238, 238, 238);
    const auto bottom = darkModeEnabled ? juce::Colour::fromRGB(22, 22, 24) : juce::Colour::fromRGB(252, 252, 252);

    g.setGradientFill(juce::ColourGradient(top, 0.0f, 0.0f, bottom, 0.0f, static_cast<float>(getHeight()), false));
    g.fillAll();

    auto header = getLocalBounds().removeFromTop(56);
    const auto titleColour = darkModeEnabled ? juce::Colours::white : juce::Colour::fromRGB(20, 20, 20);

    g.setColour(titleColour);
    g.setFont(juce::FontOptions(28.0f));
    g.drawText("PHASEUS Delay", header.removeFromLeft(getWidth() - 180).reduced(18, 8), juce::Justification::centredLeft);

    g.setColour(titleColour.withAlpha(0.15f));
    g.fillRect(18, 54, getWidth() - 36, 1);
}

void PHASEUSDelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    auto header = bounds.removeFromTop(44);
    darkModeToggle.setBounds(header.removeFromRight(98).withSizeKeepingCentre(86, 24));
    loFiToggle.setBounds(header.removeFromRight(92).withSizeKeepingCentre(80, 24));

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

    constexpr int mainKnob = 90;
    constexpr int miniKnob = 56;
    constexpr int labelH = 18;
    constexpr int textH = 20;
    constexpr int wetKnob = 63; // 30% smaller than mainKnob

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
        auto row = leftPanel.removeFromTop(mainKnob + textH + labelH + 16);
        const int colW = juce::jmax(mainKnob + 10, row.getWidth() / 2);
        placeMain(simpleTimeLabel, simpleTimeSlider, row.removeFromLeft(colW).reduced(12, 0));
        placeMain(simpleFeedbackLabel, simpleFeedbackSlider, row.removeFromLeft(colW).reduced(12, 0));
    };

    auto layoutPingLeft = [&]()
    {
        auto topRow = leftPanel.removeFromTop(mainKnob + textH + labelH + 18);
        auto bottomRow = leftPanel.removeFromTop(mainKnob + textH + labelH + 18);
        const int colW = juce::jmax(mainKnob + 14, leftPanel.getWidth() / 2);

        auto tl = topRow.removeFromLeft(colW).reduced(12, 0);
        auto tr = topRow.removeFromLeft(colW).reduced(12, 0);
        placeMain(pingTimeLeftLabel, pingTimeLeftSlider, tl);
        placeMain(pingTimeRightLabel, pingTimeRightSlider, tr);
        pingTimeLinkLabel.setBounds((tl.getRight() + tr.getX()) / 2 - 10, tl.getY() + labelH + 28, 20, 20);

        auto bl = bottomRow.removeFromLeft(colW).reduced(12, 0);
        auto br = bottomRow.removeFromLeft(colW).reduced(12, 0);
        placeMain(pingFeedbackLeftLabel, pingFeedbackLeftSlider, bl);
        placeMain(pingFeedbackRightLabel, pingFeedbackRightSlider, br);
        pingFeedbackLinkLabel.setBounds((bl.getRight() + br.getX()) / 2 - 10, bl.getY() + labelH + 28, 20, 20);
    };

    auto layoutGrainLeft = [&]()
    {
        auto area = leftPanel;
        constexpr int cols = 3;
        constexpr int rows = 2;
        const int colW = juce::jmax(mainKnob + 24, area.getWidth() / cols);
        const int rowH = juce::jmax(mainKnob + miniKnob + labelH * 2 + textH * 2 + 18, area.getHeight() / rows);

        auto placeGrainCell = [&](int index, juce::Label& mainLabel, juce::Slider& mainSlider, juce::Label& randLabel, juce::Slider& randSlider)
        {
            const int col = index % cols;
            const int row = index / cols;
            auto cell = juce::Rectangle<int>(area.getX() + col * colW, area.getY() + row * rowH, colW, rowH).reduced(8, 2);

            const int mainX = cell.getX() + (cell.getWidth() - mainKnob) / 2;
            const int miniX = cell.getX() + (cell.getWidth() - miniKnob) / 2;

            placeMain(mainLabel, mainSlider, { mainX, cell.getY(), mainKnob, mainKnob + textH + labelH });
            placeMini(randLabel, randSlider, { miniX, cell.getY() + labelH + mainKnob + textH + 6, miniKnob, miniKnob + textH + labelH });
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

        auto row = juce::Rectangle<int>(x, y, w, miniKnob + textH + labelH + 8);
        y += row.getHeight();
        const int colW = (row.getWidth() - 8) / 2;
        placeMini(simpleTimeRandomLabel, simpleTimeRandomSlider, row.removeFromLeft(colW));
        row.removeFromLeft(8);
        placeMini(simpleFeedbackRandomLabel, simpleFeedbackRandomSlider, row.removeFromLeft(colW));
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

        auto rowA = juce::Rectangle<int>(x, y, w, miniKnob + textH + labelH + 6);
        y += rowA.getHeight();
        if (pingTimeRightRandomSlider.isVisible())
        {
            const int colW = (rowA.getWidth() - 8) / 2;
            placeMini(pingTimeLeftRandomLabel, pingTimeLeftRandomSlider, rowA.removeFromLeft(colW));
            rowA.removeFromLeft(8);
            placeMini(pingTimeRightRandomLabel, pingTimeRightRandomSlider, rowA.removeFromLeft(colW));
        }
        else
        {
            placeMini(pingTimeLeftRandomLabel, pingTimeLeftRandomSlider, rowA.withTrimmedLeft(18).withTrimmedRight(18));
        }

        auto rowB = juce::Rectangle<int>(x, y, w, miniKnob + textH + labelH + 6);
        y += rowB.getHeight();
        if (pingFeedbackRightRandomSlider.isVisible())
        {
            const int colW = (rowB.getWidth() - 8) / 2;
            placeMini(pingFeedbackLeftRandomLabel, pingFeedbackLeftRandomSlider, rowB.removeFromLeft(colW));
            rowB.removeFromLeft(8);
            placeMini(pingFeedbackRightRandomLabel, pingFeedbackRightRandomSlider, rowB.removeFromLeft(colW));
        }
        else
        {
            placeMini(pingFeedbackLeftRandomLabel, pingFeedbackLeftRandomSlider, rowB.withTrimmedLeft(18).withTrimmedRight(18));
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

    auto layoutFilterRight = [&](int& y)
    {
        const int x = rightPanelViewportBounds.getX();
        const int w = rightPanelViewportBounds.getWidth();

        y += 6;
        filterTypeLabel.setBounds(x, y, w, labelH);
        y += labelH;
        auto tabRow = juce::Rectangle<int>(x, y, w, 24);
        y += 24;
        const int filterTabW = juce::jmax(42, (tabRow.getWidth() - 8) / 5);
        filterOffTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterLpTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterHpTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterNotchTab.setBounds(tabRow.removeFromLeft(filterTabW));
        tabRow.removeFromLeft(2);
        filterCombTab.setBounds(tabRow.removeFromLeft(filterTabW));

        y += 4;
        filterResponseView->setBounds(x, y, w, 96);
        y += 102;

        if (filterCutoffSlider.isVisible())
        {
            auto rowCutoff = juce::Rectangle<int>(x, y, w, 20);
            y += 20;
            filterCutoffLabel.setBounds(rowCutoff.removeFromLeft(52));
            filterCutoffSlider.setBounds(rowCutoff);

            auto rowQ = juce::Rectangle<int>(x, y, w, 20);
            y += 20;
            filterQLabel.setBounds(rowQ.removeFromLeft(52));
            filterQSlider.setBounds(rowQ);
        }

        auto rowMix = juce::Rectangle<int>(x, y, w, 20);
        y += 20;
        filterMixLabel.setBounds(rowMix.removeFromLeft(52));
        filterMixSlider.setBounds(rowMix);

        if (filterCombMsSlider.isVisible())
        {
            auto rowCombMs = juce::Rectangle<int>(x, y, w, 20);
            y += 20;
            filterCombMsLabel.setBounds(rowCombMs.removeFromLeft(80));
            filterCombMsSlider.setBounds(rowCombMs);

            auto rowCombFb = juce::Rectangle<int>(x, y, w, 20);
            y += 20;
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

    const auto mode = audioProcessor.getCurrentModeIndex();
    if (mode == simpleMode)
    {
        layoutSimpleLeft();
        layoutSimpleRight(rightY);
    }
    else if (mode == pingMode)
    {
        layoutPingLeft();
        layoutPingRight(rightY);
    }
    else
    {
        layoutGrainLeft();
        layoutGrainRight(rightY);
    }

    layoutFilterRight(rightY);

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

void PHASEUSDelayAudioProcessorEditor::applyTheme()
{
    const auto bg = darkModeEnabled ? juce::Colour::fromRGB(16, 16, 17) : juce::Colour::fromRGB(245, 245, 245);
    const auto fg = darkModeEnabled ? juce::Colour::fromRGB(243, 243, 243) : juce::Colour::fromRGB(24, 24, 24);
    const auto accent = darkModeEnabled ? juce::Colour::fromRGB(230, 230, 230) : juce::Colour::fromRGB(24, 24, 24);

    for (auto* tab : { &simpleTab, &pingTab, &grainTab })
    {
        tab->setColour(juce::TextButton::buttonColourId, bg.withAlpha(0.7f));
        tab->setColour(juce::TextButton::textColourOffId, fg.withAlpha(0.85f));
        tab->setColour(juce::TextButton::textColourOnId, darkModeEnabled ? juce::Colours::black : juce::Colours::white);
    }

    for (auto* tab : { &filterOffTab, &filterLpTab, &filterHpTab, &filterNotchTab, &filterCombTab })
    {
        tab->setColour(juce::TextButton::buttonColourId, bg.withAlpha(0.78f));
        tab->setColour(juce::TextButton::textColourOffId, fg.withAlpha(0.78f));
        tab->setColour(juce::TextButton::textColourOnId, darkModeEnabled ? juce::Colours::black : juce::Colours::white);
    }

    for (auto* combo : { &simpleSyncDivisionBox, &pingSyncDivisionLeftBox, &pingSyncDivisionRightBox })
    {
        combo->setColour(juce::ComboBox::backgroundColourId, bg);
        combo->setColour(juce::ComboBox::textColourId, fg);
        combo->setColour(juce::ComboBox::outlineColourId, fg.withAlpha(0.25f));
    }

    for (auto* toggle : { &darkModeToggle, &loFiToggle, &simpleSyncToggle, &pingSyncToggle, &pingLinkTimesToggle, &pingLinkFeedbackToggle, &grainPingPongToggle })
        toggle->setColour(juce::ToggleButton::textColourId, fg);

    rightPanelScrollBar.setColour(juce::ScrollBar::thumbColourId, juce::Colour::fromRGB(80, 190, 240).withAlpha(0.78f));
    rightPanelScrollBar.setColour(juce::ScrollBar::trackColourId, fg.withAlpha(0.22f));

    for (auto* label : { &wetDryLabel, &simpleTimeLabel, &simpleFeedbackLabel, &simpleSyncDivisionLabel,
                         &simpleTimeRandomLabel, &simpleFeedbackRandomLabel,
                         &pingTimeLeftLabel, &pingTimeRightLabel, &pingTimeLinkLabel, &pingFeedbackLeftLabel,
                         &pingFeedbackRightLabel, &pingFeedbackLinkLabel, &pingSyncDivisionLeftLabel,
                         &pingSyncDivisionRightLabel, &pingTimeLeftRandomLabel, &pingTimeRightRandomLabel,
                         &pingFeedbackLeftRandomLabel, &pingFeedbackRightRandomLabel,
                         &grainBaseTimeLabel, &grainSizeLabel, &grainRateLabel,
                         &grainPitchLabel, &grainAmountLabel, &grainFeedbackLabel, &grainBaseTimeRandomLabel,
                         &grainSizeRandomLabel, &grainRateRandomLabel, &grainPitchRandomLabel,
                         &grainAmountRandomLabel, &grainFeedbackRandomLabel, &grainPingPongPanLabel, &filterTypeLabel, &filterCutoffLabel,
                         &filterQLabel, &filterMixLabel, &filterCombMsLabel, &filterCombFeedbackLabel })
    {
        label->setColour(juce::Label::textColourId, fg.withAlpha(0.88f));
    }

    for (auto* slider : { &wetDrySlider, &simpleTimeSlider, &simpleFeedbackSlider, &pingTimeLeftSlider, &pingTimeRightSlider,
                          &simpleTimeRandomSlider, &simpleFeedbackRandomSlider,
                          &pingFeedbackLeftSlider, &pingFeedbackRightSlider, &grainBaseTimeSlider, &grainSizeSlider,
                          &grainRateSlider, &grainPitchSlider, &grainAmountSlider, &grainFeedbackSlider,
                          &grainPingPongPanSlider,
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
    pingTimeLinkLabel.setVisible(showPing);

    pingFeedbackLeftLabel.setVisible(showPing);
    pingFeedbackLeftSlider.setVisible(showPing);
    pingFeedbackRightLabel.setVisible(showPing);
    pingFeedbackRightSlider.setVisible(showPing);
    pingFeedbackLinkLabel.setVisible(showPing);

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

    updateTabState();
    updateFilterTabState();
    updateSyncTimeKnobState();
    resized();
    repaint();
}

void PHASEUSDelayAudioProcessorEditor::updateTabState()
{
    const auto mode = audioProcessor.getCurrentModeIndex();
    const auto active = darkModeEnabled ? juce::Colour::fromRGB(240, 240, 240) : juce::Colour::fromRGB(20, 20, 20);

    simpleTab.setToggleState(mode == simpleMode, juce::dontSendNotification);
    pingTab.setToggleState(mode == pingMode, juce::dontSendNotification);
    grainTab.setToggleState(mode == grainMode, juce::dontSendNotification);

    for (auto* tab : { &simpleTab, &pingTab, &grainTab })
        tab->setColour(juce::TextButton::buttonOnColourId, active);
}

void PHASEUSDelayAudioProcessorEditor::updateFilterTabState()
{
    const auto type = filterTypeBox.getSelectedItemIndex();
    const auto active = darkModeEnabled ? juce::Colour::fromRGB(236, 236, 236) : juce::Colour::fromRGB(18, 18, 18);

    filterOffTab.setToggleState(type == 0, juce::dontSendNotification);
    filterLpTab.setToggleState(type == 1, juce::dontSendNotification);
    filterHpTab.setToggleState(type == 2, juce::dontSendNotification);
    filterNotchTab.setToggleState(type == 3, juce::dontSendNotification);
    filterCombTab.setToggleState(type == 4, juce::dontSendNotification);

    for (auto* tab : { &filterOffTab, &filterLpTab, &filterHpTab, &filterNotchTab, &filterCombTab })
        tab->setColour(juce::TextButton::buttonOnColourId, active);

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

    if (mode != lastUiMode
        || simpleSync != lastSimpleSyncState
        || pingSync != lastPingSyncState
        || pingLinkTime != lastPingLinkTimeState
        || pingLinkFeedback != lastPingLinkFeedbackState)
    {
        lastUiMode = mode;
        lastSimpleSyncState = simpleSync;
        lastPingSyncState = pingSync;
        lastPingLinkTimeState = pingLinkTime;
        lastPingLinkFeedbackState = pingLinkFeedback;
        updateModeVisibility();
        return;
    }

    updateTabState();
}
