// ==========================================
// PluginEditor.cpp   （V1.1.0 全面改訂）
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/UiCommon.h"
#include "FactoryPresets.h"
#include "PresetRandomiser.h"

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

    // ---- ヘッダーの Preset / Random / Reset ----
    // 表示文字列は英字のみ（juce::String(const char*) は ASCII 前提）
    auto styleHeaderBtn = [this](juce::TextButton& b, const juce::String& text,
                                 juce::Colour accent, const juce::String& info)
    {
        b.setButtonText(text);
        b.setClickingTogglesState(false);
        b.setColour(juce::TextButton::textColourOffId, accent);
        b.setColour(juce::TextButton::textColourOnId, accent.brighter(0.4f));
        QMUI::setInfo(b, info);
        addAndMakeVisible(b);
    };

    styleHeaderBtn(presetBtn, "PRESET", QMColors::accentFilter,
        "PRESET  -  open the browser. 200 factory presets in ten categories, plus your "
        "own saved presets. Click the star to keep a favourite, and a single click loads.");
    styleHeaderBtn(randomBtn, "RANDOM", QMColors::accentMorph,
        "RANDOM  -  build a new patch. It picks one voicing character, spreads the four "
        "cutoffs across the spectrum and keeps resonance in check, so the result stays "
        "usable rather than noise. Output levels are never randomised.");
    styleHeaderBtn(resetBtn, "RESET", QMColors::rose,
        "RESET  -  return every parameter, including all five LFOs and the envelope "
        "follower, to its default. You will be asked to confirm first.");

    presetBtn.onClick = [this] { openBrowser(); };
    randomBtn.onClick = [this] { doRandom(); };
    resetBtn.onClick  = [this] { doReset(); };

    // ---- ◀ [プリセット名] ▶ ----
    // 三角は ASCII に無いので fromUTF8 で明示的に組む。
    // （juce::String(const char*) は ASCII 前提で assert するため）
    styleHeaderBtn(prevBtn, juce::String::fromUTF8("\xE2\x97\x80"), QMColors::textDim,
        "PREVIOUS PRESET  -  step back through the 200 factory presets. "
        "Wraps around at the start.");
    styleHeaderBtn(nextBtn, juce::String::fromUTF8("\xE2\x96\xB6"), QMColors::textDim,
        "NEXT PRESET  -  step forward through the 200 factory presets. "
        "Wraps around at the end.");

    prevBtn.onClick = [this] { stepPreset(-1); };
    nextBtn.onClick = [this] { stepPreset(+1); };

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
//  プリセット / Reset / Random
// ==========================================================================
juce::File QuadMorphFilterAudioProcessorEditor::Content::presetDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("OTODESK")
                   .getChildFile("QuadMorphFilter")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

void QuadMorphFilterAudioProcessorEditor::Content::openBrowser()
{
    if (browser == nullptr)
    {
        browser = std::make_unique<PresetBrowser>(presetDirectory());

        // Factory 一覧を渡す
        juce::Array<PresetBrowser::FactoryItem> items;
        for (int i = 0; i < FactoryPresets::count(); ++i)
            items.add({ FactoryPresets::nameOf(i), FactoryPresets::categoryOf(i), i });
        browser->setFactoryPresets(items);

        browser->onClose = [this] { closeBrowser(); };
        browser->onInit = [this] { doReset(); };
        browser->onLoadFactory = [this](int idx) { loadFactoryPreset(idx); };
        browser->onLoad = [this](const juce::File& f) { loadPresetFile(f); };
        browser->onSave = [this](const juce::String& n, const juce::String& s) { savePreset(n, s); };

        addAndMakeVisible(*browser);
        resized();
    }

    browser->setVisible(true);
    browser->toFront(true);
}

void QuadMorphFilterAudioProcessorEditor::Content::closeBrowser()
{
    if (browser != nullptr)
        browser->setVisible(false);
}

// --------------------------------------------------------------------------
void QuadMorphFilterAudioProcessorEditor::Content::doReset()
{
    // 破棄されたあとにコールバックが走らないよう SafePointer で守る
    juce::Component::SafePointer<Content> safe(this);

    juce::NativeMessageBox::showYesNoBox(
        juce::MessageBoxIconType::WarningIcon,
        "Reset All Parameters",
        "This returns every parameter to its default, including all four filters, "
        "all five LFOs, the envelope follower and the morph settings.\n\n"
        "Your current patch will be lost unless you have saved it.\n\n"
        "Continue?",
        nullptr,
        juce::ModalCallbackFunction::create([safe](int yes) mutable
        {
            if (yes != 1 || safe == nullptr) return;

            FactoryPresets::applyInit(safe->processor);
            safe->processor.setPresetName("Init");
            safe->processor.setPresetIndex(-1);

            if (safe->browser != nullptr)
                safe->browser->setCurrentFactory(-1);

            safe->repaint();
        }));
}

