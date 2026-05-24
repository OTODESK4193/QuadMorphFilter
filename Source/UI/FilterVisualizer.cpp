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
    g.setColour(juce::Colour(0xff00D2D3));
    // ----- LFO 情報取得 -----
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
                res[0] = p.x * 2.0f - 1.0f;
                res[1] = p.y * 2.0f - 1.0f;
                res[2] = (1.0f - p.x) * 2.0f - 1.0f;
                res[3] = (1.0f - p.y) * 2.0f - 1.0f;
            }
            return res;
        };
    std::array<float, 4> cM = getM(cPos, lfo1Mod4, lfo1_isRand1);
    std::array<float, 4> rM = getM(rPos, lfo2Mod4, lfo2_isRand1);
    // ----- XYモード情報（ループ外で1回計算）-----
    int   xyMode = (int)processor.apvts.getRawParameterValue("xyMode")->load();
    auto  mPos = processor.getLfoPos(0);
    // X: 20Hz〜20kHz、Y: 上=大（反転）
    float xyCutoff = 20.0f * std::pow(1000.0f, mPos.x);
    float xyRes = juce::jlimit(0.1f, 10.0f, 0.1f + (1.0f - mPos.y) * 9.9f);
    // ----- 有効フィルター・wMix（ループ外）-----
    bool visEnA = processor.apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool visEnB = processor.apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool visEnC = processor.apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool visEnD = processor.apvts.getRawParameterValue("enableD")->load() > 0.5f;
    float wA, wB, wC, wD;
    if (xyMode == 1)
    {
        int numActive = (visEnA ? 1 : 0) + (visEnB ? 1 : 0) + (visEnC ? 1 : 0) + (visEnD ? 1 : 0);
        float w = (numActive > 0) ? (1.0f / std::sqrt((float)numActive)) : 0.0f;
        wA = visEnA ? w : 0.0f;
        wB = visEnB ? w : 0.0f;
        wC = visEnC ? w : 0.0f;
        wD = visEnD ? w : 0.0f;
    }
    else
    {
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
    // ----- 周波数応答の計算 -----
    int wInt = (int)w;
    std::vector<float> rawMag(wInt + 1, 0.0f);
    for (int px = 0; px <= wInt; ++px)
    {
        float freq = 20.0f * std::pow(1000.0f, (float)px / w);
        auto calc = [&](juce::String s, int idx) -> float
            {
                if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f)
                    return 0.0f;
                bool lfoCutOn = processor.apvts.getRawParameterValue("lfoCut" + s)->load() > 0.5f;
                bool lfoResOn = processor.apvts.getRawParameterValue("lfoRes" + s)->load() > 0.5f;
                float fc, res;
                if (xyMode == 1)
                {
                    float baseCutoff = lfoCutOn ? xyCutoff
                        : processor.apvts.getRawParameterValue("cutoff" + s)->load();
                    float baseRes = lfoResOn ? xyRes
                        : processor.apvts.getRawParameterValue("res" + s)->load();
                    fc = lfoCutOn ? baseCutoff * std::pow(2.0f, 4.0f * cM[idx]) : baseCutoff;
                    res = lfoResOn ? baseRes * std::pow(2.0f, 2.0f * rM[idx]) : baseRes;
                }
                else
                {
                    float baseCutoff = processor.apvts.getRawParameterValue("cutoff" + s)->load();
                    float baseRes = processor.apvts.getRawParameterValue("res" + s)->load();
                    fc = lfoCutOn ? baseCutoff * std::pow(2.0f, 4.0f * cM[idx]) : baseCutoff;
                    res = lfoResOn ? baseRes * std::pow(2.0f, 2.0f * rM[idx]) : baseRes;
                }
                fc = juce::jlimit(20.0f, 20000.0f, fc);
                res = juce::jlimit(0.1f, 10.0f, res);
                int modelIdx = juce::roundToInt(processor.apvts.getRawParameterValue("model" + s)->load());
                int slopeIdx = juce::roundToInt(processor.apvts.getRawParameterValue("slope" + s)->load());
                int t        = juce::roundToInt(processor.apvts.getRawParameterValue("type" + s)->load());
                float freqLimit = fc;
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
                else if (modelIdx == 1 || modelIdx == 12 || modelIdx == 13 || modelIdx == 15) {
                    // r_scale を DSP と一致させる (Moog=4.5, SSM2040=5.0)
                    float r_scale = (modelIdx == 13) ? 5.0f : 4.5f;
                    float r_moog = juce::jmap(juce::jlimit(0.1f, 10.0f, res),
                        0.1f, 10.0f, 0.0f, r_scale);
                    if (slopeIdx == 0) {
                        // 12dB: 2段出力 (y2) を近似
                        float Q_eff = juce::jlimit(0.5f, 15.0f, 0.5f + r_moog * 2.5f);
                        float den = std::sqrt(std::pow(1.0f - w2, 2.0f)
                            + std::pow(w_norm / Q_eff, 2.0f));
                        mag = 1.0f / den;
                        if (t == 1) mag *= w_norm;
                        else if (t == 2) mag *= w2;
                        else if (t == 3) mag *= std::abs(1.0f - w2);
                        float peak = (r_moog > 2.0f) ? (1.0f + (r_moog - 2.0f) * 0.8f) : 1.0f;
                        mag *= peak;
                    }
                    else {
                        // 24dB/48dB/96dB: 4段ラダー多段カスケード
                        int cascade = (slopeIdx == 1) ? 1 : (slopeIdx == 2) ? 2 : 4;
                        float r_sc = r_moog * std::pow(0.7f, std::log2((float)cascade));
                        float real_p = std::pow(1.0f - w2, 2.0f) - 4.0f * w2 + r_sc;
                        float imag_p = 4.0f * w_norm * (1.0f - w2);
                        float denom = std::max(std::sqrt(real_p * real_p + imag_p * imag_p), 0.005f);
                        mag = std::pow(1.0f / denom, cascade);
                        if (t == 1) mag *= w2;
                        else if (t == 2) mag *= w2 * w2;
                        else if (t == 3) mag *= std::abs(1.0f - w2 * w2);
                        mag *= (1.0f + 0.5f * r_sc);
                    }
                    mag = std::min(mag, 1000.0f);
                }
                else if (modelIdx == 2)
                {
                    // =====================================================
                    // TB-303 Diode Ladder ビジュアライザー
                    // （研究資料: Tim Stinchcombe + Deep Research 推奨モデル）
                    //
                    // 【Phi 関数による実効カットオフ上方シフト】
                    //   実機では Accent トリガー時に MEG 電圧が VCF カットオフ
                    //   制御電流に加算され、フィルターが高域へシフトする。
                    //   静的ビジュアライザーでは動的エンベロープが走らないため、
                    //   そのピーク効果を「定常カットオフへの乗算」として投影する。
                    //
                    //   Off  → Φ = 1.00  (シフトなし)
                    //   Low  → Φ = 1.21  (約 3.2 半音 ≈ Accent ポット中点)
                    //   High → Φ = 1.56  (約 7.7 半音 ≈ Accent ポット最大)
                    //
                    // 【k_max（各モードの自己発振上限）】
                    //   Off=4.2 / Low=4.6 / High=5.0
                    //   k > 3.8 でソフトクランプし +∞ 発散を防止。
                    //
                    // 【G_loss】レゾナンス上昇に伴うパスバンドゲインロスを近似。
                    // =====================================================

                    // ── Phi 関数: Accent モード別カットオフシフト ──
                    static constexpr float accentPhi[4] = { 1.0f, 1.21f, 1.56f, 1.56f };
                    const float phi    = accentPhi[juce::jlimit(0, 3, slopeIdx)];
                    const float fc_eff = juce::jlimit(20.0f, 20000.0f, fc * phi);

                    // ── k_max: Accent モード別（ビジュアライザー専用）──
                    // Stinchcombe 式のピーク高さ = 1/|k-4|。k が 4 に近いほどピーク大。
                    // k_max を 4.0 未満に揃え、High が最も 4 に近い値を持つことで
                    // Off < Low < High の順でピークが高く表示されるよう設計。
                    // ※ DSP（TptFilter_Ladder.cpp）の k_max（4.2/4.6/5.0）は別管理。
                    static constexpr float kMaxTab[4] = { 3.5f, 3.7f, 3.85f, 3.85f };
                    const float k_max = kMaxTab[juce::jlimit(0, 3, slopeIdx)];

                    // ── k: 線形マッピング（res → k）──
                    float k = juce::jmap(juce::jlimit(0.1f, 10.0f, res),
                                         0.1f, 10.0f, 0.0f, k_max);

                    // ── ソフトクランプ: tanh 飽和によるピーク頭打ち近似 ──
                    if (k > 3.8f)
                    {
                        const float excess = k - 3.8f;
                        k = 3.8f + excess / (1.0f + excess * 0.6f);
                    }

                    // ── 正規化周波数（Phi シフト後の fc_eff を基準）──
                    const float w_tb  = freq / fc_eff;
                    const float w2_tb = w_tb * w_tb;

                    // ── 8Hz HPF 補正（結合コンデンサ由来の低域減衰）──
                    const float hpf_mag = (freq / 8.0f)
                        / std::sqrt(1.0f + std::pow(freq / 8.0f, 2.0f));

                    // ── Stinchcombe 4極ダイオードラダー伝達関数 ──
                    //   |H| = 1 / |(1-ω²)² - 4ω² + k + j·4ω(1-ω²)|
                    const float real_p = std::pow(1.0f - w2_tb, 2.0f) - 4.0f * w2_tb + k;
                    const float imag_p = 4.0f * w_tb * (1.0f - w2_tb);
                    const float denom  = std::max(
                        std::sqrt(real_p * real_p + imag_p * imag_p), 0.003f);

                    // ── G_loss: レゾナンス上昇に伴うパスバンドゲインロス ──
                    const float g_loss = 1.0f / (1.0f + k * 0.07f);

                    mag = (1.0f / denom) * hpf_mag * g_loss;
                    mag = std::min(mag, 1000.0f);
                }
                else if (modelIdx == 5)
                {
                    float v = juce::jmap(std::log10(freqLimit), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f);
                    int idx_v = (int)v; float frac = v - idx_v;
                    if (idx_v >= 4) { idx_v = 3; frac = 1.0f; }
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
                    float delaySamples = 44100.0f / freqLimit;
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
                    float phi = -2.0f * std::atan(freq / freqLimit);
                    float fb = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 0.8f);
                    if (t == 1 || t == 3) fb = -fb;
                    float c_ap = std::cos(stages * phi); float s_ap = std::sin(stages * phi);
                    float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
                    float rya = (c_ap - fb) / den2; float iya = s_ap / den2;
                    float ro = 0.5f * (1.0f + rya); float io = 0.5f * iya;
                    mag = std::sqrt(ro * ro + io * io);
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
                    float bp = (1.0f / den) * w_norm;
                    mag = 1.0f + std::pow(bp, 1.2f) * res * ((float)stages * 0.1f);
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
                        float sw = freq / juce::jlimit(20.0f, 20000.0f, freqLimit * freqScale);
                        float sw2 = sw * sw;
                        float sd = 1.0f / stage_q;
                        float den = std::sqrt(std::pow(1.0f - sw2, 2.0f) + std::pow(sw * sd, 2.0f));
                        float m = 1.0f / den;
                        if (modelIdx == 20) { m = std::abs(1.0f - sw2 * 0.5f) / den; }
                        else { if (t == 1) m *= sw; else if (t == 2) m *= sw2; else if (t == 3) m *= std::abs(1.0f - sw2); }
                        mag_total *= m;
                    }
                    mag = mag_total;
                }
                else if (modelIdx == 21)
                {
                    float d = 1.0f / 0.707f;
                    float w_lpg = freq / freqLimit;
                    mag = 1.0f / std::sqrt(std::pow(1.0f - w_lpg * w_lpg, 2.0f) + std::pow(w_lpg * d, 2.0f));
                }
                else if (modelIdx == 22)
                {
                    float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);
                    float mag_sum = 0.0f;
                    for (int b = 0; b < 8; ++b) {
                        float bFreq = std::clamp(fc * std::pow((float)(b + 1), inharmonicity), 20.0f, 20000.0f);
                        float sw = freq / bFreq;
                        float q = 50.0f / std::sqrt((float)(b + 1));
                        float d = 1.0f / q;
                        mag_sum += (1.0f / std::sqrt(std::pow(1.0f - sw * sw, 2.0f) + std::pow(sw * d, 2.0f)))
                            * sw * (1.0f / std::sqrt((float)(b + 1)));
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
                    const float fA[7] = { 730,1090,2440,4000,6000,8000,10000 }; const float qA[7] = { 4,4,3,1,1,1,1 };
                    const float fB[7] = { 200,500,1200,2800,5000,8500,12000 }; const float qB[7] = { .5f,.5f,.5f,.5f,.5f,.5f,.5f };
                    const float fC[7] = { 300,870,2240,4000,6000,8000,10000 }; const float qC[7] = { 5,4,2,1,1,1,1 };
                    const float fD[7] = { 80,120,200,4000,8000,12000,16000 };  const float qD[7] = { 3,2,1,1,2,3,4 };
                    float mag_total = 1.0f;
                    for (int k = 0; k < 7; ++k) {
                        float sfc = fA[k] * (1 - x_zp) * (1 - y_zp) + fB[k] * x_zp * (1 - y_zp) + fC[k] * (1 - x_zp) * y_zp + fD[k] * x_zp * y_zp;
                        float sq = std::max(0.5f, qA[k] * (1 - x_zp) * (1 - y_zp) + qB[k] * x_zp * (1 - y_zp) + qC[k] * (1 - x_zp) * y_zp + qD[k] * x_zp * y_zp);
                        float sw = freq / juce::jlimit(20.0f, 20000.0f, sfc);
                        float den = std::sqrt(std::pow(1.0f - sw * sw, 2.0f) + std::pow(sw * (1.0f / sq), 2.0f));
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
            };
        rawMag[px] = calc("A", 0) * wA
            + calc("B", 1) * wB
            + calc("C", 2) * wC
            + calc("D", 3) * wD;
    }
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
