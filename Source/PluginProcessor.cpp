#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

juce::AudioProcessorValueTreeState::ParameterLayout
QuadMorphFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "posX", 1 }, "Base X", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "posY", 1 }, "Base Y", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "masterGain", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-36.0f, 24.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dryWet", 1 }, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "limiterCeiling", 1 }, "Ceiling",
        juce::NormalisableRange<float>(-36.0f, 0.0f, 0.1f), -0.1f));

    juce::StringArray waveTypes = {
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate",
        "Billiard", "Polygon", "Attractor Orbit"
    };
    juce::StringArray syncRates = {
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };
    juce::StringArray lfoNames = { "Morph", "Cutoff", "Reso" };

    for (int i = 0; i < 3; ++i)
    {
        juce::String id = "lfo" + juce::String(i + 1);
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en",       1 }, lfoNames[i] + " Enable", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave",     1 }, lfoNames[i] + " Wave", waveTypes, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "step",     1 }, lfoNames[i] + " Step Mode", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "sync",     1 }, lfoNames[i] + " Sync", true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "rateSync", 1 }, lfoNames[i] + " Rate Sync", syncRates, 5));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, lfoNames[i] + " Rate Free", juce::NormalisableRange<float>(0.01f, 20.0f, 0.001f, 0.2f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "min",      1 }, lfoNames[i] + " Min", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "max",      1 }, lfoNames[i] + " Max", 0.0f, 1.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id + "phase", 1 }, lfoNames[i] + " Phase",
            juce::NormalisableRange<float>(0.0f, 360.0f, 0.1f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id + "fade", 1 }, lfoNames[i] + " Fade In",
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f, 0.3f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id + "spread", 1 }, lfoNames[i] + " Filter Spread",
            juce::NormalisableRange<float>(0.0f, 360.0f, 0.1f), 0.0f));
    }

    // ===== LFO4: Rate Modulation 専用 =====
    {
        juce::String id = "lfo4";
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, "LFO4 Enable", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave", 1 }, "LFO4 Wave", waveTypes, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "step", 1 }, "LFO4 Step Mode", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "sync", 1 }, "LFO4 Sync", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "rateSync", 1 }, "LFO4 Rate Sync", syncRates, 5));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, "LFO4 Rate Free", juce::NormalisableRange<float>(0.01f, 20.0f, 0.001f, 0.2f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "depth", 1 }, "LFO4 Depth", juce::NormalisableRange<float>(0.0f, 4.0f, 0.01f, 0.5f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "assignA", 1 }, "LFO4 Assign to LFO1", true));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "assignB", 1 }, "LFO4 Assign to LFO2", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "assignC", 1 }, "LFO4 Assign to LFO3", false));
    }

    // ===== LFO5: Dry/Wet Range Modulation 専用 =====
    {
        juce::String id = "lfo5";
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, "LFO5 Enable", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave", 1 }, "LFO5 Wave", waveTypes, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "step", 1 }, "LFO5 Step Mode", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "sync", 1 }, "LFO5 Sync", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "rateSync", 1 }, "LFO5 Rate Sync", syncRates, 5));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, "LFO5 Rate Free", juce::NormalisableRange<float>(0.01f, 20.0f, 0.001f, 0.2f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "min", 1 }, "LFO5 Min", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "max", 1 }, "Lfo5 Max", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    }

    // ===== Envelope Follower =====
    {
        juce::String id = "envFollow";
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, "Envelope Follower Enable", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "invert", 1 }, "Envelope Follower Invert", false));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "attack", 1 }, "Envelope Follower Attack", juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.3f), 10.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "release", 1 }, "Envelope Follower Release", juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.3f), 100.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "depth", 1 }, "Envelope Follower Depth", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));
    }

    juce::StringArray suffixes = { "A", "B", "C", "D" };
    juce::StringArray models = {
        "Clean SVF", "Moog Ladder", "Diode (TB-303)", "SEM (Oberheim)", "Bitcrush / SRR",
        "Formant (Vowel)", "Comb Filter", "MS-20 (Screaming)", "All-Pass Phaser", "Wavefolder",
        "Reverb (Metallic)", "Kilo All-Pass",
        "CEM3320", "SSM 2040", "CS-80 (Yamaha)", "Jupiter (Roland)", "EDP Wasp (CMOS)",
        "Butterworth (Flat)", "Chebyshev (Ripple)", "Bessel (Phase)", "Elliptic (Notch)",
        "Vactrol LPG", "Modal Resonator", "Waveguide Mesh", "Bode Freq Shifter",
        "2D Morph", "Phased Array", "Nyquist Anti-alias"
    };
    juce::StringArray slopes = { "12 dB/oct", "24 dB/oct", "48 dB/oct", "96 dB/oct" };

    for (const auto& s : suffixes)
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "enable" + s, 1 }, "Enable " + s, true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "model" + s, 1 }, "Model " + s, models, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type" + s, 1 }, "Type " + s, juce::StringArray{ "LP", "BP", "HP", "Notch" }, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "slope" + s, 1 }, "Slope " + s, slopes, 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cutoff" + s, 1 }, "Cutoff " + s, juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "res" + s, 1 }, "Res / Ctrl " + s, 0.1f, 10.0f, 0.707f));
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "osMode", 1 }, "OS Quality",
        juce::StringArray{ "Off", "Auto", "2x", "4x" }, 1));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "morphBlend", 1 }, "Morph Blend",
        juce::StringArray{ "EqPwr", "Linear", "Smooth", "Radial" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "cutoffAlgo", 1 }, "Cutoff Algo",
        juce::StringArray{ "Abs", "Rel", "Zone" }, 0));

    // ===== 【V1.1.0 追加】XY → Cutoff/Res の適用量 =====
    // Cutoff Algo は V1.0.0 から存在していたが、算出した xyCutoff / xyRes が
    // DSP に接続されておらず、実際には音に影響していなかった。
    // 深さ調整が無いまま接続すると既存プロジェクトの音が激変するため、
    // 適用量パラメータを新設する。
    // 既定値 0% = 従来どおり Cutoff / Res ノブの値がそのまま使われる（音は不変）。
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "xyDepth", 1 }, "XY Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    const juce::StringArray modSrcs = { "Off", "+X", "+Y", "-X", "-Y" };
    const int defaults[4] = { 0, 0, 0, 0 };
    int fi = 0;
    for (const auto& s : suffixes)
    {
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "lfoCutSrc" + s, 1 }, "LFO Cut Src " + s, modSrcs, defaults[fi]));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "lfoResSrc" + s, 1 }, "LFO Res Src " + s, modSrcs, defaults[fi]));
        ++fi;
    }

    // ===== V1.1.0: GUI カラーテーマ =====
    // 表示専用（オーディオ処理には一切関与しない）。
    // ホストのオートメーション一覧を汚さないよう withAutomatable(false) を付ける。
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "colorTheme", 1 }, "GUI Theme",
        juce::StringArray{ "Midnight", "Sakura", "Ocean", "Forest", "Sunset",
                           "Mono", "Neon", "Vaporwave", "Amber", "Arctic" },
        6,   // 既定は Neon
        juce::AudioParameterChoiceAttributes().withAutomatable(false)));

    return layout;
}

