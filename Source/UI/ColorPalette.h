// ==========================================
// UI/ColorPalette.h   （V1.1.0 新規）
//
// QuadMorphFilter のカラーテーマ定義。
// ダーク背景 × パステルアクセントを基調に 10 テーマを用意し、
// APVTS の "colorTheme" パラメータからランタイム切替する。
//
// ※ ここにあるのは「表示専用」のグローバル変数のみ。
//    オーディオ処理には一切関与せず、全ての読み書きは
//    メッセージスレッド（UI スレッド）上でのみ行われるため、
//    マルチインスタンスでもデータ競合・音の不安定化は起きない。
// ==========================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// ============================================================================
//  埋め込みフォント（LIFT-X と同じ構成）
//   ・UI 全般   : Inter          (Regular / Bold)
//   ・数値表示 : JetBrains Mono (Regular / Bold)
//  どちらも SIL OFL 1.1。実体とライセンス全文を Source/Assets/Fonts/ に同梱。
//
//  差し替えは QuadMorphLookAndFeel::getTypefaceForFont() が一括で行うため、
//  既存の juce::FontOptions(size, bold) という呼び出しは 1 箇所も変えずに
//  Inter へ切り替わる。等幅にしたい箇所だけ QMFonts::mono() を使う。
//
//  全パネルが include する ColorPalette.h に置いてある
//  （QuadMorphLookAndFeel.h を include していないパネルからも参照するため）。
// ============================================================================
namespace QMFonts
{
    // getTypefaceForFont() がこの名前を見て JetBrains Mono を返す
    inline const char* kMonoName = "QMF Mono";

    inline juce::Font mono(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(kMonoName, height,
                                            bold ? juce::Font::bold : juce::Font::plain));
    }
}

namespace QMColors
{
    // ---- 基本色（setTheme で書き換わる）----
    inline juce::Colour bg { 0xff17141f };   // ウィンドウ背景
    inline juce::Colour panel { 0xff201c2b };   // パネル / カード背景
    inline juce::Colour well { 0xff141220 };   // 一段沈んだ面（グラフ背景など）
    inline juce::Colour panelLine { 0x22ffffff };   // 区切り線
    inline juce::Colour grid { 0x14ffffff };   // グラフのグリッド
    inline juce::Colour text { 0xffe9e3f2 };   // 主テキスト
    inline juce::Colour textDim { 0xff8d86a0 };   // 補助テキスト
    inline juce::Colour track { 0xff2a2536 };   // スライダー溝 / コンボ背景

    // ---- パステルパレット（setTheme で書き換わる）----
    inline juce::Colour mint { 0xffb5ead7 };
    inline juce::Colour pink { 0xffffb7c5 };
    inline juce::Colour lavender { 0xffc7ceea };
    inline juce::Colour peach { 0xffffdac1 };
    inline juce::Colour babyBlue { 0xffaed9f7 };
    inline juce::Colour sage { 0xffe2f0cb };
    inline juce::Colour rose { 0xffffb7b2 };
    inline juce::Colour lilac { 0xffe0c3fc };

    // ---- セクション別アクセント（setTheme で再計算）----
    inline juce::Colour accentFilter { 0xffaed9f7 };   // FILTER タブ
    inline juce::Colour accentMod { 0xffe0c3fc };   // MOD タブ
    inline juce::Colour accentOut { 0xffffb7c5 };   // OUT タブ
    inline juce::Colour accentMorph { 0xffb5ead7 };   // XY / Morph

    // ================================================================
    // テーマ定義
    // ================================================================
    struct Theme
    {
        juce::uint32 bg, panel, well, text, textDim, track;
        juce::uint32 mint, pink, lavender, peach, babyBlue, sage, rose, lilac;
    };

    inline constexpr int kNumThemes = 10;

