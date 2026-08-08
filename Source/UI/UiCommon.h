// ==========================================
// UI/UiCommon.h   （V1.1.0 新規）
//
// 各タブパネル共通のレイアウト定数・数値フォーマット・描画ヘルパー。
// ノブは使わず「バー型スライダー」で統一する方針のため、
// 値の書式はスライダー本体（LookAndFeel）が描く。
// 単位はスライダーの Component プロパティ "qmUnit" で受け渡す。
// ==========================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "ColorPalette.h"

namespace QMUI
{
    // ---- 共通レイアウト定数（論理座標）----
    inline constexpr int kMargin = 12;   // パネル左右の余白
    inline constexpr int kRowH = 30;   // 1 行の高さ
    inline constexpr int kRowGap = 3;   // 行間
    inline constexpr int kBarH = 22;   // バー型スライダーの高さ
    inline constexpr int kCtrlH = 22;   // コンボ / ボタンの高さ
    inline constexpr int kHeadH = 16;   // セクション見出しの高さ

    // ================================================================
    // 単位
    //   スライダーの getProperties().set("qmUnit", (int)Unit::Hz) で指定する。
    //   未指定なら Raw（小数 2 桁）。
    // ================================================================
    enum class Unit
    {
        Raw = 0,   // 1.23
        Hz,        // 440 / 1.20k / 20.0k
        Db,        // -1.0dB
        Pct,       // 値がそのまま % （0..100）  -> "50%"
        Pct01,     // 値が 0..1                  -> "50%"
        Deg,       // 270°
        Sec,       // 0.5 -> "500ms" / 1.50 -> "1.50s"
        Ms,        // 100 -> "100ms" / 1500 -> "1.50s"
        Ratio,     // 2.50x
        Int        // 12
    };

    /** スライダー値 → 表示文字列（バー内に描画される） */
    inline juce::String formatValue(double v, Unit unit)
    {
        switch (unit)
        {
            case Unit::Hz:
                if (v >= 10000.0) return juce::String(v * 0.001, 1) + "k";
                if (v >= 1000.0)  return juce::String(v * 0.001, 2) + "k";
                if (v >= 100.0)   return juce::String(juce::roundToInt(v));
                if (v >= 10.0)    return juce::String(v, 1);
                if (v >= 1.0)     return juce::String(v, 2);
                return juce::String(v, 3);

            case Unit::Db:
                return juce::String(v, 1) + "dB";

            case Unit::Pct:
                return juce::String(juce::roundToInt(v)) + "%";

            case Unit::Pct01:
                return juce::String(juce::roundToInt(v * 100.0)) + "%";

            case Unit::Deg:
                return juce::String(juce::roundToInt(v)) + juce::String::fromUTF8("\xc2\xb0");

            case Unit::Sec:
                if (v < 1.0) return juce::String(juce::roundToInt(v * 1000.0)) + "ms";
                return juce::String(v, 2) + "s";

            case Unit::Ms:
                if (v >= 1000.0) return juce::String(v * 0.001, 2) + "s";
                return juce::String(juce::roundToInt(v)) + "ms";

            case Unit::Ratio:
                return juce::String(v, 2) + "x";

            case Unit::Int:
                return juce::String(juce::roundToInt(v));

            case Unit::Raw:
            default:
                return juce::String(v, 2);
        }
    }

    /** スライダーから単位を読む（未設定なら Raw） */
    inline Unit unitOf(const juce::Slider& s) noexcept
    {
        return (Unit)(int)s.getProperties().getWithDefault("qmUnit", (int)Unit::Raw);
    }

    // ================================================================
    // 描画ヘルパー
    // ================================================================

    /** セクション見出し（小さな大文字ラベル + アクセントの下線） */
    inline void drawSectionHeader(juce::Graphics& g, const juce::String& name,
                                  juce::Rectangle<int> area, juce::Colour accent)
    {
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(name, area.getX(), area.getY(), area.getWidth(), 12,
                   juce::Justification::centredLeft);

        // 下線を「文字の下だけ濃く」するための概算幅。
        // JUCE のバージョン差（Font::getStringWidth の非推奨化）を避けるため
        // 文字数からの見積もりで済ませている（見た目にのみ影響）。
        const int textW = 8 + name.length() * 7;

        g.setColour(accent.withAlpha(0.55f));
        g.fillRect(area.getX(), area.getY() + 13, juce::jmin(textW, area.getWidth()), 1);
        g.setColour(accent.withAlpha(0.12f));
        g.fillRect(area.getX() + juce::jmin(textW, area.getWidth()), area.getY() + 13,
                   juce::jmax(0, area.getWidth() - textW), 1);
    }

    /** 列見出し（小さなグレーの文字） */
    inline void drawColumnLabel(juce::Graphics& g, const juce::String& name,
                                juce::Rectangle<int> area,
                                juce::Justification just = juce::Justification::centred)
    {
        g.setColour(QMColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText(name, area, just, false);
    }

    /** 薄いカード背景（まとまりを見せたいところで使う） */
    inline void drawCard(juce::Graphics& g, juce::Rectangle<int> r, float radius = 6.0f)
    {
        g.setColour(QMColors::text.withAlpha(0.035f));
        g.fillRoundedRectangle(r.toFloat(), radius);
        g.setColour(QMColors::panelLine.withAlpha(0.5f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), radius, 1.0f);
    }
}
