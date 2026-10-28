#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include <atomic>

class BlezzTuneProcessor : public juce::AudioProcessor
{
public:
    BlezzTuneProcessor();
    ~BlezzTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BLEZZ TUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { applyPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);

    static juce::StringArray keyNames();
    static juce::StringArray scaleNames();

    juce::AudioProcessorValueTreeState apvts;

    // datos para la pantalla
    std::atomic<float> uiDetectedMidi { -1.0f }, uiCorrection { 0.0f }, uiTargetMidi { -1.0f };
    std::atomic<float> uiFetGR { 0.0f }, uiOptoGR { 0.0f };
    std::atomic<int> currentPresetAtomic { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int nearestScaleNote (float midi) const;
    void updateScaleMask();

    blezz::PitchTracker tracker;
    blezz::GrainShifter shifter;
    blezz::FetComp fet;
    blezz::OptoComp opto;
    blezz::Brightness bright;

    double fs = 44100.0;
    bool scaleMask[12] {};
    int lastKey = -1, lastScale = -1;

    float detectedMidi = -1.0f;
    int   targetNote = -1;
    float desiredShift = 0.0f, shift = 0.0f;
    double heldSeconds = 0.0;
    double grainLen = 1024.0, grainTarget = 1024.0;
    float wetAmount = 0.0f;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlezzTuneProcessor)
};
