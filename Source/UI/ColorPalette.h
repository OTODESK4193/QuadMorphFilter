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
            // 1 Sakura
            { 0xff1e1418,0xff2b1c22,0xff180f13,0xfff3e6ec,0xffa2888f,0xff36272e,
              0xfff7c9d8,0xffff9db4,0xffe6c8ee,0xffffcdb8,0xfff5b8cf,0xfff0dccb,0xffffb0ad,0xfff0c3e6 },
            // 2 Ocean
            { 0xff0f1720,0xff17222f,0xff0b1219,0xffdce9f2,0xff7f93a2,0xff203039,
              0xff9fe8da,0xff88cfe0,0xffa8c7ea,0xffbfe0d2,0xff8fc9f7,0xffbfe8e0,0xff9fd2d8,0xffaed0f0 },
            // 3 Forest
            { 0xff121a14,0xff1b2620,0xff0d1410,0xffe4f0e6,0xff85988c,0xff26332c,
              0xffb7ead0,0xffd6e8a8,0xffc2e0c7,0xffe0f0bd,0xffaee0c9,0xffd2f0b8,0xffb8e0a0,0xffcde0c3 },
            // 4 Sunset
            { 0xff1f1512,0xff2b1e18,0xff18100d,0xfff3e9e0,0xffa08f82,0xff362a24,
              0xffffd6a8,0xffff9db0,0xffeac7bd,0xffffc4a0,0xfff7c98f,0xfff0d9b8,0xffffb0a0,0xfff0c3c8 },
            // 5 Mono
            { 0xff161616,0xff202020,0xff101010,0xffe6e6e6,0xff8c8c8c,0xff2b2b2b,
              0xffcccccc,0xffdddddd,0xffbdbdbd,0xffd6d6d6,0xffc4c4c4,0xffe0e0e0,0xffb8b8b8,0xffcfcfcf },
            // 6 Neon
            { 0xff0d0d14,0xff16161f,0xff08080e,0xffe9eeff,0xff7d84a0,0xff20202e,
              0xff4dffd0,0xffff4d9d,0xff8f8fff,0xffffb14d,0xff4dd0ff,0xffb0ff4d,0xffff6b6b,0xffc44dff },
            // 7 Vaporwave
            { 0xff15111f,0xff1f1830,0xff100c18,0xffefe6f6,0xff938aa8,0xff2b2240,
              0xff7df0e0,0xffff8fd0,0xffb79fff,0xffffb0e0,0xff8fd0ff,0xffd0b0ff,0xffff9fd8,0xffc79fff },
            // 8 Amber
            { 0xff1a1610,0xff261f16,0xff13100b,0xfff2ebdd,0xff9c9078,0xff332a1e,
              0xffe8d6a0,0xffe0b088,0xffd6c8a8,0xffe8c890,0xffd0c090,0xffe0d8a0,0xffe0b090,0xffd8c0a0 },
            // 9 Arctic
            { 0xff14181c,0xff1e242a,0xff0f1215,0xffe8f0f5,0xff8496a0,0xff28313a,
              0xffc0f0e8,0xffbcd8ea,0xffcdd8ea,0xffd8ece8,0xffb8dcf0,0xffd8ecdc,0xffc8dce0,0xffcdd8ec }
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
