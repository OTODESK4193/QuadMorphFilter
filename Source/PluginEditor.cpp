#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizerの描画ロジック ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    g.setColour(juce::Colours::cyan);
    juce::Path path;
    auto w = (float)getWidth(); auto h = (float)getHeight();
    for (int x = 0; x <= (int)w; ++x) {
        float freq = 20.0f * std::pow(1000.0f, x / w);
        float mag = getMorphedMagnitude(freq);
        float yPos = h - (mag * h * 0.4f + h * 0.2f);
        if (x == 0) path.startNewSubPath(0, yPos); else path.lineTo((float)x, yPos);
    }
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

float FilterVisualizer::getMorphedMagnitude(float freq) {
    float x = processor.apvts.getRawParameterValue("posX")->load();
    float y = processor.apvts.getRawParameterValue("posY")->load();
    auto calc = [&](juce::String s) {
        float fc = processor.apvts.getRawParameterValue("cutoff" + s)->load();
        float res = processor.apvts.getRawParameterValue("res" + s)->load();
        int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();
        float r = freq / fc; float r2 = r * r; float d = 1.0f / res;
        float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * d, 2.0f));
        if (t == 0) return 1.0f / den; if (t == 1) return r / den; if (t == 2) return r2 / den;
        return std::abs(1.0f - r2) / den;
        };
    return (calc("A") * (1 - x) * (1 - y)) + (calc("B") * x * (1 - y)) + (calc("C") * (1 - x) * y) + (calc("D") * x * y);
}

// --- Editorの実装 ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    addAndMakeVisible(visualizer);

    // フィルターA~Dの設定
    setupFilterGroup(groupA, "A"); setupFilterGroup(groupB, "B");
    setupFilterGroup(groupC, "C"); setupFilterGroup(groupD, "D");

    // 全体コントロール (XY, LFO)
    auto setupGlobal = [&](juce::Slider& s, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, s);
        };
    setupGlobal(posXSlider, "posX", xAtt); setupGlobal(posYSlider, "posY", yAtt);
    setupGlobal(lfoRateSlider, "lfoRate", rAtt); setupGlobal(lfoAmtSlider, "lfoAmount", aAtt);

    setSize(900, 600); // 16個置くために少し広げました
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s) {
    g.cutoff.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    g.res.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1);
    addAndMakeVisible(g.cutoff); addAndMakeVisible(g.res); addAndMakeVisible(g.type);
    g.cAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff" + s, g.cutoff);
    g.rAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "res" + s, g.res);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkgrey); }

void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    auto topRow = bounds.removeFromTop(150);
    auto bottomRow = bounds.removeFromBottom(150);

    // 四隅に配置
    groupA.cutoff.setBounds(topRow.removeFromLeft(100)); groupA.res.setBounds(topRow.removeFromLeft(100)); groupA.type.setBounds(topRow.removeFromLeft(80).withSizeKeepingCentre(80, 20));
    groupB.type.setBounds(topRow.removeFromRight(80).withSizeKeepingCentre(80, 20)); groupB.res.setBounds(topRow.removeFromRight(100)); groupB.cutoff.setBounds(topRow.removeFromRight(100));

    groupC.cutoff.setBounds(bottomRow.removeFromLeft(100)); groupC.res.setBounds(bottomRow.removeFromLeft(100)); groupC.type.setBounds(bottomRow.removeFromLeft(80).withSizeKeepingCentre(80, 20));
    groupD.type.setBounds(bottomRow.removeFromRight(80).withSizeKeepingCentre(80, 20)); groupD.res.setBounds(bottomRow.removeFromRight(100)); groupD.cutoff.setBounds(bottomRow.removeFromRight(100));

    // 中央にビジュアライザ
    visualizer.setBounds(bounds.removeFromTop(200));

    // 最下部に全体設定
    auto controlArea = bounds;
    posXSlider.setBounds(controlArea.removeFromLeft(150)); posYSlider.setBounds(controlArea.removeFromLeft(150));
    lfoRateSlider.setBounds(controlArea.removeFromRight(150)); lfoAmtSlider.setBounds(controlArea.removeFromRight(150));
}