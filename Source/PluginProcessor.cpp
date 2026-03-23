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

    // Åyí«â¡Åz19éÌóﬁÇÃëSîgå`Çã§í âª
    juce::StringArray waveTypes = {
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Lorenz Attractor", "Double Pendulum", "Random Walk",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard", "Polygon", "Attractor Orbit"
    };

    // Åyí«â¡Åz8/1, 4/1, 2/1ÇÃSync RateÇí«â¡
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
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "rateFree", 1 }, lfoNames[i] + " Rate Free",
            juce::NormalisableRange<float>(0.1f, 20.0f, 0.001f, 0.3f), 1.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "min", 1 }, lfoNames[i] + " Min", 0.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ id + "max", 1 }, lfoNames[i] + " Max", 0.0f, 1.0f, 1.0f));

        // Åyí«â¡ÅzBoundary BehaviorÉpÉâÉÅÅ[É^
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id + "bound", 1 }, lfoNames[i] + " Boundary", bounds, 0));
    }

    juce::StringArray suffixes = { "A", "B", "C", "D" };
    juce::StringArray slopes = { "12 dB/oct", "24 dB/oct", "48 dB/oct", "96 dB/oct" };
    for (const auto& s : suffixes) {
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "enable" + s, 1 }, "Enable " + s, true));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cutoff" + s, 1 }, "Cutoff " + s, juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "res" + s, 1 }, "Res " + s, 0.1f, 10.0f, 0.707f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type" + s, 1 }, "Type " + s, juce::StringArray{ "LP", "BP", "HP", "Notch" }, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "slope" + s, 1 }, "Slope " + s, slopes, 0));
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

