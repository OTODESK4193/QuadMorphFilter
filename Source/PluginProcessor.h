// ==========================================
// PluginProcessor.h
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "TptFilter.h"
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

    juce::Point<float> getLfoPos(int index) const { return lfoPositions[index]; }

    juce::AudioProcessorValueTreeState apvts;

    std::array<juce::Point<float>, 2048> recBuffer[3];
    std::atomic<int> recLength[3]{ 0 };
    std::atomic<bool> isRecording[3]{ false };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    TptFilter filterA, filterB, filterC, filterD;
    std::array<juce::AudioBuffer<float>, 4> filterBuffers;

    struct LfoState {
        float phase = 0.0f;
        juce::Point<float> currentRandom{ 0.5f, 0.5f };
        juce::Point<float> nextRandom{ 0.5f, 0.5f };
        std::array<float, 4> currentRand1{ 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, 4> nextRand1{ 0.5f, 0.5f, 0.5f, 0.5f };
    };
    LfoState lfoStates[3];
    juce::Point<float> lfoPositions[3];

    float generateWave(float phase, int type);
    float getSyncTime(int selection, double bpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessor)
};