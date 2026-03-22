#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout QuadMorphFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // XY PAD パラメータ
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "posX", 1 }, "X Position", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "posY", 1 }, "Y Position", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "lfoRate", 1 }, "LFO Rate", 0.1f, 10.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "lfoAmount", 1 }, "LFO Amount", 0.0f, 1.0f, 0.0f));

    juce::StringArray suffixes = { "A", "B", "C", "D" };
    for (const auto& s : suffixes) {
        // ON/OFFスイッチ
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "enable" + s, 1 }, "Enable " + s, true));

        auto range = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cutoff" + s, 1 }, "Cutoff " + s, range, 1000.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "res" + s, 1 }, "Res " + s, 0.1f, 10.0f, 0.707f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type" + s, 1 }, "Type " + s, juce::StringArray{ "LP", "BP", "HP", "Notch" }, 0));
    }
    return layout;
}

QuadMorphFilterAudioProcessor::QuadMorphFilterAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout()) {
}

QuadMorphFilterAudioProcessor::~QuadMorphFilterAudioProcessor() {}

void QuadMorphFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    filterA.prepare(sampleRate, samplesPerBlock, 2);
    filterB.prepare(sampleRate, samplesPerBlock, 2);
    filterC.prepare(sampleRate, samplesPerBlock, 2);
    filterD.prepare(sampleRate, samplesPerBlock, 2);
}

void QuadMorphFilterAudioProcessor::releaseResources() {}

void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();

    // 各フィルターのパラメータ更新
    auto updateF = [&](TptFilter& f, juce::String s) {
        f.setCutoff(apvts.getRawParameterValue("cutoff" + s)->load());
        f.setResonance(apvts.getRawParameterValue("res" + s)->load());
        f.setType((int)apvts.getRawParameterValue("type" + s)->load());
        };
    updateF(filterA, "A"); updateF(filterB, "B"); updateF(filterC, "C"); updateF(filterD, "D");

    // LFO計算
    float baseX = apvts.getRawParameterValue("posX")->load();
    float baseY = apvts.getRawParameterValue("posY")->load();
    float lfoAmt = apvts.getRawParameterValue("lfoAmount")->load();
    float lfoRate = apvts.getRawParameterValue("lfoRate")->load();

    lfoPhase += (float)(lfoRate * juce::MathConstants<double>::twoPi / getSampleRate() * numSamples);
    if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

    // 現在の座標を決定（LFO反映済み）
    currentX = juce::jlimit(0.0f, 1.0f, baseX + std::sin(lfoPhase) * lfoAmt * 0.4f);
    currentY = juce::jlimit(0.0f, 1.0f, baseY + std::cos(lfoPhase) * lfoAmt * 0.4f);

    // 重みの計算
    float wA = (1.0f - currentX) * (1.0f - currentY);
    float wB = currentX * (1.0f - currentY);
    float wC = (1.0f - currentX) * currentY;
    float wD = currentX * currentY;

    // オーディオ処理
    juce::AudioBuffer<float> bA, bB, bC, bD;

    // Enable状態をロードし、無効なフィルターはクリア
    auto processFilter = [&](juce::AudioBuffer<float>& target, TptFilter& f, bool enabled) {
        target.makeCopyOf(buffer);
        if (enabled) f.process(target); else target.clear();
        };

    bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;

    processFilter(bA, filterA, enA);
    processFilter(bB, filterB, enB);
    processFilter(bC, filterC, enC);
    processFilter(bD, filterD, enD);

    // 最終ミックス
    buffer.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        if (enA) buffer.addFrom(ch, 0, bA, ch, 0, numSamples, wA);
        if (enB) buffer.addFrom(ch, 0, bB, ch, 0, numSamples, wB);
        if (enC) buffer.addFrom(ch, 0, bC, ch, 0, numSamples, wC);
        if (enD) buffer.addFrom(ch, 0, bD, ch, 0, numSamples, wD);
    }
}

// 他の関数は変更なし
const juce::String QuadMorphFilterAudioProcessor::getName() const { return "Quad-Morph Filter"; }
bool QuadMorphFilterAudioProcessor::acceptsMidi() const { return false; }
bool QuadMorphFilterAudioProcessor::producesMidi() const { return false; }
bool QuadMorphFilterAudioProcessor::isMidiEffect() const { return false; }
double QuadMorphFilterAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int QuadMorphFilterAudioProcessor::getNumPrograms() { return 1; }
int QuadMorphFilterAudioProcessor::getCurrentProgram() { return 0; }
void QuadMorphFilterAudioProcessor::setCurrentProgram(int index) {}
const juce::String QuadMorphFilterAudioProcessor::getProgramName(int index) { return {}; }
void QuadMorphFilterAudioProcessor::changeProgramName(int index, const juce::String& newName) {}
bool QuadMorphFilterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const { return true; }
bool QuadMorphFilterAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* QuadMorphFilterAudioProcessor::createEditor() { return new QuadMorphFilterAudioProcessorEditor(*this); }
void QuadMorphFilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
void QuadMorphFilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuadMorphFilterAudioProcessor(); }