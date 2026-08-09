// ==========================================
// UI/QuadMorphLookAndFeel.h   （V1.1.0 全面改訂）
//
// ・カラーテーマ（QMColors）に完全追従
// ・スライダーは「バー型」。名前を左、値を右にバー内へ直接描くため
//   別途 Label / TextBox を置く必要がなくなり、横幅を大きく節約できる
// ・タブ用ボタンは getProperties()["qmTab"] = true で別デザインになる
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ColorPalette.h"

class QuadMorphLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuadMorphLookAndFeel();

    /** テーマ切替後に色を貼り直す */
    void refreshColours();

    /** 埋め込みフォントへの一括差し替え（LIFT-X と同じ方式）。
        QMFonts::kMonoName を指定されたときだけ JetBrains Mono、
        それ以外は全て Inter を返すため、各パネルの
        juce::FontOptions(size, bold) 呼び出しはそのままで統一される。 */
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    /** ComboBox のテキスト位置（右の矢印と重ならないように少し詰める） */
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

private:
    // 埋め込みフォント。コンストラクタで 1 度だけ生成する。
    juce::Typeface::Ptr interRegular, interBold, monoRegular, monoBold;
};
