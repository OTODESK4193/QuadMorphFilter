// ==========================================
// UI/FilterVisualizer.cpp   （V1.1.0 全面改訂）
// ==========================================
#include "FilterVisualizer.h"
#include "ColorPalette.h"
#include "../PluginProcessor.h"
#include "../DSP/MorphEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
    // ---- dB 軸 ----
    // 共鳴ピークは +70dB を超えるモデルがあるが、線形にすると通過帯域が
    // 潰れて読めなくなる。0dB より上だけ平方根で圧縮し、
    // 「ピークの高さ順」は保ったまま全体を 1 画面に収める。
    constexpr float kDbMax = 72.0f;    // 表示上限
    constexpr float kDbMin = -96.0f;   // 表示下限
    constexpr float kZeroT = 0.30f;    // 0dB の縦位置（0=上端, 1=下端）

    constexpr float kFreqMin = 20.0f;
    constexpr float kFreqMax = 20000.0f;

    const char* const kSuffix[4] = { "A", "B", "C", "D" };
}

// ==========================================================================
FilterVisualizer::FilterVisualizer(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    startTimerHz(45);
}

FilterVisualizer::~FilterVisualizer()
{
    stopTimer();   // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）
}

// ==========================================================================
// 軸変換
// ==========================================================================
float FilterVisualizer::freqToX(float f, juce::Rectangle<float> r) noexcept
{
    const float t = std::log10(juce::jlimit(kFreqMin, kFreqMax, f) / kFreqMin) / 3.0f;
    return r.getX() + t * r.getWidth();
}

float FilterVisualizer::dbToY(float db, juce::Rectangle<float> r) noexcept
{
    db = juce::jlimit(kDbMin, kDbMax, db);

    float t;
    if (db >= 0.0f)
        t = kZeroT * (1.0f - std::sqrt(db / kDbMax));       // 上側は圧縮
    else
        t = kZeroT + (1.0f - kZeroT) * (-db / -kDbMin);     // 下側は線形

    return r.getY() + juce::jlimit(0.0f, 1.0f, t) * r.getHeight();
}

// ==========================================================================
// paint 冒頭のパラメータ読み取り
//   ピクセルループの中で APVTS を引くと 1 フレームあたり数千回の
//   文字列連結＋ハッシュ検索が走るため、ここで 1 回だけまとめて読む。
// ==========================================================================
void FilterVisualizer::readSnapshots()
{
    auto& apvts = processor.apvts;
    auto raw = [&apvts](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };

    // ---- LFO 2 / 3（Cutoff / Reso 変調）の 4 方向成分 ----
    const auto cPos = processor.getLfoPos(1);
    const auto rPos = processor.getLfoPos(2);
    const auto lfo2Mod4 = processor.getLfoMod4(1);
    const auto lfo3Mod4 = processor.getLfoMod4(2);

    const bool cutIsRand1 = ((int)raw("lfo2wave") == 3) && (raw("lfo2en") > 0.5f);
    const bool resIsRand1 = ((int)raw("lfo3wave") == 3) && (raw("lfo3en") > 0.5f);

    // DSP 側と同じ関数を使うので、表示と実音が必ず一致する
    const auto cM = MorphEngine::computeModulation(cPos, lfo2Mod4, cutIsRand1);
    const auto rM = MorphEngine::computeModulation(rPos, lfo3Mod4, resIsRand1);

    // ---- Solo 状態（DSP 側と同じく Enable より優先）----
    soloIdx = processor.soloFilter.load();
    if (soloIdx < 0 || soloIdx > 3) soloIdx = -1;

    // ---- 各フィルターの状態 ----
    int enabledCount = 0;
    for (int i = 0; i < 4; ++i)
    {
        const juce::String s = kSuffix[i];
        auto& sn = snap[(size_t)i];

        // Solo 中は該当フィルターのみ有効。Enable が off でも鳴らすため
        // ここでも強制的に true にして、表示と実音を一致させる。
        sn.enabled = (soloIdx >= 0) ? (i == soloIdx)
                                    : (raw("enable" + s) > 0.5f);
        if (sn.enabled) ++enabledCount;

        sn.model = juce::roundToInt(raw("model" + s));
        sn.slope = juce::roundToInt(raw("slope" + s));
        sn.type = juce::roundToInt(raw("type" + s));

        const int cutSrc = juce::jlimit(0, 4, juce::roundToInt(raw("lfoCutSrc" + s)));
        const int resSrc = juce::jlimit(0, 4, juce::roundToInt(raw("lfoResSrc" + s)));

        const float baseCutoff = raw("cutoff" + s);
        const float baseRes = raw("res" + s);

        sn.cutoff = (cutSrc > 0)
            ? baseCutoff * std::pow(2.0f, 4.0f * cM[(size_t)(cutSrc - 1)])
            : baseCutoff;
        sn.res = (resSrc > 0)
            ? baseRes * std::pow(2.0f, 2.0f * rM[(size_t)(resSrc - 1)])
            : baseRes;

        sn.cutoff = juce::jlimit(kFreqMin, kFreqMax, sn.cutoff);
        sn.res = juce::jlimit(0.1f, 10.0f, sn.res);
        sn.weight = 0.0f;
    }

    // ---- モーフの重み（processBlock と同一アルゴリズム）----
    const auto mPos = processor.getLfoPos(0);
    std::array<float, 4> wMix{};

    if (enabledCount <= 1)
    {
        for (int i = 0; i < 4; ++i)
            wMix[(size_t)i] = snap[(size_t)i].enabled ? 1.0f : 0.0f;
    }
    else
    {
        switch ((int)raw("morphBlend"))
        {
            case 1:  wMix = MorphEngine::computeLinearWMix(mPos.x, mPos.y);     break;
            case 2:  wMix = MorphEngine::computeSmoothstepWMix(mPos.x, mPos.y); break;
            case 3:  wMix = MorphEngine::computeRadialWMix(mPos.x, mPos.y);     break;
            default: wMix = MorphEngine::computeEqualPowerWMix(mPos.x, mPos.y); break;
        }

        float sumSq = 0.0f;
        for (int i = 0; i < 4; ++i)
            if (snap[(size_t)i].enabled) sumSq += wMix[(size_t)i] * wMix[(size_t)i];

        if (sumSq > 1.0e-8f)
        {
            const float norm = 1.0f / std::sqrt(sumSq);
            for (int i = 0; i < 4; ++i)
                if (snap[(size_t)i].enabled) wMix[(size_t)i] *= norm;
        }
    }

    for (int i = 0; i < 4; ++i)
        snap[(size_t)i].weight = snap[(size_t)i].enabled ? wMix[(size_t)i] : 0.0f;
}

