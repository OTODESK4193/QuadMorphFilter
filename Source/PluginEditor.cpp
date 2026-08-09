// ==========================================
// PluginEditor.cpp   （V1.1.0 全面改訂）
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/UiCommon.h"

#ifndef QUADMORPH_VERSION
 #define QUADMORPH_VERSION "1.1.0"
#endif

namespace
{
    constexpr int kMargin = QMUI::kMargin;

    constexpr int kHeaderY = 4;
    constexpr int kHeaderH = 36;

    constexpr int kTopY = 44;
    constexpr int kTopH = 348;

    constexpr int kTabY = 398;
    constexpr int kTabH = 28;
    constexpr int kTabW = 112;
    constexpr int kTabStep = 116;

    constexpr int kPanelY = 430;
    constexpr int kPanelH = 264;

    constexpr int kXYPadW = 308;

    const char* const kTabNames[] = { "FILTER", "MOD", "CONFIG" };
}

// ==========================================================================
//  Content
// ==========================================================================
QuadMorphFilterAudioProcessorEditor::Content::Content(QuadMorphFilterAudioProcessor& p,
                                                      QuadMorphLookAndFeel& lnf)
    : processor(p),
      laf(lnf),
      visualizer(p),
      xyPad(p),
      filterPanel(p),
      modPanel(p),
      configPanel(p)
{
    // ---- テーマを先に確定させてから子を構築済みの色で塗り直す ----
    lastThemeIndex = (int)processor.apvts.getRawParameterValue("colorTheme")->load();
    QMColors::setTheme(lastThemeIndex);
    laf.refreshColours();

    addAndMakeVisible(visualizer);
    addAndMakeVisible(xyPad);

    // ---- タブ ----
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& b = tabButtons[(size_t)i];
        b.setButtonText(kTabNames[i]);
        b.setClickingTogglesState(false);          // 見た目は手動で管理する
        b.getProperties().set("qmTab", true);
        b.onClick = [this, i] { setActiveTab(i); };
        addAndMakeVisible(b);
    }

    addAndMakeVisible(filterPanel);
    addChildComponent(modPanel);
    addChildComponent(configPanel);

    // ---- テーマ選択 ----
    // 【V1.1.0】コンボ本体はヘッダーから CONFIG タブへ移した。
    // 選択が変わったらコールバックで受け取り、ここで色を貼り直す。
    configPanel.onThemeChanged = [this](int idx)
    {
        if (idx >= 0 && idx != lastThemeIndex) applyTheme(idx);
    };

    setActiveTab(TabFilter);
    applyTheme(lastThemeIndex);

    // プリセット読み込みなどでテーマが変わったときの取りこぼしを拾う
    startTimerHz(8);
}

QuadMorphFilterAudioProcessorEditor::Content::~Content()
{
    stopTimer();   // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）
}

// ==========================================================================
void QuadMorphFilterAudioProcessorEditor::Content::timerCallback()
{
    const int idx = (int)processor.apvts.getRawParameterValue("colorTheme")->load();
    if (idx != lastThemeIndex)
        applyTheme(idx);
}

// ==========================================================================
void QuadMorphFilterAudioProcessorEditor::Content::repaintAll(juce::Component* c)
{
    if (c == nullptr) return;
    c->repaint();
    for (auto* child : c->getChildren())
        repaintAll(child);
}

void QuadMorphFilterAudioProcessorEditor::Content::applyTheme(int themeIndex)
{
    lastThemeIndex = juce::jlimit(0, QMColors::kNumThemes - 1, themeIndex);

    QMColors::setTheme(lastThemeIndex);
    laf.refreshColours();

    filterPanel.refreshTheme();
    modPanel.refreshTheme();
    configPanel.refreshTheme();

    styleTabs();
    repaintAll(this);
}

// ==========================================================================
void QuadMorphFilterAudioProcessorEditor::Content::styleTabs()
{
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto& b = tabButtons[(size_t)i];
        b.setToggleState(i == activeTab, juce::dontSendNotification);
        b.setColour(juce::TextButton::textColourOnId, QMColors::tabColour(i));
        b.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        b.repaint();
    }
}

void QuadMorphFilterAudioProcessorEditor::Content::setActiveTab(int tab)
{
    activeTab = juce::jlimit(0, kNumTabs - 1, tab);

    filterPanel.setVisible(activeTab == TabFilter);
    modPanel.setVisible(activeTab == TabMod);
    configPanel.setVisible(activeTab == TabConfig);

    styleTabs();
    repaint(0, kTabY, getWidth(), kTabH + 4);
}

