// ==========================================
// FilterA_SVF.h (修正版 v2)
// Zero-Delay Feedback State Variable Filter
// Based on Andy Simper's Linear Trapezoidal Integrator
//
// 修正点: ステレオ対応（チャンネル別状態変数）
// ==========================================

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <array>

class FilterA_SVF
{
public:
    FilterA_SVF();
    ~FilterA_SVF() = default;

    void prepare(double sampleRate, int blockSize);
    void reset();

    void setCutoff(float newCutoffHz);
    void setResonance(float newQ);
    void setType(int type);

    // ===== チャンネルを引数で指定するように変更 =====
    float processSample(float inputSample, int channel);
    void process(juce::AudioBuffer<float>& buffer);

    float getLastLowpassOutput(int channel = 0) const { return lastLp[channel]; }
    float getLastBandpassOutput(int channel = 0) const { return lastBp[channel]; }
    float getLastHighpassOutput(int channel = 0) const { return lastHp[channel]; }

private:
    double sampleRate = 48000.0;
    int currentType = 0;

    float cutoffHz = 1000.0f;
    float resQ = 0.707f;
    float k = 1.0f / 0.707f;

    // 係数（チャンネル共通）
    float g = 0.0f;
    float h = 0.0f;
    float g2 = 0.0f;

    // ===== 【修正の核心】チャンネル別状態変数 =====
    static constexpr int maxChannels = 2;

    float v1[maxChannels] = { 0.0f, 0.0f };  // L, R それぞれ独立
    float v2[maxChannels] = { 0.0f, 0.0f };  // L, R それぞれ独立

    float lastLp[maxChannels] = { 0.0f, 0.0f };
    float lastBp[maxChannels] = { 0.0f, 0.0f };
    float lastHp[maxChannels] = { 0.0f, 0.0f };

    // 係数キャッシング
    float lastComputedCutoff = -1.0f;
    float lastComputedQ = -1.0f;

    void updateCoefficients();
    static float fastTan(float x);
    static float clamp(float value, float minVal, float maxVal);
};