// ==========================================================================
// 周波数応答（全 28 モデル）
//   V1.0.0 の実装をそのまま関数へ切り出したもの。数式は一切変更していない。
// ==========================================================================
float FilterVisualizer::magnitudeFor(float freq, const Snapshot& sn)
{
    if (!sn.enabled) return 0.0f;

    const int modelIdx = sn.model;
    const int slopeIdx = sn.slope;
    const int t = sn.type;
    const float fc = sn.cutoff;
    const float res = sn.res;

    const float freqLimit = fc;
    const float w_norm = freq / freqLimit;
    const float w2 = w_norm * w_norm;

    float mag = 1.0f;

    if (modelIdx == 0 || modelIdx == 3 || modelIdx == 4 || modelIdx == 9
        || modelIdx == 14 || modelIdx == 16)
    {
        const int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
        float adjustedRes = res;
        if (modelIdx != 14 && stages > 1)
            adjustedRes = res * std::pow(0.6f, std::log2((float)stages));
        const float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
        const float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
        float m = 1.0f / den;
        if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
        mag = std::pow(m, (float)stages);
        if (modelIdx == 0 || modelIdx == 4 || modelIdx == 16)
            mag *= (1.0f + res * 0.1f);
        if (modelIdx == 9)
            mag *= juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 1.0f, 5.0f);
    }
    else if (modelIdx == 23)
    {
        // Waveguide: コム状共鳴スペクトル
        const int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
        const float delaySamples = 44100.0f / juce::jlimit(20.0f, 20000.0f, fc);
        const float fb = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 0.99f);
        const float wD = 2.0f * juce::MathConstants<float>::pi * freq * (delaySamples / 44100.0f);
        const float comb_mag = 1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD));
        const float mm = (t == 0) ? comb_mag : (0.5f + 0.5f * comb_mag);
        mag = std::pow(mm, (float)stages);
    }
    else if (modelIdx == 1 || modelIdx == 12 || modelIdx == 13 || modelIdx == 15)
    {
        const float r_scale = (modelIdx == 13) ? 5.0f : 4.5f;
        const float r_moog = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, r_scale);

        if (slopeIdx == 0)
        {
            const float Q_eff = juce::jlimit(0.5f, 15.0f, 0.5f + r_moog * 2.5f);
            const float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm / Q_eff, 2.0f));
            mag = 1.0f / den;
            if (t == 1) mag *= w_norm;
            else if (t == 2) mag *= w2;
            else if (t == 3) mag *= std::abs(1.0f - w2);
            const float peak = (r_moog > 2.0f) ? (1.0f + (r_moog - 2.0f) * 0.8f) : 1.0f;
            mag *= peak;
        }
        else
        {
            const int cascade = (slopeIdx == 1) ? 1 : (slopeIdx == 2) ? 2 : 4;
            const float r_sc = r_moog * std::pow(0.7f, std::log2((float)cascade));
            const float real_p = std::pow(1.0f - w2, 2.0f) - 4.0f * w2 + r_sc;
            const float imag_p = 4.0f * w_norm * (1.0f - w2);
            const float denom = std::max(std::sqrt(real_p * real_p + imag_p * imag_p), 0.005f);
            mag = std::pow(1.0f / denom, (float)cascade);
            if (t == 1) mag *= w2;
            else if (t == 2) mag *= w2 * w2;
            else if (t == 3) mag *= std::abs(1.0f - w2 * w2);
            mag *= (1.0f + 0.5f * r_sc);
        }
        mag = std::min(mag, 1000.0f);
    }
    else if (modelIdx == 2)
    {
        // TB-303 Diode Ladder（Stinchcombe 4 極モデル + Accent による Φ シフト）
        static constexpr float accentPhi[4] = { 1.0f, 1.21f, 1.56f, 1.56f };
        const float phi = accentPhi[juce::jlimit(0, 3, slopeIdx)];
        const float fc_eff = juce::jlimit(20.0f, 20000.0f, fc * phi);

        static constexpr float kMaxTab[4] = { 3.5f, 3.7f, 3.85f, 3.85f };
        const float k_max = kMaxTab[juce::jlimit(0, 3, slopeIdx)];

        float k = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, k_max);
        if (k > 3.8f)
        {
            const float excess = k - 3.8f;
            k = 3.8f + excess / (1.0f + excess * 0.6f);
        }

        const float w_tb = freq / fc_eff;
        const float w2_tb = w_tb * w_tb;
        const float hpf_mag = (freq / 8.0f) / std::sqrt(1.0f + std::pow(freq / 8.0f, 2.0f));

        const float real_p = std::pow(1.0f - w2_tb, 2.0f) - 4.0f * w2_tb + k;
        const float imag_p = 4.0f * w_tb * (1.0f - w2_tb);
        const float denom = std::max(std::sqrt(real_p * real_p + imag_p * imag_p), 0.003f);
        const float g_loss = 1.0f / (1.0f + k * 0.07f);

        mag = (1.0f / denom) * hpf_mag * g_loss;
        mag = std::min(mag, 1000.0f);
    }
    else if (modelIdx == 5)
    {
        // Formant (Vowel)
        float v = juce::jmap(std::log10(freqLimit), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f);
        int idx_v = (int)v;
        float frac = v - (float)idx_v;
        if (idx_v >= 4) { idx_v = 3; frac = 1.0f; }
        if (idx_v < 0) { idx_v = 0; frac = 0.0f; }

        static constexpr float f1_m[5] = { 730.f, 270.f, 300.f, 530.f, 400.f };
        static constexpr float f2_m[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f };
        static constexpr float f3_m[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };

        const float f_arr[3] = {
            f1_m[idx_v] + (f1_m[idx_v + 1] - f1_m[idx_v]) * frac,
            f2_m[idx_v] + (f2_m[idx_v + 1] - f2_m[idx_v]) * frac,
            f3_m[idx_v] + (f3_m[idx_v + 1] - f3_m[idx_v]) * frac
        };

        const float formQ = 2.0f + juce::jlimit(0.1f, 10.0f, res) * 3.0f;
        const float d_f = 1.0f / formQ;
        static constexpr float gains[3] = { 1.0f, 0.5f, 0.2f };

        float mag_sum = 0.0f;
        for (int f = 0; f < 3; ++f)
        {
            const float w_f = freq / juce::jlimit(20.0f, 20000.0f, f_arr[f]);
            float m_f = 1.0f / std::sqrt(std::pow(1.0f - w_f * w_f, 2.0f) + std::pow(w_f * d_f, 2.0f));
            m_f *= w_f;
            mag_sum += m_f * gains[f];
        }
        mag = mag_sum;
    }
    else if (modelIdx == 6)
    {
        const int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
        const float delaySamples = 44100.0f / freqLimit;
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        if (t == 1 || t == 3) fb = -fb;
        const float wD = 2.0f * juce::MathConstants<float>::pi * freq * (delaySamples / 44100.0f);
        const float m = (t == 0 || t == 1)
            ? (1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD)))
            : std::sqrt(1.0f + fb * fb + 2.0f * fb * std::cos(wD));
        mag = std::pow(m, (float)stages);
    }
    else if (modelIdx == 7)
    {
        const int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
        const float ms_res = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 2.5f);
        const float d = 1.0f / (ms_res + 0.1f);
        const float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
        float m = 1.0f / den;
        if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
        mag = std::pow(m, (float)stages) * (1.0f + ms_res * 0.2f);
    }
    else if (modelIdx == 8)
    {
        const int stages = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
        const float phi = -2.0f * std::atan(freq / freqLimit);
        float fb = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 0.95f);
        if (t == 1 || t == 3) fb = -fb;
        const float c_ap = std::cos((float)stages * phi);
        const float s_ap = std::sin((float)stages * phi);
        const float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
        const float rya = (c_ap - fb) / den2;
        const float iya = s_ap / den2;
        const float ro = 0.5f * (1.0f + rya);
        const float io = 0.5f * iya;
        mag = std::sqrt(ro * ro + io * io);
    }
    else if (modelIdx == 10)
    {
        // FDN Reverb: SVF プリフィルター × FDN コム応答
        static constexpr float base_ms_tab[4] = { 20.0f, 60.0f, 120.0f, 30.0f };
        const float base_ms_vis = base_ms_tab[juce::jlimit(0, 3, slopeIdx)];

        const float d_svf = 1.0f / 1.2f;
        const float den_svf = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d_svf, 2.0f));

        float svf_mag;
        if (t == 0)      svf_mag = 1.0f / den_svf;
        else if (t == 1) svf_mag = (w_norm * d_svf) / den_svf;
        else if (t == 2) svf_mag = w2 / den_svf;
        else             svf_mag = 1.0f;   // Open: 全帯域通過

        const float fb_fdn = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 0.98f);
        const float baseD = base_ms_vis / 1000.0f;
        const float wD1 = 2.0f * juce::MathConstants<float>::pi * freq * baseD;
        const float wD2 = 2.0f * juce::MathConstants<float>::pi * freq * (baseD * 1.313f);
        const float fdn1 = 1.0f / std::sqrt(juce::jmax(0.001f,
            1.0f + fb_fdn * fb_fdn - 2.0f * fb_fdn * std::cos(wD1)));
        const float fdn2 = 1.0f / std::sqrt(juce::jmax(0.001f,
            1.0f + fb_fdn * fb_fdn - 2.0f * fb_fdn * std::cos(wD2)));
        const float fdn_mag = (fdn1 + fdn2) * 0.5f;

        mag = svf_mag * (0.6f + fdn_mag * 0.4f);
    }
    else if (modelIdx == 11)
    {
        // Phase Shift: 各段の中心周波数を DSP と同じスプレッドで並べる
        static constexpr float randOffsets[16] = {
             0.000f,  0.618f, -0.382f,  0.854f,
            -0.146f,  0.472f, -0.764f,  0.236f,
             0.944f, -0.528f,  0.090f, -0.910f,
             0.708f, -0.292f,  0.562f, -0.438f
        };
        const int stages = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
        const float spread = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 2.5f);
        const float q = 0.5f + (spread * 2.0f);
        const float d_ap = 1.0f / q;

        float mag_total = 1.0f;
        for (int k = 0; k < stages; ++k)
        {
            float st_fc = freqLimit;
            switch (t)
            {
                case 0:
                {
                    const float off = (stages > 1) ? ((float)k / (float)(stages - 1)) * 2.0f - 1.0f : 0.0f;
                    st_fc = freqLimit + off * freqLimit * spread * 0.5f;
                    break;
                }
                case 1:
                {
                    const float off = (stages > 1) ? ((float)k / (float)(stages - 1)) * 2.0f - 1.0f : 0.0f;
                    st_fc = freqLimit * std::pow(2.0f, off * spread);
                    break;
                }
                case 2:
                {
                    const float sign = (k % 2 == 0) ? 1.0f : -1.0f;
                    const float magK = (stages > 1)
                        ? (float)((k / 2) + 1) / (float)((stages + 1) / 2) : 0.0f;
                    st_fc = freqLimit * std::pow(2.0f, sign * magK * spread);
                    break;
                }
                default:
                    st_fc = freqLimit * std::pow(2.0f, randOffsets[k % 16] * spread);
                    break;
            }
            st_fc = juce::jlimit(20.0f, 20000.0f, st_fc);
            const float sw = freq / st_fc;
            const float sw2_k = sw * sw;
            const float den_k = std::sqrt(std::pow(1.0f - sw2_k, 2.0f) + std::pow(sw * d_ap, 2.0f));
            const float bp_k = (1.0f / den_k) * sw;
            mag_total *= (1.0f + std::pow(bp_k, 1.2f) * res * 0.1f);
        }
        mag = mag_total;
    }
    else if (modelIdx >= 17 && modelIdx <= 20)
    {
        const int order = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
        const int sections = order / 2;
        const float rippleDb = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.1f, 3.0f);
        const float eps = std::sqrt(std::pow(10.0f, rippleDb / 10.0f) - 1.0f);

        const float xi_ell = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.3f, 0.9f);
        const float fz_ell = std::min(freqLimit / xi_ell, 20000.0f);

        float mag_total = 1.0f;
        for (int k = 0; k < sections; ++k)
        {
            const float theta = juce::MathConstants<float>::pi * (2.0f * (float)k + 1.0f) / (2.0f * (float)order);
            float stage_q = 0.707f;
            float freqScale = 1.0f;

            if (modelIdx == 17)
            {
                const float qButter = 1.0f / (2.0f * std::sin(theta));
                const float qBoost = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 1.0f, 3.0f);
                stage_q = qButter * qBoost;
            }
            else if (modelIdx == 18)
            {
                const float a = (1.0f / (float)order) * std::asinh(1.0f / eps);
                const float rp = -std::sinh(a) * std::sin(theta);
                const float ip = std::cosh(a) * std::cos(theta);
                const float wn2 = rp * rp + ip * ip;
                freqScale = std::sqrt(wn2);
                stage_q = std::sqrt(wn2) / (-2.0f * rp);
            }
            else if (modelIdx == 19)
            {
                const float besselFactor = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.577f, 1.0f);
                stage_q = 1.0f / (2.0f * std::sin(theta)) * besselFactor;
                const float besselNorm = (besselFactor - 0.577f) / (1.0f - 0.577f);
                freqScale = 1.0f + (float)order * 0.1f * (1.0f - besselNorm);
            }
            else if (modelIdx == 20)
            {
                const float a = (1.0f / (float)order) * std::asinh(1.0f / (eps * 0.5f));
                const float rp = -std::sinh(a) * std::sin(theta);
                const float ip = std::cosh(a) * std::cos(theta);
                const float wn2 = rp * rp + ip * ip;
                freqScale = std::sqrt(wn2);
                stage_q = std::sqrt(wn2) / (-2.0f * rp) * 1.2f;
            }

            const float fp_k = juce::jlimit(20.0f, 20000.0f, freqLimit * freqScale);
            const float sw = freq / fp_k;
            const float sw2 = sw * sw;
            const float sd = 1.0f / stage_q;
            const float den = std::sqrt(std::pow(1.0f - sw2, 2.0f) + std::pow(sw * sd, 2.0f));
            float m = 1.0f / den;

            if (modelIdx == 20)
            {
                const float alpha = fp_k / juce::jlimit(fp_k + 1.0f, 20000.0f, fz_ell);
                const float a2 = alpha * alpha;
                if (t == 0)      m = std::abs(1.0f - a2 * sw2) / den;
                else if (t == 1) m = sw / den;
                else if (t == 2) m = std::abs(sw2 - a2) / den;
                else             m = std::abs(1.0f - sw2) / den;
            }
            else
            {
                if (t == 1) m *= sw;
                else if (t == 2) m *= sw2;
                else if (t == 3) m *= std::abs(1.0f - sw2);
            }
            mag_total *= m;
        }
        mag = mag_total;
    }
    else if (modelIdx == 21)
    {
        const float d = 1.0f / 0.707f;
        const float w_lpg = freq / freqLimit;
        mag = 1.0f / std::sqrt(std::pow(1.0f - w_lpg * w_lpg, 2.0f) + std::pow(w_lpg * d, 2.0f));
    }
    else if (modelIdx == 22)
    {
        // Modal Resonator
        const float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);
        static constexpr float qBaseVis[3] = { 5.0f, 15.0f, 50.0f };
        const float qBase = qBaseVis[juce::jlimit(0, 2, slopeIdx)];

        float mag_sum = 0.0f;
        for (int b = 0; b < 8; ++b)
        {
            const float bFreq = std::clamp(fc * std::pow((float)(b + 1), inharmonicity), 20.0f, 20000.0f);
            const float sw = freq / bFreq;
            const float q = qBase / std::sqrt((float)(b + 1));
            const float d = 1.0f / q;
            mag_sum += (1.0f / std::sqrt(std::pow(1.0f - sw * sw, 2.0f) + std::pow(sw * d, 2.0f)))
                * sw * (1.0f / std::sqrt((float)(b + 1)));
        }
        mag = mag_sum;
    }
    else if (modelIdx == 24)
    {
        // Bode Freq Shifter（静的表示なので 1kHz のシフト後位置を示す）
        const float logMin_bode = std::log10(20.0f);
        const float logMax_bode = std::log10(20000.0f);
        const float logCenter_bode = (logMin_bode + logMax_bode) * 0.5f;
        const float logFc_bode = std::log10(juce::jlimit(20.0f, 20000.0f, fc));
        const float norm_bode = (logFc_bode < logCenter_bode)
            ? (logFc_bode - logCenter_bode) / (logCenter_bode - logMin_bode)
            : (logFc_bode - logCenter_bode) / (logMax_bode - logCenter_bode);
        const float shiftHz = norm_bode * 1000.0f;
        mag = (std::abs(freq - (1000.0f + shiftHz)) < 80.0f) ? 6.0f : 1.0f;
    }
    else if (modelIdx == 25)
    {
        // Z-Plane 2D Morph
        const float x_zp = juce::jlimit(0.0f, 1.0f,
            juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 1.0f));
        const float y_zp = juce::jlimit(0.0f, 1.0f, juce::jmap(res, 0.1f, 10.0f, 0.0f, 1.0f));

        static constexpr float fA[7] = { 730, 1090, 2440, 4000, 6000,  8000, 10000 };
        static constexpr float qA[7] = { 4, 4, 3, 1, 1, 1, 1 };
        static constexpr float fB[7] = { 200,  500, 1200, 2800, 5000,  8500, 12000 };
        static constexpr float qB[7] = { .5f,.5f,.5f,.5f,.5f,.5f,.5f };
        static constexpr float fC[7] = { 300,  870, 2240, 4000, 6000,  8000, 10000 };
        static constexpr float qC[7] = { 5, 4, 2, 1, 1, 1, 1 };
        static constexpr float fD[7] = { 80,  120,  200, 4000, 8000, 12000, 16000 };
        static constexpr float qD[7] = { 3, 2, 1, 1, 2, 3, 4 };

        float mag_accum = (t == 0 || t == 2) ? 1.0f : 0.0f;
        for (int k = 0; k < 7; ++k)
        {
            const float sfc = fA[k] * (1 - x_zp) * (1 - y_zp) + fB[k] * x_zp * (1 - y_zp)
                + fC[k] * (1 - x_zp) * y_zp + fD[k] * x_zp * y_zp;
            const float sq = std::max(0.5f,
                qA[k] * (1 - x_zp) * (1 - y_zp) + qB[k] * x_zp * (1 - y_zp)
                + qC[k] * (1 - x_zp) * y_zp + qD[k] * x_zp * y_zp);
            const float sw = freq / juce::jlimit(20.0f, 20000.0f, sfc);
            const float sw2 = sw * sw;
            const float d = 1.0f / sq;
            const float den = std::sqrt(std::pow(1.0f - sw2, 2.0f) + std::pow(sw * d, 2.0f));

            if (t == 0)      mag_accum *= (1.0f / den);
            else if (t == 1) mag_accum += (sw * d) / den;
            else if (t == 2) mag_accum *= (sw2 / den);
            else             mag_accum += std::abs(1.0f - sw2) / den;
        }
        if (t == 1 || t == 3) mag_accum /= 7.0f;
        mag = mag_accum;
    }
    else if (modelIdx == 26)
    {
        const int stages_pa = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 8 : 16;
        const float phi_pa = -2.0f * std::atan(w_norm);
        const float totalPhase = (float)stages_pa * phi_pa;
        const float depth = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 0.8f);
        if (t == 0) mag = (1.0f + depth) * std::abs(std::cos(totalPhase * 0.5f));
        else        mag = 2.0f * std::abs(std::sin(totalPhase * 0.5f)) * (1.0f + depth);
    }
    else if (modelIdx == 27)
    {
        const float aa_Q = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.5f, 3.0f);
        const float aa_d = 1.0f / aa_Q;
        const int aa_stages = (slopeIdx == 0) ? 2 : (slopeIdx == 1) ? 4 : (slopeIdx == 2) ? 6 : 8;
        const float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * aa_d, 2.0f));
        mag = std::pow(1.0f / den, (float)aa_stages);
    }

    // 全モデル共通の上限クランプ（+80dB 相当）
    return std::min(mag, 10000.0f);
}