QuadMorphFilterAudioProcessor::QuadMorphFilterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // パラメータポインタはここで一度だけ解決する。
    // コンストラクタはメッセージスレッドで走るため juce::String の生成は安全。
    cacheParameterPointers();
    lfoEngine.cacheParams(apvts);
    lfo5Engine.cacheParams(apvts);

    // レイテンシ変化の監視。10Hz あれば OS 切替の反映として十分。
    startTimerHz(10);
}

// ==========================================
// cacheParameterPointers
// 全パラメータの生ポインタを一度だけ解決してキャッシュする。
// オーディオスレッドから文字列検索・文字列連結を完全に排除するための前処理。
// APVTS が保持する std::atomic<float> の寿命はプロセッサと同一なので
// 一度取得したポインタが途中で無効になることはない。
// ==========================================
void QuadMorphFilterAudioProcessor::cacheParameterPointers()
{
    static constexpr const char* kSuffix[4] = { "A", "B", "C", "D" };

    for (int i = 0; i < 4; ++i)
    {
        const juce::String s(kSuffix[i]);
        auto& p = fp[(size_t)i];

        p.cutoff    = apvts.getRawParameterValue("cutoff"    + s);
        p.res       = apvts.getRawParameterValue("res"       + s);
        p.model     = apvts.getRawParameterValue("model"     + s);
        p.type      = apvts.getRawParameterValue("type"      + s);
        p.slope     = apvts.getRawParameterValue("slope"     + s);
        p.enable    = apvts.getRawParameterValue("enable"    + s);
        p.lfoCutSrc = apvts.getRawParameterValue("lfoCutSrc" + s);
        p.lfoResSrc = apvts.getRawParameterValue("lfoResSrc" + s);

        // レイアウト定義と ID が食い違っていれば起動時に気付けるようにする
        jassert(p.cutoff != nullptr && p.res       != nullptr
             && p.model  != nullptr && p.type      != nullptr
             && p.slope  != nullptr && p.enable    != nullptr
             && p.lfoCutSrc != nullptr && p.lfoResSrc != nullptr);
    }

    gp.posX             = apvts.getRawParameterValue("posX");
    gp.posY             = apvts.getRawParameterValue("posY");
    gp.morphBlend       = apvts.getRawParameterValue("morphBlend");
    gp.cutoffAlgo       = apvts.getRawParameterValue("cutoffAlgo");
    gp.xyDepth          = apvts.getRawParameterValue("xyDepth");
    gp.osMode           = apvts.getRawParameterValue("osMode");
    gp.dryWet           = apvts.getRawParameterValue("dryWet");
    gp.masterGain       = apvts.getRawParameterValue("masterGain");
    gp.limiterCeiling   = apvts.getRawParameterValue("limiterCeiling");
    gp.lfo1en           = apvts.getRawParameterValue("lfo1en");
    gp.lfo1wave         = apvts.getRawParameterValue("lfo1wave");
    gp.lfo2en           = apvts.getRawParameterValue("lfo2en");
    gp.lfo2wave         = apvts.getRawParameterValue("lfo2wave");
    gp.lfo3en           = apvts.getRawParameterValue("lfo3en");
    gp.lfo3wave         = apvts.getRawParameterValue("lfo3wave");
    gp.lfo5en           = apvts.getRawParameterValue("lfo5en");
    gp.envFollowEn      = apvts.getRawParameterValue("envFollowen");
    gp.envFollowAttack  = apvts.getRawParameterValue("envFollowattack");
    gp.envFollowRelease = apvts.getRawParameterValue("envFollowrelease");
    gp.envFollowDepth   = apvts.getRawParameterValue("envFollowdepth");
    gp.envFollowInvert  = apvts.getRawParameterValue("envFollowinvert");

    cutoffAParam = apvts.getParameter("cutoffA");
}

