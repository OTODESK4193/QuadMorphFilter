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
#include "UI/PresetBrowser.h"

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

        // ---- 【V1.1.0 追加】ヘッダーの Preset / Random / Reset ----
        juce::TextButton presetBtn, randomBtn, resetBtn;

        // ◀ [プリセット名] ▶ の順送りボタン
        juce::TextButton prevBtn, nextBtn;

        /** Factory プリセットを delta 個ぶん送る（端で折り返す）。
            現在が Factory 由来でない場合は先頭から始める。 */
        void stepPreset(int delta);

        /** ユーザープリセットの保存先。
            ~/Documents/OTODESK/QuadMorphFilter/Presets */
        static juce::File presetDirectory();

        void openBrowser();
        void closeBrowser();
        void doReset();
        void doRandom();
        void savePreset(const juce::String& name, const juce::String& subCat);
        void loadPresetFile(const juce::File& f);
        void loadFactoryPreset(int index);

        // 【V1.1.0】プリセット名と番号はプロセッサ（APVTS のステート）が持つ。
        // ここに持たせるとウィンドウを閉じただけで消えてしまうため。
        //   processor.getPresetName() / getPresetIndex() を参照する。

        /** 名前表示の矩形（resized で確定、paint で使用） */
        juce::Rectangle<int> presetNameArea;

        // ブラウザは重いので必要になるまで作らない。
        // 破棄順序: tooltip より前に宣言し、tooltip が最後に消えるようにする。
        std::unique_ptr<PresetBrowser> browser;

        juce::Random rng;

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
