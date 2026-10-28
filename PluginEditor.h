#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace blezz
{
namespace col
{
    const juce::Colour bg0     { 0xff060309 };
    const juce::Colour bg1     { 0xff120820 };
    const juce::Colour panel   { 0xff110a1b };
    const juce::Colour border  { 0xff3a1d5c };
    const juce::Colour track   { 0xff241336 };
    const juce::Colour accent  { 0xff9d3dff };
    const juce::Colour accent2 { 0xffe27bff };
    const juce::Colour dim     { 0xff7d6a96 };
    const juce::Colour text    { 0xfff1e8ff };
}

class TrapLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TrapLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class NoteDisplay : public juce::Component
{
public:
    void setValues (float detected, float target, float cents) { det = detected; tgt = target; corr = cents; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float det = -1, tgt = -1, corr = 0;
};

class GRMeter : public juce::Component
{
public:
    void setGR (float db) { gr = db; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float gr = 0;
};
} // namespace blezz

//==============================================================================
class BlezzTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BlezzTuneEditor (BlezzTuneProcessor&);
    ~BlezzTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name,
                    std::function<juce::String (double)> fmt, bool big = false);
    void setupCombo (juce::ComboBox&, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                     const juce::String& paramId, const juce::StringArray& items);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&,
                      const juce::String& paramId, const juce::String& text);
    void placeKnob (Knob&, juce::Rectangle<int> area);

    BlezzTuneProcessor& proc;
    blezz::TrapLookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::ComboBox keyBox, scaleBox, ratioBox, modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, ratioAtt, modeAtt;
    juce::ToggleButton tuneOn, fetOn, optoOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tuneOnAtt, fetOnAtt, optoOnAtt;
    juce::Label keyLbl, scaleLbl, ratioLbl, modeLbl;

    Knob retune, humanize, fetIn, fetOut, fetAtk, fetRel, optoPeak, optoGain;
    juce::Slider brillo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brilloAtt;

    blezz::NoteDisplay noteDisplay;
    blezz::GRMeter fetMeter, optoMeter;

    juce::Rectangle<int> tunePanel, fetPanel, optoPanel, brilloPanel;
    int lastPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlezzTuneEditor)
};
