// ==========================================
// PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==========================================
// QuadMorphLookAndFeel
// ==========================================
QuadMorphLookAndFeel::QuadMorphLookAndFeel()
{
    setColour(juce::Label::textColourId, juce::Colour(0xff2C3E50));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff2C3E50));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xff2C3E50));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xff2C3E50));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xffFFFFFF));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xff2C3E50));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffE0E6ED));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff2C3E50));
}

void QuadMorphLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    auto trackRect = juce::Rectangle<float>(static_cast<float>(x),
        (float)y + (float)height * 0.5f - 1.5f,
        (float)width, 3.0f);
    g.setColour(juce::Colour(0xffE0E6ED));
    g.fillRoundedRectangle(trackRect, 1.5f);

    auto activeColour = slider.findColour(juce::Slider::thumbColourId);
    if (activeColour.isTransparent()) activeColour = juce::Colour(0xff7F8C8D);
    g.setColour(activeColour.withAlpha(0.7f));
    g.fillRoundedRectangle(trackRect.withWidth(sliderPos - (float)x), 1.5f);

    g.setColour(activeColour);
    g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 6.0f, 12.0f, 12.0f);
}

void QuadMorphLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
    int, int, int, int, juce::ComboBox&)
{
    juce::Rectangle<int> boxBounds(0, 0, width, height);
    g.setColour(juce::Colour(0xffFFFFFF));
    g.fillRoundedRectangle(boxBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xffD5DDE5));
    g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f), 4.0f, 1.0f);

    juce::Path path;
    float arrowX = width - 15.0f;
    float arrowY = height * 0.45f;
    path.addTriangle(arrowX, arrowY, arrowX + 8.0f, arrowY, arrowX + 4.0f, arrowY + 5.0f);
    g.setColour(juce::Colour(0xff95A5A6));
    g.fillPath(path);
}

void QuadMorphLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    bool isOn = button.getToggleState();

    auto baseColour = isOn ? button.findColour(juce::TextButton::textColourOnId).withAlpha(0.15f)
        : juce::Colour(0xffFFFFFF);
    auto borderColour = isOn ? button.findColour(juce::TextButton::textColourOnId)
        : juce::Colour(0xffD5DDE5);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
}

