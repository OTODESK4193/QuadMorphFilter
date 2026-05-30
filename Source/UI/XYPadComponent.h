// ==========================================
// UI/XYPadComponent.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// 前方宣言: .cpp で完全なインクルードを行う
class QuadMorphFilterAudioProcessor;

class XYPadComponent : public juce::Component,
    public juce::Timer
{
public:
    explicit XYPadComponent(QuadMorphFilterAudioProcessor& p);

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // 角丸矩形の内側のみをヒット判定エリアとする
    // → 描画枠外（コーナーの円弧外）でのクリックを無効化
    bool hitTest(int x, int y) override;

private:
    void updatePosition(const juce::MouseEvent& e);

    QuadMorphFilterAudioProcessor& processor;
    int draggingLfoIndex = -1;

    std::array<juce::Point<float>, 30> trails[3];
    int trailIdx[3] = { 0, 0, 0 };
};