QuadMorphFilterAudioProcessor::~QuadMorphFilterAudioProcessor()
{
    // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）。
    // 破棄中にコールバックが走ると、解放済みメンバへのアクセスになる。
    stopTimer();
}

// ==========================================
// timerCallback
// メッセージスレッドで動く。オーディオスレッドが置いた
// pendingLatencySamples を読み、変化していればホストへ通知する。
// ==========================================
void QuadMorphFilterAudioProcessor::timerCallback()
{
    const int lat = pendingLatencySamples.load(std::memory_order_relaxed);

    if (lat != reportedLatencySamples)
    {
        reportedLatencySamples = lat;
        setLatencySamples(lat);
    }
}

void QuadMorphFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    expectedSampleRate = sampleRate;

    lfoEngine.prepare(sampleRate);
    lfo5Engine.prepare(sampleRate);
    filterA.prepare(sampleRate, samplesPerBlock, 2);
    filterB.prepare(sampleRate, samplesPerBlock, 2);
    filterC.prepare(sampleRate, samplesPerBlock, 2);
    filterD.prepare(sampleRate, samplesPerBlock, 2);

    for (auto& buf : filterBuffers)
        buf.setSize(2, samplesPerBlock, false, false, true);
    dryBuffer.setSize(2, samplesPerBlock, false, false, true);

    currentGainReduction[0] = 1.0f;
    currentGainReduction[1] = 1.0f;

    envFollowerSampleRate = sampleRate;
    envFollowEnvelopeValue = 0.0f;

    // ===== スムージング係数（1 極指数、τ = 5ms）=====
    // 旧実装の線形スルーリミッタから置き換え。SR によらず時定数を一定に保つ。
    smoothCoef = 1.0f - std::exp(-1.0f / (0.005f * (float)sampleRate));

    // 以降はコンストラクタでキャッシュしたポインタを使用（文字列検索なし）
    lastDryWet = juce::jlimit(0.0f, 1.0f, gp.dryWet->load() / 100.0f);
    lastMasterGainLinear = juce::Decibels::decibelsToGain(gp.masterGain->load());
    lastLfo5Mod = 0.5f;
    lastCeilingLinear = juce::Decibels::decibelsToGain(gp.limiterCeiling->load());

    lastMorphX = gp.posX->load();
    lastMorphY = gp.posY->load();

    // モーフ重みは「現在値 = 目標値」から始めて、再生開始直後の
    // 立ち上がりでフェードインが起きないようにする
    lastWMix.fill(0.0f);

    int maxLatency = 0;
    maxLatency = std::max(maxLatency, filterA.getOsLatencySamples());
    maxLatency = std::max(maxLatency, filterB.getOsLatencySamples());
    maxLatency = std::max(maxLatency, filterC.getOsLatencySamples());
    maxLatency = std::max(maxLatency, filterD.getOsLatencySamples());

    // prepareToPlay はメッセージ／準備スレッドなのでここは直接通知してよい。
    // 以降の OS 切替による変化は processBlock → timerCallback 経由で反映される。
    pendingLatencySamples.store(maxLatency, std::memory_order_relaxed);
    reportedLatencySamples = maxLatency;
    setLatencySamples(maxLatency);
}

void QuadMorphFilterAudioProcessor::releaseResources() {}

void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Ableton Live 対策自己防衛フェイルセーフ
    if (juce::exactlyEqual(getSampleRate(), 0.0) || !juce::exactlyEqual(getSampleRate(), expectedSampleRate))
    {
        buffer.clear();
        return;
    }

    const int numSamples = buffer.getNumSamples();

    // ===== チャンネル数の防御 =====
    // isBusesLayoutSupported() で mono / stereo のみ許可しているが、
    // ホストによっては宣言と異なるチャンネル数でバッファを渡してくることがある。
    // 以下のコードは要素数 2 の固定長配列（outPtrs 等）と 2ch 固定の
    // dryBuffer / filterBuffers を前提にしているため、必ず 2 でクランプする。
    // 万一 3ch 以上で来た場合、余剰チャンネルは無音にして出力を汚さない。
    static constexpr int kMaxChannels = 2;
    const int hostChannels = buffer.getNumChannels();
    const int numChannels = juce::jmin(hostChannels, kMaxChannels);

    for (int ch = numChannels; ch < hostChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    const float dt = numSamples / (float)expectedSampleRate;

    bool lfo5Enabled = gp.lfo5en->load() > 0.5f;
    const float releaseCoef = 1.0f - std::exp(-1.0f / (0.050f * expectedSampleRate));
    // 【V1.1.0 削除】const float smoothStepPerSample = 1.0f / (0.050f * expectedSampleRate);
    //   線形スルーリミッタ用の 1 サンプル最大変化量だった。
    //   1 極指数スムーザ（メンバ smoothCoef）へ置き換えたため不要。

    float currentDryWetNormalized = gp.dryWet->load() / 100.0f;
    currentDryWetNormalized = juce::jlimit(0.0f, 1.0f, currentDryWetNormalized);
    float currentMasterGaindB = gp.masterGain->load();
    float currentCeilingdB = gp.limiterCeiling->load();

    float currentMasterGainLinear = juce::Decibels::decibelsToGain(currentMasterGaindB);
    float currentCeilingLinear = juce::Decibels::decibelsToGain(currentCeilingdB);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm().hasValue())
                bpm = *(pos->getBpm());

    float baseX = lastMorphX;
    float baseY = lastMorphY;

    LfoEngine::RecordingContext recCtx{
        recBuffer, recLength, isWaitingForRecord, isRecordingDrag, currentRecX, currentRecY
    };
    lfoEngine.process(dt, bpm, baseX, baseY, recCtx);

    // ===== LFO5 (Dry/Wet Modulation) あなたのオリジナルロジックを完全復元 =====
    lfo5Engine.process(dt, (float)bpm);
    float lfo5Mod = lfo5Engine.getOutput();

    float posX = lfoEngine.getPosition(0).x;
    float posY = lfoEngine.getPosition(0).y;

    // XY → Cutoff/Reso 変換
    const int cutoffAlgo = (int)gp.cutoffAlgo->load();
    float xyCutoff, xyRes;

    if (cutoffAlgo == 1)
    {
        const float devX = (posX - 0.5f) * 2.0f;
        const float devY = (0.5f - posY) * 2.0f;
        xyCutoff = 632.0f * std::pow(2.0f, devX * 4.0f);
        xyRes = 1.0f * std::pow(2.0f, devY * 2.0f);
    }
    else if (cutoffAlgo == 2)
    {
        const float devX = (posX - 0.5f) * 2.0f;
        const float devY = (0.5f - posY) * 2.0f;
        const float octX = (devX >= 0.0f) ? devX * 5.0f : devX * 3.0f;
        const float octY = (devY >= 0.0f) ? devY * 3.0f : devY * 2.0f;
        xyCutoff = 632.0f * std::pow(2.0f, octX);
        xyRes = 1.0f * std::pow(2.0f, octY);
    }
    else
    {
        xyCutoff = 20.0f * std::pow(1000.0f, posX);
        xyRes = 0.1f + (1.0f - posY) * 9.9f;
    }
    xyCutoff = juce::jlimit(20.0f, 20000.0f, xyCutoff);
    xyRes = juce::jlimit(0.1f, 10.0f, xyRes);

    // ===== 【V1.1.0 修正】XY Depth =====
    // ここまでで求めた xyCutoff / xyRes は V1.0.0 以来どこからも参照されておらず、
    // Cutoff Algo コンボが音に影響しない状態だった。xyDepth で適用量を決めて
    // getFilterParams() に渡す。0% のときは補間が恒等になるため従来の音と完全に一致する。
    const float xyDepth = juce::jlimit(0.0f, 1.0f, gp.xyDepth->load() / 100.0f);
    const bool  xyActive = (xyDepth > 1.0e-4f);

    bool lfo1_isRand1 = ((int)gp.lfo2wave->load() == 3)
        && (gp.lfo2en->load() > 0.5f);
    bool lfo2_isRand1 = ((int)gp.lfo3wave->load() == 3)
        && (gp.lfo3en->load() > 0.5f);
    bool lfo1_useMod4 = lfo1_isRand1 || lfoEngine.isSpreadActive(1);
    bool lfo2_useMod4 = lfo2_isRand1 || lfoEngine.isSpreadActive(2);

    auto cM = MorphEngine::computeModulation(
        lfoEngine.getPosition(1), lfoEngine.getMod4(1), lfo1_useMod4);
    auto rM = MorphEngine::computeModulation(
        lfoEngine.getPosition(2), lfoEngine.getMod4(2), lfo2_useMod4);

    {
        int osMode = (int)gp.osMode->load();
        filterA.setOsMode(osMode);
        filterB.setOsMode(osMode);
        filterC.setOsMode(osMode);
        filterD.setOsMode(osMode);
    }

    // ===== Envelope Follower：Attack/Release で平滑化 =====
    float envFollowCutoffNormA = fp[0].cutoff->load();
    bool envFollowEnabled = gp.envFollowEn->load() > 0.5f;

    if (envFollowEnabled)
    {
        // ===== Step 1: ブロック内のピーク値を計算 =====
        // getSample() は毎サンプルの境界チェック付きアクセスになるため、
        // チャンネルごとに読み取りポインタを取ってから走査する。
        float maxInputLevel = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* in = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                maxInputLevel = std::max(maxInputLevel, std::abs(in[i]));
        }

        // ===== Step 2: Attack/Release で平滑化（パラメータ連動） =====
        const float attackTimeMs = gp.envFollowAttack->load();
        const float releaseTimeMs = gp.envFollowRelease->load();
        const float attackTime = std::max(0.001f, attackTimeMs * 0.001f);   // 秒換算
        const float releaseTime = std::max(0.001f, releaseTimeMs * 0.001f); // 秒換算

        const float attackCoeff = std::exp(-numSamples / (attackTime * expectedSampleRate));
        const float releaseCoeff = std::exp(-numSamples / (releaseTime * expectedSampleRate));

        if (maxInputLevel > envFollowEnvelopeValue)
            envFollowEnvelopeValue = attackCoeff * envFollowEnvelopeValue + (1.0f - attackCoeff) * maxInputLevel;
        else
            envFollowEnvelopeValue = releaseCoeff * envFollowEnvelopeValue + (1.0f - releaseCoeff) * maxInputLevel;

        // ===== Step 3: 平滑化された値で Cutoff を計算（Invertの論理矛盾を安全に修正） =====
        // cutoffAParam はコンストラクタで解決済み（文字列検索なし）
        if (cutoffAParam != nullptr)
        {
            float rawNormalizedCutoff = cutoffAParam->getValue(); // 0.0 ~ 1.0 の正規化空間
            float depthPercent = gp.envFollowDepth->load() / 100.0f;
            bool invert = gp.envFollowInvert->load() > 0.5f;

            float modulatedNormalized = rawNormalizedCutoff;

            if (invert)
            {
                // 反転時：音が消えている時はベース位置（200Hz）を維持し、入力音に応じて下方向（床=0.0）へ閉じる
                modulatedNormalized = rawNormalizedCutoff - (envFollowEnvelopeValue * depthPercent * rawNormalizedCutoff);
            }
            else
            {
                // 通常時：音が消えている時はベース位置（200Hz）を維持し、入力音に応じて上方向（天井=1.0）へ開く
                modulatedNormalized = rawNormalizedCutoff + (envFollowEnvelopeValue * depthPercent * (1.0f - rawNormalizedCutoff));
            }

            // 最後にフィルターが要求する正しい実数Hz空間（20〜20000Hz）に安全マッピング
            envFollowCutoffNormA = cutoffAParam->convertFrom0to1(juce::jlimit(0.0f, 1.0f, modulatedNormalized));
        }
    }

    // idx: 0=A, 1=B, 2=C, 3=D
    // 旧実装は "lfoCutSrc" + s のような文字列連結で ID を組み立てており、
    // 呼び出しのたびに juce::String がヒープ確保されていた（RT 安全性違反）。
    // 現在はコンストラクタで解決済みのポインタを添字で引くだけ。
    auto readModSrc = [](std::atomic<float>* p) -> int {
        return juce::jlimit(0, 4, juce::roundToInt(p->load()));
        };

    auto getFilterParams = [&](int idx) -> std::pair<float, float>
        {
            const auto& p = fp[(size_t)idx];

            const int cutSrc = readModSrc(p.lfoCutSrc);
            const int resSrc = readModSrc(p.lfoResSrc);
            const bool lfoCutOn = cutSrc > 0;
            const bool lfoResOn = resSrc > 0;
            const int  cutModIdx = cutSrc > 0 ? cutSrc - 1 : 0;
            const int  resModIdx = resSrc > 0 ? resSrc - 1 : 0;

            // ===== FilterA (idx 0) のみ Envelope Follower を適用 =====
            float baseCutoff = (idx == 0) ? envFollowCutoffNormA : p.cutoff->load();
            float baseRes    = p.res->load();

            // ===== XY Depth: モーフ座標由来の Cutoff/Res を混ぜる =====
            // 幾何補間（対数領域の線形補間）: base * (xy / base)^depth
            //   depth = 0 → base そのもの（恒等 = 従来の音）
            //   depth = 1 → xyCutoff / xyRes そのもの
            // 周波数も Q も比率で知覚されるため、線形ではなく対数領域で補間する。
            // base は常に正（20〜20000 / 0.1〜10）なので pow の定義域は安全。
            // ブロックレートの計算であり、TptFilter 側の SmoothedValue が
            // サンプル間を補間するためジッパーノイズは生じない。
            if (xyActive)
            {
                baseCutoff *= std::pow(xyCutoff / baseCutoff, xyDepth);
                baseRes    *= std::pow(xyRes    / baseRes,    xyDepth);
            }

            float fc = lfoCutOn ? MorphEngine::applyFrequencyMod(baseCutoff, cM[cutModIdx]) : baseCutoff;
            float res = lfoResOn ? MorphEngine::applyResonanceMod(baseRes, rM[resModIdx]) : baseRes;

            return { juce::jlimit(20.0f, 20000.0f, fc), juce::jlimit(0.1f, 10.0f, res) };
        };

    auto updateTpt = [&](TptFilter& f, int idx)
        {
            const auto& p = fp[(size_t)idx];
            auto [fc, res] = getFilterParams(idx);
            f.setModel(juce::roundToInt(p.model->load()));
            f.setCutoff(fc);
            f.setResonance(res);
            f.setType(juce::roundToInt(p.type->load()));
            f.setSlope(juce::roundToInt(p.slope->load()));
        };

    updateTpt(filterA, 0);
    updateTpt(filterB, 1);
    updateTpt(filterC, 2);
    updateTpt(filterD, 3);

    // ===== レイテンシ変化の検出 =====
    // OS ファクターは OS Quality パラメータと、Auto 時のモデル変更で変わる。
    // ここではアトミックに値を置くだけ（ロック・確保・ホスト通知なし）。
    // 実際の setLatencySamples() は timerCallback がメッセージスレッドで行う。
    {
        int lat = 0;
        lat = std::max(lat, filterA.getOsLatencySamples());
        lat = std::max(lat, filterB.getOsLatencySamples());
        lat = std::max(lat, filterC.getOsLatencySamples());
        lat = std::max(lat, filterD.getOsLatencySamples());
        pendingLatencySamples.store(lat, std::memory_order_relaxed);
    }

    bool enA = fp[0].enable->load() > 0.5f;
    bool enB = fp[1].enable->load() > 0.5f;
    bool enC = fp[2].enable->load() > 0.5f;
    bool enD = fp[3].enable->load() > 0.5f;

    // ===== 【V1.1.0 追加】Solo =====
    // ミキサーの Solo と同じく Enable より優先する（Enable が off の
    // フィルターを Solo しても鳴る）。他の 3 つは処理自体を飛ばすので
    // CPU も下がる。UI スレッドが書く atomic を 1 回読むだけ。
    const int soloIdx = soloFilter.load(std::memory_order_relaxed);
    if (soloIdx >= 0 && soloIdx < 4)
    {
        enA = (soloIdx == 0);
        enB = (soloIdx == 1);
        enC = (soloIdx == 2);
        enD = (soloIdx == 3);
    }

    const int enabledCount = (int)enA + (int)enB + (int)enC + (int)enD;


    int modelA = (int)fp[0].model->load();
    int modelB = (int)fp[1].model->load();
    int modelC = (int)fp[2].model->load();
    int modelD = (int)fp[3].model->load();

    // 【V1.1.0 削除】旧 svfQuad.processBuffer(buffer, filterBuffers);
    //   svfQuad は 4 インスタンスとも常時 disabled で出力を clear() するだけであり、
    //   直後の procTptIfNeeded() が同じ filterBuffers を必ず上書き（または clear）する。
    //   完全な冗長処理だったためオーディオパスから除去した。
    //   Clean SVF を将来モデルとして復活させる場合は FilterA_SVF_SIMD を再統合すること。

    auto procTptIfNeeded = [&](juce::AudioBuffer<float>& dst,
        TptFilter& tpt, int model, bool enabled)
        {
            if (!enabled) { dst.clear(); return; }
            for (int ch = 0; ch < numChannels; ++ch)
                dst.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            tpt.process(dst);
        };

    procTptIfNeeded(filterBuffers[0], filterA, modelA, enA);
    procTptIfNeeded(filterBuffers[1], filterB, modelB, enB);
    procTptIfNeeded(filterBuffers[2], filterC, modelC, enC);
    procTptIfNeeded(filterBuffers[3], filterD, modelD, enD);

    buffer.clear();

    // =========================================================================
    // オーディオバッファのマルチチャンネルスタックポインタキャッシュ
    // =========================================================================
    float* outPtrs[2] = { nullptr, nullptr };
    const float* dryPtrs[2] = { nullptr, nullptr };
    const float* fAPtrs[2] = { nullptr, nullptr };
    const float* fBPtrs[2] = { nullptr, nullptr };
    const float* fCPtrs[2] = { nullptr, nullptr };
    const float* fDPtrs[2] = { nullptr, nullptr };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        outPtrs[ch] = buffer.getWritePointer(ch);
        dryPtrs[ch] = dryBuffer.getReadPointer(ch);
        fAPtrs[ch] = filterBuffers[0].getReadPointer(ch);
        fBPtrs[ch] = filterBuffers[1].getReadPointer(ch);
        fCPtrs[ch] = filterBuffers[2].getReadPointer(ch);
        fDPtrs[ch] = filterBuffers[3].getReadPointer(ch);
    }

    // =========================================================================
    // ループ不変量の事前計算
    //
    // 旧実装はこれらをサンプルループ内で毎回読んでいたが、いずれも
    // ブロック内で変化しない値であり、文字列キー検索を 48kHz なら
    // 毎秒 19 万回繰り返しているだけだった。
    // =========================================================================
    const float targetMorphX     = gp.posX->load();
    const float targetMorphY     = gp.posY->load();
    const bool  lfo1Enabled      = gp.lfo1en->load() > 0.5f;
    const int   morphBlendCurrent = (int)gp.morphBlend->load();

    // 有効フィルターだけを対象に等パワー正規化する（無効分は加算側で無視される）。
    // ラムダは参照キャプチャのみでヒープ確保を伴わない。
    auto normalizeWMix = [&](std::array<float, 4>& w)
        {
            float sumSq = 0.0f;
            if (enA) sumSq += w[0] * w[0];
            if (enB) sumSq += w[1] * w[1];
            if (enC) sumSq += w[2] * w[2];
            if (enD) sumSq += w[3] * w[3];
            if (sumSq > 1e-8f)
            {
                const float norm = 1.0f / std::sqrt(sumSq);
                if (enA) w[0] *= norm;
                if (enB) w[1] *= norm;
                if (enC) w[2] *= norm;
                if (enD) w[3] *= norm;
            }
        };

    // モーフ座標 (mx, my) から wMix を計算するヘルパー。
    auto computeWMixNormalized = [&](float mx, float my) -> std::array<float, 4>
        {
            std::array<float, 4> w;
            switch (morphBlendCurrent)
            {
            case 1:  w = MorphEngine::computeLinearWMix(mx, my); break;
            case 2:  w = MorphEngine::computeSmoothstepWMix(mx, my); break;
            case 3:  w = MorphEngine::computeRadialWMix(mx, my); break;
            default: w = MorphEngine::computeEqualPowerWMix(mx, my); break;
            }
            normalizeWMix(w);
            return w;
        };

    // ===== LFO1 (Morph) が per-filter 値を出しているか =====
    // 【V1.1.0 修正】
    //   LFO1 は Rand1 波形または Spread>0 のとき mod4[0][f] に
    //   4 フィルター分の独立した値を書き込んでいる。
    //   ところが旧実装は positions[0]（= mod4[0][0] と mod4[0][1] の 2 つだけ）を
    //   2D 座標として使っており、3 番目と 4 番目が捨てられていた。
    //   LFO2 / LFO3 側には同じ用途の lfo1_useMod4 / lfo2_useMod4 が既にあるため、
    //   LFO1 も同じパターンに揃えて 4 値をそのまま重みに使う。
    //
    //   Rand1  : 4 フィルターが独立にランダムな音量で出入りする
    //   Spread : 4 フィルターが位相をずれて順に立ち上がる（回るような効果）
    const bool lfo0_isRand1 = lfo1Enabled && ((int)gp.lfo1wave->load() == 3);
    const bool lfo0_useMod4 = lfo1Enabled
                            && (lfo0_isRand1 || lfoEngine.isSpreadActive(0));

    // wMix がブロック全体で不変になるケースを事前に判定する。
    //   1) 有効フィルターが 1 個以下   → 重みは 0/1 の固定値
    //   2) LFO1 が per-filter 値を出力 → mod4[0] はブロック定数
    //   3) LFO1 有効                   → morph 座標にブロック定数 posX/posY を使う
    // 残る「LFO1 無効 かつ 有効フィルター 2 個以上」のときだけ、
    // サンプル毎に平滑化される lastMorphX/Y に追従する必要がある。
    const bool wMixIsBlockConstant = (enabledCount <= 1) || lfo1Enabled;

    std::array<float, 4> wMix_current{};
    if (enabledCount <= 1)
    {
        wMix_current = { enA ? 1.0f : 0.0f, enB ? 1.0f : 0.0f,
                         enC ? 1.0f : 0.0f, enD ? 1.0f : 0.0f };
    }
    else if (lfo0_useMod4)
    {
        // Rand1 / Spread: 4 フィルターそれぞれの値を直接重みにする
        wMix_current = lfoEngine.getMod4(0);
        normalizeWMix(wMix_current);
    }
    else if (lfo1Enabled)
    {
        wMix_current = computeWMixNormalized(posX, posY);
    }

    // =========================================================================
    // サンプルループ
    // =========================================================================
    for (int i = 0; i < numSamples; ++i)
    {
        // ---- 出力段パラメータ（1 極指数スムーザ、τ=5ms）----
        lastDryWet += smoothCoef * (currentDryWetNormalized - lastDryWet);
        lastMasterGainLinear += smoothCoef * (currentMasterGainLinear - lastMasterGainLinear);
        lastCeilingLinear += smoothCoef * (currentCeilingLinear - lastCeilingLinear);

        // lastMorphX/Y の平滑化は毎サンプル必ず実行する。
        // 次ブロックの baseX/baseY として持ち越されるため、
        // wMix がブロック定数のケースでも省略してはならない。
        lastMorphX += smoothCoef * (targetMorphX - lastMorphX);
        lastMorphY += smoothCoef * (targetMorphY - lastMorphY);

        // 平滑化された morph 座標に追従する必要があるときだけ再計算する
        if (!wMixIsBlockConstant)
            wMix_current = computeWMixNormalized(lastMorphX, lastMorphY);

        // ---- モーフ重みの平滑化 ----
        // wMix_current はブロック単位でしか更新されないため、そのまま掛けると
        // ブロック境界でゲインが階段状に飛ぶ（LFO1 有効時のジッパーノイズの原因）。
        // 有効なフィルターの分だけ指数スムーザに通してから使う。
        for (int wi = 0; wi < 4; ++wi)
            lastWMix[(size_t)wi] += smoothCoef * (wMix_current[(size_t)wi] - lastWMix[(size_t)wi]);

        float dryWetSmoothed = lastDryWet;
        if (lfo5Enabled)
        {
            // 旧実装はスルーリミッタだったため、LFO5 のレートを上げるほど
            // 振幅が潰れて波形が三角に化けていた。指数スムーザなら
            // 速い LFO にもそのまま追従する。
            lastLfo5Mod += smoothCoef * (lfo5Mod - lastLfo5Mod);
            dryWetSmoothed = lastLfo5Mod;
        }

        float w_rad = dryWetSmoothed * juce::MathConstants<float>::pi * 0.5f;
        float dry_amp = juce::dsp::FastMathApproximations::cos(w_rad);
        float wet_amp = juce::dsp::FastMathApproximations::sin(w_rad);

        float gainLinear = lastMasterGainLinear;
        float ceilingLinear = lastCeilingLinear;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float wet = 0.0f;
            if (enA) wet += fAPtrs[ch][i] * lastWMix[0];
            if (enB) wet += fBPtrs[ch][i] * lastWMix[1];
            if (enC) wet += fCPtrs[ch][i] * lastWMix[2];
            if (enD) wet += fDPtrs[ch][i] * lastWMix[3];

            float gained = (dryPtrs[ch][i] * dry_amp + wet * wet_amp) * gainLinear;
            float absSignal = std::abs(gained);
            float targetGr = (absSignal > ceilingLinear) ? ceilingLinear / absSignal : 1.0f;

            if (targetGr < currentGainReduction[ch])
                currentGainReduction[ch] = targetGr;
            else
                currentGainReduction[ch] += releaseCoef * (targetGr - currentGainReduction[ch]);

            outPtrs[ch][i] = gained * currentGainReduction[ch];
        }
    }
}

