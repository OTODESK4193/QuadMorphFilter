// ==========================================
// PluginEditor.h   （V1.1.0 全面改訂）
//
// ・論理サイズ 1000 x 700 の ContentComponent を
//   juce::AffineTransform でスケーリングしてリサイズに対応する
//   （アスペクト比固定なのでレイアウト崩れが起きない）
// ・FILTER / MOD / CONFIG の 3 タブ構成
// ・上部はフィルターカーブ（大）と XY パッドを常時表示
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "UI/ColorPalette.h"
#include "UI/QuadMorphLookAndFeel.h"
#include "UI/FilterVisualizer.h"
#include "UI/XYPadComponent.h"
#include "UI/FilterPanel.h"
#include "UI/ModPanel.h"
#include "UI/ConfigPanel.h"

class QuadMorphFilterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor&);
    ~QuadMorphFilterAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // 論理座標（この寸法で全レイアウトを組み、表示時に拡大縮小する）
    static constexpr int kBaseW = 1000;
    static constexpr int kBaseH = 700;

    // ======================================================================
    //  ContentComponent
    // ======================================================================
    struct Content : public juce::Component,
                     private juce::Timer
    {
        Content(QuadMorphFilterAudioProcessor& p, QuadMorphLookAndFeel& lnf);
        ~Content() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        enum Tab { TabFilter = 0, TabMod, TabConfig };
        static constexpr int kNumTabs = 3;

        void setActiveTab(int tab);
        void styleTabs();
        void applyTheme(int themeIndex);
        static void repaintAll(juce::Component* c);
        void timerCallback() override;

        QuadMorphFilterAudioProcessor& processor;
        QuadMorphLookAndFeel& laf;

        FilterVisualizer visualizer;
        XYPadComponent   xyPad;

        std::array<juce::TextButton, (size_t)kNumTabs> tabButtons;
        int activeTab = TabFilter;

        FilterPanel filterPanel;
        ModPanel    modPanel;
        ConfigPanel configPanel;

        // themeCombo は CONFIG タブへ移設した（V1.1.0）。
        // ここではテーマ変更の取りこぼしを拾うための現在値だけを持つ。
        int lastThemeIndex = -1;

        juce::Rectangle<int> headerArea, tabStripArea;

        // ツールチップ。最後に宣言＝最初に破棄されるので、
        // 参照先のコンポーネントより長生きすることがない。
        juce::TooltipWindow tooltip { this, 900 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
    };

    QuadMorphFilterAudioProcessor& audioProcessor;

    // ---- 破棄順序に注意 ----
    // メンバは宣言の逆順で破棄されるため、content より後ろに置いた laf /
    // constrainer が先に消えることはない（CLAUDE.md §3）。
    QuadMorphLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};