// ==========================================================================
// グリッド
// ==========================================================================
void FilterVisualizer::drawGrid(juce::Graphics& g, juce::Rectangle<float> r) const
{
    // ---- 周波数（縦線）----
    static constexpr float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                                       2000.0f, 5000.0f, 10000.0f };
    static const char* const freqLabels[] = { "50", "100", "200", "500", "1k",
                                              "2k", "5k", "10k" };

    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));

    for (int i = 0; i < 8; ++i)
    {
        const float x = freqToX(freqs[i], r);
        g.setColour(QMColors::grid);
        g.drawVerticalLine((int)x, r.getY(), r.getBottom());

        g.setColour(QMColors::textDim.withAlpha(0.55f));
        g.drawText(freqLabels[i], (int)x + 3, (int)r.getBottom() - 12, 34, 11,
                   juce::Justification::left, false);
    }

    // ---- dB（横線）----
    static constexpr float dbLines[] = { 48.0f, 24.0f, 12.0f, -12.0f, -24.0f,
                                         -48.0f, -72.0f };
    for (float db : dbLines)
    {
        const float y = dbToY(db, r);
        g.setColour(QMColors::grid);
        g.drawHorizontalLine((int)y, r.getX(), r.getRight());
        g.setColour(QMColors::textDim.withAlpha(0.45f));
        g.drawText(juce::String((int)db), (int)r.getX() + 3, (int)y - 11, 34, 10,
                   juce::Justification::left, false);
    }

    // ---- 0dB は少し強調 ----
    const float y0 = dbToY(0.0f, r);
    g.setColour(QMColors::text.withAlpha(0.20f));
    g.drawHorizontalLine((int)y0, r.getX(), r.getRight());
    g.setColour(QMColors::textDim.withAlpha(0.85f));
    g.drawText("0dB", (int)r.getX() + 3, (int)y0 - 11, 34, 10, juce::Justification::left, false);
}