// ==========================================
// FilterVisualizer
// ==========================================
void FilterVisualizer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1E272E));
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // ===== 【Slope表示修正】dB表示範囲を -120dB まで拡張 =====
    // 旧: +40dB 〜 -60dB  → 48dB/oct 以上で曲線が見切れる
    // 新: +40dB 〜 -120dB → 96dB/oct でも曲線が収まる
    const float dbTop = 40.0f;
    const float dbBottom = -120.0f;

    // ----- 0dB ライン -----
    float y0dB = juce::jmap(0.0f, dbTop, dbBottom, 0.0f, h);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawHorizontalLine((int)y0dB, 0.0f, w);
    g.setFont(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText("0dB", 2, (int)y0dB - 12, 35, 10, juce::Justification::left);

    // ----- dB グリッドライン（-20, -40, -60, -80, -100 dB）-----
    float dbGridLines[] = { -20.0f, -40.0f, -60.0f, -80.0f, -100.0f };
    for (float dbLine : dbGridLines)
    {
        float yDb = juce::jmap(dbLine, dbTop, dbBottom, 0.0f, h);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine((int)yDb, 0.0f, w);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawText(juce::String((int)dbLine) + "dB", 2, (int)yDb - 12, 40, 10,
            juce::Justification::left);
    }

    // ----- 周波数グリッドライン -----
    float freqs[] = { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f };
    juce::String labels[] = { "100Hz", "500Hz", "1kHz", "5kHz", "10kHz" };

    for (int i = 0; i < 5; ++i)
    {
        float x = w * std::log10(freqs[i] / 20.0f) / 3.0f;
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawVerticalLine((int)x, 0.0f, h);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawText(labels[i], (int)x + 2, (int)h - 15, 40, 10, juce::Justification::left);
    }

    // ----- カーブ描画 -----
    g.setColour(juce::Colour(0xff00D2D3));

    auto cPos = processor.getLfoPos(1);
    auto rPos = processor.getLfoPos(2);

    bool lfo1_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo2wave")->load() == 3)
        && (processor.apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    bool lfo2_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo3wave")->load() == 3)
        && (processor.apvts.getRawParameterValue("lfo3en")->load() > 0.5f);

    auto lfo1Mod4 = processor.getLfoMod4(1);
    auto lfo2Mod4 = processor.getLfoMod4(2);

    auto getM = [](juce::Point<float> p, const std::array<float, 4>& m4, bool isRand1)
        -> std::array<float, 4>
        {
            std::array<float, 4> res;
            if (isRand1) { for (int i = 0; i < 4; ++i) res[i] = m4[i] * 2.0f - 1.0f; }
            else {
                res[0] = p.x * 2.0f - 1.0f;
                res[1] = p.y * 2.0f - 1.0f;
                res[2] = (1.0f - p.x) * 2.0f - 1.0f;
                res[3] = (1.0f - p.y) * 2.0f - 1.0f;
            }
            return res;
        };

    std::array<float, 4> cM = getM(cPos, lfo1Mod4, lfo1_isRand1);
    std::array<float, 4> rM = getM(rPos, lfo2Mod4, lfo2_isRand1);

    int wInt = (int)w;
    std::vector<float> rawMag(wInt + 1, 0.0f);

    for (int px = 0; px <= wInt; ++px)
    {
        float freq = 20.0f * std::pow(1000.0f, (float)px / w);

        auto calc = [&](juce::String s, int idx) -> float
            {
                if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f) return 0.0f;

                float baseCutoff = processor.apvts.getRawParameterValue("cutoff" + s)->load();
                float fc = baseCutoff * std::pow(2.0f, 4.0f * cM[idx]);
                float baseRes = processor.apvts.getRawParameterValue("res" + s)->load();
                float res = baseRes * std::pow(2.0f, 2.0f * rM[idx]);
                int modelIdx = (int)processor.apvts.getRawParameterValue("model" + s)->load();
                int slopeIdx = (int)processor.apvts.getRawParameterValue("slope" + s)->load();
                int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();

                float freqLimit = juce::jlimit(20.0f, 20000.0f, fc);
                float w_norm = freq / freqLimit;
                float w2 = w_norm * w_norm;
                float mag = 1.0f;

                if (modelIdx == 0 || modelIdx == 3 || modelIdx == 4 || modelIdx == 9
                    || modelIdx == 14 || modelIdx == 16 || modelIdx == 23)
                {
                    int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
                    float adjustedRes = res;
                    if (modelIdx != 14 && stages > 1)
                        adjustedRes = res * std::pow(0.6f, std::log2((float)stages));
                    float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
                    float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
                    float m = 1.0f / den;
                    if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
                    mag = std::pow(m, stages);
                    if (modelIdx == 0 || modelIdx == 4 || modelIdx == 16 || modelIdx == 23)
                        mag *= (1.0f + res * 0.1f);
                    if (modelIdx == 9)
                        mag *= juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 1.0f, 5.0f);
                }
                else if (modelIdx == 1 || modelIdx == 12 || modelIdx == 13 || modelIdx == 15)
                {
                    int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 1 : (slopeIdx == 2) ? 2 : 4;
                    float r_scale = (modelIdx == 13) ? 5.0f : 4.0f;
                    float r_moog = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, r_scale);
                    if (stages > 1) r_moog *= std::pow(0.7f, std::log2((float)stages));
                    float real_p = std::pow(1.0f - w2, 2.0f) - 4.0f * w2 + r_moog;
                    float imag_p = 4.0f * w_norm * (1.0f - w2);
                    mag = std::pow(1.0f / std::sqrt(real_p * real_p + imag_p * imag_p), stages);
                    if (slopeIdx == 0) { if (t == 1) mag *= w_norm; else if (t == 2) mag *= w2; else if (t == 3) mag *= std::abs(1.0f - w2); }
                    else { if (t == 1) mag *= w2;     else if (t == 2) mag *= w2 * w2; else if (t == 3) mag *= std::abs(1.0f - w2 * w2); }
                    mag *= (1.0f + 0.5f * r_moog);
                }
                else if (modelIdx == 2)
                {
                    int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 1 : (slopeIdx == 2) ? 2 : 4;
                    float r_diode = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 15.0f);
                    if (stages > 1) r_diode *= std::pow(0.7f, std::log2((float)stages));
                    float real_p = std::pow(1.0f - w2, 2.0f) - 3.5f * w2 + r_diode;
                    float imag_p = 3.5f * w_norm * (1.0f - w2);
                    mag = std::pow(1.0f / std::sqrt(real_p * real_p + imag_p * imag_p), stages);
                    if (slopeIdx == 0) { if (t == 1) mag *= w_norm; else if (t == 2) mag *= w2; else if (t == 3) mag *= std::abs(1.0f - w2); }
                    else { if (t == 1) mag *= w2 * w_norm; else if (t == 2) mag *= w2 * w2; else if (t == 3) mag *= std::abs(1.0f - w2 * w_norm); }
                    mag *= (1.0f + 0.2f * r_diode);
                }
                else if (modelIdx == 5)
                {
                    float v = juce::jmap(std::log10(freqLimit), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f);
                    int   idx_v = (int)v; float frac = v - idx_v; if (idx_v >= 4) { idx_v = 3; frac = 1.0f; }
                    float f1_m[5] = { 730.f, 270.f, 300.f, 530.f, 400.f };
                    float f2_m[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f };
                    float f3_m[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
                    float f_arr[3] = {
                        f1_m[idx_v] + (f1_m[idx_v + 1] - f1_m[idx_v]) * frac,
                        f2_m[idx_v] + (f2_m[idx_v + 1] - f2_m[idx_v]) * frac,
                        f3_m[idx_v] + (f3_m[idx_v + 1] - f3_m[idx_v]) * frac
                    };
                    float mag_sum = 0.0f; float gains[3] = { 1.0f, 0.5f, 0.2f };
                    for (int f = 0; f < 3; ++f) {
                        float w_f = freq / juce::jlimit(20.0f, 20000.0f, f_arr[f]);
                        float d_f = 1.0f / juce::jlimit(0.1f, 10.0f, res);
                        float m_f = 1.0f / std::sqrt(std::pow(1.0f - w_f * w_f, 2.0f) + std::pow(w_f * d_f, 2.0f));
                        if (t == 1) m_f *= w_f; else if (t == 2) m_f *= w_f * w_f; else if (t == 3) m_f *= std::abs(1.0f - w_f * w_f);
                        mag_sum += m_f * gains[f];
                    }
                    mag = mag_sum * (1.0f + res * 0.1f);
                }
                else if (modelIdx == 6)
                {
                    int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
                    float delaySamples = 44100.0f / juce::jlimit(20.0f, 20000.0f, fc);
                    float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
                    if (t == 1 || t == 3) fb = -fb;
                    float wD = 2.0f * juce::MathConstants<float>::pi * freq * (delaySamples / 44100.0f);
                    float m = (t == 0 || t == 1)
                        ? (1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD)))
                        : std::sqrt(1.0f + fb * fb + 2.0f * fb * std::cos(wD));
                    mag = std::pow(m, stages);
                }
                else if (modelIdx == 7)
                {
                    int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
                    float ms_res = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 2.5f);
                    float d = 1.0f / (ms_res + 0.1f);
                    float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
                    float m = 1.0f / den;
                    if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
                    mag = std::pow(m, stages) * (1.0f + ms_res * 0.2f);
                }
                else if (modelIdx == 8)
                {
                    int stages = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
                    float phi = -2.0f * std::atan(freq / juce::jlimit(20.0f, 20000.0f, fc));
                    float fb = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 0.95f);
                    if (t == 1 || t == 3) fb = -fb;
                    float c_ap = std::cos(stages * phi); float s_ap = std::sin(stages * phi);
                    float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
                    float real_yap = (c_ap - fb) / den2; float imag_yap = s_ap / den2;
                    float real_out = 0.5f * (1.0f + real_yap); float imag_out = 0.5f * imag_yap;
                    mag = std::sqrt(real_out * real_out + imag_out * imag_out);
                }
                else if (modelIdx == 10)
                {
                    float ms = juce::jmap(fc, 20.0f, 20000.0f, 50.0f, 0.5f);
                    float baseD = (ms / 1000.0f);
                    float wD1 = 2.0f * juce::MathConstants<float>::pi * freq * (baseD * 1.000f);
                    float wD2 = 2.0f * juce::MathConstants<float>::pi * freq * (baseD * 1.313f);
                    float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
                    float m1 = 1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD1));
                    float m2 = 1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD2));
                    mag = (m1 + m2) * 0.5f;
                }
                else if (modelIdx == 11)
                {
                    int stages = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
                    float d = 1.0f / juce::jlimit(0.1f, 10.0f, res);
                    float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
                    float bp_mag = (1.0f / den) * w_norm;
                    mag = 1.0f + std::pow(bp_mag, 1.2f) * res * ((float)stages * 0.1f);
                }
                else if (modelIdx >= 17 && modelIdx <= 20)
                {
                    int order = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
                    int sections = order / 2;
                    float rippleDb = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.1f, 3.0f);
                    float eps = std::sqrt(std::pow(10.0f, rippleDb / 10.0f) - 1.0f);
                    float mag_total = 1.0f;
                    for (int k = 0; k < sections; ++k)
                    {
                        float theta = juce::MathConstants<float>::pi * (2.0f * k + 1.0f) / (2.0f * order);
                        float stage_q = 0.707f;
                        float freqScale = 1.0f;
                        if (modelIdx == 17) {
                            stage_q = 1.0f / (2.0f * std::sin(theta));
                        }
                        else if (modelIdx == 18) {
                            float a = 1.0f / order * std::asinh(1.0f / eps);
                            float real_p = -std::sinh(a) * std::sin(theta);
                            float imag_p = std::cosh(a) * std::cos(theta);
                            float wn2 = real_p * real_p + imag_p * imag_p;
                            freqScale = std::sqrt(wn2); stage_q = std::sqrt(wn2) / (-2.0f * real_p);
                        }
                        else if (modelIdx == 19) {
                            stage_q = 1.0f / (2.0f * std::sin(theta)) * 0.577f;
                            freqScale = 1.0f + (float)order * 0.1f;
                        }
                        else if (modelIdx == 20) {
                            float a = 1.0f / order * std::asinh(1.0f / (eps * 0.5f));
                            float real_p = -std::sinh(a) * std::sin(theta);
                            float imag_p = std::cosh(a) * std::cos(theta);
                            float wn2 = real_p * real_p + imag_p * imag_p;
                            freqScale = std::sqrt(wn2); stage_q = std::sqrt(wn2) / (-2.0f * real_p) * 1.2f;
                        }
                        float stage_w = freq / juce::jlimit(20.0f, 20000.0f, freqLimit * freqScale);
                        float stage_w2 = stage_w * stage_w;
                        float stage_d = 1.0f / stage_q;
                        float den = std::sqrt(std::pow(1.0f - stage_w2, 2.0f) + std::pow(stage_w * stage_d, 2.0f));
                        float m = 1.0f / den;
                        if (modelIdx == 20) { m = std::abs(1.0f - stage_w2 * 0.5f) / den; }
                        else { if (t == 1) m *= stage_w; else if (t == 2) m *= stage_w2; else if (t == 3) m *= std::abs(1.0f - stage_w2); }
                        mag_total *= m;
                    }
                    mag = mag_total;
                }
                else if (modelIdx == 21)
                {
                    float lpgFc = juce::jlimit(20.0f, 15000.0f, fc);
                    float d = 1.0f / 0.707f;
                    float w_lpg = freq / lpgFc;
                    mag = (1.0f / std::sqrt(std::pow(1.0f - w_lpg * w_lpg, 2.0f)
                        + std::pow(w_lpg * d, 2.0f)));
                }
                else if (modelIdx == 22)
                {
                    float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);
                    float mag_sum = 0.0f;
                    for (int b = 0; b < 8; ++b) {
                        float bFreq = std::clamp(fc * std::pow((float)(b + 1), inharmonicity), 20.0f, 20000.0f);
                        float stage_w = freq / bFreq;
                        float q = 50.0f / std::sqrt((float)(b + 1));
                        float d = 1.0f / q;
                        mag_sum += (1.0f / std::sqrt(std::pow(1.0f - stage_w * stage_w, 2.0f)
                            + std::pow(stage_w * d, 2.0f)))
                            * stage_w * (1.0f / std::sqrt((float)(b + 1)));
                    }
                    mag = mag_sum;
                }
                else if (modelIdx == 24)
                {
                    float shiftHz = juce::jmap(fc, 20.0f, 20000.0f, -1000.0f, 1000.0f);
                    if (std::abs(freq - (1000.0f + shiftHz)) < 100.0f) mag = 5.0f; else mag = 1.0f;
                }
                else if (modelIdx == 25)
                {
                    float x_zp = juce::jlimit(0.0f, 1.0f, juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 1.0f));
                    float y_zp = juce::jlimit(0.0f, 1.0f, juce::jmap(res, 0.1f, 10.0f, 0.0f, 1.0f));
                    const float fA[7] = { 730, 1090, 2440, 4000, 6000, 8000, 10000 };
                    const float qA[7] = { 4.0f, 4.0f, 3.0f, 1.0f, 1.0f, 1.0f, 1.0f };
                    const float fB[7] = { 200, 500, 1200, 2800, 5000, 8500, 12000 };
                    const float qB[7] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
                    const float fC[7] = { 300, 870, 2240, 4000, 6000, 8000, 10000 };
                    const float qC[7] = { 5.0f, 4.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f };
                    const float fD[7] = { 80, 120, 200, 4000, 8000, 12000, 16000 };
                    const float qD[7] = { 3.0f, 2.0f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f };
                    float mag_total = 1.0f;
                    for (int k = 0; k < 7; ++k) {
                        float stage_fc = fA[k] * (1 - x_zp) * (1 - y_zp) + fB[k] * x_zp * (1 - y_zp)
                            + fC[k] * (1 - x_zp) * y_zp + fD[k] * x_zp * y_zp;
                        float stage_q = std::max(0.5f, qA[k] * (1 - x_zp) * (1 - y_zp)
                            + qB[k] * x_zp * (1 - y_zp)
                            + qC[k] * (1 - x_zp) * y_zp
                            + qD[k] * x_zp * y_zp);
                        float stage_w = freq / juce::jlimit(20.0f, 20000.0f, stage_fc);
                        float den = std::sqrt(std::pow(1.0f - stage_w * stage_w, 2.0f)
                            + std::pow(stage_w * (1.0f / stage_q), 2.0f));
                        mag_total *= (1.0f / den);
                    }
                    mag = mag_total;
                }
                else if (modelIdx == 26)
                {
                    int stages = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
                    float phi = -2.0f * std::atan(w_norm);
                    float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.8f);
                    float c_ap = std::cos(stages * phi); float s_ap = std::sin(stages * phi);
                    float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
                    float real_yap = (c_ap - fb) / den2; float imag_yap = s_ap / den2;
                    mag = std::sqrt(real_yap * real_yap + imag_yap * imag_yap);
                }
                else if (modelIdx == 27)
                {
                    float d = 1.0f / 1.5f;
                    float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
                    mag = std::pow(1.0f / den, 4.0f);
                }

                return static_cast<float>(mag);
            };

        // ===== 【Option A】ビジュアライザーも等パワーwMixに統一 =====
        auto mPos = processor.getLfoPos(0);
        float x = mPos.x;
        float y = mPos.y;

        float angleX = x * juce::MathConstants<float>::halfPi;
        float angleY = y * juce::MathConstants<float>::halfPi;
        float cosX = std::cos(angleX), sinX = std::sin(angleX);
        float cosY = std::cos(angleY), sinY = std::sin(angleY);

        float wA = cosX * cosY;
        float wB = sinX * cosY;
        float wC = cosX * sinY;
        float wD = sinX * sinY;

        rawMag[px] = calc("A", 0) * wA
            + calc("B", 1) * wB
            + calc("C", 2) * wC
            + calc("D", 3) * wD;
    }

    // ----- スムージングと描画 -----
    juce::Path path;
    int smoothRadius = 8;

    for (int px = 0; px <= wInt; ++px)
    {
        float sum = 0.0f; int count = 0;
        for (int k = -smoothRadius; k <= smoothRadius; ++k)
        {
            int idx = px + k;
            if (idx >= 0 && idx <= wInt) { sum += rawMag[idx]; count++; }
        }
        float smoothedMag = sum / (float)count;

        float db = 20.0f * std::log10(smoothedMag + 1e-5f);

        // ===== 【Slope表示修正】拡張された dB 範囲でマッピング =====
        float yPos = juce::jmap(db, dbTop, dbBottom, 0.0f, h);

        if (px == 0) path.startNewSubPath((float)px, yPos);
        else         path.lineTo((float)px, yPos);
    }

    g.strokePath(path, juce::PathStrokeType(2.0f));
}

