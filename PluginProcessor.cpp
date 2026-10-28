#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APVTS = juce::AudioProcessorValueTreeState;

juce::StringArray BlezzTuneProcessor::keyNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::StringArray BlezzTuneProcessor::scaleNames()
{
    return { "Cromatica", "Mayor", "Menor", "Menor Armonica", "Frigia (Dembow)", "Pentatonica Menor" };
}

APVTS::ParameterLayout BlezzTuneProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withLabel ("%");
    auto db  = AudioParameterFloatAttributes().withLabel ("dB");

    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "tuneOn", 1 }, "Tune On", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 2));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "retune", 1 }, "Retune Speed",
                                                   NormalisableRange<float> (0.0f, 400.0f, 1.0f, 0.4f), 25.0f,
                                                   AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "humanize", 1 }, "Humanize",
                                                   NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f, pct));

    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "fetOn", 1 }, "FET 76 On", true));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fetIn", 1 }, "FET Input",
                                                   NormalisableRange<float> (0.0f, 30.0f, 0.1f), 12.0f, db));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fetOut", 1 }, "FET Output",
                                                   NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -4.0f, db));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fetAtk", 1 }, "FET Attack",
                                                   NormalisableRange<float> (1.0f, 7.0f, 0.01f), 4.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fetRel", 1 }, "FET Release",
                                                   NormalisableRange<float> (1.0f, 7.0f, 0.01f), 5.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "fetRatio", 1 }, "FET Ratio",
                                                   StringArray { "4:1", "8:1", "12:1", "20:1", "ALL" }, 0));

    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "optoOn", 1 }, "OPTO 2A On", true));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "optoPeak", 1 }, "Peak Reduction",
                                                   NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "optoGain", 1 }, "OPTO Gain",
                                                   NormalisableRange<float> (0.0f, 30.0f, 0.1f), 4.0f, db));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "optoMode", 1 }, "OPTO Mode",
                                                   StringArray { "Compress", "Limit" }, 0));

    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "brillo", 1 }, "Brillo",
                                                   NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pct));
    return l;
}

BlezzTuneProcessor::BlezzTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool BlezzTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void BlezzTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    const int ch = std::max (1, getTotalNumOutputChannels());
    tracker.prepare (fs);
    shifter.prepare (ch, (int) (0.08 * fs));
    fet.prepare (fs);
    opto.prepare (fs);
    bright.prepare (fs, ch, samplesPerBlock);
    grainLen = grainTarget = 0.03 * fs;
    shift = desiredShift = 0.0f; wetAmount = 0.0f;
    detectedMidi = -1.0f; targetNote = -1; heldSeconds = 0.0;
    lastKey = lastScale = -1;
    setLatencySamples (0);
}

void BlezzTuneProcessor::updateScaleMask()
{
    const int key   = (int) apvts.getRawParameterValue ("key")->load();
    const int scale = (int) apvts.getRawParameterValue ("scale")->load();
    if (key == lastKey && scale == lastScale) return;
    lastKey = key; lastScale = scale;

    static const std::vector<int> intervals[] = {
        { 0,1,2,3,4,5,6,7,8,9,10,11 },  // cromática
        { 0,2,4,5,7,9,11 },             // mayor
        { 0,2,3,5,7,8,10 },             // menor natural
        { 0,2,3,5,7,8,11 },             // menor armónica
        { 0,1,3,5,7,8,10 },             // frigia
        { 0,3,5,7,10 }                  // pentatónica menor
    };
    for (auto& m : scaleMask) m = false;
    for (int i : intervals[juce::jlimit (0, 5, scale)]) scaleMask[(key + i) % 12] = true;
    targetNote = -1;
}

int BlezzTuneProcessor::nearestScaleNote (float midi) const
{
    const int centre = (int) std::lround (midi);
    int best = centre; float bestDist = 1000.0f;
    for (int n = centre - 6; n <= centre + 6; ++n)
    {
        if (! scaleMask[((n % 12) + 12) % 12]) continue;
        const float d = std::abs (midi - (float) n);
        if (d < bestDist) { bestDist = d; best = n; }
    }
    // histéresis: no saltar de nota por un vibrato pequeño
    if (targetNote >= 0 && scaleMask[((targetNote % 12) + 12) % 12]
        && std::abs (midi - (float) targetNote) < bestDist + 0.12f)
        return targetNote;
    return best;
}

void BlezzTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (numCh == 0) return;

    updateScaleMask();
    const bool  tuneOn   = apvts.getRawParameterValue ("tuneOn")->load() > 0.5f;
    const float retune   = apvts.getRawParameterValue ("retune")->load();
    const float humanize = apvts.getRawParameterValue ("humanize")->load() / 100.0f;
    const bool  fetOn    = apvts.getRawParameterValue ("fetOn")->load() > 0.5f;
    const bool  optoOn   = apvts.getRawParameterValue ("optoOn")->load() > 0.5f;

    fet.setParams (apvts.getRawParameterValue ("fetIn")->load(), apvts.getRawParameterValue ("fetOut")->load(),
                   apvts.getRawParameterValue ("fetAtk")->load(), apvts.getRawParameterValue ("fetRel")->load(),
                   (int) apvts.getRawParameterValue ("fetRatio")->load());
    opto.setParams (apvts.getRawParameterValue ("optoPeak")->load(), apvts.getRawParameterValue ("optoGain")->load(),
                    apvts.getRawParameterValue ("optoMode")->load() > 0.5f);
    bright.setAmount (apvts.getRawParameterValue ("brillo")->load() / 100.0f);

    auto shiftCoeff = [&]
    {
        if (targetNote < 0) return blezz::coeffForTime (0.04, fs);
        const double held01 = juce::jlimit (0.0, 1.0, (heldSeconds - 0.15) / 0.3);
        return blezz::coeffForTime (retune / 1000.0 + humanize * held01 * 0.25, fs);
    };
    float aShift = shiftCoeff();
    const float aGrain = blezz::coeffForTime (0.03, fs);
    const float aWet   = blezz::coeffForTime (0.02, fs);
    const double minGrain = 0.022 * fs, maxGrain = 0.07 * fs;
    int samplesSinceUpdate = 0;

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float fetMax = 0.0f, optoMax = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float l = ch0[i];
        float r = ch1 ? ch1[i] : l;

        // ---------------- TUNE ----------------
        ++samplesSinceUpdate;
        if (tracker.push (0.5f * (l + r)))
        {
            const float f = tracker.getFrequency();
            if (f > 0.0f)
            {
                detectedMidi = 69.0f + 12.0f * std::log2 (f / 440.0f);
                const int note = nearestScaleNote (detectedMidi);
                if (note != targetNote) { targetNote = note; heldSeconds = 0.0; }
                else heldSeconds += samplesSinceUpdate / fs;
                desiredShift = (float) targetNote - detectedMidi;

                const double period = fs / f;
                const double k = std::ceil (minGrain / (2.0 * period));
                grainTarget = juce::jlimit (minGrain, maxGrain, 2.0 * k * period);
            }
            else
            {
                detectedMidi = -1.0f; targetNote = -1; heldSeconds = 0.0; desiredShift = 0.0f;
            }
            samplesSinceUpdate = 0;
            aShift = shiftCoeff();
        }

        if (tuneOn)
        {
            shift += aShift * (desiredShift - shift);
            grainLen += aGrain * (grainTarget - grainLen);
            const float wetTarget = std::abs (shift) > 0.02f ? 1.0f : 0.0f;
            wetAmount += aWet * (wetTarget - wetAmount);

            shifter.advance (std::pow (2.0, shift / 12.0), grainLen);
            const float sl = shifter.processSample (0, l);
            const float sr = ch1 ? shifter.processSample (1, r) : sl;
            shifter.endSample();
            l += wetAmount * (sl - l);
            r += wetAmount * (sr - r);
        }
        else
        {
            shift = 0.0f; wetAmount = 0.0f;
        }

        // ---------------- COMPRESIÓN (FET -> OPTO, cadena clásica) ----------------
        if (fetOn)  { fet.process (l, r);  fetMax  = std::max (fetMax,  fet.getGainReduction()); }
        if (optoOn) { opto.process (l, r); optoMax = std::max (optoMax, opto.getGainReduction()); }

        // ---------------- BRILLO ----------------
        ch0[i] = bright.process (0, l);
        if (ch1) ch1[i] = bright.process (1, r);
    }

    uiDetectedMidi = detectedMidi;
    uiTargetMidi   = (float) targetNote;
    uiCorrection   = tuneOn ? shift * 100.0f : 0.0f;   // en cents
    uiFetGR  = fetMax;
    uiOptoGR = optoMax;
}

//==============================================================================
int BlezzTuneProcessor::getNumPrograms() { return blezz::numPresets; }

const juce::String BlezzTuneProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, blezz::numPresets) ? juce::String::fromUTF8 (blezz::presets[index].name) : juce::String();
}

void BlezzTuneProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, blezz::numPresets)) return;
    const auto& p = blezz::presets[index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };
    set ("scale", (float) p.scale);
    set ("retune", p.retuneMs);
    set ("humanize", p.humanize);
    set ("fetOn", p.fetOn ? 1.0f : 0.0f);
    set ("fetIn", p.fetIn);   set ("fetOut", p.fetOut);
    set ("fetAtk", p.fetAtk); set ("fetRel", p.fetRel);
    set ("fetRatio", (float) p.fetRatio);
    set ("optoOn", p.optoOn ? 1.0f : 0.0f);
    set ("optoPeak", p.optoPeak); set ("optoGain", p.optoGain);
    set ("optoMode", (float) p.optoMode);
    set ("brillo", p.brillo);
    currentPreset = index;
    currentPresetAtomic = index;
}

void BlezzTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BlezzTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) tree.getProperty ("preset", 0);
            currentPresetAtomic = currentPreset;
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* BlezzTuneProcessor::createEditor() { return new BlezzTuneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BlezzTuneProcessor(); }
