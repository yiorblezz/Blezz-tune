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
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modo",
                                                   StringArray { "Estudio", "Tiempo Real" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 2));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "retune", 1 }, "Retune Speed",
                                                   NormalisableRange<float> (0.0f, 400.0f, 1.0f, 0.4f), 20.0f,
                                                   AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "humanize", 1 }, "Humanize",
                                                   NormalisableRange<float> (0.0f, 100.0f, 1.0f), 25.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "flex", 1 }, "Flex Tune",
                                                   NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "vibrato", 1 }, "Vibrato",
                                                   NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, pct));

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
    for (auto& h : histIn)  h = -1.0f;
    for (auto& h : histOut) h = -1.0f;
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
    psola.setRealtime (apvts.getRawParameterValue ("mode")->load() > 0.5f);
    psola.prepare (fs, ch);
    fet.prepare (fs);
    opto.prepare (fs);
    bright.prepare (fs, ch, samplesPerBlock);
    voiced = false; detectedMidi = midiSlow = -1.0f; period = 0.0f;
    targetNote = -1; heldSeconds = 0.0; shift = desiredShift = 0.0f;
    tuneMix = apvts.getRawParameterValue ("tuneOn")->load() > 0.5f ? 1.0f : 0.0f;
    lastKey = lastScale = -1;
    setLatencySamples (psola.getLatency());   // FL Studio compensa este retraso automáticamente
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
    for (int i = 0; i < 12; ++i) scaleMaskUi[(size_t) i] = scaleMask[i];
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
    // histéresis: un vibrato o un deslizamiento corto no cambia la nota objetivo
    if (targetNote >= 0 && scaleMask[((targetNote % 12) + 12) % 12]
        && std::abs (midi - (float) targetNote) < bestDist + 0.15f)
        return targetNote;
    return best;
}

void BlezzTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (numCh == 0) return;

    updateScaleMask();
    const bool realtime = apvts.getRawParameterValue ("mode")->load() > 0.5f;
    if (realtime != psola.isRealtime())
    {
        psola.setRealtime (realtime);
        setLatencySamples (psola.getLatency());
    }
    const bool  tuneOn   = apvts.getRawParameterValue ("tuneOn")->load() > 0.5f;
    const float retuneMs = apvts.getRawParameterValue ("retune")->load();
    const float humanize = apvts.getRawParameterValue ("humanize")->load() / 100.0f;
    const float flex     = apvts.getRawParameterValue ("flex")->load() / 100.0f;
    const float vibAmt   = apvts.getRawParameterValue ("vibrato")->load() / 100.0f;
    const bool  fetOn    = apvts.getRawParameterValue ("fetOn")->load() > 0.5f;
    const bool  optoOn   = apvts.getRawParameterValue ("optoOn")->load() > 0.5f;

    fet.setParams (apvts.getRawParameterValue ("fetIn")->load(), apvts.getRawParameterValue ("fetOut")->load(),
                   apvts.getRawParameterValue ("fetAtk")->load(), apvts.getRawParameterValue ("fetRel")->load(),
                   (int) apvts.getRawParameterValue ("fetRatio")->load());
    opto.setParams (apvts.getRawParameterValue ("optoPeak")->load(), apvts.getRawParameterValue ("optoGain")->load(),
                    apvts.getRawParameterValue ("optoMode")->load() > 0.5f);
    bright.setAmount (apvts.getRawParameterValue ("brillo")->load() / 100.0f);

    // Retune: 0 ms = salto instantáneo (efecto T-Pain / trap). Humanize alarga el tiempo en notas sostenidas.
    auto shiftCoeff = [&]
    {
        const double held01 = juce::jlimit (0.0, 1.0, (heldSeconds - 0.12) / 0.3);
        return blezz::coeffForTime (retuneMs / 1000.0 + humanize * held01 * 0.25, fs);
    };
    float aShift = shiftCoeff();
    const float aMix = blezz::coeffForTime (0.015, fs);
    const float vibPass = vibAmt / (1.0f + retuneMs / 20.0f);   // con retune lento el vibrato ya pasa solo
    const int histEvery = std::max (1, (int) (fs / 100.0));    // 100 puntos por segundo
    int samplesSinceUpdate = 0;

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float fetMax = 0.0f, optoMax = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float x[2] = { ch0[i], ch1 ? ch1[i] : ch0[i] };

        // ---------------- DETECCIÓN ----------------
        ++samplesSinceUpdate;
        if (tracker.push (numCh > 1 ? 0.5f * (x[0] + x[1]) : x[0]))
        {
            const bool wasVoiced = voiced;
            voiced = tracker.isVoiced();
            if (voiced)
            {
                const float f = tracker.getFrequency();
                period = (float) (fs / f);
                detectedMidi = 69.0f + 12.0f * std::log2 (f / 440.0f);
                if (! wasVoiced || midiSlow < 0.0f) midiSlow = detectedMidi;
                midiSlow += blezz::coeffForTime (0.18, fs / samplesSinceUpdate) * (detectedMidi - midiSlow);

                const int note = nearestScaleNote (detectedMidi);
                if (note != targetNote) { targetNote = note; heldSeconds = 0.0; }
                else heldSeconds += samplesSinceUpdate / fs;

                // Flex-Tune: solo corrige cuando ya estás cerca de la nota
                float amount = 1.0f;
                if (flex > 0.0f)
                {
                    const float devCents = std::abs (detectedMidi - (float) targetNote) * 100.0f;
                    const float inner = 50.0f * (1.0f - flex);
                    amount = juce::jlimit (0.0f, 1.0f, 1.0f - (devCents - inner) / 15.0f);
                }
                const float vib = detectedMidi - midiSlow;
                desiredShift = amount * (((float) targetNote - detectedMidi) + vibPass * vib);

                // inicio de nota: sin "scoop" desde la corrección anterior
                if (! wasVoiced && retuneMs < 60.0f) shift = desiredShift;
            }
            else
            {
                detectedMidi = -1.0f; targetNote = -1; heldSeconds = 0.0; midiSlow = -1.0f;
            }
            samplesSinceUpdate = 0;
            aShift = shiftCoeff();
        }

        if (voiced) shift += aShift * (desiredShift - shift);
        const float ratio = std::pow (2.0f, shift / 12.0f);

        // ---------------- AFINACIÓN (PSOLA) ----------------
        tuneMix += aMix * ((tuneOn ? 1.0f : 0.0f) - tuneMix);
        psola.process (x, voiced && tuneOn, period, tuneOn ? ratio : 1.0f);
        float l = x[0], r = numCh > 1 ? x[1] : x[0];
        if (tuneMix < 0.999f)
        {
            const float dl = psola.delayedDry (0), dr = numCh > 1 ? psola.delayedDry (1) : dl;
            l = dl + tuneMix * (l - dl);
            r = dr + tuneMix * (r - dr);
        }

        // ---------------- COMPRESIÓN (FET -> OPTO) ----------------
        if (fetOn)  { fet.process (l, r);  fetMax  = std::max (fetMax,  fet.getGainReduction()); }
        if (optoOn) { opto.process (l, r); optoMax = std::max (optoMax, opto.getGainReduction()); }

        // ---------------- BRILLO ----------------
        ch0[i] = bright.process (0, l);
        if (ch1) ch1[i] = bright.process (1, r);

        // ---------------- HISTORIAL PARA EL GRÁFICO ----------------
        if (++histCounter >= histEvery)
        {
            histCounter = 0;
            const int w = histWrite.load (std::memory_order_relaxed);
            histIn[(size_t) w]  = voiced ? detectedMidi : -1.0f;
            histOut[(size_t) w] = voiced ? detectedMidi + (tuneOn ? shift : 0.0f) : -1.0f;
            histWrite.store ((w + 1) % historySize, std::memory_order_release);
        }
    }

    uiDetectedMidi = voiced ? detectedMidi : -1.0f;
    uiTargetMidi   = voiced ? (float) targetNote : -1.0f;
    uiCorrection   = tuneOn && voiced ? shift * 100.0f : 0.0f;
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
    set ("flex", p.flex);
    set ("vibrato", p.vibrato);
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
