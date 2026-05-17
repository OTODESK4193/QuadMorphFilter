// ==========================================
// UI/FilterVisualizer.cpp
// ==========================================
#include "FilterVisualizer.h"
#include "../PluginProcessor.h"

FilterVisualizer::FilterVisualizer(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    startTimerHz(60);
}

void FilterVisualizer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1E272E));
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    const float dbTop = 60.0f;
    const float dbBottom = -120.0f;

    // ----- 0dB ライン -----
    float y0dB = juce::jmap(0.0f, dbTop, dbBottom, 0.0f, h);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawHorizontalLine((int)y0dB, 0.0f, w);
    g.setFont(10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawText("0dB", 2, (int)y0dB - 12, 35, 10, juce::Justification::left);

    // ----- dB グリッド -----
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

    // ----- 周波数グリッド -----
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

    // ----- カーブ描画準備 -----
    g.setColour(juce::Colour(0xff00D2D3));

    // LFO2/3 情報
    auto cPos = processor.getLfoPos(1);
    auto rPos = processor.getLfoPos(2);
    auto lfo1Mod4 = processor.getLfoMod4(1);
    auto lfo2Mod4 = processor.getLfoMod4(2);

    bool lfo1_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo2wave")->load() == 3)
        && (processor.apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    bool lfo2_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo3wave")->load() == 3)
        && (processor.apvts.getRawParameterValue("lfo3en")->load() > 0.5f);

    auto getM = [](juce::Point<float> p, const std::array<float, 4>& m4, bool isRand1)
        -> std::array<float, 4>
        {
            std::array<float, 4> res;
            if (isRand1) { for (int i = 0; i < 4; ++i) res[i] = m4[i] * 2.0f - 1.0f; }
            else {
                res[0] = p.x * 2.0f - 1.0f;  res[1] = p.y * 2.0f - 1.0f;
                res[2] = (1.0f - p.x) * 2.0f - 1.0f;  res[3] = (1.0f - p.y) * 2.0f - 1.0f;
            }
            return res;
        };

    std::array<float, 4> cM = getM(cPos, lfo1Mod4, lfo1_isRand1);
    std::array<float, 4> rM = getM(rPos, lfo2Mod4, lfo2_isRand1);

    // ===== XYモード読み取り（ループ外で1回計算）=====
    int xyMode = (int)processor.apvts.getRawParameterValue("xyMode")->load();
    auto mPos = processor.getLfoPos(0);
    float xyCutoff = 20.0f * std::pow(1000.0f, mPos.x);
    float xyRes = juce::jlimit(0.1f, 10.0f, 0.1f + mPos.y * 9.9f);

    // ===== 有効フィルターフラグ（ループ外で1回取得）=====
    bool visEnA = processor.apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool visEnB = processor.apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool visEnC = processor.apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool visEnD = processor.apvts.getRawParameterValue("enableD")->load() > 0.5f;

    // ===== wMix 計算（ループ外で1回、xyModeで切り替え）=====
    float wA, wB, wC, wD;
    if (xyMode == 1)
    {
        // Cutoffモード: 等パワー
        int numActive = (visEnA ? 1 : 0) + (visEnB ? 1 : 0) + (visEnC ? 1 : 0) + (visEnD ? 1 : 0);
        float w = (numActive > 0) ? (1.0f / std::sqrt((float)numActive)) : 0.0f;
        wA = visEnA ? w : 0.0f;
        wB = visEnB ? w : 0.0f;
        wC = visEnC ? w : 0.0f;
        wD = visEnD ? w : 0.0f;
    }
    else
    {
        // Morphモード: 等パワー + 有効フィルター正規化
        float angleX = mPos.x * juce::MathConstants<float>::halfPi;
        float angleY = mPos.y * juce::MathConstants<float>::halfPi;
        float cosX = std::cos(angleX), sinX = std::sin(angleX);
        float cosY = std::cos(angleY), sinY = std::sin(angleY);
        wA = cosX * cosY;  wB = sinX * cosY;
        wC = cosX * sinY;  wD = sinX * sinY;

        float sumSq = 0.0f;
        if (visEnA) sumSq += wA * wA;
        if (visEnB) sumSq += wB * wB;
        if (visEnC) sumSq += wC * wC;
        if (visEnD) sumSq += wD * wD;
        if (sumSq > 1e-8f)
        {
            float norm = 1.0f / std::sqrt(sumSq);
            if (visEnA) wA *= norm;
            if (visEnB) wB *= norm;
            if (visEnC) wC *= norm;
            if (visEnD) wD *= norm;
        }
    }

    // ===== 周波数応答の計算 =====
    int wInt = (int)w;
    std::vector<float> rawMag(wInt + 1, 0.0f);

    for (int px = 0; px <= wInt; ++px)
    {
        float freq = 20.0f * std::pow(1000.0f, (float)px / w);

        auto calc = [&](juce::String s, int idx) -> float
            {
                if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f)
                    return 0.0f;

                // ===== xyModeに応じて fc/res を切り替え =====
                float fc, res;
                if (xyMode == 1)
                {
                    // Cutoffモード: XY位置から直接
                    fc = xyCutoff;
                    res = xyRes;
                }
                else
                {
                    // Morphモード: スライダー値 + LFOモジュレーション
                    float baseCutoff = processor.apvts.getRawParameterValue("cutoff" + s)->load();
                    fc = baseCutoff * std::pow(2.0f, 4.0f * cM[idx]);
                    float baseRes = processor.apvts.getRawParameterValue("res" + s)->load();
                    res = baseRes * std::pow(2.0f, 2.0f * rM[idx]);
                }

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
                    if (slopeIdx == 0) {
                        if (t == 1) mag *= w_norm; else if (t == 2) mag *= w2; else if (t == 3) mag *= std::abs(1.0f - w2);
                    }
                    else {
                        if (t == 1) mag *= w2; else if (t == 2) mag *= w2 * w2; else if (t == 3) mag *= std::abs(1.0f - w2 * w2);
                    }
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
                    if (slopeIdx == 0) {
                        if (t == 1) mag *= w_norm; else if (t == 2) mag *= w2; else if (t == 3) mag *= std::abs(1.0f - w2);
                    }
                    else {
                        if (t == 1) mag *= w2 * w_norm; else if (t == 2) mag *= w2 * w2; else if (t == 3) mag *= std::abs(1.0f - w2 * w_norm);
                    }
                    mag *= (1.0f + 0.2f * r_diode);
                }
                else if (modelIdx == 5)
                {
                    float v = juce::jmap(std::log10(freqLimit), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f);
                    int idx_v = (int)v; float frac = v - idx_v;
                    if (idx_v >= 4) { idx_v = 3; frac = 1.0f; }
                    float f1_m[5] = { 730.f,  270.f,  300.f,  530.f,  400.f };
                    float f2_m[5] = { 1090.f, 2290.f, 870.f,  1840.f, 840.f };
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
                            float rp = -std::sinh(a) * std::sin(theta);
                            float ip = std::cosh(a) * std::cos(theta);
                            float wn2 = rp * rp + ip * ip;
                            freqScale = std::sqrt(wn2); stage_q = std::sqrt(wn2) / (-2.0f * rp);
                        }
                        else if (modelIdx == 19) {
                            stage_q = 1.0f / (2.0f * std::sin(theta)) * 0.577f;
                            freqScale = 1.0f + (float)order * 0.1f;
                        }
                        else if (modelIdx == 20) {
                            float a = 1.0f / order * std::asinh(1.0f / (eps * 0.5f));
                            float rp = -std::sinh(a) * std::sin(theta);
                            float ip = std::cosh(a) * std::cos(theta);
                            float wn2 = rp * rp + ip * ip;
                            freqScale = std::sqrt(wn2); stage_q = std::sqrt(wn2) / (-2.0f * rp) * 1.2f;
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
                    mag = 1.0f / std::sqrt(std::pow(1.0f - w_lpg * w_lpg, 2.0f) + std::pow(w_lpg * d, 2.0f));
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
                        mag_sum += (1.0f / std::sqrt(std::pow(1.0f - stage_w * stage_w, 2.0f) + std::pow(stage_w * d, 2.0f)))
                            * stage_w * (1.0f / std::sqrt((float)(b + 1)));
                    }
                    mag = mag_sum;
                }
                else if (modelIdx == 24)
                {
                    float shiftHz = juce::jmap(fc, 20.0f, 20000.0f, -1000.0f, 1000.0f);
                    mag = (std::abs(freq - (1000.0f + shiftHz)) < 100.0f) ? 5.0f : 1.0f;
                }
                else if (modelIdx == 25)
                {
                    float x_zp = juce::jlimit(0.0f, 1.0f, juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 1.0f));
                    float y_zp = juce::jlimit(0.0f, 1.0f, juce::jmap(res, 0.1f, 10.0f, 0.0f, 1.0f));
                    const float fA[7] = { 730,  1090, 2440, 4000, 6000, 8000, 10000 };
                    const float qA[7] = { 4.0f, 4.0f, 3.0f, 1.0f, 1.0f, 1.0f, 1.0f };
                    const float fB[7] = { 200,  500,  1200, 2800, 5000, 8500, 12000 };
                    const float qB[7] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
                    const float fC[7] = { 300,  870,  2240, 4000, 6000, 8000, 10000 };
                    const float qC[7] = { 5.0f, 4.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f };
                    const float fD[7] = { 80,   120,  200,  4000, 8000, 12000, 16000 };
                    const float qD[7] = { 3.0f, 2.0f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f };
                    float mag_total = 1.0f;
                    for (int k = 0; k < 7; ++k) {
                        float stage_fc = fA[k] * (1 - x_zp) * (1 - y_zp) + fB[k] * x_zp * (1 - y_zp) + fC[k] * (1 - x_zp) * y_zp + fD[k] * x_zp * y_zp;
                        float stage_q = std::max(0.5f, qA[k] * (1 - x_zp) * (1 - y_zp) + qB[k] * x_zp * (1 - y_zp) + qC[k] * (1 - x_zp) * y_zp + qD[k] * x_zp * y_zp);
                        float stage_w = freq / juce::jlimit(20.0f, 20000.0f, stage_fc);
                        float den = std::sqrt(std::pow(1.0f - stage_w * stage_w, 2.0f) + std::pow(stage_w * (1.0f / stage_q), 2.0f));
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
                    float rya = (c_ap - fb) / den2; float iya = s_ap / den2;
                    mag = std::sqrt(rya * rya + iya * iya);
                }
                else if (modelIdx == 27)
                {
                    float d = 1.0f / 1.5f;
                    float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
                    mag = std::pow(1.0f / den, 4.0f);
                }

                return static_cast<float>(mag);
            }; // end calc lambda

        rawMag[px] = calc("A", 0) * wA
            + calc("B", 1) * wB
            + calc("C", 2) * wC
            + calc("D", 3) * wD;
    }

    // ----- スムージングと描画 -----
    juce::Path path;
    const int smoothRadius = 8;

    for (int px = 0; px <= wInt; ++px)
    {
        float sum = 0.0f; int count = 0;
        for (int k = -smoothRadius; k <= smoothRadius; ++k)
        {
            int idx = px + k;
            if (idx >= 0 && idx <= wInt) { sum += rawMag[idx]; count++; }
        }
        float smoothedMag = (count > 0) ? (sum / (float)count) : 0.0f;
        float db = 20.0f * std::log10(smoothedMag + 1e-5f);
        float yPos = juce::jmap(db, dbTop, dbBottom, 0.0f, h);
        yPos = juce::jlimit(0.0f, h, yPos);

        if (px == 0) path.startNewSubPath((float)px, yPos);
        else         path.lineTo((float)px, yPos);
    }

    g.strokePath(path, juce::PathStrokeType(2.0f));
}