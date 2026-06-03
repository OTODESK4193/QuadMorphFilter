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
    // ===== LFO4: Rate Modulation 処理 =====
    processLFO4(dt, bpm, apvts);

    // ===== LFO4 アサイン先チェック =====
    // アサイン先でない場合は lfo4RateModulation を無視（1.0 として動作）
    bool assignLFO1 = apvts.getRawParameterValue("lfo4assignA")->load() > 0.5f;
    bool assignLFO2 = apvts.getRawParameterValue("lfo4assignB")->load() > 0.5f;
    bool assignLFO3 = apvts.getRawParameterValue("lfo4assignC")->load() > 0.5f;

    for (int i = 0; i < 3; ++i)
    {
        // アサイン状態に応じて、処理前に lfo4RateModulation を設定
        bool isAssigned = (i == 0 && assignLFO1) || (i == 1 && assignLFO2) || (i == 2 && assignLFO3);
        lfo4RateModulationActive[i] = isAssigned;

        processSingleLfo(i, dt, bpm, baseX, baseY, apvts, rec);
    }
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

    // ===== Min > Max のとき逆方向動作 =====
    // Min=95%, Max=20% → LFO は高い側から低い側へ動く（逆方向）。
    // spread が負になることで方向情報を保持する。
    // 旧実装の std::min/max 正規化は廃止。
    float rangeStart = minVal;
    float spread     = maxVal - minVal;   // 負 = 逆方向

    bool isWait = rec.isWaiting[i].load(std::memory_order_acquire);
    bool isDrag = rec.isDragging[i].load(std::memory_order_acquire);

    if (!enabled)
    {
        positions[i] = { baseX, baseY };
        states[i].fadeEnv = 0.0f;  // 次に有効化されたときフェードを先頭から再開
        return;
    }

    // ===== Phase Offset + Fade-in パラメータ読み取り =====
    const float phaseOffsetDeg = apvts.getRawParameterValue(id + "phase")->load();
    const float fadeTime       = apvts.getRawParameterValue(id + "fade" )->load();

    // ===== Fade-in エンベロープ更新 =====
    // isWait=true（Recording 録音待機中）はフェードを一時停止
    if (fadeTime < 0.001f)
        states[i].fadeEnv = 1.0f;           // フェード無効 → 即時フル
    else if (!isWait)
        states[i].fadeEnv = juce::jmin(1.0f, states[i].fadeEnv + dt / fadeTime);

    // ===== レート計算 =====
    // LFO4 Rate Modulation をアサイン先フラグに基づいて適用
    float rateModulation = lfo4RateModulationActive[i] ? lfo4RateModulation : 1.0f;
    float rate = (sync
        ? (1.0f / getSyncTime((int)apvts.getRawParameterValue(id + "rateSync")->load(), bpm))
        : apvts.getRawParameterValue(id + "rateFree")->load()) * rateModulation;

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

    // ===== Phase Offset 適用 =====
    // 0°=変化なし, 180°=逆位相, 360°=一周（= 0° と同等）
    // fmod で [0, 2π] に正規化 → sin/cos/Recording/Billiard 以外の全波形に有効
    const float twoPi = juce::MathConstants<float>::twoPi;
    float p = std::fmod(states[i].phase + (phaseOffsetDeg / 360.0f) * twoPi, twoPi);
    if (p < 0.0f) p += twoPi;
    float t = p / twoPi;  // STEP/Smooth 両モードで t を計算 [0, 1]
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
            // LfoEngine 内部の recordingData を使用（XYPadComponent から setRecordingData() で設定）
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
                    // Smooth モード: 事前平滑化済みデータ + 修正 Hermite 補間
                    // smoothedRecData は setRecordingData 時に3パス移動平均で作成済み。
                    // これにより折れ線パスのオーバーシュートを防ぎ、滑らかな軌道を実現する。
                    int idx0 = (idx1 - 1 + len) % len;
                    int idx2 = (idx1 + 1) % len;
                    int idx3 = (idx2 + 1) % len;
                    float frac = exactIdx - std::floor(exactIdx);

                    float interp_x = hermite(
                        smoothedRecData[i][idx0].x,
                        smoothedRecData[i][idx1].x,
                        smoothedRecData[i][idx2].x,
                        smoothedRecData[i][idx3].x,
                        frac);
                    float interp_y = hermite(
                        smoothedRecData[i][idx0].y,
                        smoothedRecData[i][idx1].y,
                        smoothedRecData[i][idx2].y,
                        smoothedRecData[i][idx3].y,
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

    // ===== Fade-in 適用 =====
    // fadeEnv=0 → W=0（変調量ゼロ = Min/Max の中点で静止）
    // fadeEnv=1 → フル変調
    // Recording 再生を含む全波形に一律適用。isWait 中は fadeEnv が増えないため録音中は変調停止。
    W_x *= states[i].fadeEnv;
    W_y *= states[i].fadeEnv;

    // ===== Step モード =====
    if (step)
    {
        W_x = std::round(W_x * 4.0f) / 4.0f;
        W_y = std::round(W_y * 4.0f) / 4.0f;
    }

    // ===== Filter Phase Spread の計算 =====
    // 周期的な波形に対して、フィルター(A=0,B=1,C=2,D=3)ごとに
    // spread° ずつ位相をずらした値を mod4[i][f] に格納する。
    // 例: Spread=90°,Sine → A=sin(p), B=sin(p+90°), C=sin(p+180°), D=sin(p+270°)
    const float spreadDeg   = apvts.getRawParameterValue(id + "spread")->load();
    const float spreadRad   = spreadDeg * juce::MathConstants<float>::pi / 180.0f;
    // Spread が有効な周期波形 (ステートフル波形は除外)
    const bool  canSpread   = (spreadRad > 0.001f) &&
                              (wave == 0 || wave == 1 || wave == 2 ||
                               (wave >= 8 && wave <= 15) || wave == 17 || wave == 18);

    // ===== 出力への書き込み =====
    if (wave == 3) // Random 1: フィルターごとに独立したランダム値
    {
        spreadActive[i] = false;
        for (int f = 0; f < 4; ++f)
        {
            float raw = states[i].currentRand1[f]
                + (states[i].nextRand1[f] - states[i].currentRand1[f]) * t;
            float w = raw * 2.0f - 1.0f;
            if (step) w = std::round(w * 4.0f) / 4.0f;
            w *= states[i].fadeEnv;
            mod4[i][f] = juce::jlimit(0.0f, 1.0f, rangeStart + (w + 1.0f) / 2.0f * spread);
        }
        positions[i].x = mod4[i][0];
        positions[i].y = mod4[i][1];
    }
    else if (canSpread)
    {
        // Spread 有効: フィルター f ごとに位相を f*spreadRad ずらして評価
        spreadActive[i] = true;
        const float basePhaseFull = states[i].phase + (phaseOffsetDeg / 360.0f) * twoPi;
        for (int f = 0; f < 4; ++f)
        {
            float pf = std::fmod(basePhaseFull + f * spreadRad, twoPi);
            if (pf < 0.0f) pf += twoPi;
            float tf = pf / twoPi;
            float wf = evaluateWaveX(wave, pf, tf);
            wf *= states[i].fadeEnv;
            if (step) wf = std::round(wf * 4.0f) / 4.0f;
            mod4[i][f] = juce::jlimit(0.0f, 1.0f, rangeStart + (wf + 1.0f) / 2.0f * spread);
        }
        positions[i].x = mod4[i][0];  // メイン表示は F=0 (phase+0)
        positions[i].y = mod4[i][1];  // Y 表示は F=1 (phase+spread)
    }
    else
    {
        // Spread 無効 (通常モード)
        spreadActive[i] = false;
        positions[i].x = juce::jlimit(0.0f, 1.0f, rangeStart + (W_x + 1.0f) / 2.0f * spread);
        positions[i].y = juce::jlimit(0.0f, 1.0f, rangeStart + (W_y + 1.0f) / 2.0f * spread);
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
// ヘルパー: Filter Phase Spread 用 波形X軸値評価
// 周期的な数式波形を任意位相 p で評価して返す。
// ステートフル波形（Billiard, SmoothNoise, Recording 等）は Sine にフォールバック。
// ==========================================
float LfoEngine::evaluateWaveX(int wave, float p, float t)
{
    const float pi = juce::MathConstants<float>::pi;
    switch (wave)
    {
    case 0:  return std::sin(p);                                         // Sine
    case 1:  return 1.0f - (p / pi);                                    // SAW
    case 2:  return (p < pi) ? 1.0f : -1.0f;                           // Pulse
    case 8:  return 0.5f * std::cos(p) + 0.5f * std::cos(5.2f * p);   // Spirograph
    case 9:  return (std::sin(p) + std::sin(1.37f * p)                  // Harmonic Swarm
                  +  std::sin(2.11f * p)) / 3.0f;
    case 10: { float r = std::cos(2.0f * p) + 2.0f;                    // Torus Knot
               return r * std::cos(3.0f * p) / 3.0f; }
    case 11: return std::sin(p * 3.0f);                                 // Lissajous
    case 12: return t * std::cos(p * 5.0f);                             // Spiral
    case 13: return 0.6f * std::cos(p) + 0.4f * std::cos(1.5f * p);   // Star
    case 14: return std::cos(2.5f * p) * std::cos(p);                  // Rose
    case 15: { float sc = 2.0f / (3.0f - std::cos(2.0f * p));         // Lemniscate
               return sc * std::cos(p); }
    case 17: { float rP = std::cos(pi / 6.0f)                           // Polygon
                   / std::cos(std::fmod(p, pi / 3.0f) - pi / 6.0f);
               return rP * std::cos(p); }
    case 18: { float rO = 0.6f + 0.4f * std::sin(p * 7.0f);           // Attractor Orbit
               return rO * std::cos(p); }
    default: return std::sin(p);                                         // フォールバック
    }
}

// ==========================================
// ヘルパー: Hermite 補間（3次スプライン）
// ==========================================
float LfoEngine::hermite(float p0, float p1, float p2, float p3, float t)
{
    // 正しい Catmull-Rom スプライン
    // 区間 [p1, p2] を補間。p0, p3 はタンジェント計算用コンテキスト点。
    // タンジェント: p1 での傾き = 0.5*(p2-p0), p2 での傾き = 0.5*(p3-p1)
    // → セグメント間で C1 連続（速度連続）を保証、キンク無し。
    //
    // 旧実装の a0 = -0.5*mt³+mt²-0.5*mt は誤りで
    // 実際には 0.5t³-0.5t² (= a3 と同値) になっており、
    // p0 の影響が過小/過大になってセグメント境界で鋭いキンクが発生していた。
    float t2 = t * t;
    float t3 = t2 * t;

    float a0 = -0.5f * t3 + t2 - 0.5f * t;   // p0 の寄与（タンジェント用コンテキスト）
    float a1 =  1.5f * t3 - 2.5f * t2 + 1.0f; // p1（補間区間の始点）
    float a2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t; // p2（補間区間の終点）
    float a3 =  0.5f * t3 - 0.5f * t2;        // p3 の寄与（タンジェント用コンテキスト）

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
// LFO4 専用処理: Rate Modulation
// ==========================================
void LfoEngine::processLFO4(float dt,
    double bpm,
    juce::AudioProcessorValueTreeState& apvts)
{
    // ===== パラメータ読み込み =====
    bool enabled = apvts.getRawParameterValue("lfo4en")->load() > 0.5f;

    if (!enabled)
    {
        lfo4RateModulation = 1.0f;  // 変調なし
        lfo4State.fadeEnv = 0.0f;
        return;
    }

    int   wave      = (int)apvts.getRawParameterValue("lfo4wave")->load();
    bool  step      = apvts.getRawParameterValue("lfo4step")->load() > 0.5f;
    bool  sync      = apvts.getRawParameterValue("lfo4sync")->load() > 0.5f;
    int   rateSync  = (int)apvts.getRawParameterValue("lfo4rateSync")->load();
    float rateFree  = apvts.getRawParameterValue("lfo4rateFree")->load();
    float depth     = apvts.getRawParameterValue("lfo4depth")->load();

    // ===== Rate計算 =====
    float rate = sync ? getSyncTime(rateSync, bpm) : rateFree;

    // ===== 位相更新 =====
    const float twoPi = 6.283185307f;
    lfo4State.phase += rate * dt;
    while (lfo4State.phase >= twoPi) lfo4State.phase -= twoPi;
    if (lfo4State.phase < 0.0f) lfo4State.phase += twoPi;

    // ===== Fade エンベロープ計算 =====
    float targetFade = 1.0f;
    float fadeCoeff = fade > 0.01f ? 1.0f - std::exp(-dt / fade) : 1.0f;
    lfo4State.fadeEnv = lfo4State.fadeEnv * (1.0f - fadeCoeff) + targetFade * fadeCoeff;

    // ===== Wave計算（Rate変調用） =====
    float phaseVal = phase * twoPi / 360.0f;
    float pX = lfo4State.phase + phaseVal;
    float tX = pX / twoPi;

    // ノーマライズ
    tX = tX - std::floor(tX);
    if (tX < 0.0f) tX += 1.0f;

    float waveX = evaluateWaveX(wave, pX, tX);
    waveX *= lfo4State.fadeEnv;
    if (step) waveX = std::round(waveX * 4.0f) / 4.0f;

    // ===== Rate変調係数の計算 =====
    // waveX: [-1, 1]
    // depth: [0, 4] オクターブ
    // 例: waveX=1.0, depth=2 → lfo4RateModulation = 2^2 = 4倍
    // 例: waveX=-1.0, depth=2 → lfo4RateModulation = 2^(-2) = 0.25倍
    float modAmount = waveX * depth;
    lfo4RateModulation = std::pow(2.0f, modAmount);
}