// ==========================================================================
// 凡例（有効なフィルターと、いまのモーフ比率）
// ==========================================================================
void FilterVisualizer::drawLegend(juce::Graphics& g, juce::Rectangle<float> r) const
{
    int shown = 0;
    for (const auto& s : snap) if (s.enabled) ++shown;
    if (shown == 0) return;

    const float itemW = 80.0f;
    float x = r.getRight() - itemW * (float)shown;
    const float y = r.getY() + 1.0f;

    // 見出し
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.setColour(QMColors::textDim.withAlpha(0.7f));
    g.drawText("MORPH MIX", (int)x - 74, (int)y, 68, 12,
               juce::Justification::centredRight, false);

    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));

    for (int i = 0; i < 4; ++i)
    {
        const auto& s = snap[(size_t)i];
        if (!s.enabled) continue;

        const auto col = QMColors::filterColour(i);
        const float wgt = juce::jlimit(0.0f, 1.0f, s.weight);

        // 色チップ + 名前
        g.setColour(col.withAlpha(0.35f + 0.65f * wgt));
        g.fillRoundedRectangle(x, y + 2.0f, 3.0f, 8.0f, 1.5f);
        g.drawText(juce::String(kSuffix[i]), (int)x + 6, (int)y, 10, 12,
                   juce::Justification::left, false);

        // 比率バー
        const float bx = x + 18.0f;
        const float bw = 34.0f;
        g.setColour(QMColors::text.withAlpha(0.09f));
        g.fillRoundedRectangle(bx, y + 4.0f, bw, 4.0f, 2.0f);
        g.setColour(col.withAlpha(0.9f));
        g.fillRoundedRectangle(bx, y + 4.0f, juce::jmax(1.5f, bw * wgt), 4.0f, 2.0f);

        // 比率の数値
        g.setColour(QMColors::textDim.withAlpha(0.55f + 0.45f * wgt));
        g.drawText(juce::String(juce::roundToInt(wgt * 100.0f)) + "%",
                   (int)bx + (int)bw + 3, (int)y, 24, 12,
                   juce::Justification::left, false);

        x += itemW;
    }
}

// ==========================================================================
// paint
// ==========================================================================
void FilterVisualizer::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 60.0f || bounds.getHeight() < 60.0f) return;

    // ---- 背景カード ----
    g.setColour(QMColors::well);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(QMColors::panelLine.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // ---- プロット領域 ----
    auto plot = bounds.reduced(12.0f, 12.0f);
    plot.removeFromTop(12.0f);       // 凡例のぶん
    plot.removeFromBottom(4.0f);

    drawGrid(g, plot);

    // ---- 応答の計算 ----
    readSnapshots();

    const int numPts = juce::jlimit(64, kMaxPoints, (int)plot.getWidth() + 1);
    const float lastIdx = (float)(numPts - 1);

    for (int px = 0; px < numPts; ++px)
    {
        const float tt = (float)px / lastIdx;
        const float freq = kFreqMin * std::pow(1000.0f, tt);

        float sum = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float m = magnitudeFor(freq, snap[(size_t)i]);
            mag[(size_t)i][(size_t)px] = m;
            sum += m * snap[(size_t)i].weight;
        }
        magSum[(size_t)px] = sum;
    }

    // ---- 移動平均で滑らかにする ----
    constexpr int kSmoothR = 6;
    auto smoothInto = [this, numPts](std::array<float, kMaxPoints>& buf)
    {
        for (int px = 0; px < numPts; ++px)
        {
            float acc = 0.0f;
            int cnt = 0;
            const int lo = juce::jmax(0, px - kSmoothR);
            const int hi = juce::jmin(numPts - 1, px + kSmoothR);
            for (int k = lo; k <= hi; ++k) { acc += buf[(size_t)k]; ++cnt; }
            smoothBuf[(size_t)px] = (cnt > 0) ? acc / (float)cnt : 0.0f;
        }
        for (int px = 0; px < numPts; ++px)
            buf[(size_t)px] = smoothBuf[(size_t)px];
    };

    for (int i = 0; i < 4; ++i)
        if (snap[(size_t)i].enabled) smoothInto(mag[(size_t)i]);
    smoothInto(magSum);

    // ---- パスを構築（clear() は確保済みメモリを保持する）----
    auto buildPath = [&](juce::Path& path, const std::array<float, kMaxPoints>& buf)
    {
        path.clear();
        path.preallocateSpace(numPts * 3 + 8);
        for (int px = 0; px < numPts; ++px)
        {
            const float db = 20.0f * std::log10(buf[(size_t)px] + 1.0e-5f);
            const float x = plot.getX() + (float)px / lastIdx * plot.getWidth();
            const float y = dbToY(db, plot);
            if (px == 0) path.startNewSubPath(x, y);
            else         path.lineTo(x, y);
        }
    };

    const bool anyEnabled = std::any_of(snap.begin(), snap.end(),
                                        [](const Snapshot& s) { return s.enabled; });

    if (!anyEnabled)
    {
        g.setColour(QMColors::textDim.withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("ALL FILTERS BYPASSED", plot, juce::Justification::centred, false);
        drawLegend(g, bounds.reduced(12.0f, 8.0f));
        return;
    }

    buildPath(sumPath, magSum);

    // ======================================================================
    // 1) 合成カーブの下をグラデーションで塗る
    //    塗りの色は「いま鳴っているフィルターの色をモーフ比率で混ぜた色」。
    //    どのフィルターが主役かが、カーブを読まなくても色で分かる。
    // ======================================================================
    {
        fillPath.clear();
        fillPath.preallocateSpace(numPts * 3 + 16);
        fillPath.addPath(sumPath);
        fillPath.lineTo(plot.getRight(), plot.getBottom());
        fillPath.lineTo(plot.getX(), plot.getBottom());
        fillPath.closeSubPath();

        g.saveState();
        g.reduceClipRegion(plot.getSmallestIntegerContainer());

        if (soloIdx >= 0)
        {
            // ---- Solo 表示 ----
            // 対象フィルター固有の色で、通常より濃く・階調を増やして塗る。
            // 上端を明るく、下へ向けて 3 段階で抜くことで
            // カーブの「面」がはっきり浮かび上がる。
            const auto col = QMColors::filterColour(soloIdx);

            juce::ColourGradient grad(col.withAlpha(0.62f), plot.getX(), plot.getY(),
                                      col.withAlpha(0.0f), plot.getX(), plot.getBottom(), false);
            grad.addColour(0.28, col.withAlpha(0.34f));
            grad.addColour(0.62, col.withAlpha(0.14f));

            g.setGradientFill(grad);
            g.fillPath(fillPath);

            // カーブの稜線を同色で強調して、塗りと線を一体に見せる
            g.setColour(col.withAlpha(0.95f));
            g.strokePath(sumPath, juce::PathStrokeType(2.6f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            const auto blend = blendedMorphColour();

            juce::ColourGradient grad(blend.withAlpha(0.30f), plot.getX(), plot.getY(),
                                      blend.withAlpha(0.0f), plot.getX(), plot.getBottom(), false);
            grad.addColour(0.6, blend.withAlpha(0.07f));

            g.setGradientFill(grad);
            g.fillPath(fillPath);
        }

        g.restoreState();
    }

    // ======================================================================
    // 2) 個別カーブ
    //    地図の道路と同じ「ケーシング」方式：まず背景色で少し太く描き、
    //    その上に本体色を重ねる。線が交差しても手前／奥がはっきり分かれる。
    //    モーフ比率の小さい順に描くので、主役のカーブが必ず一番上に来る。
    // ======================================================================
    {
        std::array<int, 4> order{ 0, 1, 2, 3 };
        std::sort(order.begin(), order.end(), [this](int a, int b)
        {
            return snap[(size_t)a].weight < snap[(size_t)b].weight;
        });

        for (int k = 0; k < 4; ++k)
        {
            const int i = order[(size_t)k];
            const auto& s = snap[(size_t)i];
            if (!s.enabled) continue;

            buildPath(curvePath[(size_t)i], mag[(size_t)i]);

            const auto col = QMColors::filterColour(i);
            const float w = juce::jlimit(0.0f, 1.0f, s.weight);

            // ケーシング（背景色で縁取り）
            g.setColour(QMColors::well.withAlpha(0.70f));
            g.strokePath(curvePath[(size_t)i], juce::PathStrokeType(3.4f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // 本体（比率が高いほど濃く・太く）
            g.setColour(col.withAlpha(0.34f + 0.61f * w));
            g.strokePath(curvePath[(size_t)i], juce::PathStrokeType(1.3f + 1.0f * w,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // ======================================================================
    // 3) 合成カーブ（主役）
    //    こちらもケーシングを敷いてから明るい線を重ねる。
    // ======================================================================
    // Solo 中は主線もそのフィルターの色にする。白のままだと
    // せっかく色で塗った面の上を白線が横切って印象が濁るため。
    const juce::Colour sumLine = (soloIdx >= 0)
        ? QMColors::filterColour(soloIdx).brighter(0.35f)
        : QMColors::text;

    g.setColour(QMColors::well.withAlpha(0.85f));
    g.strokePath(sumPath, juce::PathStrokeType(6.0f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(sumLine.withAlpha(0.20f));
    g.strokePath(sumPath, juce::PathStrokeType(5.0f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(sumLine);
    g.strokePath(sumPath, juce::PathStrokeType(2.2f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ======================================================================
    // 4) カットオフ位置マーカー
    //    縦線は薄く、下端の三角＋文字だけをはっきり描く。
    //    LFO が掛かっていると三角がそのまま動くので、変調量が一目で分かる。
    // ======================================================================
    drawCutoffMarkers(g, plot);

    drawLegend(g, bounds.reduced(12.0f, 8.0f));
}

// ==========================================================================
// モーフ比率で混ぜたフィルター色（塗りに使う）
// ==========================================================================
juce::Colour FilterVisualizer::blendedMorphColour() const
{
    float r = 0.0f, gr = 0.0f, b = 0.0f, wsum = 0.0f;

    for (int i = 0; i < 4; ++i)
    {
        const auto& s = snap[(size_t)i];
        if (!s.enabled) continue;

        const float w = juce::jmax(0.02f, juce::jlimit(0.0f, 1.0f, s.weight));
        const auto c = QMColors::filterColour(i);
        r += c.getFloatRed() * w;
        gr += c.getFloatGreen() * w;
        b += c.getFloatBlue() * w;
        wsum += w;
    }

    if (wsum < 1.0e-4f) return QMColors::accentMorph;

    return juce::Colour::fromFloatRGBA(juce::jlimit(0.0f, 1.0f, r / wsum),
                                       juce::jlimit(0.0f, 1.0f, gr / wsum),
                                       juce::jlimit(0.0f, 1.0f, b / wsum), 1.0f);
}

// ==========================================================================
// カットオフ位置マーカー
// ==========================================================================
void FilterVisualizer::drawCutoffMarkers(juce::Graphics& g, juce::Rectangle<float> r) const
{
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));

    for (int i = 0; i < 4; ++i)
    {
        const auto& s = snap[(size_t)i];
        if (!s.enabled) continue;

        const auto col = QMColors::filterColour(i);
        const float w = juce::jlimit(0.0f, 1.0f, s.weight);
        const float cx = juce::jlimit(r.getX() + 6.0f, r.getRight() - 6.0f, freqToX(s.cutoff, r));

        // 縦のガイド（かなり薄く）
        g.setColour(col.withAlpha(0.05f + 0.13f * w));
        g.drawVerticalLine((int)cx, r.getY() + 5.0f, r.getBottom());

        // 上端の三角マーカー（周波数目盛りと重ならないよう上に置く）
        juce::Path tri;
        tri.addTriangle(cx - 4.5f, r.getY(), cx + 4.5f, r.getY(), cx, r.getY() + 6.0f);
        g.setColour(QMColors::well);
        g.fillPath(tri);
        g.setColour(col.withAlpha(0.45f + 0.55f * w));
        g.fillPath(tri);

        // フィルター名（主役級のときだけ。常時出すと上端がうるさくなる）
        if (w > 0.18f)
        {
            g.setColour(col.withAlpha(0.35f + 0.65f * w));
            g.drawText(kSuffix[i], (int)cx - 8, (int)r.getY() + 7, 16, 11,
                       juce::Justification::centred, false);
        }
    }
}
