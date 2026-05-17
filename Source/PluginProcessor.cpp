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
    juce::StringArray bounds = { "Clip", "Bounce", "Wrap" };
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
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "bound",    1 }, lfoNames[i] + " Boundary", bounds, 0));
    }

    juce::StringArray suffixes = { "A", "B", "C", "D" };
    juce::StringArray models = {
        "Clean SVF", "Moog Ladder", "Diode (TB-303)", "SEM (Oberheim)", "Bitcrush / SRR",
        "Formant (Vowel)", "Comb Filter", "MS-20 (Screaming)", "All-Pass Phaser", "Wavefolder",
        "Reverb (Metallic)", "Kilo All-Pass",
        "Prophet (Curtis)", "SSM 2040", "CS-80 (Yamaha)", "Jupiter (Roland)", "EDP Wasp (CMOS)",
        "Butterworth (Flat)", "Chebyshev (Ripple)", "Bessel (Phase)", "Elliptic (Notch)",
        "Vactrol LPG", "Modal Resonator", "Waveguide Mesh", "Bode Freq Shifter",
        "Z-Plane (2D Morph)", "Phased Array", "Nyquist Anti-alias"
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

// ==========================================
// prepareToPlay
// ==========================================
void QuadMorphFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lfoEngine.prepare(sampleRate);

    svfA.prepare(sampleRate, samplesPerBlock);
    svfB.prepare(sampleRate, samplesPerBlock);
    svfC.prepare(sampleRate, samplesPerBlock);
    svfD.prepare(sampleRate, samplesPerBlock);

    filterA.prepare(sampleRate, samplesPerBlock, 2);
    filterB.prepare(sampleRate, samplesPerBlock, 2);
    filterC.prepare(sampleRate, samplesPerBlock, 2);
    filterD.prepare(sampleRate, samplesPerBlock, 2);

    for (auto& buf : filterBuffers)
        buf.setSize(2, samplesPerBlock, false, false, true);

    dryBuffer.setSize(2, samplesPerBlock, false, false, true);

    currentGainReduction[0] = 1.0f;
    currentGainReduction[1] = 1.0f;
}

void QuadMorphFilterAudioProcessor::releaseResources() {}

