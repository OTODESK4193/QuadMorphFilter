// ==========================================
// UI/FilterVisualizer.h   （V1.1.0 全面改訂）
//
// ・A / B / C / D の個別カーブを淡く、合成カーブをグロー付きの主線で描く
// ・グラデーション塗り + 対数周波数軸 + 圧縮 dB 軸で、
//   共鳴ピークが天井に張り付かず、かつ通過帯域も読み取れるようにした
// ・paint() 内でのアロケーションを避けるため、
//   応答バッファと juce::Path はすべてメンバとして保持する（CLAUDE.md §4）
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class QuadMorphFilterAudioProcessor;

class FilterVisualizer : public juce::Component,
                         private juce::Timer
{
public:
    explicit FilterVisualizer(QuadMorphFilterAudioProcessor& p);
    ~FilterVisualizer() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }

    /** 1 本のフィルターについて、paint 冒頭で読み取った値をまとめて持つ。
        （ピクセルループの中で APVTS を引かないための最適化） */
    struct Snapshot
    {
        bool  enabled = false;
        int   model = 0;
        int   slope = 0;
        int   type = 0;
        float cutoff = 1000.0f;
        float res = 0.707f;
        float weight = 0.0f;    // モーフの重み（0..1）
    };

    void   readSnapshots();
    static float magnitudeFor(float freq, const Snapshot& s);

    // ---- 軸変換 ----
    static float freqToX(float f, juce::Rectangle<float> r) noexcept;
    static float dbToY(float db, juce::Rectangle<float> r) noexcept;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> r) const;
    void drawLegend(juce::Graphics& g, juce::Rectangle<float> r) const;

    QuadMorphFilterAudioProcessor& processor;

    static constexpr int kMaxPoints = 1024;

    std::array<Snapshot, 4> snap;

    // 応答バッファ（paint 内アロケーション禁止のためメンバ保持）
    std::array<std::array<float, kMaxPoints>, 4> mag{};
    std::array<float, kMaxPoints> magSum{};
    std::array<float, kMaxPoints> smoothBuf{};

    // 描画パス（clear() は確保済みメモリを保持するので再アロケートされない）
    std::array<juce::Path, 4> curvePath;
    juce::Path sumPath, fillPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterVisualizer)
};
