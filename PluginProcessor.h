#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include <atomic>
#include <array>

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

    const juce::String getName() const override { return "BLEZZ TUNE V2"; }
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
    bool isInScale (int midiNote) const { return scaleMaskUi[(size_t) (((midiNote % 12) + 12) % 12)].load(); }

    juce::AudioProcessorValueTreeState apvts;

    // datos para la pantalla
    std::atomic<float> uiDetectedMidi { -1.0f }, uiCorrection { 0.0f }, uiTargetMidi { -1.0f };
    std::atomic<float> uiFetGR { 0.0f }, uiOptoGR { 0.0f };
    std::atomic<int> currentPresetAtomic { 0 };

    // historial para el gráfico (entrada y salida en notas MIDI, -1 = sin tono)
    static constexpr int historySize = 512;
    std::array<std::atomic<float>, historySize> histIn {}, histOut {};
    std::atomic<int> histWrite { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int nearestScaleNote (float midi) const;
    void updateScaleMask();

    blezz::PitchTracker tracker;
    blezz::PsolaShifter psola;
    blezz::FetComp fet;
    blezz::OptoComp opto;
    blezz::Brightness bright;

    double fs = 44100.0;
    bool scaleMask[12] {};
    std::array<std::atomic<bool>, 12> scaleMaskUi {};
    int lastKey = -1, lastScale = -1;

    bool  voiced = false;
    float detectedMidi = -1.0f, midiSlow = -1.0f, period = 0.0f;
    int   targetNote = -1;
    float desiredShift = 0.0f, shift = 0.0f;
    double heldSeconds = 0.0;
    float tuneMix = 1.0f;
    int histCounter = 0;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlezzTuneProcessor)
};
