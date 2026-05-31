// ==========================================
// LFO4 専用処理実装（LfoEngine.cpp に追加）
// ==========================================

void LfoEngine::processLFO4(float dt,
    double bpm,
    juce::AudioProcessorValueTreeState& apvts)
{
    // ===== パラメータ読み込み =====
    bool enabled = apvts.getRawParameterValue("lfo4en")->load() > 0.5f;

    if (!enabled)
    {
        lfo4RateModulation = 1.0f;  // 変調なし（=1倍速）
        return;
    }

    int   wave      = (int)apvts.getRawParameterValue("lfo4wave")->load();
    bool  step      = apvts.getRawParameterValue("lfo4step")->load() > 0.5f;
    bool  sync      = apvts.getRawParameterValue("lfo4sync")->load() > 0.5f;
    int   rateSync  = (int)apvts.getRawParameterValue("lfo4rateSync")->load();
    float rateFree  = apvts.getRawParameterValue("lfo4rateFree")->load();
    float depth     = apvts.getRawParameterValue("lfo4depth")->load();
    float phase     = apvts.getRawParameterValue("lfo4phase")->load();
    float fade      = apvts.getRawParameterValue("lfo4fade")->load();
    float spread    = apvts.getRawParameterValue("lfo4spread")->load();

    // ===== Rate計算（LFO1と同じ） =====
    float rate = sync ? getSyncTime(rateSync, bpm) : rateFree;

    // ===== 位相更新 =====
    lfo4State.phaseX += rate * dt;
    lfo4State.phaseY += rate * dt;

    // ===== ラップアラウンド =====
    const float twoPi = 6.283185307f;
    while (lfo4State.phaseX >= twoPi) lfo4State.phaseX -= twoPi;
    while (lfo4State.phaseY >= twoPi) lfo4State.phaseY -= twoPi;
    if (lfo4State.phaseX < 0.0f) lfo4State.phaseX += twoPi;
    if (lfo4State.phaseY < 0.0f) lfo4State.phaseY += twoPi;

    // ===== Fade エンベロープ計算 =====
    float targetFade = enabled ? 1.0f : 0.0f;
    float fadeCoeff = fade > 0.01f ? 1.0f - std::exp(-dt / fade) : 1.0f;
    lfo4State.fadeEnv = lfo4State.fadeEnv * (1.0f - fadeCoeff) + targetFade * fadeCoeff;

    // ===== Wave計算（X軸: Rate変調用） =====
    float phaseVal = phase * twoPi / 360.0f;
    float pX = lfo4State.phaseX + phaseVal;
    float tX = pX / twoPi;

    // Normalize
    tX = tX - std::floor(tX);
    if (tX < 0.0f) tX += 1.0f;

    float waveX = evaluateWaveX(wave, pX, tX);
    waveX *= lfo4State.fadeEnv;
    if (step) waveX = std::round(waveX * 4.0f) / 4.0f;

    // ===== Rate変調係数の計算 =====
    // waveXは[-1, 1]の範囲
    // depth オクターブの範囲で変調
    // 例: waveX=1.0, depth=2 → 変調係数=2^2=4倍
    // 例: waveX=-1.0, depth=2 → 変調係数=2^(-2)=0.25倍
    float modAmount = waveX * depth;  // [-depth, +depth]
    lfo4RateModulation = std::pow(2.0f, modAmount);  // 2^modAmount

    // ===== ランダム値の更新（Double モード用） =====
    // 独立した乱数生成
    static constexpr float phi1 = 0.6180339887f;
    static constexpr float phi2 = 0.4656612873f;

    lfo4State.randomSeed += 0.123456f;
    float r1 = std::fmod(lfo4State.randomSeed * phi1, 1.0f);
    if (r1 < 0.0f) r1 += 1.0f;
    lfo4State.currentRandom = { r1, std::fmod(r1 * phi2, 1.0f) };
}