// --------------------------------------------------------------------------
void QuadMorphFilterAudioProcessorEditor::Content::doRandom()
{
    PresetRandomiser::randomise(processor, rng);
    processor.setPresetName("Random");
    processor.setPresetIndex(-1);

    if (browser != nullptr)
        browser->setCurrentFactory(-1);

    repaint();
}

// --------------------------------------------------------------------------
// stepPreset
// ヘッダーの ◀ ▶。Factory プリセットを順送りする。
// 現在が Factory 由来でない（User / Random / Init）ときは先頭から始める。
// --------------------------------------------------------------------------
void QuadMorphFilterAudioProcessorEditor::Content::stepPreset(int delta)
{
    const int n = FactoryPresets::count();
    if (n <= 0) return;

    const int cur = processor.getPresetIndex();

    // 端で折り返す。負数の剰余に注意して正へ寄せる。
    const int next = (cur < 0) ? (delta > 0 ? 0 : n - 1)
                               : ((cur + delta) % n + n) % n;

    loadFactoryPreset(next);

    if (browser != nullptr)
        browser->setCurrentFactory(next);
}

// --------------------------------------------------------------------------
void QuadMorphFilterAudioProcessorEditor::Content::loadFactoryPreset(int index)
{
    FactoryPresets::apply(processor, index);
    processor.setPresetName(FactoryPresets::nameOf(index));
    processor.setPresetIndex(index);
    repaint();
}

void QuadMorphFilterAudioProcessorEditor::Content::loadPresetFile(const juce::File& f)
{
    if (!f.existsAsFile()) return;

    if (auto xml = juce::XmlDocument::parse(f))
        if (xml->hasTagName(processor.apvts.state.getType()))
        {
            processor.apvts.replaceState(juce::ValueTree::fromXml(*xml));
            processor.setPresetName(f.getFileNameWithoutExtension());
            processor.setPresetIndex(-1);
            repaint();
        }
}

void QuadMorphFilterAudioProcessorEditor::Content::savePreset(const juce::String& name,
                                                              const juce::String& subCat)
{
    auto dir = presetDirectory();
    if (subCat.isNotEmpty())
    {
        dir = dir.getChildFile(juce::File::createLegalFileName(subCat));
        dir.createDirectory();
    }

    auto file = dir.getChildFile(juce::File::createLegalFileName(name) + ".xml");

    if (auto xml = processor.apvts.copyState().createXml())
        xml->writeTo(file);

    processor.setPresetName(name);
    processor.setPresetIndex(-1);

    if (browser != nullptr)
        browser->setCurrentFile(file);

    repaint();
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

    // ---- ヘッダー右のボタン列（右から Reset / Random / Preset）----
    {
        int x = w - kMargin;
        const int y = kHeaderY + 7;
        const int h = 22;

        auto place = [&x, y, h](juce::TextButton& b, int bw)
        {
            x -= bw;
            b.setBounds(x, y, bw, h);
            x -= 6;
        };

        place(resetBtn, 64);
        place(randomBtn, 74);
        place(presetBtn, 74);

        // ◀ [名前] ▶
        x -= 6;
        place(nextBtn, 22);
        const int nameW = 168;
        x -= nameW;
        presetNameArea = { x, y, nameW, h };
        x -= 2;
        place(prevBtn, 22);
    }

    // ブラウザは上部の表示領域を覆うように出す
    if (browser != nullptr)
        browser->setBounds(kMargin, kTopY, w - kMargin * 2, kTopH + kTabH + 8);

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
                   r.getX() + 322, r.getY(), 300, r.getHeight(),
                   juce::Justification::centredLeft, false);

        // ---- 現在のプリセット名（◀ ▶ の間）----
        if (!presetNameArea.isEmpty())
        {
            g.setColour(QMColors::well.withAlpha(0.75f));
            g.fillRoundedRectangle(presetNameArea.toFloat(), 4.0f);
            g.setColour(QMColors::panelLine.withAlpha(0.35f));
            g.drawRoundedRectangle(presetNameArea.toFloat().reduced(0.5f), 4.0f, 1.0f);

            g.setColour(QMColors::text.withAlpha(0.90f));
            g.setFont(QMFonts::mono(11.5f, true));
            g.drawText(processor.getPresetName(), presetNameArea.reduced(6, 0),
                       juce::Justification::centred, false);
        }

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

    // 【V1.1.0】前回の表示倍率を復元する。
    // 旧実装は常に 100% で開いていたため、リサイズしてもウィンドウを
    // 閉じるたびに元へ戻ってしまっていた。
    const int pct = audioProcessor.getEditorScalePercent();
    setSize(kBaseW * pct / 100, kBaseH * pct / 100);
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

    // 表示倍率を APVTS のステートへ控える。
    // これで DAW 保存にもエディタの開き直しにも引き継がれる。
    audioProcessor.setEditorScalePercent(juce::roundToInt(scale * 100.0f));
}