float QuadMorphFilterAudioProcessor::generateWave(float phase, int type) {
    switch (type) {
    case 0: return std::sin(phase);
    case 1: return 1.0f - (phase / juce::MathConstants<float>::pi);
    case 2: return (phase < juce::MathConstants<float>::pi) ? 1.0f : -1.0f;
    case 5: return (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
    default: return 0.0f;
    }
}

// ÅyèCê≥Åz8/1, 4/1, 2/1 Ç…ëŒâûÇµÇΩåvéZéÆÇ÷ç¸êV
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
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (pos->getBpm().hasValue()) bpm = *(pos->getBpm());
        }
    }

    float baseX = apvts.getRawParameterValue("posX")->load();
    float baseY = apvts.getRawParameterValue("posY")->load();

    std::array<float, 4> lfoMod4[3] = {
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f},
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f},
        std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}
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

        bool isWait = isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = isRecordingDrag[i].load(std::memory_order_acquire);

        if (enabled) {
            float rate = sync ? (1.0f / getSyncTime((int)apvts.getRawParameterValue(id + "rateSync")->load(), bpm))
                : apvts.getRawParameterValue(id + "rateFree")->load();
            float actualDt = rate * dt;

            if (!isWait) {
                lfoStates[i].phase += (juce::MathConstants<float>::twoPi * actualDt);
                if (lfoStates[i].phase >= juce::MathConstants<float>::twoPi) {
                    lfoStates[i].phase -= juce::MathConstants<float>::twoPi;

                    lfoStates[i].currentRandom = lfoStates[i].nextRandom;
                    lfoStates[i].nextRandom = { lfoStates[i].rng.nextFloat(), lfoStates[i].rng.nextFloat() };

                    lfoStates[i].currentRand1 = lfoStates[i].nextRand1;
                    for (int f = 0; f < 4; ++f) lfoStates[i].nextRand1[f] = lfoStates[i].rng.nextFloat();

                    lfoStates[i].smoothNx = lfoStates[i].tNextNx;
                    lfoStates[i].smoothNy = lfoStates[i].tNextNy;
                    lfoStates[i].tNextNx = lfoStates[i].rng.nextFloat();
                    lfoStates[i].tNextNy = lfoStates[i].rng.nextFloat();
                }
            }

            float normX = 0.0f, normY = 0.0f;
            float p = lfoStates[i].phase;
            float t = step ? 0.0f : (p / juce::MathConstants<float>::twoPi);
            std::array<float, 4> rand1Vals = { 0.0f, 0.0f, 0.0f, 0.0f };

            // Åyí«â¡Åz19éÌÇÃîgå`ÅEï®óùÉGÉìÉWÉìÉçÉWÉbÉN
            switch (wave) {
            case 3: // Random 1
                for (int f = 0; f < 4; ++f) rand1Vals[f] = lfoStates[i].currentRand1[f] + (lfoStates[i].nextRand1[f] - lfoStates[i].currentRand1[f]) * t;
                normX = rand1Vals[0]; normY = rand1Vals[1]; break;
            case 4: // Random 2
                normX = lfoStates[i].currentRandom.x + (lfoStates[i].nextRandom.x - lfoStates[i].currentRandom.x) * t;
                normY = lfoStates[i].currentRandom.y + (lfoStates[i].nextRandom.y - lfoStates[i].currentRandom.y) * t; break;
            case 6: // Recording
                if (isWait) {
                    if (isDrag) { normX = currentRecX[i].load(std::memory_order_relaxed); normY = currentRecY[i].load(std::memory_order_relaxed); }
                    else {
                        int len = recLength[i].load();
                        if (len > 0) { normX = recBuffer[i][len - 1].x; normY = recBuffer[i][len - 1].y; }
                        else { normX = 0.5f; normY = 0.5f; }
                    }
                }
                else {
                    int len = recLength[i].load();
                    if (len > 0) {
                        float exactIdx = t * len;
                        int idx1 = (int)exactIdx % len; int idx2 = (idx1 + 1) % len; float frac = exactIdx - std::floor(exactIdx);
                        normX = recBuffer[i][idx1].x + (recBuffer[i][idx2].x - recBuffer[i][idx1].x) * frac;
                        normY = recBuffer[i][idx1].y + (recBuffer[i][idx2].y - recBuffer[i][idx1].y) * frac;
                    }
                    else { normX = baseX; normY = baseY; }
                } break;
            case 7: // Smooth Noise (Perlin Approx)
            {
                float smT = t * t * (3.0f - 2.0f * t); // Smoothstep
                normX = lfoStates[i].smoothNx + (lfoStates[i].tNextNx - lfoStates[i].smoothNx) * smT;
                normY = lfoStates[i].smoothNy + (lfoStates[i].tNextNy - lfoStates[i].smoothNy) * smT;
                break;
            }
            case 8: // Lorenz Attractor
            {
                if (!isWait) {
                    float dx = 10.0f * (lfoStates[i].lorenzY - lfoStates[i].lorenzX);
                    float dy = lfoStates[i].lorenzX * (28.0f - lfoStates[i].lorenzZ) - lfoStates[i].lorenzY;
                    float dz = lfoStates[i].lorenzX * lfoStates[i].lorenzY - (8.0f / 3.0f) * lfoStates[i].lorenzZ;
                    lfoStates[i].lorenzX += dx * actualDt * 2.0f;
                    lfoStates[i].lorenzY += dy * actualDt * 2.0f;
                    lfoStates[i].lorenzZ += dz * actualDt * 2.0f;
                }
                normX = (lfoStates[i].lorenzX / 40.0f) + 0.5f;
                normY = (lfoStates[i].lorenzY / 40.0f) + 0.5f;
                break;
            }
            case 9: // Double Pendulum (Symplectic Euler)
            {
                if (!isWait) {
                    float dTheta = lfoStates[i].dpT1 - lfoStates[i].dpT2;
                    float den = 2.0f - std::pow(std::cos(dTheta), 2.0f);
                    float d2t1 = (-19.62f * std::sin(lfoStates[i].dpT1) - 9.81f * std::sin(lfoStates[i].dpT1 - 2.0f * lfoStates[i].dpT2) - 2.0f * std::sin(dTheta) * (std::pow(lfoStates[i].dpW2, 2.0f) + std::pow(lfoStates[i].dpW1, 2.0f) * std::cos(dTheta))) / den;
                    float d2t2 = (2.0f * std::sin(dTheta) * (2.0f * std::pow(lfoStates[i].dpW1, 2.0f) + 19.62f * std::cos(lfoStates[i].dpT1) + std::pow(lfoStates[i].dpW2, 2.0f) * std::cos(dTheta))) / den;
                    lfoStates[i].dpW1 = (lfoStates[i].dpW1 + d2t1 * actualDt * 2.0f) * 0.999f;
                    lfoStates[i].dpW2 = (lfoStates[i].dpW2 + d2t2 * actualDt * 2.0f) * 0.999f;
                    lfoStates[i].dpT1 += lfoStates[i].dpW1 * actualDt * 2.0f;
                    lfoStates[i].dpT2 += lfoStates[i].dpW2 * actualDt * 2.0f;
                }
                normX = 0.5f + 0.25f * std::sin(lfoStates[i].dpT1) + 0.25f * std::sin(lfoStates[i].dpT2);
                normY = 0.5f + 0.25f * std::cos(lfoStates[i].dpT1) + 0.25f * std::cos(lfoStates[i].dpT2);
                break;
            }
            case 10: // Random Walk
                if (!isWait) {
                    lfoStates[i].walkX += (lfoStates[i].rng.nextFloat() * 2.0f - 1.0f) * actualDt * 0.5f;
                    lfoStates[i].walkY += (lfoStates[i].rng.nextFloat() * 2.0f - 1.0f) * actualDt * 0.5f;
                }
                normX = 0.5f + lfoStates[i].walkX; normY = 0.5f + lfoStates[i].walkY; break;
            case 11: // Lissajous
                normX = 0.5f + 0.5f * std::sin(p * 3.0f); normY = 0.5f + 0.5f * std::sin(p * 4.0f); break;
            case 12: // Spiral
                normX = 0.5f + 0.5f * t * std::cos(p * 5.0f); normY = 0.5f + 0.5f * t * std::sin(p * 5.0f); break;
            case 13: // Star
                normX = 0.5f + 0.1f * (3.0f * std::cos(p) + 2.0f * std::cos(1.5f * p));
                normY = 0.5f + 0.1f * (3.0f * std::sin(p) - 2.0f * std::sin(1.5f * p)); break;
            case 14: // Rose
                normX = 0.5f + 0.5f * std::cos(2.5f * p) * std::cos(p);
                normY = 0.5f + 0.5f * std::cos(2.5f * p) * std::sin(p); break;
            case 15: // Lemniscate
            {
                float scale = 2.0f / (3.0f - std::cos(2.0f * p));
                normX = 0.5f + 0.5f * scale * std::cos(p);
                normY = 0.5f + 0.5f * scale * std::sin(2.0f * p) / 2.0f; break;
            }
            case 16: // Billiard
                if (!isWait) { lfoStates[i].bilX += lfoStates[i].bilVx * actualDt; lfoStates[i].bilY += lfoStates[i].bilVy * actualDt; }
                normX = lfoStates[i].bilX; normY = lfoStates[i].bilY; break;
            case 17: // Polygon (Hexagon)
            {
                float angle = p; float rP = std::cos(juce::MathConstants<float>::pi / 6.0f) / std::cos(std::fmod(angle, juce::MathConstants<float>::pi / 3.0f) - juce::MathConstants<float>::pi / 6.0f);
                normX = 0.5f + 0.4f * rP * std::cos(angle); normY = 0.5f + 0.4f * rP * std::sin(angle); break;
            }
            case 18: // Attractor Orbit
            {
                float rO = 0.3f + 0.2f * std::sin(p * 7.0f);
                normX = 0.5f + rO * std::cos(p); normY = 0.5f + rO * std::sin(p); break;
            }
            default: // 0:Sine, 1:SAW, 2:Pulse, 5:Noise
                float rawX = generateWave(p, wave); float rawY = generateWave(p + juce::MathConstants<float>::halfPi, wave);
                normX = (rawX + 1.0f) * 0.5f; normY = (rawY + 1.0f) * 0.5f;
                if (step) { normX = std::round(normX * 4.0f) / 4.0f; normY = std::round(normY * 4.0f) / 4.0f; } break;
            }

            // Åyí«â¡ÅzBoundary Behavior (Clip, Bounce, Wrap) ÇÃìKóp
            auto applyBound = [](float v, int mode) {
                if (mode == 0) return juce::jlimit(0.0f, 1.0f, v); // Clip
                if (mode == 1) { // Bounce
                    v = std::fmod(v, 2.0f); if (v < 0.0f) v += 2.0f;
                    if (v > 1.0f) v = 2.0f - v; return v;
                }
                // Wrap
                v = std::fmod(v, 1.0f); if (v < 0.0f) v += 1.0f; return v;
                };

            normX = applyBound(normX, boundMode);
            normY = applyBound(normY, boundMode);

            if (wave == 3) {
                for (int f = 0; f < 4; ++f) {
                    float val = applyBound(rand1Vals[f], boundMode);
                    lfoMod4[i][f] = minVal + val * (maxVal - minVal);
                }
                lfoPositions[i].x = juce::jlimit(0.0f, 1.0f, minVal + normX * (maxVal - minVal));
                lfoPositions[i].y = juce::jlimit(0.0f, 1.0f, minVal + normY * (maxVal - minVal));
            }
            else {
                lfoPositions[i].x = juce::jlimit(0.0f, 1.0f, minVal + normX * (maxVal - minVal));
                lfoPositions[i].y = juce::jlimit(0.0f, 1.0f, minVal + normY * (maxVal - minVal));
            }
        }
        else {
            lfoPositions[i] = { baseX, baseY };
        }
        currentLfoMod4[i] = lfoMod4[i];
    }

    std::array<float, 4> wMix{ 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> cMod{ 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> rMod{ 0.0f, 0.0f, 0.0f, 0.0f };

    bool lfo0_isRand1 = ((int)apvts.getRawParameterValue("lfo1wave")->load() == 3) && (apvts.getRawParameterValue("lfo1en")->load() > 0.5f);
    if (lfo0_isRand1) { wMix = lfoMod4[0]; }
    else {
        float x = lfoPositions[0].x; float y = lfoPositions[0].y;
        wMix[0] = (1.0f - x) * (1.0f - y); wMix[1] = x * (1.0f - y);
        wMix[2] = (1.0f - x) * y;          wMix[3] = x * y;
    }

    bool lfo1_isRand1 = ((int)apvts.getRawParameterValue("lfo2wave")->load() == 3) && (apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    if (lfo1_isRand1) { cMod = lfoMod4[1]; }
    else {
        float dist = std::sqrt(std::pow(lfoPositions[1].x - 0.5f, 2.0f) + std::pow(lfoPositions[1].y - 0.5f, 2.0f)) / 0.707f;
        cMod = { dist, dist, dist, dist };
    }

    bool lfo2_isRand1 = ((int)apvts.getRawParameterValue("lfo3wave")->load() == 3) && (apvts.getRawParameterValue("lfo3en")->load() > 0.5f);
    if (lfo2_isRand1) { rMod = lfoMod4[2]; }
    else {
        float dist = std::sqrt(std::pow(lfoPositions[2].x - 0.5f, 2.0f) + std::pow(lfoPositions[2].y - 0.5f, 2.0f)) / 0.707f;
        rMod = { dist, dist, dist, dist };
    }

    auto updateF = [&](TptFilter& f, juce::String s, int idx) {
        float fc = lfo1_isRand1 ? (20.0f * std::pow(1000.0f, cMod[idx])) : (apvts.getRawParameterValue("cutoff" + s)->load() * (1.0f + cMod[idx] * 2.0f));
        float res = lfo2_isRand1 ? (0.1f * std::pow(100.0f, rMod[idx])) : (apvts.getRawParameterValue("res" + s)->load() * (1.0f + rMod[idx] * 3.0f));
        f.setCutoff(juce::jlimit(20.0f, 20000.0f, fc));
        f.setResonance(juce::jlimit(0.1f, 10.0f, res));
        f.setType((int)apvts.getRawParameterValue("type" + s)->load());
        f.setSlope((int)apvts.getRawParameterValue("slope" + s)->load());
        };

    updateF(filterA, "A", 0); updateF(filterB, "B", 1);
    updateF(filterC, "C", 2); updateF(filterD, "D", 3);

    bool enA = apvts.getRawParameterValue("enableA")->load() > 0.5f;
    bool enB = apvts.getRawParameterValue("enableB")->load() > 0.5f;
    bool enC = apvts.getRawParameterValue("enableC")->load() > 0.5f;
    bool enD = apvts.getRawParameterValue("enableD")->load() > 0.5f;

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