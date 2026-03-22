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
    // 【修正】波形リストの分離
    juce::StringArray waveTypes1 = { "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording" };
    juce::StringArray waveTypes23 = { "Sine", "SAW", "Pulse", "Random 1", "Noise", "Recording" };

    juce::StringArray syncRates = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                                   "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
                                   "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T" };

    for (int i = 0; i < 3; ++i) {
        juce::String id = "lfo" + juce::String(i + 1);
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, lfoNames[i] + " Enable", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave", 1 }, lfoNames[i] + " Wave", (i == 0) ? waveTypes1 : waveTypes23, 0));
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

    for (auto& buf : filterBuffers) {
        buf.setSize(2, samplesPerBlock, false, false, true);
    }
}

void QuadMorphFilterAudioProcessor::releaseResources() {}

// 【修正】Noiseの独立。Random処理はprocessBlock側へ移行
float QuadMorphFilterAudioProcessor::generateWave(float phase, int type) {
    switch (type) {
    case 0: return std::sin(phase);
    case 1: return 1.0f - (phase / juce::MathConstants<float>::pi);
    case 2: return (phase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
    case 3: return (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f); // Noise
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

        // 【追加】波形ごとのフラグ判別ルーティング
        bool isRand1 = false, isRand2 = false, isNoise = false, isRec = false;
        int stdWave = 0;
        if (i == 0) { // LFO 1
            if (wave == 3) isRand1 = true;
            else if (wave == 4) isRand2 = true;
            else if (wave == 5) isNoise = true;
            else if (wave == 6) isRec = true;
            else stdWave = wave;
        }
        else { // LFO 2/3
            if (wave == 3) isRand1 = true;
            else if (wave == 4) isNoise = true;
            else if (wave == 5) isRec = true;
            else stdWave = wave;
        }

        if (enabled) {
            float rate = sync ? (1.0f / getSyncTime((int)apvts.getRawParameterValue(id + "rateSync")->load(), bpm))
                : apvts.getRawParameterValue(id + "rateFree")->load();

            lfoStates[i].phase += (juce::MathConstants<float>::twoPi * rate * numSamples / sampleRate);

            // 位相リセット時のRandom再生成
            if (lfoStates[i].phase >= juce::MathConstants<float>::twoPi) {
                lfoStates[i].phase -= juce::MathConstants<float>::twoPi;
                lfoStates[i].currentRandom = lfoStates[i].nextRandom;

                if (isRand2) {
                    float rx = juce::Random::getSystemRandom().nextBool() ? 1.0f : 0.0f;
                    float ry = juce::Random::getSystemRandom().nextBool() ? 1.0f : 0.0f;
                    lfoStates[i].nextRandom = { rx, ry };
                }
                else if (isRand1) {
                    float rx = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                    float ry = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                    lfoStates[i].nextRandom = { rx, ry };
                }
            }

            float rawX = 0.0f, rawY = 0.0f;
            bool isAbsolute = false;

            if (isRec) {
                // 【追加】Recording再生時のタイムストレッチと線形補間
                int len = recLength[i].load(std::memory_order_acquire);
                if (len > 0) {
                    float t = lfoStates[i].phase / juce::MathConstants<float>::twoPi;
                    float exactIdx = t * len;
                    int idx1 = (int)exactIdx % len;
                    int idx2 = (idx1 + 1) % len;
                    float frac = exactIdx - std::floor(exactIdx);
                    rawX = recBuffer[i][idx1].x + (recBuffer[i][idx2].x - recBuffer[i][idx1].x) * frac;
                    rawY = recBuffer[i][idx1].y + (recBuffer[i][idx2].y - recBuffer[i][idx1].y) * frac;
                    isAbsolute = true;
                }
                else {
                    rawX = baseX; rawY = baseY;
                    isAbsolute = true;
                }
            }
            else if (isRand1 || isRand2) {
                // 【追加】Randomの線形スムージング（StepOff時）
                float t = step ? 0.0f : (lfoStates[i].phase / juce::MathConstants<float>::twoPi);
                rawX = lfoStates[i].currentRandom.x + (lfoStates[i].nextRandom.x - lfoStates[i].currentRandom.x) * t;
                rawY = lfoStates[i].currentRandom.y + (lfoStates[i].nextRandom.y - lfoStates[i].currentRandom.y) * t;
                if (isRand2) isAbsolute = true;
            }
            else {
                if (isNoise) stdWave = 3;
                rawX = generateWave(lfoStates[i].phase, stdWave);
                rawY = generateWave(lfoStates[i].phase + juce::MathConstants<float>::halfPi, stdWave);
                if (step) {
                    rawX = std::round(rawX * 4.0f) / 4.0f;
                    rawY = std::round(rawY * 4.0f) / 4.0f;
                }
            }

            if (isAbsolute) {
                lfoPositions[i].x = juce::jlimit(0.0f, 1.0f, rawX);
                lfoPositions[i].y = juce::jlimit(0.0f, 1.0f, rawY);
            }
            else {
                lfoPositions[i].x = juce::jlimit(0.0f, 1.0f, baseX + rawX * amt * 0.4f);
                lfoPositions[i].y = juce::jlimit(0.0f, 1.0f, baseY + rawY * amt * 0.4f);
            }
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

    auto proc = [&](juce::AudioBuffer<float>& t, TptFilter& f, bool e) {
        for (int ch = 0; ch < numChannels; ++ch) t.copyFrom(ch, 0, buffer, ch, 0, numSamples);
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