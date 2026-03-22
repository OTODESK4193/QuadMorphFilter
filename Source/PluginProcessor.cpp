#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

juce::AudioProcessorValueTreeState::ParameterLayout QuadMorphFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // XY PAD 座標 (0.0 ~ 1.0)
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "posX", 1 }, "X Position", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "posY", 1 }, "Y Position", 0.0f, 1.0f, 0.5f));

    // LFO設定
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "lfoRate", 1 }, "LFO Rate", 0.1f, 10.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "lfoAmount", 1 }, "LFO Amount", 0.0f, 1.0f, 0.0f));

    // 4つのコーナー設定 (A, B, C, D)
    juce::StringArray suffixes = { "A", "B", "C", "D" };
    for (const auto& s : suffixes)
    {
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

    // 1. 各フィルターの最新パラメータを適用
    auto updateF = [&](TptFilter& f, juce::String s) {
        f.setCutoff(apvts.getRawParameterValue("cutoff" + s)->load());
        f.setResonance(apvts.getRawParameterValue("res" + s)->load());
        f.setType((int)apvts.getRawParameterValue("type" + s)->load());
        };
    updateF(filterA, "A"); updateF(filterB, "B"); updateF(filterC, "C"); updateF(filterD, "D");

    // 2. XY座標の取得とLFO適用
    float x = apvts.getRawParameterValue("posX")->load();
    float y = apvts.getRawParameterValue("posY")->load();
    float lfoAmt = apvts.getRawParameterValue("lfoAmount")->load();
    float lfoRate = apvts.getRawParameterValue("lfoRate")->load();

    // LFOによる円運動 (簡易実装)
    if (lfoAmt > 0.0f) {
        float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
        x += std::sin(time * lfoRate) * lfoAmt * 0.5f;
        y += std::cos(time * lfoRate) * lfoAmt * 0.5f;
        x = juce::jlimit(0.0f, 1.0f, x);
        y = juce::jlimit(0.0f, 1.0f, y);
    }

    // 3. 重みの計算 (Bilinear)
    float wA = (1.0f - x) * (1.0f - y);
    float wB = x * (1.0f - y);
    float wC = (1.0f - x) * y;
    float wD = x * y;

    // 4. オーディオ処理: 各フィルターの出力をミックス
    juce::AudioBuffer<float> bufferA, bufferB, bufferC, bufferD;
    bufferA.makeCopyOf(buffer); bufferB.makeCopyOf(buffer);
    bufferC.makeCopyOf(buffer); bufferD.makeCopyOf(buffer);

    filterA.process(bufferA); filterB.process(bufferB);
    filterC.process(bufferC); filterD.process(bufferD);

    buffer.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        buffer.addFrom(ch, 0, bufferA, ch, 0, numSamples, wA);
        buffer.addFrom(ch, 0, bufferB, ch, 0, numSamples, wB);
        buffer.addFrom(ch, 0, bufferC, ch, 0, numSamples, wC);
        buffer.addFrom(ch, 0, bufferD, ch, 0, numSamples, wD);
    }
}

// 固定情報の定義 (省略)
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