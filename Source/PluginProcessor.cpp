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

    juce::StringArray waveTypes = {
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard", "Polygon", "Attractor Orbit"
    };

    juce::StringArray syncRates = {
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };

    juce::StringArray bounds = { "Clip", "Bounce", "Wrap" };
    juce::StringArray lfoNames = { "Morph", "Cutoff", "Reso" };

    for (int i = 0; i < 3; ++i) {
        juce::String id = "lfo" + juce::String(i + 1);
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "en", 1 }, lfoNames[i] + " Enable", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "wave", 1 }, lfoNames[i] + " Wave", waveTypes, 0));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "step", 1 }, lfoNames[i] + " Step Mode", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id + "sync", 1 }, lfoNames[i] + " Sync", true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "rateSync", 1 }, lfoNames[i] + " Rate Sync", syncRates, 5));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, lfoNames[i] + " Rate Free", juce::NormalisableRange<float>(0.01f, 20.0f, 0.001f, 0.2f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "min", 1 }, lfoNames[i] + " Min", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "max", 1 }, lfoNames[i] + " Max", 0.0f, 1.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "bound", 1 }, lfoNames[i] + " Boundary", bounds, 0));
    }

    juce::StringArray suffixes = { "A", "B", "C", "D" };
    // Åyí«â¡ÅzëÊ6ÇÃÉÇÉfÉã Formant (Vowel)
    juce::StringArray models = { "Clean SVF", "Moog Ladder", "Diode (TB-303)", "SEM (Oberheim)", "Bitcrush / SRR", "Formant (Vowel)" };
    juce::StringArray slopes = { "12 dB/oct", "24 dB/oct", "48 dB/oct", "96 dB/oct" };

    for (const auto& s : suffixes) {
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "enable" + s, 1 }, "Enable " + s, true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "model" + s, 1 }, "Model " + s, models, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type" + s, 1 }, "Type " + s, juce::StringArray{ "LP", "BP", "HP", "Notch" }, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "slope" + s, 1 }, "Slope " + s, slopes, 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cutoff" + s, 1 }, "Cutoff " + s, juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "res" + s, 1 }, "Res " + s, 0.1f, 10.0f, 0.707f));
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

    for (auto& buf : filterBuffers) buf.setSize(2, samplesPerBlock, false, false, true);
}

void QuadMorphFilterAudioProcessor::releaseResources() {}

float QuadMorphFilterAudioProcessor::getSyncTime(int selection, double bpm) {
    double beatLen = 60.0 / bpm;
    if (selection < 10) return (float)(beatLen * std::pow(2.0, 3 - selection));
    if (selection < 16) return (float)(beatLen * std::pow(2.0, 0 - (selection - 10)) * 1.5);
    return (float)(beatLen * std::pow(2.0, 0 - (selection - 16)) * (2.0 / 3.0));
}