// ==========================================
// XYPadComponent
// ==========================================
XYPadComponent::XYPadComponent(QuadMorphFilterAudioProcessor& p) : processor(p)
{
    for (int i = 0; i < 3; ++i) trails[i].fill({ 0.5f, 0.5f });
    startTimerHz(60);
}

void XYPadComponent::timerCallback()
{
    for (int i = 0; i < 3; ++i) {
        trails[i][trailIdx[i]] = processor.getLfoPos(i);
        trailIdx[i] = (trailIdx[i] + 1) % 30;
    }
    repaint();
}

void XYPadComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff1E272E));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xffD5DDE5));
    g.drawRoundedRectangle(b, 8.0f, 1.5f);

    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(14.0f);
    g.drawText("A", 10, 10, 20, 20, juce::Justification::centred);
    g.drawText("B", getWidth() - 30, 10, 20, 20, juce::Justification::centred);
    g.drawText("C", 10, getHeight() - 30, 20, 20, juce::Justification::centred);
    g.drawText("D", getWidth() - 30, getHeight() - 30, 20, 20, juce::Justification::centred);

    juce::Colour colors[] = {
        juce::Colour(0xff00D2D3),
        juce::Colour(0xffFF9FF3),
        juce::Colour(0xffFEECA1)
    };

    for (int i = 0; i < 3; ++i)
    {
        if (processor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "en")->load() < 0.5f)
            continue;

        for (int t = 0; t < 30; ++t) {
            int idx = (trailIdx[i] + t) % 30;
            auto pt = trails[i][idx];
            float alpha = (float)t / 30.0f * 0.5f;
            g.setColour(colors[i].withAlpha(alpha));
            g.fillEllipse(pt.x * getWidth() - 3, pt.y * getHeight() - 3, 6, 6);
        }

        auto p = processor.getLfoPos(i);
        bool isWait = processor.isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = processor.isRecordingDrag[i].load(std::memory_order_acquire);

        if (isWait) {
            if (isDrag) {
                g.setColour(colors[i].brighter(0.8f));
                g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16);
            }
            else {
                float alpha = 0.3f + 0.7f * std::abs(std::sin(juce::Time::getMillisecondCounter() * 0.005f));
                g.setColour(colors[i].withAlpha(alpha));
                g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16);
            }
        }
        else {
            g.setColour(colors[i]);
            g.fillEllipse(p.x * getWidth() - 6, p.y * getHeight() - 6, 12, 12);
        }
        g.setColour(juce::Colours::white);
        g.drawEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16, 1.0f);
    }
}

void XYPadComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) {
                processor.isWaitingForRecord[i].store(false);
                processor.isRecordingDrag[i].store(false);
                draggingLfoIndex = -1;
                return;
            }
        }
        float ww = (float)getWidth();
        float hh = (float)getHeight();
        for (int i = 0; i < 3; ++i) {
            int wave = (int)processor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "wave")->load();
            if (wave == 6) {
                auto p = processor.getLfoPos(i);
                float px = p.x * ww, py = p.y * hh;
                if (std::hypot(e.x - px, e.y - py) < 15.0f) {
                    processor.recLength[i].store(0);
                    processor.isWaitingForRecord[i].store(true);
                    return;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) {
                draggingLfoIndex = i;
                processor.isRecordingDrag[i].store(true);
                float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
                float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
                processor.currentRecX[i].store(nx);
                processor.currentRecY[i].store(ny);
                return;
            }
        }
    }
    updatePosition(e);
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingLfoIndex != -1 && processor.isRecordingDrag[draggingLfoIndex].load())
    {
        int len = processor.recLength[draggingLfoIndex].load();
        if (len < 2048) {
            float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
            float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
            processor.recBuffer[draggingLfoIndex][len] = { nx, ny };
            processor.recLength[draggingLfoIndex].store(len + 1);
            processor.currentRecX[draggingLfoIndex].store(nx);
            processor.currentRecY[draggingLfoIndex].store(ny);
        }
        return;
    }
    updatePosition(e);
}

void XYPadComponent::mouseUp(const juce::MouseEvent&)
{
    if (draggingLfoIndex != -1) {
        processor.isRecordingDrag[draggingLfoIndex].store(false);
        draggingLfoIndex = -1;
    }
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e)
{
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
    float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
}

// ==========================================
// Editor
// ==========================================
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

    addAndMakeVisible(visualizer);
    addAndMakeVisible(xyPad);

    setupFilterGroup(groupA, "A", "Filter A");
    setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C");
    setupFilterGroup(groupD, "D", "Filter D");

    setupLfoGroup(lfos[0], 1, "LFO 1 (Morph)");
    setupLfoGroup(lfos[1], 2, "LFO 2 (Cutoff)");
    setupLfoGroup(lfos[2], 3, "LFO 3 (Reso)");

    auto setupMaster = [&](juce::Label& l, juce::Slider& sl, juce::String txt,
        juce::String id,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            l.setText(txt, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(l);
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff2980B9));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id, sl);
        };

    setupMaster(masterGainLabel, masterGainSlider, "Out Gain", "masterGain", mgAtt);
    setupMaster(dryWetLabel, dryWetSlider, "Dry/Wet", "dryWet", dwAtt);
    setupMaster(ceilingLabel, ceilingSlider, "Limit Ceil", "limiterCeiling", clAtt);

    setSize(1000, 680);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g,
    juce::String s,
    juce::String name)
{
    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00D2D3));
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "enable" + s, g.enableButton);

    g.model.addItemList({
        "Clean SVF", "Moog Ladder", "Diode (TB-303)", "SEM (Oberheim)", "Bitcrush / SRR",
        "Formant (Vowel)", "Comb Filter", "MS-20 (Screaming)", "All-Pass Phaser", "Wavefolder",
        "Reverb (Metallic)", "Kilo All-Pass",
        "Prophet (Curtis)", "SSM 2040", "CS-80 (Yamaha)", "Jupiter (Roland)", "EDP Wasp (CMOS)",
        "Butterworth (Flat)", "Chebyshev (Ripple)", "Bessel (Phase)", "Elliptic (Notch)",
        "Vactrol LPG", "Modal Resonator", "Waveguide Mesh", "Bode Freq Shifter",
        "Z-Plane (2D Morph)", "Phased Array", "Nyquist Anti-alias"
        }, 1);
    addAndMakeVisible(g.model);
    g.mAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "model" + s, g.model);

    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1);
    addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "type" + s, g.type);

    g.slope.addItemList({ "12dB", "24dB", "48dB", "96dB" }, 1);
    addAndMakeVisible(g.slope);
    g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "slope" + s, g.slope);

    auto setup = [&](juce::Label& l, juce::Slider& sl, juce::String txt, juce::String id,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            l.setText(txt, juce::dontSendNotification);
            addAndMakeVisible(l);
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00D2D3));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id + s, sl);
        };

    setup(g.cutoffLabel, g.cutoff, "Cut", "cutoff", g.cAtt);
    setup(g.resLabel, g.res, "Res", "res", g.rAtt);
}

void QuadMorphFilterAudioProcessorEditor::setupLfoGroup(LfoGroup& g, int idx, juce::String name)
{
    juce::String id = "lfo" + juce::String(idx);
    juce::Colour lfoCols[] = {
        juce::Colour(0xff00D2D3),
        juce::Colour(0xffFF9FF3),
        juce::Colour(0xffFEECA1)
    };

    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "en", g.enableButton);

    juce::StringArray waveTypes = {
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard", "Polygon", "Attractor Orbit"
    };
    g.wave.addItemList(waveTypes, 1);
    addAndMakeVisible(g.wave);
    g.wAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "wave", g.wave);

    g.boundCombo.addItemList({ "Clip", "Bounce", "Wrap" }, 1);
    addAndMakeVisible(g.boundCombo);
    g.bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "bound", g.boundCombo);

    g.stepMode.setButtonText("Step");
    g.stepMode.setClickingTogglesState(true);
    g.stepMode.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.stepMode);
    g.sAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "step", g.stepMode);

    g.syncToggle.setButtonText("Sync");
    g.syncToggle.setClickingTogglesState(true);
    g.syncToggle.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    g.syncToggle.onClick = [this] { resized(); };
    addAndMakeVisible(g.syncToggle);
    g.syAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "sync", g.syncToggle);

    juce::StringArray syncRates = {
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };
    g.rateSync.addItemList(syncRates, 1);
    addAndMakeVisible(g.rateSync);
    g.rsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "rateSync", g.rateSync);

    auto setupSlider = [&](juce::Slider& sl,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
        juce::String paramId)
        {
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
            sl.setColour(juce::Slider::thumbColourId, lfoCols[idx - 1]);
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, paramId, sl);
        };

    setupSlider(g.rateFree, g.rfAtt, id + "rateFree");
    setupSlider(g.minSlider, g.minAtt, id + "min");
    setupSlider(g.maxSlider, g.maxAtt, id + "max");
}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffF0F4F8));
}

