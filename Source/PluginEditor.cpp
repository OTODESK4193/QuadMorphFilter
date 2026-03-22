#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizer & XYPad (描画ロジックは継続、Enable判定を追加) ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    g.setColour(juce::Colours::cyan);
    auto pos = processor.getCurrentXY();
    auto w = (float)getWidth(); auto h = (float)getHeight();
    juce::Path path;

    for (int px = 0; px <= (int)w; ++px) {
        float freq = 20.0f * std::pow(1000.0f, px / w);
        auto calc = [&](juce::String s) {
            if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f) return 0.0f; // OFFなら0
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

// --- Editor本体 ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    addAndMakeVisible(visualizer); addAndMakeVisible(xyPad);
    setupFilterGroup(groupA, "A", "Filter A"); setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C"); setupFilterGroup(groupD, "D", "Filter D");

    auto setupGlobal = [&](juce::Label& l, juce::Slider& s, juce::String text, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(text, juce::dontSendNotification); addAndMakeVisible(l);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 15); addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, s);
        };
    setupGlobal(lfoRateLabel, lfoRateSlider, "LFO Rate", "lfoRate", lfoR_Att);
    setupGlobal(lfoAmtLabel, lfoAmtSlider, "LFO Amt", "lfoAmount", lfoA_Att);

    setSize(1000, 750); // 横長に調整
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s, juce::String groupName) {
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "enable" + s, g.enableButton);

    g.groupLabel.setText(groupName, juce::dontSendNotification);
    g.groupLabel.setFont(juce::Font(16.0f, juce::Font::bold)); addAndMakeVisible(g.groupLabel);

    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1); addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);

    auto setupParam = [&](juce::Label& l, juce::Slider& sl, juce::String txt, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(txt, juce::dontSendNotification); addAndMakeVisible(l);
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20); addAndMakeVisible(sl);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + s, sl);
        };
    setupParam(g.cutoffLabel, g.cutoff, "Cutoff", "cutoff", g.cAtt);
    setupParam(g.resLabel, g.res, "Res", "res", g.rAtt);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}
void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkgrey.darker(0.8f)); }

void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced(20);

    // 1. 上段（モニターエリア）
    auto topArea = b.removeFromTop(300);
    visualizer.setBounds(topArea.removeFromLeft(topArea.getWidth() * 0.7f).reduced(5));
    auto topRight = topArea;
    xyPad.setBounds(topRight.removeFromTop(topRight.getWidth()).reduced(5));

    // LFOはXYパッドの下に配置
    lfoRateLabel.setBounds(topRight.removeFromTop(20)); lfoRateSlider.setBounds(topRight.removeFromTop(30));
    lfoAmtLabel.setBounds(topRight.removeFromTop(20)); lfoAmtSlider.setBounds(topRight.removeFromTop(30));

    // 2. 下段（フィルターラック）
    b.removeFromTop(20); // 余白
    auto rowH = b.getHeight() / 4;
    layoutFilterGroup(groupA, b.removeFromTop(rowH));
    layoutFilterGroup(groupB, b.removeFromTop(rowH));
    layoutFilterGroup(groupC, b.removeFromTop(rowH));
    layoutFilterGroup(groupD, b.removeFromTop(rowH));
}

void QuadMorphFilterAudioProcessorEditor::layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> b)
{
    auto r = b.reduced(5);
    g.enableButton.setBounds(r.removeFromLeft(30));
    g.groupLabel.setBounds(r.removeFromLeft(80));
    g.type.setBounds(r.removeFromLeft(80).withSizeKeepingCentre(70, 24));

    // Cutoffセクション: 名称(40) -> スライダー(残り半分)
    auto cutArea = r.removeFromLeft(r.getWidth() / 2).reduced(5, 0);
    g.cutoffLabel.setBounds(cutArea.removeFromLeft(50));
    g.cutoff.setBounds(cutArea);

    // Resoセクション: 同上
    auto resArea = r.reduced(5, 0);
    g.resLabel.setBounds(resArea.removeFromLeft(40));
    g.res.setBounds(resArea);
}