// ==========================================================================
void QuadMorphFilterAudioProcessorEditor::Content::resized()
{
    const int w = getWidth();

    headerArea = { kMargin, kHeaderY, w - kMargin * 2, kHeaderH };

    // ---- 上部: フィルターカーブ（大）+ XY パッド ----
    const int visW = w - kMargin * 2 - kXYPadW - 12;
    visualizer.setBounds(kMargin, kTopY, visW, kTopH);
    xyPad.setBounds(kMargin + visW + 12, kTopY, kXYPadW, kTopH);

    // ---- タブ ----
    tabStripArea = { 0, kTabY, w, kTabH };
    for (int i = 0; i < kNumTabs; ++i)
        tabButtons[(size_t)i].setBounds(kMargin + i * kTabStep, kTabY, kTabW, kTabH);

    // ---- パネル ----
    const auto panelBounds = juce::Rectangle<int>(0, kPanelY, w, kPanelH);
    filterPanel.setBounds(panelBounds);
    modPanel.setBounds(panelBounds);
    configPanel.setBounds(panelBounds);
}

// ==========================================================================
void QuadMorphFilterAudioProcessorEditor::Content::paint(juce::Graphics& g)
{
    // 複数インスタンスがそれぞれ別テーマを選んでいると、
    // カラーテーブル（表示専用のグローバル）が他方に書き換えられていることがある。
    // 子コンポーネントは親のあとに描画されるので、ここで自分のテーマへ戻しておけば
    // このインスタンス全体が正しい配色で描かれる。
    if (QMColors::currentTheme != lastThemeIndex)
        QMColors::setTheme(lastThemeIndex);

    g.fillAll(QMColors::bg);

    // ---- ヘッダー ----
    {
        auto r = headerArea;

        // 左端のアクセントバー
        g.setGradientFill(juce::ColourGradient(
            QMColors::accentMorph, (float)r.getX(), (float)r.getY() + 6.0f,
            QMColors::accentOut, (float)r.getX(), (float)r.getBottom() - 6.0f, false));
        g.fillRoundedRectangle((float)r.getX(), (float)r.getY() + 6.0f, 3.0f,
                               (float)r.getHeight() - 12.0f, 1.5f);

        // タイトル
        g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
        juce::ColourGradient titleGrad(QMColors::accentMorph, (float)r.getX() + 12.0f,
                                       (float)r.getY(),
                                       QMColors::accentOut, (float)r.getX() + 300.0f,
                                       (float)r.getBottom(), false);
        titleGrad.addColour(0.5, QMColors::accentFilter);
        g.setGradientFill(titleGrad);
        g.drawText("QUAD-MORPH FILTER", r.getX() + 12, r.getY(), 300, r.getHeight(),
                   juce::Justification::centredLeft, false);

        // サブタイトル
        g.setColour(QMColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText("OTODESK   /   28 Filter Models   /   V" QUADMORPH_VERSION,
                   r.getX() + 322, r.getY(), 380, r.getHeight(),
                   juce::Justification::centredLeft, false);

        // 下境界
        g.setColour(QMColors::panelLine.withAlpha(0.35f));
        g.fillRect(r.getX(), r.getBottom(), r.getWidth(), 1);
    }

    // ---- タブ帯の下線 ----
    g.setColour(QMColors::panelLine.withAlpha(0.30f));
    g.fillRect(kMargin, kTabY + kTabH, getWidth() - kMargin * 2, 1);
}

// ==========================================================================
//  Editor
// ==========================================================================
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(
    QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      content(p, laf)
{
    setOpaque(true);

    // LookAndFeel はこのエディタにだけ適用する（子は自動的に継承する）。
    // setDefaultLookAndFeel はプロセス全体を書き換えるため、
    // 複数インスタンス時に他のウィンドウを巻き込む危険がある。
    setLookAndFeel(&laf);

    addAndMakeVisible(content);

    // ---- リサイズ（アスペクト比固定）----
    constrainer.setFixedAspectRatio((double)kBaseW / (double)kBaseH);
    constrainer.setMinimumSize(kBaseW * 7 / 10, kBaseH * 7 / 10);
    constrainer.setMaximumSize(kBaseW * 8 / 5, kBaseH * 8 / 5);
    setConstrainer(&constrainer);
    setResizable(true, true);

    setSize(kBaseW, kBaseH);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor()
{
    // 参照を先に切ってから各メンバの破棄に入る（COM 参照カウント事故の予防）
    setConstrainer(nullptr);
    setLookAndFeel(nullptr);
}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Content が全面を塗るので、リサイズ途中の残像だけ潰しておく
    g.fillAll(QMColors::bg);
}

void QuadMorphFilterAudioProcessorEditor::resized()
{
    const float scale = (float)getWidth() / (float)kBaseW;
    content.setTransform(juce::AffineTransform::scale(scale));
    content.setBounds(0, 0, kBaseW, kBaseH);
}
