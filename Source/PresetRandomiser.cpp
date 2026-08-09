// ==========================================
// PresetRandomiser.cpp
// ==========================================
#include "PresetRandomiser.h"
#include "PluginProcessor.h"
#include "FactoryPresets.h"
#include "DSP/ModelCapabilities.h"

#include <array>

namespace PresetRandomiser
{

namespace
{
    // ---- キャラクター（相性のよい 4 本組）----
    // ここから 1 つ選び、その中でだけモデルを振る。
    struct Character
    {
        const char* name;
        std::array<int, 6> pool;   // このキャラクターで使うモデル番号
        float cutLow;              // 最低フィルターのカットオフ下限 [Hz]
        float cutHigh;             // 最高フィルターのカットオフ上限 [Hz]
        float resMax;              // レゾナンス上限
    };

    const Character kCharacters[] =
    {
        // Moog / CEM3320 / SSM2040 / Jupiter / SEM / CS-80
        { "Analog",     {  1, 12, 13, 15,  3, 14 },  90.0f,  6000.0f,  6.5f },
        // TB-303 / MS-20 / Wasp / Moog / SSM / CEM
        { "Aggressive", {  2,  7, 16,  1, 13, 12 }, 120.0f,  4000.0f,  9.5f },
        // Comb / Modal / Waveguide / Bode / Vowel / Z-Plane
        { "Resonant",   {  6, 22, 23, 24,  5, 25 },  80.0f,  2500.0f,  8.0f },
        // Butterworth / Chebyshev / Bessel / Elliptic / Nyquist / Clean
        { "Clean",      { 17, 18, 19, 20, 27,  0 }, 150.0f, 12000.0f,  4.0f },
        // Bitcrush / Nyquist / Wavefolder / Phaser / Phase Shift / Clean
        { "Digital",    {  4, 27,  9,  8, 11,  0 }, 200.0f,  8000.0f,  5.5f },
        // FDN / Waveguide / Modal / Vactrol / SEM / Clean
        { "Spatial",    { 10, 23, 22, 21,  3,  0 }, 100.0f,  3500.0f,  7.0f },
    };

    constexpr int kNumCharacters = (int)(sizeof(kCharacters) / sizeof(kCharacters[0]));

    // ---- 実用的な同期刻み ----
    //   0="8/1"(32拍) 1="4/1"(16拍) 2="2/1"(8拍) 3="1/1"(1小節)
    //   4="1/2"(2拍)  5="1/4"(1拍)
    // フィルターのモーフは 1〜8 小節かけて動くくらいが心地よい。
    // 8分・16分は忙しすぎて「常に揺れている」だけの音になりやすいので外す。
    const int kUsefulSyncRates[] = { 0, 1, 2, 3, 4, 5 };
    constexpr int kNumSyncRates = (int)(sizeof(kUsefulSyncRates) / sizeof(kUsefulSyncRates[0]));

    // ---- 落ち着いた 2D 波形（Morph 用）----
    // Noise（5）のように暴れるものは Morph には使わない。
    const int kMorphWaves[] = { 0, 1, 3, 7, 8, 10, 11, 14, 16 };
    constexpr int kNumMorphWaves = (int)(sizeof(kMorphWaves) / sizeof(kMorphWaves[0]));

    // ---- 変調用のおとなしい波形 ----
    const int kModWaves[] = { 0, 1, 2, 3, 7 };
    constexpr int kNumModWaves = (int)(sizeof(kModWaves) / sizeof(kModWaves[0]));

    // ---- 自己発振しない Res の上限 ----
    // DSP の帰還量マッピングから逆算した値（FactoryPresets 生成器と同じ根拠）。
    //   ラダー系 (1,12,15): ladderRes = jmap(res, 0.1..10, 0..4.5)
    //     4 極ラダーは帰還 4 で発振 → res ≈ 8.9。余裕を見て 7.5。
    //   SSM2040 (13): r_scale = 5.0 → 閾値 ≈ 8.0 → 6.8
    //   TB-303 (2) / MS-20 (7) / Comb (6) / Waveguide (23) / FDN (10) も同様に
    //     フィードバックが 1 に近づくと鳴り続けるため上限を設ける。
    //   ここに無いモデルは構造上発振しない（SVF は Q が上がるだけ）。
    float safeRes(int model)
    {
        switch (model)
        {
        case 1: case 12: case 15: return 7.5f;   // Moog / CEM3320 / Jupiter
        case 13:                  return 6.8f;   // SSM2040
        case 2:                   return 7.0f;   // TB-303
        case 7:                   return 7.0f;   // MS-20
        case 6:                   return 7.5f;   // Comb
        case 23:                  return 6.5f;   // Waveguide
        case 10:                  return 8.0f;   // FDN Reverb (decay)
        case 16:                  return 8.0f;   // EDP Wasp
        default:                  return 10.0f;  // 上限なし
        }
    }

