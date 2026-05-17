// ==========================================
// PluginProcessor.h
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/TptFilter.h"       // ← パス変更
#include "DSP/FilterA_SVF.h"     // ← パス変更
#include <vector>
#include <array>
#include <atomic>

class QuadMorphFilterAudioProcessor : public juce::AudioProcessor
{
public:
    QuadMorphFilterAudioProcessor();
    ~QuadMorphFilterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::Point<float>   getLfoPos(int index) const { return lfoPositions[index]; }
    std::array<float, 4> getLfoMod4(int index) const { return currentLfoMod4[index]; }

    juce::AudioProcessorValueTreeState apvts;

    std::array<juce::Point<float>, 2048> recBuffer[3];
    std::atomic<int>   recLength[3]{ 0 };
    std::atomic<bool>  isWaitingForRecord[3]{ false };
    std::atomic<bool>  isRecordingDrag[3]{ false };
    std::atomic<float> currentRecX[3]{ 0.5f };
    std::atomic<float> currentRecY[3]{ 0.5f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Clean SVF 専用（4フィルター分）
    FilterA_SVF svfA, svfB, svfC, svfD;

    // その他27モデル用
    TptFilter filterA, filterB, filterC, filterD;

    std::array<juce::AudioBuffer<float>, 4> filterBuffers;
    juce::AudioBuffer<float> dryBuffer;

    float currentGainReduction[2] = { 1.0f, 1.0f };

    struct LfoState {
        juce::Random rng;
        float phase = 0.0f;
        juce::Point<float> currentRandom{ 0.5f, 0.5f };
        juce::Point<float> nextRandom{ 0.5f, 0.5f };
        std::array<float, 4> currentRand1{ 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, 4> nextRand1{ 0.5f, 0.5f, 0.5f, 0.5f };
        float bilX = 0.0f, bilY = 0.0f, bilVx = 1.3f, bilVy = 1.7f;
        float smoothNx = 0.5f, smoothNy = 0.5f;
        float tNextNx = 0.5f, tNextNy = 0.5f;
    };

    LfoState lfoStates[3];
    juce::Point<float> lfoPositions[3];

    std::array<float, 4> currentLfoMod4[3] = {
        std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f },
        std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f },
        std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }
    };

    float getSyncTime(int selection, double bpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessor)
};