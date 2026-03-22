#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizer & XYPad ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    g.setColour(juce::Colours::cyan);
    auto pos = processor.getCurrentXY();
    auto w = (float)getWidth(); auto h = (float)getHeight();
    juce::Path path;

    for (int px = 0; px <= (int)w; ++px) {
        float freq = 20.0f * std::pow(1000.0f, px / w);
        auto calc = [&](juce::String s) {
            if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f) return 0.0f;
            float fc = processor.apvts.getRawParameterValue("cutoff" + s)->load();
            float res = processor.apvts.getRawParameterValue("res" + s)->load();
            int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();
            float r = freq / fc; float r2 = r * r; float d = 1.0f / res;
            float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * d, 2.0f));
            float mag = 1.0f / den;
            if (t == 1) mag = r * mag; if (t == 2) mag = r2 * mag; if (t == 3) mag = std::abs(1.0f - r2) * mag;
            return mag;
            };
        float mag = (calc("A") * (1 - pos.getX()) * (1 - pos.getY())) + (calc("B") * pos.getX() * (1 - pos.getY())) + (calc("C") * (1 - pos.getX()) * pos.getY()) + (calc("D") * pos.getX() * pos.getY());
        float yPos = h - (std::log10(mag + 1.0f) * 1.5f * h * 0.4f + h * 0.1f);
        yPos = juce::jlimit(2.0f, h - 2.0f, yPos);
        if (px == 0) path.startNewSubPath(0, yPos); else path.lineTo((float)px, yPos);
    }
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void XYPadComponent::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(b, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.2f)); g.drawRoundedRectangle(b, 10.0f, 2.0f);

    // ABCD表示
    g.setColour(juce::Colours::white.withAlpha(0.5f)); g.setFont(14.0f);
    g.drawText("A", 10, 10, 20, 20, juce::Justification::centred);
    g.drawText("B", getWidth() - 30, 10, 20, 20, juce::Justification::centred);
    g.drawText("C", 10, getHeight() - 30, 20, 20, juce::Justification::centred);
    g.drawText("D", getWidth() - 30, getHeight() - 30, 20, 20, juce::Justification::centred);

    auto pos = processor.getCurrentXY();
    g.setColour(juce::Colours::cyan);
    g.fillEllipse(pos.getX() * getWidth() - 5, pos.getY() * getHeight() - 5, 10, 10);
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e) {
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
    float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
}

// --- Editor ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    addAndMakeVisible(visualizer); addAndMakeVisible(xyPad);
    setupFilterGroup(groupA, "A", "Filter A"); setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C"); setupFilterGroup(groupD, "D", "Filter D");

    // LFOを最下段に配置するためのセットアップ
    lfoLabel.setText("LFO Modulation", juce::dontSendNotification);
    lfoLabel.setFont(juce::Font(14.0f, juce::Font::bold)); addAndMakeVisible(lfoLabel);

    auto setupLfo = [&](juce::Slider& s, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20); addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, s);
        };
    setupLfo(lfoRateSlider, "lfoRate", lfoR_Att);
    setupLfo(lfoAmtSlider, "lfoAmount", lfoA_Att);

    setSize(1000, 750);
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s, juce::String groupName) {
    g.enableButton.setButtonText(groupName);
    g.enableButton.setClickingTogglesState(true);
    // ON時：シアン、OFF時：グレーで点灯を表現
    g.enableButton.setColour(juce::TextButton::textColourOnId, juce::Colours::cyan);
    g.enableButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "enable" + s, g.enableButton);

    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1); addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);

    auto setupParam = [&](juce::Label& l, juce::Slider& sl, juce::String txt, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(txt, juce::dontSendNotification); addAndMakeVisible(l);
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        // 数値を左、スライダーを右に配置
        sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
        addAndMakeVisible(sl);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + s, sl);
        };
    setupParam(g.cutoffLabel, g.cutoff, "Cutoff", "cutoff", g.cAtt);
    setupParam(g.resLabel, g.res, "Res", "res", g.rAtt);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}
void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkgrey.darker(0.9f)); }

void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced(20);

    // 1. 上段（モニターエリア）
    auto topArea = b.removeFromTop(300);
    visualizer.setBounds(topArea.removeFromLeft(topArea.getWidth() * 0.7f).reduced(5));
    xyPad.setBounds(topArea.reduced(5));

    // 2. 最下段（LFOエリア）
    auto lfoArea = b.removeFromBottom(60);
    lfoLabel.setBounds(lfoArea.removeFromLeft(120));
    lfoRateSlider.setBounds(lfoArea.removeFromLeft(lfoArea.getWidth() / 2).reduced(10, 15));
    lfoAmtSlider.setBounds(lfoArea.reduced(10, 15));

    // 3. 中段（フィルターラック：行間を詰める）
    b.removeFromTop(10);
    int rowH = 45; // 高さを固定して密集させる
    layoutFilterGroup(groupA, b.removeFromTop(rowH));
    layoutFilterGroup(groupB, b.removeFromTop(rowH));
    layoutFilterGroup(groupC, b.removeFromTop(rowH));
    layoutFilterGroup(groupD, b.removeFromTop(rowH));
}

void QuadMorphFilterAudioProcessorEditor::layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> b)
{
    auto r = b.reduced(5, 2);
    g.enableButton.setBounds(r.removeFromLeft(100).reduced(0, 5));
    g.type.setBounds(r.removeFromLeft(80).withSizeKeepingCentre(70, 24));

    // 名称 -> 数値 -> スライダー の順
    auto cutArea = r.removeFromLeft(r.getWidth() / 2).reduced(10, 0);
    g.cutoffLabel.setBounds(cutArea.removeFromLeft(50));
    g.cutoff.setBounds(cutArea);

    auto resArea = r.reduced(10, 0);
    g.resLabel.setBounds(resArea.removeFromLeft(40));
    g.res.setBounds(resArea);
}