// ==========================================
// processBlock（薄いオーケストレーション層）
// ==========================================
void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int   numSamples = buffer.getNumSamples();
    const auto  sampleRate = getSampleRate();
    const int   numChannels = buffer.getNumChannels();
    const float dt = numSamples / (float)sampleRate;

    // ドライ信号を保存
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // BPM 取得
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm().hasValue())
                bpm = *(pos->getBpm());

    float baseX = apvts.getRawParameterValue("posX")->load();
    float baseY = apvts.getRawParameterValue("posY")->load();

    // ===== LfoEngine に録音状態を渡して LFO 処理 =====
    LfoEngine::RecordingContext recCtx{
        recBuffer,
        recLength,
        isWaitingForRecord,
        isRecordingDrag,
        currentRecX,
        currentRecY
    };
    lfoEngine.process(dt, bpm, baseX, baseY, apvts, recCtx);

    // ===== MorphEngine でモジュレーション値を計算 =====
    bool lfo1_isRand1 = ((int)apvts.getRawParameterValue("lfo2wave")->load() == 3)
        && (apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    bool lfo2_isRand1 = ((int)apvts.getRawParameterValue("lfo3wave")->load() == 3)
        && (apvts.getRawParameterValue("lfo3en")->load() > 0.5f);

    // cM: カットオフモジュレーション量 (4フィルター分)
    auto cM = MorphEngine::computeModulation(
        lfoEngine.getPosition(1), lfoEngine.getMod4(1), lfo1_isRand1);

    // rM: レゾナンスモジュレーション量 (4フィルター分)
    auto rM = MorphEngine::computeModulation(
        lfoEngine.getPosition(2), lfoEngine.getMod4(2), lfo2_isRand1);

    // ===== TptFilter パラメータ更新 =====
    auto updateTpt = [&](TptFilter& f, const juce::String& s, int idx)
        {
            float fc = MorphEngine::applyFrequencyMod(
                apvts.getRawParameterValue("cutoff" + s)->load(), cM[idx]);
            float res = MorphEngine::applyResonanceMod(
                apvts.getRawParameterValue("res" + s)->load(), rM[idx]);

            f.setModel((int)apvts.getRawParameterValue("model" + s)->load());
            f.setCutoff(juce::jlimit(20.0f, 20000.0f, fc));
            f.setResonance(juce::jlimit(0.1f, 10.0f, res));
            f.setType((int)apvts.getRawParameterValue("type" + s)->load());
            f.setSlope((int)apvts.getRawParameterValue("slope" + s)->load());
        };

    updateTpt(filterA, "A", 0);
    updateTpt(filterB, "B", 1);
    updateTpt(filterC, "C", 2);
    updateTpt(filterD, "D", 3);

    // ===== FilterA_SVF パラメータ更新 =====
    auto updateSvf = [&](FilterA_SVF& svf, const juce::String& s, int idx)
        {
            if ((int)apvts.getRawParameterValue("model" + s)->load() != 0)
                return; // Clean SVF 以外はスキップ

            float fc = MorphEngine::applyFrequencyMod(
                apvts.getRawParameterValue("cutoff" + s)->load(), cM[idx]);
            float res = MorphEngine::applyResonanceMod(
                apvts.getRawParameterValue("res" + s)->load(), rM[idx]);

            svf.setCutoff(juce::jlimit(20.0f, 20000.0f, fc));
            svf.setResonance(juce::jlimit(0.1f, 10.0f, res));
            svf.setType((int)apvts.getRawParameterValue("type" + s)->load());
        };

    updateSvf(svfA, "A", 0);
    updateSvf(svfB, "B", 1);
    updateSvf(svfC, "C", 2);
    updateSvf(svfD, "D", 3);

    // ===== 等パワーwMix 計算 =====
    std::array<float, 4> wMix;
    bool lfo0_isRand1 = ((int)apvts.getRawParameterValue("lfo1wave")->load() == 3)
        && (apvts.getRawParameterValue("lfo1en")->load() > 0.5f);

    if (lfo0_isRand1)
        wMix = lfoEngine.getMod4(0);
    else
        wMix = MorphEngine::computeEqualPowerWMix(
            lfoEngine.getPosition(0).x,
            lfoEngine.getPosition(0).y);

    // ===== フィルター処理 =====
    bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;

    // model == 0 → SVF, それ以外 → TptFilter
    auto procUnified = [&](juce::AudioBuffer<float>& dst,
        TptFilter& tpt,
        FilterA_SVF& svf,
        const juce::String& s,
        bool enabled)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                dst.copyFrom(ch, 0, buffer, ch, 0, numSamples);

            if (!enabled) { dst.clear(); return; }

            if ((int)apvts.getRawParameterValue("model" + s)->load() == 0)
                svf.process(dst);
            else
                tpt.process(dst);
        };

    procUnified(filterBuffers[0], filterA, svfA, "A", enA);
    procUnified(filterBuffers[1], filterB, svfB, "B", enB);
    procUnified(filterBuffers[2], filterC, svfC, "C", enC);
    procUnified(filterBuffers[3], filterD, svfD, "D", enD);

    // ===== ABCD ミキシング =====
    buffer.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (enA) buffer.addFrom(ch, 0, filterBuffers[0], ch, 0, numSamples, wMix[0]);
        if (enB) buffer.addFrom(ch, 0, filterBuffers[1], ch, 0, numSamples, wMix[1]);
        if (enC) buffer.addFrom(ch, 0, filterBuffers[2], ch, 0, numSamples, wMix[2]);
        if (enD) buffer.addFrom(ch, 0, filterBuffers[3], ch, 0, numSamples, wMix[3]);
    }

    // ===== Dry/Wet + マスターゲイン + リミッター =====
    const float mixRatio = apvts.getRawParameterValue("dryWet")->load() / 100.0f;
    const float gainLinear = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("masterGain")->load());
    const float ceilingLinear = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("limiterCeiling")->load());
    const float releaseCoef = 1.0f - std::exp(-1.0f / (0.050f * sampleRate));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float gained = (dry[i] * (1.0f - mixRatio) + out[i] * mixRatio) * gainLinear;

            float absSignal = std::abs(gained);
            float targetGr = (absSignal > ceilingLinear)
                ? ceilingLinear / absSignal : 1.0f;

            if (targetGr < currentGainReduction[ch])
                currentGainReduction[ch] = targetGr;
            else
                currentGainReduction[ch] += releaseCoef * (targetGr - currentGainReduction[ch]);

            out[i] = gained * currentGainReduction[ch];
        }
    }
}

// ==========================================
// 残りのメソッド（変更なし）
// ==========================================
const juce::String QuadMorphFilterAudioProcessor::getName() const { return "Quad-Morph Filter"; }
bool QuadMorphFilterAudioProcessor::acceptsMidi()  const { return false; }
bool QuadMorphFilterAudioProcessor::producesMidi() const { return false; }
bool QuadMorphFilterAudioProcessor::isMidiEffect() const { return false; }
double QuadMorphFilterAudioProcessor::getTailLengthSeconds() const { return 0.0; }
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