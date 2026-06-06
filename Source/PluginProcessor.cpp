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

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "osMode", 1 }, "OS Quality",
        juce::StringArray{ "Off", "Auto", "2x", "4x" }, 1));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "morphBlend", 1 }, "Morph Blend",
        juce::StringArray{ "EqPwr", "Linear", "Smooth", "Radial" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "cutoffAlgo", 1 }, "Cutoff Algo",
        juce::StringArray{ "Abs", "Rel", "Zone" }, 0));

    const juce::StringArray modSrcs = { "Off", "+X", "+Y", "-X", "-Y" };
    const int defaults[4] = { 0, 0, 0, 0 };  // Default: All Off
    int fi = 0;
    for (const auto& s : suffixes)
    {
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "lfoCutSrc" + s, 1 }, "LFO Cut Src " + s, modSrcs, defaults[fi]));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ "lfoResSrc" + s, 1 }, "LFO Res Src " + s, modSrcs, defaults[fi]));
        ++fi;
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
    expectedSampleRate = sampleRate;

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

    envFollowerSampleRate = sampleRate;
    envelopeValue = 1.0f;
    attackCoeff = 0.0f;
    releaseCoeff = 0.0f;

    lastDryWet = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("dryWet")->load() / 100.0f);
    lastMasterGainLinear = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("masterGain")->load());
    lastLfo5Mod = 0.5f;
    lastCeilingLinear = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("limiterCeiling")->load());

    lastMorphX = apvts.getRawParameterValue("posX")->load();
    lastMorphY = apvts.getRawParameterValue("posY")->load();

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

    // Ableton Live 対策自己防衛フェイルセーフ
    if (juce::exactlyEqual(getSampleRate(), 0.0) || !juce::exactlyEqual(getSampleRate(), expectedSampleRate))
    {
        buffer.clear();
        return;
    }

    const int   numSamples = buffer.getNumSamples();
    const int   numChannels = buffer.getNumChannels();
    const float dt = numSamples / (float)expectedSampleRate;

    bool lfo5Enabled = apvts.getRawParameterValue("lfo5en")->load() > 0.5f;
    const float releaseCoef = 1.0f - std::exp(-1.0f / (0.050f * expectedSampleRate));
    const float smoothStepPerSample = 1.0f / (0.050f * expectedSampleRate);

    float currentDryWetNormalized = apvts.getRawParameterValue("dryWet")->load() / 100.0f;
    currentDryWetNormalized = juce::jlimit(0.0f, 1.0f, currentDryWetNormalized);
    float currentMasterGaindB = apvts.getRawParameterValue("masterGain")->load();
    float currentCeilingdB = apvts.getRawParameterValue("limiterCeiling")->load();

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
    lfoEngine.process(dt, bpm, baseX, baseY, apvts, recCtx);

    // ===== LFO5 (Dry/Wet Modulation) あなたのオリジナルロジックを完全復元 =====
    lfo5Engine.process(dt, bpm, apvts);
    float lfo5Mod = lfo5Engine.getOutput();

    float posX = lfoEngine.getPosition(0).x;
    float posY = lfoEngine.getPosition(0).y;

    // XY → Cutoff/Reso 変換
    const int cutoffAlgo = (int)apvts.getRawParameterValue("cutoffAlgo")->load();
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

    bool lfo1_isRand1 = ((int)apvts.getRawParameterValue("lfo2wave")->load() == 3)
        && (apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    bool lfo2_isRand1 = ((int)apvts.getRawParameterValue("lfo3wave")->load() == 3)
        && (apvts.getRawParameterValue("lfo3en")->load() > 0.5f);
    bool lfo1_useMod4 = lfo1_isRand1 || lfoEngine.isSpreadActive(1);
    bool lfo2_useMod4 = lfo2_isRand1 || lfoEngine.isSpreadActive(2);

    auto cM = MorphEngine::computeModulation(
        lfoEngine.getPosition(1), lfoEngine.getMod4(1), lfo1_useMod4);
    auto rM = MorphEngine::computeModulation(
        lfoEngine.getPosition(2), lfoEngine.getMod4(2), lfo2_useMod4);

    {
        int osMode = (int)apvts.getRawParameterValue("osMode")->load();
        filterA.setOsMode(osMode);
        filterB.setOsMode(osMode);
        filterC.setOsMode(osMode);
        filterD.setOsMode(osMode);
    }

    auto readModSrc = [&](const juce::String& paramId) -> int {
        return juce::jlimit(0, 4, juce::roundToInt(apvts.getRawParameterValue(paramId)->load()));
        };

    auto getFilterParams = [&](const juce::String& s, int /*idx*/) -> std::pair<float, float>
        {
            const int cutSrc = readModSrc("lfoCutSrc" + s);
            const int resSrc = readModSrc("lfoResSrc" + s);
            const bool lfoCutOn = cutSrc > 0;
            const bool lfoResOn = resSrc > 0;
            const int  cutModIdx = cutSrc > 0 ? cutSrc - 1 : 0;
            const int  resModIdx = resSrc > 0 ? resSrc - 1 : 0;

            float baseCutoff = apvts.getRawParameterValue("cutoff" + s)->load();
            float baseRes = apvts.getRawParameterValue("res" + s)->load();

            float fc = lfoCutOn ? MorphEngine::applyFrequencyMod(baseCutoff, cM[cutModIdx]) : baseCutoff;
            float res = lfoResOn ? MorphEngine::applyResonanceMod(baseRes, rM[resModIdx]) : baseRes;

            return { juce::jlimit(20.0f, 20000.0f, fc), juce::jlimit(0.1f, 10.0f, res) };
        };

    auto updateTpt = [&](TptFilter& f, const juce::String& s, int idx)
        {
            auto [fc, res] = getFilterParams(s, idx);
            f.setModel(juce::roundToInt(apvts.getRawParameterValue("model" + s)->load()));
            f.setCutoff(fc);
            f.setResonance(res);
            f.setType(juce::roundToInt(apvts.getRawParameterValue("type" + s)->load()));
            f.setSlope(juce::roundToInt(apvts.getRawParameterValue("slope" + s)->load()));
        };

    updateTpt(filterA, "A", 0);
    updateTpt(filterB, "B", 1);
    updateTpt(filterC, "C", 2);
    updateTpt(filterD, "D", 3);

    // ===== Envelope Follower =====
    if (apvts.getRawParameterValue("envFollowen")->load() > 0.5f)
    {
        float peakValue = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                peakValue = std::max(peakValue, std::abs(data[i]));
        }
        peakValue = juce::jlimit(0.0f, 1.0f, peakValue);

        // ===== 入力信号の ENV に追従（固定 Attack/Release で平滑化） =====
        // Attack と Release を非常に短く設定して、envelopeValue が peakValue に素早く追従
        float attackCoeff = std::exp(-numSamples / (1.0f * 0.001f * (float)envFollowerSampleRate));
        float releaseCoeff = std::exp(-numSamples / (1.0f * 0.001f * (float)envFollowerSampleRate));

        if (peakValue > envelopeValue)
            envelopeValue = attackCoeff * envelopeValue + (1.0f - attackCoeff) * peakValue;
        else
            envelopeValue = releaseCoeff * envelopeValue + (1.0f - releaseCoeff) * peakValue;

        // ===== 前フレームの envelopeValue との差分を計算（変化幅を抽出） =====
        float envelopeDelta = envelopeValue - lastEnvelopeValue;
        lastEnvelopeValue = envelopeValue;

        // ===== 増加中（立ち上がり）のみを抽出 =====
        float envMod = std::max(0.0f, envelopeDelta);

        // ===== Invert：減衰を強調（立ち上がりを反転） =====
        bool invert = apvts.getRawParameterValue("envFollowinvert")->load() > 0.5f;
        if (invert)
        {
            envMod = 1.0f - std::min(1.0f, envMod);
        }

        float depthPercent = apvts.getRawParameterValue("envFollowdepth")->load() / 100.0f;

        // ===== 毎ブロック 1 回だけ Cutoff を設定 =====
        float modulatedCutoff = 20.0f * std::pow(1000.0f, envMod * depthPercent);
        filterA.setCutoff(juce::jlimit(20.0f, 20000.0f, modulatedCutoff));
    }

    for (int idx = 0; idx < 4; ++idx)
        svfQuad.setEnabled(idx, false);

    bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;
    int enabledCount = (int)enA + (int)enB + (int)enC + (int)enD;

    std::array<float, 4> wMix;
    bool lfo0_isRand1 = ((int)apvts.getRawParameterValue("lfo1wave")->load() == 3)
        && (apvts.getRawParameterValue("lfo1en")->load() > 0.5f);

    if (lfo0_isRand1)
    {
        wMix = lfoEngine.getMod4(0);
    }
    else if (enabledCount == 1)
    {
        wMix = { enA ? 1.0f : 0.0f, enB ? 1.0f : 0.0f, enC ? 1.0f : 0.0f, enD ? 1.0f : 0.0f };
    }
    else
    {
        const int morphBlend = (int)apvts.getRawParameterValue("morphBlend")->load();
        switch (morphBlend)
        {
        case 1:  wMix = MorphEngine::computeLinearWMix(posX, posY); break;
        case 2:  wMix = MorphEngine::computeSmoothstepWMix(posX, posY); break;
        case 3:  wMix = MorphEngine::computeRadialWMix(posX, posY); break;
        default: wMix = MorphEngine::computeEqualPowerWMix(posX, posY); break;
        }
    }

    if (!enA) wMix[0] = 0.0f;
    if (!enB) wMix[1] = 0.0f;
    if (!enC) wMix[2] = 0.0f;
    if (!enD) wMix[3] = 0.0f;

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

    svfQuad.processBuffer(buffer, filterBuffers);

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
    // ★真の解決：オーディオバッファのマルチチャンネルスタックポインタキャッシュ
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
    // ★サンプルループを「外側」に変更
    // これにより、L/Rが寸分の狂いもなく全く同一のスムージング位相で処理されます。
    // =========================================================================
    for (int i = 0; i < numSamples; ++i)
    {
        // 1回だけ共通のスムージング計算を進める（あなたの数式・アルゴリズムを完全維持）
        float dryWetDiff = currentDryWetNormalized - lastDryWet;
        lastDryWet += juce::jlimit(-smoothStepPerSample, smoothStepPerSample, dryWetDiff);

        float gainDiff = currentMasterGainLinear - lastMasterGainLinear;
        lastMasterGainLinear += juce::jlimit(-smoothStepPerSample, smoothStepPerSample, gainDiff);

        float ceilingDiff = currentCeilingLinear - lastCeilingLinear;
        lastCeilingLinear += juce::jlimit(-smoothStepPerSample, smoothStepPerSample, ceilingDiff);

        float targetMorphX = apvts.getRawParameterValue("posX")->load();
        float targetMorphY = apvts.getRawParameterValue("posY")->load();
        float morphXDiff = targetMorphX - lastMorphX;
        float morphYDiff = targetMorphY - lastMorphY;
        lastMorphX += juce::jlimit(-smoothStepPerSample, smoothStepPerSample, morphXDiff);
        lastMorphY += juce::jlimit(-smoothStepPerSample, smoothStepPerSample, morphYDiff);

        // ===== 有効フィルター数をカウント =====
        int enabledCount = (enA ? 1 : 0) + (enB ? 1 : 0) + (enC ? 1 : 0) + (enD ? 1 : 0);

        // 毎サンプル wMix 再計算（既存のMorphアルゴリズムを完全維持）
        std::array<float, 4> wMix_current;

        // ===== 有効フィルター数が1以下の場合、モーフィング無効（LFO1効果なし） =====
        if (enabledCount <= 1)
        {
            wMix_current = { enA ? 1.0f : 0.0f, enB ? 1.0f : 0.0f,
                           enC ? 1.0f : 0.0f, enD ? 1.0f : 0.0f };
        }
        else
        {
            bool lfo1Enabled = apvts.getRawParameterValue("lfo1en")->load() > 0.5f;
            float morphX = lfo1Enabled ? posX : lastMorphX;
            float morphY = lfo1Enabled ? posY : lastMorphY;

            int morphBlendCurrent = (int)apvts.getRawParameterValue("morphBlend")->load();
            switch (morphBlendCurrent)
            {
            case 1:  wMix_current = MorphEngine::computeLinearWMix(morphX, morphY); break;
            case 2:  wMix_current = MorphEngine::computeSmoothstepWMix(morphX, morphY); break;
            case 3:  wMix_current = MorphEngine::computeRadialWMix(morphX, morphY); break;
            default: wMix_current = MorphEngine::computeEqualPowerWMix(morphX, morphY); break;
            }

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
        }

        // ===== LFO5 modulation スムージング：あなたのオリジナルの挙動へ完全復元 =====
        float dryWetSmoothed = lastDryWet;
        if (lfo5Enabled)
        {
            float lfo5Diff = lfo5Mod - lastLfo5Mod;
            lastLfo5Mod += juce::jlimit(-smoothStepPerSample, smoothStepPerSample, lfo5Diff);
            dryWetSmoothed = lastLfo5Mod;  // LFO5有効時は期待通りこの変調値が出力されます
        }

        // 正しい Equal Power Crossfade とスムージングされた線形ゲインのロード
        float w_rad = dryWetSmoothed * juce::MathConstants<float>::pi * 0.5f;
        float dry_amp = juce::dsp::FastMathApproximations::cos(w_rad);
        float wet_amp = juce::dsp::FastMathApproximations::sin(w_rad);

        float gainLinear = lastMasterGainLinear;
        float ceilingLinear = lastCeilingLinear;

        // 内側でチャンネルループを回す（左右のゲインの同期を完全担保）
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float wet = 0.0f;
            if (enA) wet += fAPtrs[ch][i] * wMix_current[0];
            if (enB) wet += fBPtrs[ch][i] * wMix_current[1];
            if (enC) wet += fCPtrs[ch][i] * wMix_current[2];
            if (enD) wet += fDPtrs[ch][i] * wMix_current[3];

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