// ==========================================
// DSP/LfoEngine.cpp
// ==========================================
#include "LfoEngine.h"
#include <cmath>

LfoEngine::LfoEngine()
{
    for (int i = 0; i < 3; ++i)
    {
        positions[i] = { 0.5f, 0.5f };
        mod4[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
}

void LfoEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
}

// ==========================================
// メイン処理: 3つのLFOを順番に処理
// ==========================================
void LfoEngine::process(float dt,
    double bpm,
    float baseX,
    float baseY,
    juce::AudioProcessorValueTreeState& apvts,
    const RecordingContext& rec)
{
    for (int i = 0; i < 3; ++i)
        processSingleLfo(i, dt, bpm, baseX, baseY, apvts, rec);
}

// ==========================================
// 1つのLFOを処理
// ==========================================
void LfoEngine::processSingleLfo(int i,
    float dt,
    double bpm,
    float baseX,
    float baseY,
    juce::AudioProcessorValueTreeState& apvts,
    const RecordingContext& rec)
{
    juce::String id = "lfo" + juce::String(i + 1);

    bool  enabled = apvts.getRawParameterValue(id + "en")->load() > 0.5f;
    int   wave = (int)apvts.getRawParameterValue(id + "wave")->load();
    bool  step = apvts.getRawParameterValue(id + "step")->load() > 0.5f;
    bool  sync = apvts.getRawParameterValue(id + "sync")->load() > 0.5f;
    float minVal = apvts.getRawParameterValue(id + "min")->load();
    float maxVal = apvts.getRawParameterValue(id + "max")->load();
    int   boundMode = (int)apvts.getRawParameterValue(id + "bound")->load();

    // ===== 自動正規化: Min > Max でも自動修正 =====
    // ユーザーが Min=100% Max=0% と逆に設定しても正しく動作する。
    // これにより直感的で誤操作に強いUIを実現
    float actualMin = std::min(minVal, maxVal);
    float actualMax = std::max(minVal, maxVal);
    float spread = actualMax - actualMin;

    bool isWait = rec.isWaiting[i].load(std::memory_order_acquire);
    bool isDrag = rec.isDragging[i].load(std::memory_order_acquire);

    if (!enabled)
    {
        positions[i] = { baseX, baseY };
        return;
    }

    // ===== レート計算 =====
    float rate = sync
        ? (1.0f / getSyncTime((int)apvts.getRawParameterValue(id + "rateSync")->load(), bpm))
        : apvts.getRawParameterValue(id + "rateFree")->load();

    float actualDt = rate * dt;

    // ===== フェーズ更新 =====
    if (!isWait)
    {
        states[i].phase += juce::MathConstants<float>::twoPi * actualDt;

        if (states[i].phase >= juce::MathConstants<float>::twoPi)
        {
            states[i].phase -= juce::MathConstants<float>::twoPi;

            states[i].currentRandom = states[i].nextRandom;
            states[i].nextRandom = { states[i].rng.nextBool() ? 1.0f : 0.0f,
                                        states[i].rng.nextBool() ? 1.0f : 0.0f };

            states[i].currentRand1 = states[i].nextRand1;
            for (int f = 0; f < 4; ++f)
                states[i].nextRand1[f] = states[i].rng.nextFloat();

            states[i].smoothNx = states[i].tNextNx;
            states[i].smoothNy = states[i].tNextNy;
            states[i].tNextNx = states[i].rng.nextFloat();
            states[i].tNextNy = states[i].rng.nextFloat();
        }
    }

    float p = states[i].phase;
    float t = step ? 0.0f : (p / juce::MathConstants<float>::twoPi);
    float W_x = 0.0f, W_y = 0.0f;

    // ===== 波形計算 (19種類) =====
    switch (wave)
    {
    case 0: // Sine
        W_x = std::sin(p);
        W_y = std::sin(p + juce::MathConstants<float>::halfPi);
        break;

    case 1: // SAW
        W_x = 1.0f - (p / juce::MathConstants<float>::pi);
        W_y = 1.0f - (std::fmod(p + juce::MathConstants<float>::halfPi,
            juce::MathConstants<float>::twoPi)
            / juce::MathConstants<float>::pi);
        break;

    case 2: // Pulse
        W_x = (p < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
        W_y = (std::fmod(p + juce::MathConstants<float>::halfPi,
            juce::MathConstants<float>::twoPi)
            < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
        break;

    case 3: // Random 1 (per-filter independent)
        // Wave==3 は後処理で mod4 に直接書き込む
        break;

    case 4: // Random 2 (linear interpolation)
        W_x = (states[i].currentRandom.x
            + (states[i].nextRandom.x - states[i].currentRandom.x) * t) * 2.0f - 1.0f;
        W_y = (states[i].currentRandom.y
            + (states[i].nextRandom.y - states[i].currentRandom.y) * t) * 2.0f - 1.0f;
        break;

    case 5: // Noise
        W_x = states[i].rng.nextFloat() * 2.0f - 1.0f;
        W_y = states[i].rng.nextFloat() * 2.0f - 1.0f;
        break;

    case 6: // Recording
        if (isWait) {
            // Recording 待機中: 現在のマウス位置またはラスト位置を表示
            if (isDrag) {
                W_x = rec.currentX[i].load() * 2.f - 1.f;
                W_y = rec.currentY[i].load() * 2.f - 1.f;
            }
            else {
                int len = rec.lengths[i].load();
                if (len > 0) {
                    W_x = rec.buffers[i][len - 1].x * 2.f - 1.f;
                    W_y = rec.buffers[i][len - 1].y * 2.f - 1.f;
                }
            }
        }
        else {
            // Recording 再生中
            // LfoEngine 内部の recordingData を使用（XYGridComponent から setRecordingData() で設定）
            int len = recordingLength[i];
            if (len > 0) {
                float exactIdx = t * len;
                int idx1 = (int)exactIdx % len;

                if (step) {
                    // Step モード: ピクセルそのまま（量子化）
                    W_x = recordingData[i][idx1].x * 2.f - 1.f;
                    W_y = recordingData[i][idx1].y * 2.f - 1.f;
                }
                else {
                    // Smooth モード: Hermite 補間
                    int idx0 = (idx1 - 1 + len) % len;
                    int idx2 = (idx1 + 1) % len;
                    int idx3 = (idx2 + 1) % len;
                    float frac = exactIdx - std::floor(exactIdx);

                    float interp_x = hermite(
                        recordingData[i][idx0].x,
                        recordingData[i][idx1].x,
                        recordingData[i][idx2].x,
                        recordingData[i][idx3].x,
                        frac);
                    float interp_y = hermite(
                        recordingData[i][idx0].y,
                        recordingData[i][idx1].y,
                        recordingData[i][idx2].y,
                        recordingData[i][idx3].y,
                        frac);

                    W_x = interp_x * 2.f - 1.f;
                    W_y = interp_y * 2.f - 1.f;
                }
            }
        }
        break;

    case 7: { // Smooth Noise
        float smT = t * t * (3.0f - 2.0f * t);
        W_x = (states[i].smoothNx
            + (states[i].tNextNx - states[i].smoothNx) * smT) * 2.f - 1.f;
        W_y = (states[i].smoothNy
            + (states[i].tNextNy - states[i].smoothNy) * smT) * 2.f - 1.f;
        break;
    }

    case 8: // Spirograph
        W_x = 0.5f * std::cos(p) + 0.5f * std::cos(5.2f * p);
        W_y = 0.5f * std::sin(p) - 0.5f * std::sin(5.2f * p);
        break;

    case 9: // Harmonic Swarm
        W_x = (std::sin(p) + std::sin(1.37f * p) + std::sin(2.11f * p)) / 3.0f;
        W_y = (std::cos(1.09f * p) + std::cos(1.73f * p) + std::cos(2.51f * p)) / 3.0f;
        break;

    case 10: { // 3D Torus Knot
        float r_knot = std::cos(2.0f * p) + 2.0f;
        W_x = r_knot * std::cos(3.0f * p) / 3.0f;
        W_y = r_knot * std::sin(3.0f * p) / 3.0f;
        break;
    }

    case 11: // Lissajous
        W_x = std::sin(p * 3.0f);
        W_y = std::sin(p * 4.0f);
        break;

    case 12: // Spiral
        W_x = t * std::cos(p * 5.0f);
        W_y = t * std::sin(p * 5.0f);
        break;

    case 13: // Star
        W_x = 0.6f * std::cos(p) + 0.4f * std::cos(1.5f * p);
        W_y = 0.6f * std::sin(p) - 0.4f * std::sin(1.5f * p);
        break;

    case 14: // Rose
        W_x = std::cos(2.5f * p) * std::cos(p);
        W_y = std::cos(2.5f * p) * std::sin(p);
        break;

    case 15: { // Lemniscate
        float scale = 2.0f / (3.0f - std::cos(2.0f * p));
        W_x = scale * std::cos(p);
        W_y = scale * std::sin(2.0f * p) / 2.0f;
        break;
    }

    case 16: // Billiard
        if (!isWait) {
            states[i].bilX += states[i].bilVx * actualDt * 1.5f;
            states[i].bilY += states[i].bilVy * actualDt * 1.5f;
            if (states[i].bilX > 1.0f || states[i].bilX < -1.0f) states[i].bilVx *= -1.0f;
            if (states[i].bilY > 1.0f || states[i].bilY < -1.0f) states[i].bilVy *= -1.0f;
        }
        W_x = states[i].bilX;
        W_y = states[i].bilY;
        break;

    case 17: { // Polygon
        float rP = std::cos(juce::MathConstants<float>::pi / 6.0f)
            / std::cos(std::fmod(p, juce::MathConstants<float>::pi / 3.0f)
                - juce::MathConstants<float>::pi / 6.0f);
        W_x = rP * std::cos(p);
        W_y = rP * std::sin(p);
        break;
    }

    case 18: { // Attractor Orbit
        float rO = 0.6f + 0.4f * std::sin(p * 7.0f);
        W_x = rO * std::cos(p);
        W_y = rO * std::sin(p);
        break;
    }

    default: break;
    }

    // ===== Step モード =====
    if (step)
    {
        W_x = std::round(W_x * 4.0f) / 4.0f;
        W_y = std::round(W_y * 4.0f) / 4.0f;
    }

    // ===== 出力への書き込み =====
    if (wave == 3) // Random 1: フィルターごとに独立したランダム値
    {
        for (int f = 0; f < 4; ++f)
        {
            float raw = states[i].currentRand1[f]
                + (states[i].nextRand1[f] - states[i].currentRand1[f]) * t;
            float w = raw * 2.0f - 1.0f;
            if (step) w = std::round(w * 4.0f) / 4.0f;
            // actualMin + w * spread で [actualMin, actualMax] 範囲内で変動
            mod4[i][f] = applyBound(actualMin + (w + 1.0f) / 2.0f * spread, boundMode);
        }
        positions[i].x = mod4[i][0];
        positions[i].y = mod4[i][1];
    }
    else
    {
        // W_x, W_y は [-1, 1] の範囲。これを [actualMin, actualMax] に変換
        // W = -1 → actualMin (0%)
        // W = +1 → actualMax (100%)
        // W = 0  → (actualMin + actualMax) / 2 (50%)
        positions[i].x = applyBound(actualMin + (W_x + 1.0f) / 2.0f * spread, boundMode);
        positions[i].y = applyBound(actualMin + (W_y + 1.0f) / 2.0f * spread, boundMode);
        for (int f = 0; f < 4; ++f)
            mod4[i][f] = positions[i].x;
    }
}

// ==========================================
// アクセサ
// ==========================================
juce::Point<float> LfoEngine::getPosition(int index) const
{
    jassert(index >= 0 && index < 3);
    return positions[index];
}

std::array<float, 4> LfoEngine::getMod4(int index) const
{
    jassert(index >= 0 && index < 3);
    return mod4[index];
}

// ==========================================
// ヘルパー: Hermite 補間（3次スプライン）
// ==========================================
float LfoEngine::hermite(float p0, float p1, float p2, float p3, float t)
{
    // 3次 Hermite 補間（Catmull-Rom）
    // t: [0, 1]
    // p0, p1, p2, p3: 4つのコントロールポイント
    float t2 = t * t;
    float t3 = t2 * t;
    float mt = 1.0f - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;

    // Catmull-Rom 係数
    float a0 = -0.5f * mt3 + mt2 - 0.5f * mt;
    float a1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
    float a2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
    float a3 = 0.5f * t3 - 0.5f * t2;

    return a0 * p0 + a1 * p1 + a2 * p2 + a3 * p3;
}

// ==========================================
// ヘルパー: BPM同期時間計算
// ==========================================
float LfoEngine::getSyncTime(int selection, double bpm)
{
    double beatLen = 60.0 / bpm;
    if (selection < 10) return (float)(beatLen * std::pow(2.0, 3 - selection));
    if (selection < 16) return (float)(beatLen * std::pow(2.0, 0 - (selection - 10)) * 1.5);
    return (float)(beatLen * std::pow(2.0, 0 - (selection - 16)) * (2.0 / 3.0));
}

// ==========================================
// ヘルパー: Boundary処理
// ==========================================
float LfoEngine::applyBound(float v, int mode)
{
    if (mode == 0) // Clip
        return juce::jlimit(0.0f, 1.0f, v);

    if (mode == 1) // Bounce
    {
        v = std::fmod(v, 2.0f);
        if (v < 0.0f) v += 2.0f;
        if (v > 1.0f) v = 2.0f - v;
        return v;
    }

    // Wrap
    v = std::fmod(v, 1.0f);
    if (v < 0.0f) v += 1.0f;
    return v;
}