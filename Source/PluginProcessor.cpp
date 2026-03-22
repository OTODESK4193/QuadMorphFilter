// ==========================================
// PluginProcessor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout QuadMorphFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "posX", 1 }, "Base X", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "posY", 1 }, "Base Y", 0.0f, 1.0f, 0.5f));

    juce::StringArray lfoNames = { "Morph", "Cutoff", "Reso" };
    juce::StringArray waveTypes = { "Sine", "SAW", "Pulse", "Random", "Noise" };
    juce::StringArray syncRates = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                                   "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
                                   "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T" };

    for (int i = 0; i < 3; ++i) {
        juce::String id = "lfo" + juce::String(i + 1);
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, lfoNames[i] + " Enable", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave", 1 }, lfoNames[i] + " Wave", waveTypes, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "step", 1 }, lfoNames[i] + " Step Mode", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "sync", 1 }, lfoNames[i] + " Sync", true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "rateSync", 1 }, lfoNames[i] + " Rate Sync", syncRates, 2));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, lfoNames[i] + " Rate Free", 0.1f, 20.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "amt", 1 }, lfoNames[i] + " Amount", 0.0f, 1.0f, 0.2f));
    }

    juce::StringArray suffixes = { "A", "B", "C", "D" };
    for (const auto& s : suffixes) {
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "enable" + s, 1 }, "Enable " + s, true));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cutoff" + s, 1 }, "Cutoff " + s, juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f));
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

    // 【修正】オーディオパス内での動的メモリ確保を防ぐため、ここでサイズを確定する
    for (auto& buf : filterBuffers) {
        buf.setSize(2, samplesPerBlock, false, false, true);
    }
}

void QuadMorphFilterAudioProcessor::releaseResources() {}

float QuadMorphFilterAudioProcessor::generateWave(float phase, int type) {
    switch (type) {
    case 0: return std::sin(phase);
    case 1: return 1.0f - (phase / juce::MathConstants<float>::pi);
    case 2: return (phase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
    case 3: return (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
    case 4: return (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
    default: return 0.0f;
    }
}

float QuadMorphFilterAudioProcessor::getSyncTime(int selection, double bpm) {
    double beatLen = 60.0 / bpm;
    double base = 0.0;
    if (selection < 7) base = std::pow(2.0, 2 - selection);
    else if (selection < 13) base = std::pow(2.0, 2 - (selection - 7)) * 1.5;
    else base = std::pow(2.0, 2 - (selection - 13)) * (2.0 / 3.0);
    return (float)(beatLen * base);
}

void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();
    auto sampleRate = getSampleRate();
    auto numChannels = buffer.getNumChannels();

    double bpm = 120.0;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (pos->getBpm().hasValue()) bpm = *(pos->getBpm());
        }
    }

    float baseX = apvts.getRawParameterValue("posX")->load();
    float baseY = apvts.getRawParameterValue("posY")->load();

    for (int i = 0; i < 3; ++i) {
        juce::String id = "lfo" + juce::String(i + 1);
        bool enabled = apvts.getRawParameterValue(id + "en")->load() > 0.5f;
        float amt = apvts.getRawParameterValue(id + "amt")->load();
        int wave = (int)apvts.getRawParameterValue(id + "wave")->load();
        bool step = apvts.getRawParameterValue(id + "step")->load() > 0.5f;
        bool sync = apvts.getRawParameterValue(id + "sync")->load() > 0.5f;

        if (enabled) {
            float rate = sync ? (1.0f / getSyncTime((int)apvts.getRawParameterValue(id + "rateSync")->load(), bpm))
                : apvts.getRawParameterValue(id + "rateFree")->load();

            lfoStates[i].phase += (juce::MathConstants<float>::twoPi * rate * numSamples / sampleRate);
            if (lfoStates[i].phase >= juce::MathConstants<float>::twoPi) {
                lfoStates[i].phase -= juce::MathConstants<float>::twoPi;
                lfoStates[i].lastRandomX = generateWave(0, 3);
                lfoStates[i].lastRandomY = generateWave(0, 3);
            }

            float rawX = (wave == 3) ? lfoStates[i].lastRandomX : generateWave(lfoStates[i].phase, wave);
            float rawY = (wave == 3) ? lfoStates[i].lastRandomY : generateWave(lfoStates[i].phase + juce::MathConstants<float>::halfPi, wave);

            if (step) {
                rawX = std::round(rawX * 4.0f) / 4.0f;
                rawY = std::round(rawY * 4.0f) / 4.0f;
            }

            lfoPositions[i].x = juce::jlimit(0.0f, 1.0f, baseX + rawX * amt * 0.4f);
            lfoPositions[i].y = juce::jlimit(0.0f, 1.0f, baseY + rawY * amt * 0.4f);
        }
        else {
            lfoPositions[i] = { baseX, baseY };
        }
    }

    float mX = lfoPositions[0].x; float mY = lfoPositions[0].y;
    float wA = (1 - mX) * (1 - mY); float wB = mX * (1 - mY); float wC = (1 - mX) * mY; float wD = mX * mY;

    auto getDist = [&](juce::Point<float> p) {
        float dx = p.x - 0.5f; float dy = p.y - 0.5f;
        return std::sqrt(dx * dx + dy * dy) / 0.707f;
        };
    float cMod = getDist(lfoPositions[1]);
    float rMod = getDist(lfoPositions[2]);

    auto updateF = [&](TptFilter& f, juce::String s) {
        float fc = apvts.getRawParameterValue("cutoff" + s)->load() * (1.0f + cMod * 2.0f);
        float res = apvts.getRawParameterValue("res" + s)->load() * (1.0f + rMod * 3.0f);
        f.setCutoff(juce::jlimit(20.0f, 20000.0f, fc));
        f.setResonance(juce::jlimit(0.1f, 10.0f, res));
        f.setType((int)apvts.getRawParameterValue("type" + s)->load());
        };
    updateF(filterA, "A"); updateF(filterB, "B"); updateF(filterC, "C"); updateF(filterD, "D");

    bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;

    // 【修正】動的メモリ確保を排除し、事前に用意したバッファへコピーする
    auto proc = [&](juce::AudioBuffer<float>& t, TptFilter& f, bool e) {
        for (int ch = 0; ch < numChannels; ++ch) {
            t.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }
        if (e) f.process(t); else t.clear();
        };

    proc(filterBuffers[0], filterA, enA);
    proc(filterBuffers[1], filterB, enB);
    proc(filterBuffers[2], filterC, enC);
    proc(filterBuffers[3], filterD, enD);

    buffer.clear();
    for (int ch = 0; ch < numChannels; ++ch) {
        if (enA) buffer.addFrom(ch, 0, filterBuffers[0], ch, 0, numSamples, wA);
        if (enB) buffer.addFrom(ch, 0, filterBuffers[1], ch, 0, numSamples, wB);
        if (enC) buffer.addFrom(ch, 0, filterBuffers[2], ch, 0, numSamples, wC);
        if (enD) buffer.addFrom(ch, 0, filterBuffers[3], ch, 0, numSamples, wD);
    }
}

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