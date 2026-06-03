    // ==========================================
    // PluginProcessor.cpp
    // ==========================================
    #include "PluginProcessor.h"
    #include "PluginEditor.h"

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
            juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));
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
            // ===== 新規追加 =====
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ id + "phase", 1 }, lfoNames[i] + " Phase",
                juce::NormalisableRange<float>(0.0f, 360.0f, 0.1f), 0.0f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ id + "fade", 1 }, lfoNames[i] + " Fade In",
                juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f, 0.3f), 0.0f));
            // Filter Phase Spread: A/B/C/D に位相差を付けてフィルターごとの独立した動きを実現
            // 0°=全フィルター同位相, 90°=各フィルター90°ずつズレ(Sine時に最大独立)
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ id + "spread", 1 }, lfoNames[i] + " Filter Spread",
                juce::NormalisableRange<float>(0.0f, 360.0f, 0.1f), 0.0f));
        }

        // ===== LFO4: Rate Modulation 専用 =====
        // LFO1と同じアルゴリズムだが、Recording不要
        {
            juce::String id = "lfo4";
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, "LFO4 Enable", false));
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave", 1 }, "LFO4 Wave", waveTypes, 0));
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "step", 1 }, "LFO4 Step Mode", false));
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "sync", 1 }, "LFO4 Sync", false));  // ← 初期値: Off
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "rateSync", 1 }, "LFO4 Rate Sync", syncRates, 5));
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, "LFO4 Rate Free", juce::NormalisableRange<float>(0.01f, 20.0f, 0.001f, 0.2f), 1.0f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "depth", 1 }, "LFO4 Depth", juce::NormalisableRange<float>(0.0f, 4.0f, 0.01f, 0.5f), 0.0f));
            // ===== LFO4 アサイン先 =====
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
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "max", 1 }, "LFO5 Max", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
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

        // ===== 【新規追加】ここに挿入 =====
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "osMode", 1 }, "OS Quality",
            juce::StringArray{ "Off", "Auto", "2x", "4x" }, 0));

        // Morph ブレンドアルゴリズム (Morph モード時に使用)
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "morphBlend", 1 }, "Morph Blend",
            juce::StringArray{ "EqPwr", "Linear", "Smooth", "Radial" }, 0));

        // Cutoff モード アルゴリズム (Cutoff モード時に使用)
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "cutoffAlgo", 1 }, "Cutoff Algo",
            juce::StringArray{ "Abs", "Rel", "Zone" }, 0));

        // lfoCutSrc/lfoResSrc: 5択 (0=Off, 1=+X, 2=+Y, 3=-X, 4=-Y)
        // デフォルト: A=+X(1), B=+Y(2), C=-X(3), D=-Y(4) — 旧実装の動作を維持
        {
            const juce::StringArray modSrcs = { "Off", "+X", "+Y", "-X", "-Y" };
            const int defaults[4] = { 1, 2, 3, 4 };
            int fi = 0;
            for (const auto& s : suffixes)
            {
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID{ "lfoCutSrc" + s, 1 }, "LFO Cut Src " + s, modSrcs, defaults[fi]));
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID{ "lfoResSrc" + s, 1 }, "LFO Res Src " + s, modSrcs, defaults[fi]));
                ++fi;
            }
        }

        return layout;
    }

    QuadMorphFilterAudioProcessor::QuadMorphFilterAudioProcessor()
        : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        apvts(*this, nullptr, "Parameters", createParameterLayout())
    {
    }

    QuadMorphFilterAudioProcessor::~QuadMorphFilterAudioProcessor() {}

    void QuadMorphFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        lfoEngine.prepare(sampleRate);
        lfo5Engine.prepare(sampleRate);
        svfQuad.prepare(sampleRate, samplesPerBlock);
        filterA.prepare(sampleRate, samplesPerBlock, 2);
        filterB.prepare(sampleRate, samplesPerBlock, 2);
        filterC.prepare(sampleRate, samplesPerBlock, 2);
        filterD.prepare(sampleRate, samplesPerBlock, 2);

        for (auto& buf : filterBuffers)
            buf.setSize(2, samplesPerBlock, false, false, true);
        dryBuffer.setSize(2, samplesPerBlock, false, false, true);

        currentGainReduction[0] = 1.0f;
        currentGainReduction[1] = 1.0f;

        // ===== Envelope Follower 初期化 =====
        envFollowerSampleRate = sampleRate;
        envelopeValue = 0.0f;
        attackCoeff = 0.0f;
        releaseCoeff = 0.0f;

        // ===== パラメータスムージング初期化（修正版）=====
        // Dry/Wet: 0-100% → 0.0-1.0 に正規化、かつ安全性チェック
        lastDryWet = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("dryWet")->load() / 100.0f);

        // Master Gain: dB値を linear に変換
        float masterGaindB = apvts.getRawParameterValue("masterGain")->load();
        lastMasterGainLinear = juce::Decibels::decibelsToGain(masterGaindB);

        // LFO5 mod 初期化
        lastLfo5Mod = 0.5f;  // デフォルト 50% (LFO5 無効時の値)

        // Ceiling: dB値を linear に変換
        float ceilingdB = apvts.getRawParameterValue("limiterCeiling")->load();
        lastCeilingLinear = juce::Decibels::decibelsToGain(ceilingdB);

        // Morph パラメータの初期化
        lastMorphX = apvts.getRawParameterValue("posX")->load();
        lastMorphY = apvts.getRawParameterValue("posY")->load();

        // ===== 【新規追加】ここに挿入 =====
        // OSのレイテンシをホストに報告
        // (filterA~D で最大のレイテンシを使用)
        int maxLatency = 0;
        maxLatency = std::max(maxLatency, filterA.getOsLatencySamples());
        maxLatency = std::max(maxLatency, filterB.getOsLatencySamples());
        maxLatency = std::max(maxLatency, filterC.getOsLatencySamples());
        maxLatency = std::max(maxLatency, filterD.getOsLatencySamples());
        setLatencySamples(maxLatency);

    }

    void QuadMorphFilterAudioProcessor::releaseResources() {}

    void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        const int   numSamples = buffer.getNumSamples();
        const auto  sampleRate = getSampleRate();
        const int   numChannels = buffer.getNumChannels();
        const float dt = numSamples / (float)sampleRate;

        // ===== Dry/Wet + ゲイン + リミッター（修正版）=====
        bool lfo5Enabled = apvts.getRawParameterValue("lfo5en")->load() > 0.5f;
        const float releaseCoef = 1.0f - std::exp(-1.0f / (0.050f * sampleRate));

        // ===== スムージング時定数の最適化 (毎サンプル更新) =====
        // τ = 5ms: Dry/Wet クロスフェードの安定性・パワー保存重視
        // 毎サンプル更新により、sin/cos 計算が常に連続的に進行
        // → クリック・ジッパーノイズを完全に排除
        // 計算: smoothCoef = 1.0 - exp(-1.0 / (tau_seconds * sampleRate))
        // 例) 48kHz, 5ms: smoothCoef ≈ 0.00408 (毎サンプル 0.408% 更新)
        const float smoothCoef = 1.0f - std::exp(-1.0f / (0.005f * sampleRate));  // τ = 5ms, per-sample

        // 現在のパラメータ値を取得（正規化: 0-1 range）
        // パラメータ定義は 0-100 の NormalisableRange なので、/100.0f で正規化
        float currentDryWetNormalized = apvts.getRawParameterValue("dryWet")->load() / 100.0f;
        // クリッピング（安全性）: float の丸め誤差やパラメータ外れ値を回避
        currentDryWetNormalized = juce::jlimit(0.0f, 1.0f, currentDryWetNormalized);
        float currentMasterGaindB = apvts.getRawParameterValue("masterGain")->load();
        float currentCeilingdB = apvts.getRawParameterValue("limiterCeiling")->load();

        // Linear domain に変換
        float currentMasterGainLinear = juce::Decibels::decibelsToGain(currentMasterGaindB);
        float currentCeilingLinear = juce::Decibels::decibelsToGain(currentCeilingdB);

        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (pos->getBpm().hasValue())
                    bpm = *(pos->getBpm());

        // ===== Morph スムージング値を使用（毎ブロック初期化時に更新）=====
        // baseX, baseY は後で毎 16 サンプルごとに更新される
        float baseX = lastMorphX;
        float baseY = lastMorphY;

        LfoEngine::RecordingContext recCtx{
            recBuffer, recLength, isWaitingForRecord, isRecordingDrag, currentRecX, currentRecY
        };
        lfoEngine.process(dt, bpm, baseX, baseY, apvts, recCtx);

        // ===== LFO5 (Dry/Wet Modulation) =====
        lfo5Engine.process(dt, bpm, apvts);
        float lfo5Mod = lfo5Engine.getOutput();  // 0.0 ~ depth%

        float posX = lfoEngine.getPosition(0).x;
        float posY = lfoEngine.getPosition(0).y;

        // ===== XY → Cutoff/Reso 変換 (cutoffAlgo で方式を選択) =====
        const int cutoffAlgo = (int)apvts.getRawParameterValue("cutoffAlgo")->load();
        float xyCutoff, xyRes;

        if (cutoffAlgo == 1)
        {
            // 案II: 相対モード — 中央 (0.5, 0.5) = 632Hz・Res 1.0 をゼロ点とし
            //   X 軸: ±4 オクターブ (左=低、右=高)
            //   Y 軸: ±2 オクターブ (上=高、下=低)
            const float devX =  (posX - 0.5f) * 2.0f;    // -1 to +1
            const float devY =  (0.5f - posY) * 2.0f;    // +1=上(高), -1=下(低)
            xyCutoff = 632.0f * std::pow(2.0f, devX * 4.0f);
            xyRes    = 1.0f   * std::pow(2.0f, devY * 2.0f);
        }
        else if (cutoffAlgo == 2)
        {
            // 案III: ゾーン非対称 — 正方向がより広いレンジ
            //   X 右: +5oct、X 左: -3oct
            //   Y 上: +3oct(Res)、Y 下: -2oct(Res)
            const float devX = (posX - 0.5f) * 2.0f;
            const float devY = (0.5f - posY) * 2.0f;
            const float octX = (devX >= 0.0f) ? devX * 5.0f : devX * 3.0f;
            const float octY = (devY >= 0.0f) ? devY * 3.0f : devY * 2.0f;
            xyCutoff = 632.0f * std::pow(2.0f, octX);
            xyRes    = 1.0f   * std::pow(2.0f, octY);
        }
        else
        {
            // 案I: 絶対モード (従来通り)
            // X: 20Hz〜20kHz（対数スケール）、Y: 上=大(Reso)
            xyCutoff = 20.0f * std::pow(1000.0f, posX);
            xyRes    = 0.1f + (1.0f - posY) * 9.9f;
        }
        xyCutoff = juce::jlimit(20.0f, 20000.0f, xyCutoff);
        xyRes    = juce::jlimit(0.1f,  10.0f,    xyRes);

        // ===== LFO2/3 モジュレーション量 =====
        bool lfo1_isRand1   = ((int)apvts.getRawParameterValue("lfo2wave")->load() == 3)
            && (apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
        bool lfo2_isRand1   = ((int)apvts.getRawParameterValue("lfo3wave")->load() == 3)
            && (apvts.getRawParameterValue("lfo3en")->load() > 0.5f);
        // Spread 有効時は mod4 を直接使用（Random1 と同じパス）
        bool lfo1_useMod4   = lfo1_isRand1 || lfoEngine.isSpreadActive(1);
        bool lfo2_useMod4   = lfo2_isRand1 || lfoEngine.isSpreadActive(2);

        auto cM = MorphEngine::computeModulation(
            lfoEngine.getPosition(1), lfoEngine.getMod4(1), lfo1_useMod4);
        auto rM = MorphEngine::computeModulation(
            lfoEngine.getPosition(2), lfoEngine.getMod4(2), lfo2_useMod4);

        // ===== 【新規追加】ここに挿入 =====
    // OSモードを各フィルターに反映
    // (毎ブロック呼ぶが、値が変わらなければ内部でスキップされる)
        {
            int osMode = (int)apvts.getRawParameterValue("osMode")->load();
            filterA.setOsMode(osMode);
            filterB.setOsMode(osMode);
            filterC.setOsMode(osMode);
            filterD.setOsMode(osMode);
        }

        // ===== フィルターパラメータ統合ゲッター =====
        //
        // lfoCutOn の意味:
        //   Cutoffモード: true → XY X位置を使用 + LFO2変調
        //                 false → 個別スライダーを使用（XY・LFO2無効）
        //   Morphモード:  true → スライダー + LFO2変調
        //                 false → スライダーのみ（LFO2無効）
        //
        // lfoResOn も同様
        // AudioParameterChoice (5択) の正規化値 → インデックス変換
        // 5択なので normalized = index / 4.0f → index = round(raw * 4.0f)
        // AudioParameterChoice は getRawParameterValue がインデックス値をそのまま返す
        // 0=Off, 1=+X, 2=+Y, 3=-X, 4=-Y → * 4.0f は不要
        auto readModSrc = [&](const juce::String& paramId) -> int {
            return juce::jlimit(0, 4, juce::roundToInt(apvts.getRawParameterValue(paramId)->load()));
        };

        auto getFilterParams = [&](const juce::String& s, int /*idx*/) -> std::pair<float, float>
            {
                // 0=Off, 1=+X(cM[0]), 2=+Y(cM[1]), 3=-X(cM[2]), 4=-Y(cM[3])
                const int cutSrc = readModSrc("lfoCutSrc" + s);
                const int resSrc = readModSrc("lfoResSrc" + s);
                const bool lfoCutOn = cutSrc > 0;
                const bool lfoResOn = resSrc > 0;
                const int  cutModIdx = cutSrc > 0 ? cutSrc - 1 : 0;  // cM の添字
                const int  resModIdx = resSrc > 0 ? resSrc - 1 : 0;

                float baseCutoff, baseRes;
                // Morphモード: 個別スライダーから取得
                baseCutoff = apvts.getRawParameterValue("cutoff" + s)->load();
                baseRes = apvts.getRawParameterValue("res" + s)->load();

                float fc  = lfoCutOn ? MorphEngine::applyFrequencyMod(baseCutoff, cM[cutModIdx]) : baseCutoff;
                float res = lfoResOn ? MorphEngine::applyResonanceMod(baseRes,    rM[resModIdx]) : baseRes;

                return { juce::jlimit(20.0f, 20000.0f, fc), juce::jlimit(0.1f, 10.0f, res) };
            };

        // ===== TptFilter パラメータ更新 =====
        auto updateTpt = [&](TptFilter& f, const juce::String& s, int idx)
            {
                auto [fc, res] = getFilterParams(s, idx);
                f.setModel(juce::roundToInt(apvts.getRawParameterValue("model" + s)->load()));
                f.setCutoff(fc);
                f.setResonance(res);
                f.setType (juce::roundToInt(apvts.getRawParameterValue("type"  + s)->load()));
                f.setSlope(juce::roundToInt(apvts.getRawParameterValue("slope" + s)->load()));
            };

        updateTpt(filterA, "A", 0);
        updateTpt(filterB, "B", 1);
        updateTpt(filterC, "C", 2);
        updateTpt(filterD, "D", 3);

        // ===== Envelope Follower (Filter A にのみ適用) =====
        if (apvts.getRawParameterValue("envFollowen")->load() > 0.5f)
        {
            // Attack/Release 係数を毎ブロック計算（パラメータ変更に追従）
            float attackMs = apvts.getRawParameterValue("envFollowattack")->load();
            float releaseMs = apvts.getRawParameterValue("envFollowrelease")->load();

            // 係数計算: exp(-dt / tau) where tau = ms / 1000
            attackCoeff = std::exp(-numSamples / (attackMs * 0.001f * (float)envFollowerSampleRate));
            releaseCoeff = std::exp(-numSamples / (releaseMs * 0.001f * (float)envFollowerSampleRate));

            // 入力ピーク値を計算（ブロック単位）
            float peakValue = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto* data = buffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    peakValue = std::max(peakValue, std::abs(data[i]));
            }
            peakValue = juce::jlimit(0.0f, 1.0f, peakValue);

            // Envelope を Attack/Release で更新
            if (peakValue > envelopeValue)
                envelopeValue = attackCoeff * envelopeValue + (1.0f - attackCoeff) * peakValue;
            else
                envelopeValue = releaseCoeff * envelopeValue + (1.0f - releaseCoeff) * peakValue;

            // Invert フラグを確認
            bool invert = apvts.getRawParameterValue("envFollowinvert")->load() > 0.5f;
            float envMod = invert ? (1.0f - envelopeValue) : envelopeValue;

            // FilterA の cutoff を変調
            float depthPercent = apvts.getRawParameterValue("envFollowdepth")->load() / 100.0f;
            float baseCutoff = apvts.getRawParameterValue("cutoffA")->load();

            // Envelope で周波数を変調（±4 octave range）
            float modulatedCutoff = baseCutoff * std::pow(2.0f, (envMod - 0.5f) * 8.0f * depthPercent);
            filterA.setCutoff(juce::jlimit(20.0f, 20000.0f, modulatedCutoff));
        }

        // ===== SIMD SVF パラメータ更新 =====
        // 【注意】FilterA_SVF_SIMD はスロープ（ステージ数）に非対応のため常に無効化。
        // CleanSVF (model==0) を含む全モデルを TptFilter 経由で処理することで
        // 12dB/24dB/48dB/96dB のスロープ選択が正しく機能する。
        juce::StringArray suffixes = { "A", "B", "C", "D" };
        for (int idx = 0; idx < 4; ++idx)
        {
            svfQuad.setEnabled(idx, false);
        }

        // ===== 有効フィルター =====
        bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f;
        bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
        bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f;
        bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;

        // ===== 有効フィルター数をカウント =====
        int enabledCount = (int)enA + (int)enB + (int)enC + (int)enD;

        // ===== wMix 計算 =====
        // Morphモード: morphBlend パラメータでブレンドアルゴリズムを切替
        std::array<float, 4> wMix;
        bool lfo0_isRand1 = ((int)apvts.getRawParameterValue("lfo1wave")->load() == 3)
            && (apvts.getRawParameterValue("lfo1en")->load() > 0.5f);

        if (lfo0_isRand1)
        {
            wMix = lfoEngine.getMod4(0);
        }
        else if (enabledCount == 1)
        {
            // 有効フィルターが1個のみ: Morph しない（常に100%）
            wMix = { enA ? 1.0f : 0.0f, enB ? 1.0f : 0.0f, enC ? 1.0f : 0.0f, enD ? 1.0f : 0.0f };
        }
        else
        {
            // 有効フィルターが2個以上: 通常の Morph 計算
            const int morphBlend = (int)apvts.getRawParameterValue("morphBlend")->load();
            switch (morphBlend)
            {
                case 1:  wMix = MorphEngine::computeLinearWMix    (posX, posY); break;
                case 2:  wMix = MorphEngine::computeSmoothstepWMix(posX, posY); break;
                case 3:  wMix = MorphEngine::computeRadialWMix    (posX, posY); break;
                default: wMix = MorphEngine::computeEqualPowerWMix(posX, posY); break;
            }
        }

        // ===== 改善: 有効フィルターのみで正規化（無効フィルターは 0 に設定） =====
        // 有効でないフィルターの重みを完全にゼロクリア（ノイズ防止）
        if (!enA) wMix[0] = 0.0f;
        if (!enB) wMix[1] = 0.0f;
        if (!enC) wMix[2] = 0.0f;
        if (!enD) wMix[3] = 0.0f;

        // 有効フィルターのみで正規化（パワー保存）
        float sumSq = 0.0f;
        if (enA) sumSq += wMix[0] * wMix[0];
        if (enB) sumSq += wMix[1] * wMix[1];
        if (enC) sumSq += wMix[2] * wMix[2];
        if (enD) sumSq += wMix[3] * wMix[3];

        if (sumSq > 1e-8f)
        {
            float norm = 1.0f / std::sqrt(sumSq);
            if (enA) wMix[0] *= norm;
            if (enB) wMix[1] *= norm;
            if (enC) wMix[2] *= norm;
            if (enD) wMix[3] *= norm;
        }

        int modelA = (int)apvts.getRawParameterValue("modelA")->load();
        int modelB = (int)apvts.getRawParameterValue("modelB")->load();
        int modelC = (int)apvts.getRawParameterValue("modelC")->load();
        int modelD = (int)apvts.getRawParameterValue("modelD")->load();

        // ===== フィルター処理 =====
        svfQuad.processBuffer(buffer, filterBuffers);

        auto procTptIfNeeded = [&](juce::AudioBuffer<float>& dst,
            TptFilter& tpt, int model, bool enabled)
            {
                // model==0 (CleanSVF) も TptFilter 経由で処理する。
                // これにより currentStages (1/2/4/8) が正しく適用され、
                // 12dB/24dB/48dB/96dB スロープが実際の音声に反映される。
                if (!enabled) { dst.clear(); return; }
                for (int ch = 0; ch < numChannels; ++ch)
                    dst.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                tpt.process(dst);
            };

        procTptIfNeeded(filterBuffers[0], filterA, modelA, enA);
        procTptIfNeeded(filterBuffers[1], filterB, modelB, enB);
        procTptIfNeeded(filterBuffers[2], filterC, modelC, enC);
        procTptIfNeeded(filterBuffers[3], filterD, modelD, enD);

        // ===== ABCD ミキシング（毎サンプル更新用に初期化）=====
        buffer.clear();
        // 【重要】毎サンプルループ内で wMix を更新するため、ここではミキシングしない
        // 代わりに毎サンプルループ内で加算する

        // ===== 毎サンプルミキシング + Dry/Wet フェーダー処理 =====
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            auto* fA = filterBuffers[0].getReadPointer(ch);
            auto* fB = filterBuffers[1].getReadPointer(ch);
            auto* fC = filterBuffers[2].getReadPointer(ch);
            auto* fD = filterBuffers[3].getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // ===== パラメータスムージング（毎サンプル更新） =====
                // Dry/Wet スムージング（0-1 range で統一）
                lastDryWet += smoothCoef * (currentDryWetNormalized - lastDryWet);

                // Master Gain スムージング（linear domain）
                lastMasterGainLinear += smoothCoef * (currentMasterGainLinear - lastMasterGainLinear);

                // Ceiling スムージング（linear domain）
                lastCeilingLinear += smoothCoef * (currentCeilingLinear - lastCeilingLinear);

                // Morph パラメータのスムージング
                float targetMorphX = apvts.getRawParameterValue("posX")->load();
                float targetMorphY = apvts.getRawParameterValue("posY")->load();
                lastMorphX += smoothCoef * (targetMorphX - lastMorphX);
                lastMorphY += smoothCoef * (targetMorphY - lastMorphY);

                // ===== 毎サンプルごとに wMix を再計算（Morph スムージング反映） =====
                std::array<float, 4> wMix_current;
                int morphBlendCurrent = (int)apvts.getRawParameterValue("morphBlend")->load();
                switch (morphBlendCurrent)
                {
                    case 1:  wMix_current = MorphEngine::computeLinearWMix    (lastMorphX, lastMorphY); break;
                    case 2:  wMix_current = MorphEngine::computeSmoothstepWMix(lastMorphX, lastMorphY); break;
                    case 3:  wMix_current = MorphEngine::computeRadialWMix    (lastMorphX, lastMorphY); break;
                    default: wMix_current = MorphEngine::computeEqualPowerWMix(lastMorphX, lastMorphY); break;
                }
                // 正規化（パワー保存） — 無効フィルターは 0 のままに
                float sumSq_current = 0.0f;
                if (enA) sumSq_current += wMix_current[0] * wMix_current[0];
                if (enB) sumSq_current += wMix_current[1] * wMix_current[1];
                if (enC) sumSq_current += wMix_current[2] * wMix_current[2];
                if (enD) sumSq_current += wMix_current[3] * wMix_current[3];
                if (sumSq_current > 1e-8f)
                {
                    float norm = 1.0f / std::sqrt(sumSq_current);
                    if (enA) wMix_current[0] *= norm;
                    if (enB) wMix_current[1] *= norm;
                    if (enC) wMix_current[2] *= norm;
                    if (enD) wMix_current[3] *= norm;
                }

                // ===== ABCD ミキシング（毎サンプル更新） =====
                float wet = 0.0f;
                if (enA) wet += fA[i] * wMix_current[0];
                if (enB) wet += fB[i] * wMix_current[1];
                if (enC) wet += fC[i] * wMix_current[2];
                if (enD) wet += fD[i] * wMix_current[3];

                // ===== LFO5 modulation スムージング =====
                float dryWetSmoothed = lastDryWet;  // デフォルト: Dry/Wet スライダー値
                if (lfo5Enabled)
                {
                    lastLfo5Mod += smoothCoef * (lfo5Mod - lastLfo5Mod);
                    dryWetSmoothed = lastLfo5Mod;  // LFO5 有効時: LFO5 モジュレーション値を使用
                }

                // ===== 正しい Equal Power Crossfade（sin/cos）=====
                // Equal Power: Power_dry + Power_wet = constant
                // 実装: dry_amp = cos(π/2 * w), wet_amp = sin(π/2 * w)
                // w=0（Dry）   → dry_amp=1.0, wet_amp=0.0
                // w=0.5（中央）→ dry_amp≈0.707, wet_amp≈0.707 (パワー保存)
                // w=1（Wet）   → dry_amp=0.0, wet_amp=1.0
                // 注: Denormal 安全。dryWetSmoothed は [0, 1] に正規化済み
                float w_rad = dryWetSmoothed * juce::MathConstants<float>::pi * 0.5f;  // [0, π/2]
                float dry_amp = std::cos(w_rad);
                float wet_amp = std::sin(w_rad);

                // Gain および Ceiling は既にスムージング済みの linear 値
                float gainLinear = lastMasterGainLinear;
                float ceilingLinear = lastCeilingLinear;

                float gained = (dry[i] * dry_amp + wet * wet_amp) * gainLinear;
                float absSignal = std::abs(gained);
                float targetGr = (absSignal > ceilingLinear) ? ceilingLinear / absSignal : 1.0f;

                if (targetGr < currentGainReduction[ch])
                    currentGainReduction[ch] = targetGr;
                else
                    currentGainReduction[ch] += releaseCoef * (targetGr - currentGainReduction[ch]);

                out[i] = gained * currentGainReduction[ch];
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
    bool QuadMorphFilterAudioProcessor::isBusesLayoutSupported(const BusesLayout&) const { return true; }
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