void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();
    auto sampleRate = getSampleRate();
    auto numChannels = buffer.getNumChannels();
    float dt = numSamples / (float)sampleRate;

    double bpm = 120.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (pos->getBpm().hasValue()) bpm = *(pos->getBpm());

    float baseX = apvts.getRawParameterValue("posX")->load();
    float baseY = apvts.getRawParameterValue("posY")->load();

    std::array<float, 4> lfoMod4[3] = { {0.f,0.f,0.f,0.f}, {0.f,0.f,0.f,0.f}, {0.f,0.f,0.f,0.f} };

    auto applyBound = [](float v, int mode) {
        if (mode == 0) return juce::jlimit(0.0f, 1.0f, v);
        if (mode == 1) { v = std::fmod(v, 2.0f); if (v < 0.0f) v += 2.0f; if (v > 1.0f) v = 2.0f - v; return v; }
        v = std::fmod(v, 1.0f); if (v < 0.0f) v += 1.0f; return v;
        };

    for (int i = 0; i < 3; ++i) {
        juce::String id = "lfo" + juce::String(i + 1);
        bool enabled = apvts.getRawParameterValue(id + "en")->load() > 0.5f;
        int wave = (int)apvts.getRawParameterValue(id + "wave")->load();
        bool step = apvts.getRawParameterValue(id + "step")->load() > 0.5f;
        bool sync = apvts.getRawParameterValue(id + "sync")->load() > 0.5f;
        float minVal = apvts.getRawParameterValue(id + "min")->load();
        float maxVal = apvts.getRawParameterValue(id + "max")->load();
        int boundMode = (int)apvts.getRawParameterValue(id + "bound")->load();
        float spread = maxVal - minVal;

        bool isWait = isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = isRecordingDrag[i].load(std::memory_order_acquire);

        if (enabled) {
            float rate = sync ? (1.0f / getSyncTime((int)apvts.getRawParameterValue(id + "rateSync")->load(), bpm)) : apvts.getRawParameterValue(id + "rateFree")->load();
            float actualDt = rate * dt;

            if (!isWait) {
                lfoStates[i].phase += (juce::MathConstants<float>::twoPi * actualDt);
                if (lfoStates[i].phase >= juce::MathConstants<float>::twoPi) {
                    lfoStates[i].phase -= juce::MathConstants<float>::twoPi;
                    lfoStates[i].currentRandom = lfoStates[i].nextRandom;
                    lfoStates[i].nextRandom = { lfoStates[i].rng.nextBool() ? 1.0f : 0.0f, lfoStates[i].rng.nextBool() ? 1.0f : 0.0f };
                    lfoStates[i].currentRand1 = lfoStates[i].nextRand1;
                    for (int f = 0; f < 4; ++f) lfoStates[i].nextRand1[f] = lfoStates[i].rng.nextFloat();
                    lfoStates[i].smoothNx = lfoStates[i].tNextNx; lfoStates[i].smoothNy = lfoStates[i].tNextNy;
                    lfoStates[i].tNextNx = lfoStates[i].rng.nextFloat(); lfoStates[i].tNextNy = lfoStates[i].rng.nextFloat();
                }
            }

            float p = lfoStates[i].phase; float t = step ? 0.0f : (p / juce::MathConstants<float>::twoPi);
            float W_x = 0.0f, W_y = 0.0f;

            switch (wave) {
            case 0: W_x = std::sin(p); W_y = std::sin(p + juce::MathConstants<float>::halfPi); break;
            case 1: W_x = 1.0f - (p / juce::MathConstants<float>::pi); W_y = 1.0f - (std::fmod(p + juce::MathConstants<float>::halfPi, juce::MathConstants<float>::twoPi) / juce::MathConstants<float>::pi); break;
            case 2: W_x = (p < juce::MathConstants<float>::pi) ? 1.0f : -1.0f; W_y = (std::fmod(p + juce::MathConstants<float>::halfPi, juce::MathConstants<float>::twoPi) < juce::MathConstants<float>::pi) ? 1.0f : -1.0f; break;
            case 3: break;
            case 4: W_x = (lfoStates[i].currentRandom.x + (lfoStates[i].nextRandom.x - lfoStates[i].currentRandom.x) * t) * 2.0f - 1.0f;
                W_y = (lfoStates[i].currentRandom.y + (lfoStates[i].nextRandom.y - lfoStates[i].currentRandom.y) * t) * 2.0f - 1.0f; break;
            case 5: W_x = lfoStates[i].rng.nextFloat() * 2.0f - 1.0f; W_y = lfoStates[i].rng.nextFloat() * 2.0f - 1.0f; break;
            case 6:
                if (isWait) {
                    if (isDrag) { W_x = currentRecX[i].load() * 2.f - 1.f; W_y = currentRecY[i].load() * 2.f - 1.f; }
                    else { int len = recLength[i].load(); if (len > 0) { W_x = recBuffer[i][len - 1].x * 2.f - 1.f; W_y = recBuffer[i][len - 1].y * 2.f - 1.f; } }
                }
                else {
                    int len = recLength[i].load();
                    if (len > 0) {
                        float exactIdx = t * len; int idx1 = (int)exactIdx % len; int idx2 = (idx1 + 1) % len; float frac = exactIdx - std::floor(exactIdx);
                        W_x = (recBuffer[i][idx1].x + (recBuffer[i][idx2].x - recBuffer[i][idx1].x) * frac) * 2.f - 1.f;
                        W_y = (recBuffer[i][idx1].y + (recBuffer[i][idx2].y - recBuffer[i][idx1].y) * frac) * 2.f - 1.f;
                    }
                } break;
            case 7: {
                float smT = t * t * (3.0f - 2.0f * t);
                W_x = (lfoStates[i].smoothNx + (lfoStates[i].tNextNx - lfoStates[i].smoothNx) * smT) * 2.f - 1.f;
                W_y = (lfoStates[i].smoothNy + (lfoStates[i].tNextNy - lfoStates[i].smoothNy) * smT) * 2.f - 1.f; break;
            }
            case 8: W_x = 0.5f * std::cos(p) + 0.5f * std::cos(5.2f * p); W_y = 0.5f * std::sin(p) - 0.5f * std::sin(5.2f * p); break;
            case 9: W_x = (std::sin(p) + std::sin(1.37f * p) + std::sin(2.11f * p)) / 3.0f; W_y = (std::cos(1.09f * p) + std::cos(1.73f * p) + std::cos(2.51f * p)) / 3.0f; break;
            case 10: { float r_knot = std::cos(2.0f * p) + 2.0f; W_x = r_knot * std::cos(3.0f * p) / 3.0f; W_y = r_knot * std::sin(3.0f * p) / 3.0f; break; }
            case 11: W_x = std::sin(p * 3.0f); W_y = std::sin(p * 4.0f); break;
            case 12: W_x = t * std::cos(p * 5.0f); W_y = t * std::sin(p * 5.0f); break;
            case 13: W_x = 0.6f * std::cos(p) + 0.4f * std::cos(1.5f * p); W_y = 0.6f * std::sin(p) - 0.4f * std::sin(1.5f * p); break;
            case 14: W_x = std::cos(2.5f * p) * std::cos(p); W_y = std::cos(2.5f * p) * std::sin(p); break;
            case 15: { float scale = 2.0f / (3.0f - std::cos(2.0f * p)); W_x = scale * std::cos(p); W_y = scale * std::sin(2.0f * p) / 2.0f; break; }
            case 16: if (!isWait) {
                lfoStates[i].bilX += lfoStates[i].bilVx * actualDt * 1.5f; lfoStates[i].bilY += lfoStates[i].bilVy * actualDt * 1.5f;
                if (lfoStates[i].bilX > 1.0f || lfoStates[i].bilX < -1.0f) lfoStates[i].bilVx *= -1.0f;
                if (lfoStates[i].bilY > 1.0f || lfoStates[i].bilY < -1.0f) lfoStates[i].bilVy *= -1.0f;
            }
                   W_x = lfoStates[i].bilX; W_y = lfoStates[i].bilY; break;
            case 17: { float rP = std::cos(juce::MathConstants<float>::pi / 6.0f) / std::cos(std::fmod(p, juce::MathConstants<float>::pi / 3.0f) - juce::MathConstants<float>::pi / 6.0f); W_x = rP * std::cos(p); W_y = rP * std::sin(p); break; }
            case 18: { float rO = 0.6f + 0.4f * std::sin(p * 7.0f); W_x = rO * std::cos(p); W_y = rO * std::sin(p); break; }
            }

            if (step) { W_x = std::round(W_x * 4.0f) / 4.0f; W_y = std::round(W_y * 4.0f) / 4.0f; }

            if (wave == 3) {
                for (int f = 0; f < 4; ++f) {
                    float raw = lfoStates[i].currentRand1[f] + (lfoStates[i].nextRand1[f] - lfoStates[i].currentRand1[f]) * t;
                    float w = raw * 2.0f - 1.0f;
                    if (step) w = std::round(w * 4.0f) / 4.0f;
                    lfoMod4[i][f] = applyBound(baseX + w * spread, boundMode);
                }
                lfoPositions[i].x = lfoMod4[i][0]; lfoPositions[i].y = lfoMod4[i][1];
            }
            else {
                lfoPositions[i].x = applyBound(baseX + W_x * spread, boundMode); lfoPositions[i].y = applyBound(baseY + W_y * spread, boundMode);
                for (int f = 0; f < 4; ++f) lfoMod4[i][f] = lfoPositions[i].x;
            }
        }
        else {
            lfoPositions[i] = { baseX, baseY };
        }
        currentLfoMod4[i] = lfoMod4[i];
    }

    auto getM = [](juce::Point<float> p, const std::array<float, 4>& m4, bool isRand1) -> std::array<float, 4> {
        std::array<float, 4> res;
        if (isRand1) { for (int i = 0; i < 4; ++i) res[i] = m4[i] * 2.0f - 1.0f; }
        else { res[0] = p.x * 2.0f - 1.0f; res[1] = p.y * 2.0f - 1.0f; res[2] = (1.0f - p.x) * 2.0f - 1.0f; res[3] = (1.0f - p.y) * 2.0f - 1.0f; }
        return res;
        };

    bool lfo1_isRand1 = ((int)apvts.getRawParameterValue("lfo2wave")->load() == 3) && (apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    std::array<float, 4> cM = getM(lfoPositions[1], lfoMod4[1], lfo1_isRand1);

    bool lfo2_isRand1 = ((int)apvts.getRawParameterValue("lfo3wave")->load() == 3) && (apvts.getRawParameterValue("lfo3en")->load() > 0.5f);
    std::array<float, 4> rM = getM(lfoPositions[2], lfoMod4[2], lfo2_isRand1);

    auto updateF = [&](TptFilter& f, juce::String s, int idx) {
        float baseCutoff = apvts.getRawParameterValue("cutoff" + s)->load();
        float baseRes = apvts.getRawParameterValue("res" + s)->load();

        float fc = baseCutoff * std::pow(2.0f, 4.0f * cM[idx]);
        float res = baseRes * std::pow(2.0f, 2.0f * rM[idx]);

        f.setModel((int)apvts.getRawParameterValue("model" + s)->load());
        f.setCutoff(juce::jlimit(20.0f, 20000.0f, fc));
        f.setResonance(juce::jlimit(0.1f, 10.0f, res));
        f.setType((int)apvts.getRawParameterValue("type" + s)->load());
        f.setSlope((int)apvts.getRawParameterValue("slope" + s)->load());
        };

    updateF(filterA, "A", 0); updateF(filterB, "B", 1); updateF(filterC, "C", 2); updateF(filterD, "D", 3);

    std::array<float, 4> wMix{ 0.f, 0.f, 0.f, 0.f };
    bool lfo0_isRand1 = ((int)apvts.getRawParameterValue("lfo1wave")->load() == 3) && (apvts.getRawParameterValue("lfo1en")->load() > 0.5f);
    if (lfo0_isRand1) { wMix = lfoMod4[0]; }
    else {
        float x = lfoPositions[0].x; float y = lfoPositions[0].y;
        wMix[0] = (1.0f - x) * (1.0f - y); wMix[1] = x * (1.0f - y); wMix[2] = (1.0f - x) * y; wMix[3] = x * y;
    }

    bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f; bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f; bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;

    auto proc = [&](juce::AudioBuffer<float>& t, TptFilter& f, bool e) {
        for (int ch = 0; ch < numChannels; ++ch) t.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        if (e) f.process(t); else t.clear();
        };

    proc(filterBuffers[0], filterA, enA); proc(filterBuffers[1], filterB, enB);
    proc(filterBuffers[2], filterC, enC); proc(filterBuffers[3], filterD, enD);

    buffer.clear();
    for (int ch = 0; ch < numChannels; ++ch) {
        if (enA) buffer.addFrom(ch, 0, filterBuffers[0], ch, 0, numSamples, wMix[0]);
        if (enB) buffer.addFrom(ch, 0, filterBuffers[1], ch, 0, numSamples, wMix[1]);
        if (enC) buffer.addFrom(ch, 0, filterBuffers[2], ch, 0, numSamples, wMix[2]);
        if (enD) buffer.addFrom(ch, 0, filterBuffers[3], ch, 0, numSamples, wMix[3]);
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
    auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml(state.createXml()); copyXmlToBinary(*xml, destData);
}
void QuadMorphFilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuadMorphFilterAudioProcessor(); }