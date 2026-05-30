// ==========================================
// PluginProcessor.h
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/TptFilter.h"

#include "DSP/FilterA_SVF_SIMD.h"     // ← 追加
#include "DSP/LfoEngine.h"
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

    // ===== Clean SVF 4インスタンス（SIMD並列処理）=====
    // 旧: svfA, svfB, svfC, svfD の4つ
    // 新: svfQuad の1つで4インスタンス分を内部管理
    FilterA_SVF_SIMD svfQuad;

    // その他27モデル用
    TptFilter filterA, filterB, filterC, filterD;

    std::array<juce::AudioBuffer<float>, 4> filterBuffers;
    juce::AudioBuffer<float> dryBuffer;

    float currentGainReduction[2] = { 1.0f, 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessor)
};