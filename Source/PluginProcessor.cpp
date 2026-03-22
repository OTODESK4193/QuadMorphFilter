#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

juce::AudioProcessorValueTreeState::ParameterLayout QuadMorphFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto cutoffRange = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cutoff", 1 }, "Cutoff", cutoffRange, 1000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "resonance", 1 }, "Resonance", 0.1f, 10.0f, 0.707f));

    juce::StringArray filterTypes = { "LowPass", "BandPass", "HighPass", "Notch" };
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "type", 1 }, "Filter Type", filterTypes, 0));

    return layout;
}

QuadMorphFilterAudioProcessor::QuadMorphFilterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    ),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

QuadMorphFilterAudioProcessor::~QuadMorphFilterAudioProcessor() {}

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

void QuadMorphFilterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    filter.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void QuadMorphFilterAudioProcessor::releaseResources()
{
    filter.reset();
}

bool QuadMorphFilterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void QuadMorphFilterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    float currentCutoff = apvts.getRawParameterValue("cutoff")->load();
    float currentResonance = apvts.getRawParameterValue("resonance")->load();
    int currentType = static_cast<int>(apvts.getRawParameterValue("type")->load());

    filter.setCutoff(currentCutoff);
    filter.setResonance(currentResonance);
    filter.setType(currentType);

    filter.process(buffer);
}

bool QuadMorphFilterAudioProcessor::hasEditor() const { return true; }

// 【修正箇所】重複していた Processor 名を修正しました
juce::AudioProcessorEditor* QuadMorphFilterAudioProcessor::createEditor()
{
    return new QuadMorphFilterAudioProcessorEditor(*this);
}

void QuadMorphFilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void QuadMorphFilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuadMorphFilterAudioProcessor();
}