const juce::String QuadMorphFilterAudioProcessor::getName() const { return "Quad-Morph Filter"; }
bool QuadMorphFilterAudioProcessor::acceptsMidi()  const { return false; }
bool QuadMorphFilterAudioProcessor::producesMidi() const { return false; }
bool QuadMorphFilterAudioProcessor::isMidiEffect() const { return false; }
double QuadMorphFilterAudioProcessor::getTailLengthSeconds() const { return 10.0; }
int  QuadMorphFilterAudioProcessor::getNumPrograms() { return 1; }
int  QuadMorphFilterAudioProcessor::getCurrentProgram() { return 0; }
void QuadMorphFilterAudioProcessor::setCurrentProgram(int) {}
const juce::String QuadMorphFilterAudioProcessor::getProgramName(int) { return {}; }
void QuadMorphFilterAudioProcessor::changeProgramName(int, const juce::String&) {}
// ==========================================
// isBusesLayoutSupported
// 【V1.1.0 修正】
//   旧実装は無条件に true を返していたが、processBlock 側は
//   float* outPtrs[2] のような要素数 2 の固定長配列を実チャンネル数で
//   ループしており、dryBuffer と filterBuffers も 2ch 固定、
//   currentGainReduction[2] も同様だった。
//   ホストが 3ch 以上のレイアウト（5.1 など）で問い合わせて
//   インスタンス化した場合、スタックとバッファの範囲外書き込みになり
//   スキャン失敗やクラッシュにつながる。
//   実際に処理できる mono / stereo のみを受け付けるよう明示する。
// ==========================================
bool QuadMorphFilterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // 出力は mono か stereo のみ
    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // 入出力のチャンネル構成は一致していること（サイドチェイン等は非対応）
    if (mainIn != mainOut)
        return false;

    return true;
}
bool QuadMorphFilterAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* QuadMorphFilterAudioProcessor::createEditor()
{
    return new QuadMorphFilterAudioProcessorEditor(*this);
}
void QuadMorphFilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
void QuadMorphFilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuadMorphFilterAudioProcessor();
}