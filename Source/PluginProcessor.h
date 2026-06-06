// ==========================================
// PluginProcessor.h
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h> // FastMathApproximations 等のために必須
#include "DSP/TptFilter.h"
#include "DSP/FilterA_SVF_SIMD.h"
#include "DSP/LfoEngine.h"
#include "DSP/Lfo5Engine.h"
#include "DSP/MorphEngine.h"
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

    juce::Point<float>   getLfoPos(int index)  const { return lfoEngine.getPosition(index); }
    std::array<float, 4> getLfoMod4(int index) const { return lfoEngine.getMod4(index); }

    // ===== Recording データセッター =====
    void setLfoRecordingData(int index, const std::array<juce::Point<float>, 2048>& buffer, int len)
    {
        lfoEngine.setRecordingData(index, buffer, len);
    }

    // ===== Recording 完了後フェーズリセット =====
    void resetLfoPhase(int index)
    {
        lfoEngine.resetPhase(index);
    }

    juce::AudioProcessorValueTreeState apvts;

    std::array<juce::Point<float>, 2048> recBuffer[3];
    std::atomic<int>   recLength[3]{ 0 };
    std::atomic<bool>  isWaitingForRecord[3]{ false };
    std::atomic<bool>  isRecordingDrag[3]{ false };
    std::atomic<float> currentRecX[3]{ 0.5f };
    std::atomic<float> currentRecY[3]{ 0.5f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    LfoEngine lfoEngine;
    Lfo5Engine lfo5Engine;

    // ===== Clean SVF 4インスタンス（SIMD並列処理）=====
    FilterA_SVF_SIMD svfQuad;

    // その他27モデル用
    TptFilter filterA, filterB, filterC, filterD;

    std::array<juce::AudioBuffer<float>, 4> filterBuffers;
    juce::AudioBuffer<float> dryBuffer;

    float currentGainReduction[2] = { 1.0f, 1.0f };

    // ===== Ableton Live フェイルセーフ用 =====
    double expectedSampleRate = 0.0;

    // ===== パラメータスムージング（統一的な 5ms タイムコンスタント）オリジナル復元 =====
    float lastDryWet = 0.5f;                   // 0.0-1.0 range（dB domain ではない）
    float lastMasterGainLinear = 1.0f;         // linear scale（dB→linear 変換済み）
    float lastCeilingLinear = 0.977f;          // linear scale
    float lastMorphX = 0.5f;                   // Morph X スムージング
    float lastMorphY = 0.5f;                   // Morph Y スムージング
    float lastLfo5Mod = 0.5f;                  // LFO5 modulation スムージング（P4）

    // ===== Envelope Follower =====
    float peakValue = 0.0f;            // Current peak value
    float lastPeakValue = 0.0f;        // Previous peak for delta calculation
    double envFollowerSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessor)
};