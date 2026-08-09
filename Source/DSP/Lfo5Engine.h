#pragma once
// APVTS と juce::jlimit を直接使うため、juce_core だけでなく
// juce_audio_processors を明示的にインクルードする
// （従来は PluginProcessor.h 側のインクルード順に依存していた）。
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <array>
#include <atomic>

class Lfo5Engine
{
public:
    // LFO5 は位相を dt（ブロック長の秒数）で進めるため、サンプルレート自体は
    // 使用しない。呼び出し側との互換のため引数は残すが値は保持しない。
    // 【V1.1.0】旧実装のメンバ sampleRate は代入されるだけで一度も読まれて
    // いなかったため削除した。
    void prepare(double /*newSampleRate*/)
    {
        phase = 0.0f;
        output = 0.0f;
    }

    // ===== パラメータポインタのキャッシュ =====
    // 【V1.1.0 追加】
    // 旧 process() は毎ブロック 8 回の getRawParameterValue（文字列キー検索）を
    // オーディオスレッドから呼んでいた。ポインタを一度だけ解決して保持する。
    // AudioProcessor のコンストラクタ（メッセージスレッド）から呼ぶこと。
    void cacheParams(juce::AudioProcessorValueTreeState& apvts)
    {
        prm.en       = apvts.getRawParameterValue("lfo5en");
        prm.sync     = apvts.getRawParameterValue("lfo5sync");
        prm.rateSync = apvts.getRawParameterValue("lfo5rateSync");
        prm.rateFree = apvts.getRawParameterValue("lfo5rateFree");
        prm.wave     = apvts.getRawParameterValue("lfo5wave");
        prm.step     = apvts.getRawParameterValue("lfo5step");
        prm.minVal   = apvts.getRawParameterValue("lfo5min");
        prm.maxVal   = apvts.getRawParameterValue("lfo5max");

        jassert(prm.en != nullptr && prm.sync != nullptr && prm.rateSync != nullptr
             && prm.rateFree != nullptr && prm.wave != nullptr && prm.step != nullptr
             && prm.minVal != nullptr && prm.maxVal != nullptr);

        paramsCached = true;
    }

    void process(float dt, float bpm)
    {
        jassert(paramsCached);
        if (!paramsCached)
        {
            output = 0.0f;
            return;
        }

        bool enabled = prm.en->load() > 0.5f;
        if (!enabled)
        {
            output = 0.0f;
            return;
        }

        // ===== Rate 計算 =====
        float rate = 0.0f;
        bool isSynced = prm.sync->load() > 0.5f;

        if (isSynced)
        {
            int syncIdx = (int)prm.rateSync->load();
            rate = computeSyncRate(syncIdx, bpm);
        }
        else
        {
            rate = prm.rateFree->load();
        }

        // ===== Phase アップデート =====
        float prevPhase = phase;
        phase += rate * dt;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            randomValue = rng.nextFloat();  // サイクル開始時に新しいランダム値を生成
        }
        else if (phase < 0.0f)
        {
            phase += 1.0f;
            randomValue = rng.nextFloat();  // サイクル開始時に新しいランダム値を生成
        }

        // ===== Wave 形を計算 =====
        int waveType = (int)prm.wave->load();
        bool stepMode = prm.step->load() > 0.5f;

        float baseOutput = computeWave(phase, waveType);

        // ===== Step Mode =====
        if (stepMode)
        {
            baseOutput = std::floor(phase * 4.0f) / 4.0f;  // 4-step quantize
            baseOutput = (baseOutput < 0.5f) ? 0.0f : 1.0f;  // Binary
        }

        // ===== Min/Max 範囲を適用 (0.0 ~ 1.0 の output を Min～Max 範囲にマップ) =====
        float minVal = prm.minVal->load() / 100.0f;
        float maxVal = prm.maxVal->load() / 100.0f;
        minVal = juce::jlimit(0.0f, 1.0f, minVal);
        maxVal = juce::jlimit(0.0f, 1.0f, maxVal);

        output = baseOutput * (maxVal - minVal) + minVal;
    }

    float getOutput() const { return output; }

private:
    // ===== パラメータポインタキャッシュ =====
    struct ParamPtrs
    {
        std::atomic<float>* en       = nullptr;
        std::atomic<float>* sync     = nullptr;
        std::atomic<float>* rateSync = nullptr;
        std::atomic<float>* rateFree = nullptr;
        std::atomic<float>* wave     = nullptr;
        std::atomic<float>* step     = nullptr;
        std::atomic<float>* minVal   = nullptr;
        std::atomic<float>* maxVal   = nullptr;
    };

    // 変数名に注意: computeWave / billiardNoise / smoothNoise が
    // 位相を表す引数 float p を取るため、メンバ名は p ではなく prm とする。
    ParamPtrs prm;
    bool      paramsCached = false;

    float phase = 0.0f;
    float output = 0.0f;
    juce::Random rng;
    float randomValue = 0.5f;

    float computeSyncRate(int selection, float bpm)
    {
        // ===== 正しい音符計算：1/1 = 全音符（4拍）, 1/2 = 二分音符（2拍）, 1/4 = 四分音符（1拍） =====
        // getLfoEngine::getSyncTime() をベースに、rate（周波数）を返す
        double beatLen = 60.0 / bpm;  // 1拍の時間
        float beatTime = 0.0f;

        if (selection < 10)
            beatTime = (float)(beatLen * std::pow(2.0, 5 - selection));
        else if (selection < 16)
            beatTime = (float)(beatLen * std::pow(2.0, 2 - (selection - 10)) * 1.5);
        else
            beatTime = (float)(beatLen * std::pow(2.0, 2 - (selection - 16)) * (2.0 / 3.0));

        return (beatTime > 0.0f) ? (1.0f / beatTime) : 1.0f;
    }

    float computeWave(float p, int waveType)
    {
        // p: 0.0 ~ 1.0
        switch (waveType)
        {
            case 0:  // Sine
                return 0.5f + 0.5f * std::sin(p * 6.28318f);

            case 1:  // Triangle
            {
                float tri = (p < 0.5f) ? (p * 4.0f) : (2.0f - p * 4.0f);
                return juce::jlimit(0.0f, 1.0f, tri);
            }

            case 2:  // Square
                return (p < 0.5f) ? 1.0f : 0.0f;

            case 3:  // Sawtooth
                return p;

            case 4:  // Random (held per-cycle)
                return randomValue;

            case 5:  // Billiard (復帰的ノイズ)
                return billiardNoise(p);

            case 6:  // SmoothNoise
                return smoothNoise(p);

            default:
                return 0.5f;
        }
    }

    float billiardNoise(float p)
    {
        // 疑似ランダム: sin の組み合わせ
        float val = std::sin(p * 12.566f) * std::sin(p * 7.123f);
        return 0.5f + 0.5f * val;
    }

    float smoothNoise(float p)
    {
        // Hermite スムージング
        float t = p;
        float smooth = t * t * (3.0f - 2.0f * t);
        return smooth;
    }
};
