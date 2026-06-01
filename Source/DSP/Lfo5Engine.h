#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <array>

class Lfo5Engine
{
public:
    void prepare(double sampleRate)
    {
        this->sampleRate = sampleRate;
        phase = 0.0f;
        output = 0.0f;
    }

    void process(float dt, float bpm, const juce::AudioProcessorValueTreeState& apvts)
    {
        bool enabled = apvts.getRawParameterValue("lfo5en")->load() > 0.5f;
        if (!enabled)
        {
            output = 0.0f;
            return;
        }

        // ===== Rate 計算 =====
        float rate = 0.0f;
        bool isSynced = apvts.getRawParameterValue("lfo5sync")->load() > 0.5f;

        if (isSynced)
        {
            int syncIdx = (int)apvts.getRawParameterValue("lfo5rateSync")->load();
            rate = computeSyncRate(syncIdx, bpm);
        }
        else
        {
            rate = apvts.getRawParameterValue("lfo5rateFree")->load();
        }

        // ===== Phase アップデート =====
        phase += rate * dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
        else if (phase < 0.0f)
            phase += 1.0f;

        // ===== Wave 形を計算 =====
        int waveType = (int)apvts.getRawParameterValue("lfo5wave")->load();
        bool stepMode = apvts.getRawParameterValue("lfo5step")->load() > 0.5f;

        float baseOutput = computeWave(phase, waveType);

        // ===== Step Mode =====
        if (stepMode)
        {
            baseOutput = std::floor(phase * 4.0f) / 4.0f;  // 4-step quantize
            baseOutput = (baseOutput < 0.5f) ? 0.0f : 1.0f;  // Binary
        }

        // ===== Depth 適用 =====
        float depthPercent = apvts.getRawParameterValue("lfo5depth")->load() / 100.0f;
        output = baseOutput * depthPercent;
    }

    float getOutput() const { return output; }
    float getPhase() const { return phase; }

private:
    double sampleRate = 0.0;
    float phase = 0.0f;
    float output = 0.0f;

    float computeSyncRate(int selection, float bpm)
    {
        // LfoEngine::getSyncTime() をベースに、rate（周波数）を返す
        // getSyncTime() は beat length を返すため、逆数を取る
        double beatLen = 60.0 / bpm;
        float beatTime = 0.0f;

        if (selection < 10)
            beatTime = (float)(beatLen * std::pow(2.0, 3 - selection));
        else if (selection < 16)
            beatTime = (float)(beatLen * std::pow(2.0, 0 - (selection - 10)) * 1.5);
        else
            beatTime = (float)(beatLen * std::pow(2.0, 0 - (selection - 16)) * (2.0 / 3.0));

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

    float randomValue = 0.5f;
};