void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced(15);
    auto top = b.removeFromTop(300);

    visualizer.setBounds(top.removeFromLeft(top.getWidth() * 0.7f).reduced(5));
    xyPad.setBounds(top.reduced(5));

    b.removeFromTop(10);

    for (auto* g : { &groupA, &groupB, &groupC, &groupD })
    {
        auto r = b.removeFromTop(38).reduced(5, 2);
        g->enableButton.setBounds(r.removeFromLeft(60).reduced(0, 5));
        g->model.setBounds(r.removeFromLeft(115).reduced(2, 5));
        g->type.setBounds(r.removeFromLeft(60).reduced(2, 5));
        g->slope.setBounds(r.removeFromLeft(85).reduced(2, 5));
        auto cutArea = r.removeFromLeft(r.getWidth() / 2).reduced(5, 0);
        auto resArea = r.reduced(5, 0);
        g->cutoffLabel.setBounds(cutArea.removeFromLeft(30));
        g->cutoff.setBounds(cutArea);
        g->resLabel.setBounds(resArea.removeFromLeft(30));
        g->res.setBounds(resArea);
    }

    b.removeFromTop(10);

    for (int i = 0; i < 3; ++i)
    {
        auto r = b.removeFromTop(38).reduced(5, 2);
        lfos[i].enableButton.setBounds(r.removeFromLeft(100).reduced(0, 5));
        lfos[i].wave.setBounds(r.removeFromLeft(120).withSizeKeepingCentre(115, 22));
        lfos[i].boundCombo.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 22));
        lfos[i].stepMode.setBounds(r.removeFromLeft(50).reduced(2, 5));
        lfos[i].syncToggle.setBounds(r.removeFromLeft(50).reduced(2, 5));

        auto remainingWidth = r.getWidth();
        auto rateArea = r.removeFromLeft(remainingWidth / 3);
        auto minArea = r.removeFromLeft(remainingWidth / 3);
        auto maxArea = r;

        if (audioProcessor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "sync")->load() > 0.5f)
        {
            lfos[i].rateSync.setBounds(rateArea.withSizeKeepingCentre(rateArea.getWidth() - 5, 22));
            lfos[i].rateFree.setVisible(false);
            lfos[i].rateSync.setVisible(true);
        }
        else
        {
            lfos[i].rateFree.setBounds(rateArea.reduced(2, 8));
            lfos[i].rateFree.setVisible(true);
            lfos[i].rateSync.setVisible(false);
        }
        lfos[i].minSlider.setBounds(minArea.reduced(2, 8));
        lfos[i].maxSlider.setBounds(maxArea.reduced(2, 8));
    }

    b.removeFromTop(15);
    auto masterArea = b.removeFromTop(38).reduced(5, 2);
    auto cellW = masterArea.getWidth() / 3;

    auto gainRect = masterArea.removeFromLeft(cellW);
    masterGainLabel.setBounds(gainRect.removeFromLeft(60));
    masterGainSlider.setBounds(gainRect);

    auto dwRect = masterArea.removeFromLeft(cellW);
    dryWetLabel.setBounds(dwRect.removeFromLeft(60));
    dryWetSlider.setBounds(dwRect);

    auto ceilRect = masterArea;
    ceilingLabel.setBounds(ceilRect.removeFromLeft(60));
    ceilingSlider.setBounds(ceilRect);
}