    // ------------------------------------------------------------------
    void setParam(QuadMorphFilterAudioProcessor& proc, const juce::String& id, float value)
    {
        if (auto* p = proc.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
        else
            jassertfalse;   // ID の書き間違い検出
    }

    float randRange(juce::Random& r, float lo, float hi)
    {
        return lo + (hi - lo) * r.nextFloat();
    }

    int pick(juce::Random& r, const int* arr, int n) { return arr[r.nextInt(n)]; }
}

// ==========================================================================
void randomise(QuadMorphFilterAudioProcessor& proc, juce::Random& rng)
{
    // 既定値から始めて、前の設定を引きずらないようにする
    FactoryPresets::applyInit(proc);

    const auto& ch = kCharacters[rng.nextInt(kNumCharacters)];

    // ---- 有効にするフィルター本数（2〜4。1 本だとモーフの意味がない）----
    const int numActive = 2 + rng.nextInt(3);

    // 本数が多いほどレゾナンスを抑える。4 本とも高 Q だと合成後に破綻する。
    const float resCeiling = ch.resMax * (numActive >= 4 ? 0.62f
                                        : numActive == 3 ? 0.78f : 1.0f);

    // ---- 4 本のカットオフを低→高に分担させる ----
    // 対数軸で等分し、各点をランダムに少しずらす。
    const float logLo = std::log(ch.cutLow);
    const float logHi = std::log(ch.cutHigh);

    // 自己発振ガードのために、選んだ内容を控えておく
    int   chosenModel[4] = { 0, 0, 0, 0 };
    float chosenRes[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool  resModulated[4] = { false, false, false, false };
    bool  slotActive[4]  = { false, false, false, false };

    for (int k = 0; k < 4; ++k)
    {
        const juce::String s = juce::String::charToString((juce::juce_wchar)('A' + k));
        const bool on = (k < numActive);

        setParam(proc, "enable" + s, on ? 1.0f : 0.0f);

        const int model = ch.pool[(size_t)rng.nextInt((int)ch.pool.size())];
        setParam(proc, "model" + s, (float)model);

        // モデルが対応している type / slope の中から選ぶ
        auto [maxSlope, hasLP, hasBP, hasHP, hasNotch] = getModelCaps(model);

        int types[4]; int nTypes = 0;
        if (hasLP)    types[nTypes++] = 0;
        if (hasBP)    types[nTypes++] = 1;
        if (hasHP)    types[nTypes++] = 2;
        if (hasNotch) types[nTypes++] = 3;

        // LP を厚めに引く（LP 主体のほうが音楽的に扱いやすい）
        const int type = (nTypes > 0)
            ? ((hasLP && rng.nextFloat() < 0.55f) ? 0 : types[rng.nextInt(nTypes)])
            : 0;
        setParam(proc, "type" + s, (float)type);
        setParam(proc, "slope" + s, (float)rng.nextInt(maxSlope + 1));

        // 帯域の分担。k 番目のスロットを対数軸の k/4 付近に置く。
        const float t = (numActive > 1) ? ((float)k / 3.0f) : 0.5f;
        const float jitter = randRange(rng, -0.10f, 0.10f);
        const float cutoff = std::exp(logLo + (logHi - logLo) * juce::jlimit(0.0f, 1.0f, t + jitter));
        setParam(proc, "cutoff" + s, juce::jlimit(20.0f, 20000.0f, cutoff));

        // 高い帯域ほどレゾナンスを控えめにする（耳に痛くなりやすいため）。
        // さらにモデル固有の発振閾値でクランプする。
        const float resScale = 1.0f - 0.35f * t;
        const float res = juce::jlimit(0.1f, safeRes(model) * 0.90f,
                                       randRange(rng, 0.7f, resCeiling) * resScale);
        setParam(proc, "res" + s, res);

        // LFO 割り当ては 4 本すべてに配らず、半分程度に留める
        const bool cutMod = on && rng.nextFloat() < 0.5f;
        const bool resMod = on && rng.nextFloat() < 0.25f;
        setParam(proc, "lfoCutSrc" + s, cutMod ? (float)(1 + rng.nextInt(4)) : 0.0f);
        setParam(proc, "lfoResSrc" + s, resMod ? (float)(1 + rng.nextInt(4)) : 0.0f);

        chosenModel[k]  = model;
        chosenRes[k]    = res;
        resModulated[k] = resMod;
        slotActive[k]   = on;
    }

    // ---- MORPH ----
    // 中央付近に寄せる。隅に置くと 1 本しか鳴らずモーフの意味が薄れる。
    setParam(proc, "posX", randRange(rng, 0.25f, 0.75f));
    setParam(proc, "posY", randRange(rng, 0.25f, 0.75f));
    setParam(proc, "morphBlend", (float)rng.nextInt(4));
    setParam(proc, "cutoffAlgo", (float)rng.nextInt(3));

    // XY Depth は 3 回に 1 回だけ、しかも控えめに掛ける
    setParam(proc, "xyDepth", rng.nextFloat() < 0.33f ? randRange(rng, 10.0f, 40.0f) : 0.0f);

    // ---- LFO1 (Morph) : 3/4 の確率で有効 ----
    {
        const bool on = rng.nextFloat() < 0.75f;
        setParam(proc, "lfo1en", on ? 1.0f : 0.0f);
        setParam(proc, "lfo1wave", (float)pick(rng, kMorphWaves, kNumMorphWaves));
        setParam(proc, "lfo1sync", 1.0f);
        setParam(proc, "lfo1rateSync", (float)pick(rng, kUsefulSyncRates, kNumSyncRates));
        setParam(proc, "lfo1min", randRange(rng, 0.0f, 0.25f));
        setParam(proc, "lfo1max", randRange(rng, 0.75f, 1.0f));
        setParam(proc, "lfo1phase", (float)(rng.nextInt(4) * 90));
        // Spread は 1/3 の確率。掛かると 4 本が順に立ち上がって回る感じになる。
        setParam(proc, "lfo1spread", rng.nextFloat() < 0.33f ? randRange(rng, 45.0f, 270.0f) : 0.0f);
        setParam(proc, "lfo1step", rng.nextFloat() < 0.2f ? 1.0f : 0.0f);
    }

    // ---- LFO2 (Cutoff) / LFO3 (Reso) ----
    // 割り当て済みのフィルターがあるときだけ有効にする（表示と挙動を一致させる）
    auto anyAssigned = [&proc](const juce::String& prefix)
    {
        for (int k = 0; k < 4; ++k)
        {
            const juce::String s = juce::String::charToString((juce::juce_wchar)('A' + k));
            if (proc.apvts.getRawParameterValue(prefix + s)->load() > 0.5f)
                return true;
        }
        return false;
    };

    {
        const bool on = anyAssigned("lfoCutSrc") && rng.nextFloat() < 0.85f;
        setParam(proc, "lfo2en", on ? 1.0f : 0.0f);
        setParam(proc, "lfo2wave", (float)pick(rng, kModWaves, kNumModWaves));
        setParam(proc, "lfo2sync", 1.0f);
        setParam(proc, "lfo2rateSync", (float)pick(rng, kUsefulSyncRates, kNumSyncRates));
        setParam(proc, "lfo2min", randRange(rng, 0.05f, 0.3f));
        setParam(proc, "lfo2max", randRange(rng, 0.7f, 0.95f));
    }
    {
        const bool on = anyAssigned("lfoResSrc") && rng.nextFloat() < 0.8f;
        setParam(proc, "lfo3en", on ? 1.0f : 0.0f);
        setParam(proc, "lfo3wave", (float)pick(rng, kModWaves, kNumModWaves));
        setParam(proc, "lfo3sync", 1.0f);
        setParam(proc, "lfo3rateSync", (float)pick(rng, kUsefulSyncRates, kNumSyncRates));

        // ---- 自己発振の回避 ----
        // DSP は applyResonanceMod(base, mod) = base * 2^(2*mod)、
        // mod は LFO 出力 pos から mod = 2*pos - 1 で作られる。
        // pos を [0.5-m/2, 0.5+m/2] に収めれば |mod| <= m になるので、
        //   base * 2^(2*m) <= 閾値   →   m <= log2(閾値/base) / 2
        // LFO3 は 4 本共通なので、変調が掛かっている中で最も厳しい制約を採る。
        float limit = randRange(rng, 0.30f, 0.55f);   // 変調しないときの既定幅

        for (int k = 0; k < 4; ++k)
        {
            if (!slotActive[k] || !resModulated[k]) continue;

            const float ceiling = safeRes(chosenModel[k]) * 0.92f;
            if (chosenRes[k] >= ceiling) { limit = 0.05f; break; }

            limit = juce::jmin(limit, std::log2(ceiling / chosenRes[k]) * 0.5f);
        }

        limit = juce::jlimit(0.05f, 0.9f, limit);
        setParam(proc, "lfo3min", 0.5f - limit * 0.5f);
        setParam(proc, "lfo3max", 0.5f + limit * 0.5f);
    }

    // ---- LFO4 (Rate Mod) : 1/4 の確率。深さは控えめ ----
    {
        const bool on = rng.nextFloat() < 0.25f;
        setParam(proc, "lfo4en", on ? 1.0f : 0.0f);
        setParam(proc, "lfo4wave", 0.0f);
        setParam(proc, "lfo4sync", 1.0f);
        setParam(proc, "lfo4rateSync", 0.0f);   // 8/1。レート変調はゆっくりが自然
        setParam(proc, "lfo4depth", on ? randRange(rng, 0.5f, 1.5f) : 0.0f);
        setParam(proc, "lfo4assignA", 1.0f);
        setParam(proc, "lfo4assignB", rng.nextFloat() < 0.4f ? 1.0f : 0.0f);
        setParam(proc, "lfo4assignC", rng.nextFloat() < 0.3f ? 1.0f : 0.0f);
    }

    // ---- LFO5 (Dry/Wet) : 1/5 の確率。完全に消えないよう Min を高めに ----
    {
        const bool on = rng.nextFloat() < 0.2f;
        setParam(proc, "lfo5en", on ? 1.0f : 0.0f);
        setParam(proc, "lfo5wave", (float)pick(rng, kModWaves, kNumModWaves));
        setParam(proc, "lfo5sync", 1.0f);
        setParam(proc, "lfo5rateSync", (float)pick(rng, kUsefulSyncRates, kNumSyncRates));
        setParam(proc, "lfo5min", randRange(rng, 35.0f, 60.0f));
        setParam(proc, "lfo5max", 100.0f);
    }

    // ---- Envelope Follower : 1/5 の確率 ----
    {
        const bool on = rng.nextFloat() < 0.2f;
        setParam(proc, "envFollowen", on ? 1.0f : 0.0f);
        setParam(proc, "envFollowdepth", on ? randRange(rng, 35.0f, 80.0f) : 50.0f);
        setParam(proc, "envFollowattack", randRange(rng, 5.0f, 40.0f));
        setParam(proc, "envFollowrelease", randRange(rng, 80.0f, 260.0f));
        setParam(proc, "envFollowinvert", rng.nextFloat() < 0.25f ? 1.0f : 0.0f);
    }

    // ---- 出力段は安全側で固定 ----
    // ここをランダムにすると音量事故になるため、意図的に触らない。
    setParam(proc, "dryWet", 100.0f);
    setParam(proc, "masterGain", 0.0f);
    setParam(proc, "limiterCeiling", -0.1f);
    setParam(proc, "osMode", 1.0f);   // Auto
}

} // namespace PresetRandomiser
