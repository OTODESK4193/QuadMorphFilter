#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "TptFilter.h"
#include <vector>

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

    // 【追加】Editorから現在の(LFO反映済み)座標を取得するための関数
    juce::Point<float> getCurrentXY() const { return { currentX, currentY }; }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    TptFilter filterA, filterB, filterC, filterD;

    // LFOおよび現在の座標保持用
    float currentX = 0.5f;
    float currentY = 0.5f;
    float lfoPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessor)
};