    inline const std::array<Theme, kNumThemes>& themes()
    {
        static const std::array<Theme, kNumThemes> t = { {
            // 0 Midnight (default)
            { 0xff17141f,0xff201c2b,0xff141220,0xffe9e3f2,0xff8d86a0,0xff2a2536,
              0xffb5ead7,0xffffb7c5,0xffc7ceea,0xffffdac1,0xffaed9f7,0xffe2f0cb,0xffffb7b2,0xffe0c3fc },
            // 1 Sakura — 暖色寄りだが 8 色は色相を散らす（春の花壇のイメージ）
            { 0xff1e1418,0xff2b1c22,0xff180f13,0xfff3e6ec,0xffa2888f,0xff36272e,
              0xff7fe6c0,0xffff7fac,0xffc79cf5,0xffffc078,0xff7fc0f0,0xffd6e878,0xffff8a72,0xffef86dd },
            // 2 Ocean — 海の青緑を主役にしつつ、珊瑚/砂/夕陽の暖色を差す
            { 0xff0f1720,0xff17222f,0xff0b1219,0xffdce9f2,0xff7f93a2,0xff203039,
              0xff3fe6c4,0xffff6fa8,0xff8fa8ff,0xffffb257,0xff35c4ff,0xffb4ec55,0xffff8264,0xffb46bff },
            // 3 Forest — 深緑ベース。木漏れ日/花/苔で色相を分ける
            { 0xff121a14,0xff1b2620,0xff0d1410,0xffe4f0e6,0xff85988c,0xff26332c,
              0xff4fe0a0,0xffff8fb4,0xff9fb8f0,0xffffc063,0xff6fd4e8,0xffc8ec4f,0xffff7f63,0xffc48ff0 },
            // 4 Sunset — 夕焼けの赤橙が主役。海と空の寒色を対比に置く
            { 0xff1f1512,0xff2b1e18,0xff18100d,0xfff3e9e0,0xffa08f82,0xff362a24,
              0xff5fe0c8,0xffff6f9c,0xffb097f5,0xffff9f3f,0xff5fb4f0,0xffecdc57,0xffff6a45,0xffee7fd8 },
            // 5 Mono — 彩度はほぼ捨て、明度差＋ごく淡い色相差で 4 本を見分ける
            { 0xff161616,0xff202020,0xff101010,0xffe6e6e6,0xff8c8c8c,0xff2b2b2b,
              0xfff4f6f5,0xffdcd6d2,0xff9aa0ac,0xffcfc3b4,0xff98a4b4,0xffe8ead8,0xff8f8a8a,0xff6e7080 },
            // 6 Neon
            { 0xff0d0d14,0xff16161f,0xff08080e,0xffe9eeff,0xff7d84a0,0xff20202e,
              0xff4dffd0,0xffff4d9d,0xff8f8fff,0xffffb14d,0xff4dd0ff,0xffb0ff4d,0xffff6b6b,0xffc44dff },
            // 7 Vaporwave
            { 0xff15111f,0xff1f1830,0xff100c18,0xffefe6f6,0xff938aa8,0xff2b2240,
              0xff7df0e0,0xffff8fd0,0xffb79fff,0xffffb0e0,0xff8fd0ff,0xffd0b0ff,0xffff9fd8,0xffc79fff },
            // 8 Amber — 琥珀/真鍮の暖色が基調。ヴィンテージ機材の指標色を混ぜる
            { 0xff1a1610,0xff261f16,0xff13100b,0xfff2ebdd,0xff9c9078,0xff332a1e,
              0xff86dfb0,0xffff97a0,0xffbfa8ee,0xffffbe4f,0xff7cc0e0,0xffe6d84a,0xffff8f5c,0xffdf90d8 },
            // 9 Arctic — 氷の寒色が基調。オーロラの緑/紫/桃を差し色に
            { 0xff14181c,0xff1e242a,0xff0f1215,0xffe8f0f5,0xff8496a0,0xff28313a,
              0xff6ff0d4,0xffff8fb8,0xff9fb4f5,0xffffc98a,0xff56cff5,0xffb8f07f,0xffff8f8f,0xffbf90f5 }
        } };
        return t;
    }

    inline juce::StringArray getThemeNames()
    {
        return { "Midnight", "Sakura", "Ocean", "Forest", "Sunset",
                 "Mono", "Neon", "Vaporwave", "Amber", "Arctic" };
    }

    /** いまグローバル変数に載っているテーマ番号。
        複数インスタンスがそれぞれ別テーマを選んでいる場合の取り違えを
        検出するために使う（Content::paint の冒頭で貼り直す）。 */
    inline int currentTheme = -1;

    inline void setTheme(int idx) noexcept
    {
        idx = juce::jlimit(0, kNumThemes - 1, idx);
        currentTheme = idx;

        const auto& t = themes()[(size_t)idx];

        bg = juce::Colour(t.bg);
        panel = juce::Colour(t.panel);
        well = juce::Colour(t.well);
        text = juce::Colour(t.text);
        textDim = juce::Colour(t.textDim);
        track = juce::Colour(t.track);

        mint = juce::Colour(t.mint);
        pink = juce::Colour(t.pink);
        lavender = juce::Colour(t.lavender);
        peach = juce::Colour(t.peach);
        babyBlue = juce::Colour(t.babyBlue);
        sage = juce::Colour(t.sage);
        rose = juce::Colour(t.rose);
        lilac = juce::Colour(t.lilac);

        panelLine = text.withAlpha(0.13f);
        grid = text.withAlpha(0.07f);

        accentFilter = babyBlue;
        accentMod = lilac;
        accentOut = pink;
        accentMorph = mint;
    }

    // ================================================================
    // 用途別カラー取得
    // ================================================================

    /** フィルター A / B / C / D のアクセント色（0..3） */
    inline juce::Colour filterColour(int i) noexcept
    {
        switch (juce::jlimit(0, 3, i))
        {
            case 0:  return mint;
            case 1:  return babyBlue;
            case 2:  return peach;
            default: return lilac;
        }
    }

    /** フィルターの Resonance 側に使う色。
        Cutoff と同じ色だと 1 行が単調になるので、色相を少しだけ回して差を付ける。
        Mono テーマのように彩度がほぼ無いテーマでは自動的にほぼ無変化になる。 */
    inline juce::Colour filterResColour(int i) noexcept
    {
        return filterColour(i).withRotatedHue(0.075f).withMultipliedSaturation(0.88f);
    }

    /** LFO 1..5 のアクセント色（0..4） */
    inline juce::Colour lfoColour(int i) noexcept
    {
        switch (juce::jlimit(0, 4, i))
        {
            case 0:  return babyBlue;   // LFO1 = Morph
            case 1:  return pink;       // LFO2 = Cutoff
            case 2:  return peach;      // LFO3 = Reso
            case 3:  return lilac;      // LFO4 = Rate Mod
            default: return sage;       // LFO5 = Dry/Wet Mod
        }
    }

    /** Envelope Follower のアクセント色 */
    inline juce::Colour envColour() noexcept { return rose; }

    /** タブ（0=FILTER, 1=MOD, 2=OUT）のアクセント色 */
    inline juce::Colour tabColour(int i) noexcept
    {
        switch (juce::jlimit(0, 2, i))
        {
            case 0:  return accentFilter;
            case 1:  return accentMod;
            default: return accentOut;
        }
    }
}
