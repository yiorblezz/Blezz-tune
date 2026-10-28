#include "PluginEditor.h"
#include "Presets.h"

using namespace blezz;

static juce::Font boldFont (float h)  { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
static juce::Font plainFont (float h) { return juce::Font (juce::FontOptions (h)); }

//==============================================================================
TrapLookAndFeel::TrapLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, col::dim);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::track);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff120a1c));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, col::track);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::CaretComponent::caretColourId, col::accent2);
}

void TrapLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float stroke = std::max (3.0f, r * 0.09f);
    const float arcR = r - stroke;
    const float ang = a0 + pos * (a1 - a0);

    juce::Path trackArc; trackArc.addCentredArc (c.x, c.y, arcR, arcR, 0, a0, a1, true);
    g.setColour (col::track);
    g.strokePath (trackArc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc; valArc.addCentredArc (c.x, c.y, arcR, arcR, 0, a0, ang, true);
    g.setColour (col::accent.withAlpha (0.22f));
    g.strokePath (valArc, juce::PathStrokeType (stroke * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (col::accent, c.x - r, c.y + r, col::accent2, c.x + r, c.y - r, false));
    g.strokePath (valArc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = arcR - stroke * 1.6f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a1740), c.x, c.y - kr,
                                             juce::Colour (0xff0b0611), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (col::border);
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

    const juce::Point<float> p1 (c.x + std::sin (ang) * kr * 0.35f, c.y - std::cos (ang) * kr * 0.35f);
    const juce::Point<float> p2 (c.x + std::sin (ang) * kr * 0.82f, c.y - std::cos (ang) * kr * 0.82f);
    g.setColour (col::accent2.withAlpha (0.35f));
    g.drawLine ({ p1, p2 }, 5.0f);
    g.setColour (juce::Colours::white);
    g.drawLine ({ p1, p2 }, 2.2f);
}

void TrapLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                        float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float bw = std::min (34.0f, (float) w);
    auto track = juce::Rectangle<float> ((float) x + ((float) w - bw) * 0.5f, (float) y, bw, (float) h);
    g.setColour (col::track);
    g.fillRoundedRectangle (track, 10.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (track, 10.0f, 1.0f);

    auto fill = track.withTop (sliderPos);
    g.setColour (col::accent.withAlpha (0.25f));
    g.fillRoundedRectangle (fill.expanded (5.0f, 3.0f), 13.0f);
    g.setGradientFill (juce::ColourGradient (col::accent2, track.getCentreX(), track.getY(),
                                             col::accent, track.getCentreX(), track.getBottom(), false));
    g.fillRoundedRectangle (fill, 10.0f);

    // marcas
    g.setColour (col::bg0.withAlpha (0.55f));
    for (int i = 1; i < 10; ++i)
    {
        const float ly = track.getY() + track.getHeight() * (float) i / 10.0f;
        g.drawHorizontalLine ((int) ly, track.getX() + 6.0f, track.getRight() - 6.0f);
    }
    // tirador
    auto thumb = juce::Rectangle<float> (track.getX() - 6.0f, sliderPos - 4.0f, track.getWidth() + 12.0f, 8.0f);
    g.setColour (juce::Colours::white);
    g.fillRoundedRectangle (thumb, 4.0f);
}

void TrapLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    auto pill = juce::Rectangle<float> (2.0f, ((float) b.getHeight() - 18.0f) * 0.5f, 36.0f, 18.0f);
    if (on)
    {
        g.setColour (col::accent.withAlpha (0.3f));
        g.fillRoundedRectangle (pill.expanded (3.0f), 12.0f);
        g.setGradientFill (juce::ColourGradient (col::accent, pill.getX(), 0, col::accent2, pill.getRight(), 0, false));
    }
    else g.setColour (col::track);
    g.fillRoundedRectangle (pill, 9.0f);
    g.setColour (on ? juce::Colours::white : col::dim);
    const float cx = on ? pill.getRight() - 9.0f : pill.getX() + 9.0f;
    g.fillEllipse (cx - 6.5f, pill.getCentreY() - 6.5f, 13.0f, 13.0f);

    g.setColour (on ? col::text : col::dim);
    g.setFont (boldFont (12.5f));
    g.drawText (b.getButtonText(), 46, 0, b.getWidth() - 46, b.getHeight(), juce::Justification::centredLeft);
}

void TrapLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::track);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.hasKeyboardFocus (true) ? col::accent : col::border);
    g.drawRoundedRectangle (r, 8.0f, 1.2f);

    juce::Path arrow;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 5, ay - 2.5f); arrow.lineTo (ax, ay + 2.5f); arrow.lineTo (ax + 5, ay - 2.5f);
    g.setColour (col::accent2);
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font TrapLookAndFeel::getComboBoxFont (juce::ComboBox&) { return boldFont (14.0f); }
juce::Font TrapLookAndFeel::getPopupMenuFont()                { return plainFont (15.0f); }
juce::Font TrapLookAndFeel::getLabelFont (juce::Label& l)     { return l.getFont(); }

void TrapLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Label* TrapLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, col::text);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::accent);
    l->setFont (boldFont (13.0f));
    return l;
}

//==============================================================================
static juce::String noteName (int midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}


void PitchGraph::update()
{
    const int N = BlezzTuneProcessor::historySize;
    const int w = proc.histWrite.load (std::memory_order_acquire);
    in.resize ((size_t) N); out.resize ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const int idx = (w + i) % N;    // del más viejo al más nuevo
        in[(size_t) i]  = proc.histIn[(size_t) idx].load();
        out[(size_t) i] = proc.histOut[(size_t) idx].load();
    }
    // zoom automático: encuadrar todo lo que se ve en pantalla
    float lo = 1000.0f, hi = -1000.0f;
    for (int i = 0; i < N; ++i)
        for (float v : { in[(size_t) i], out[(size_t) i] })
            if (v > 0.0f) { lo = std::min (lo, v); hi = std::max (hi, v); }
    if (hi > lo)
    {
        const float c = 0.5f * (lo + hi);
        const float sp = juce::jlimit (5.0f, 18.0f, 0.5f * (hi - lo) + 1.5f);
        if (first) { centre = c; span = sp; first = false; }
        centre += 0.2f * (c - centre);
        span   += 0.2f * (sp - span);
    }
    det = proc.uiDetectedMidi.load(); tgt = proc.uiTargetMidi.load(); corr = proc.uiCorrection.load();
    repaint();
}

void PitchGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0717), r.getX(), r.getY(),
                                             juce::Colour (0xff05020a), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);

    auto plot = r.reduced (0, 12).withTrimmedLeft (38).withTrimmedRight (8);
    auto yFor = [&] (float midi) { return plot.getCentreY() - (midi - centre) / span * plot.getHeight() * 0.5f; };

    // líneas de notas (las de la escala resaltadas)
    g.setFont (plainFont (10.0f));
    for (int nn = (int) std::floor (centre - span); nn <= (int) std::ceil (centre + span); ++nn)
    {
        const float y = yFor ((float) nn);
        if (y < plot.getY() || y > plot.getBottom()) continue;
        const bool inScale = proc.isInScale (nn);
        g.setColour (inScale ? col::border.withAlpha (0.9f) : col::track.withAlpha (0.45f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        const bool roomy = plot.getHeight() / (2.0f * span) > 9.0f;
        const bool tonic = (((nn % 12) + 12) % 12) == (int) proc.apvts.getRawParameterValue ("key")->load();
        if (inScale && (roomy || tonic))
        {
            g.setColour (col::dim);
            g.drawText (noteName (nn), (int) r.getX() + 6, (int) y - 7, 32, 14, juce::Justification::centredLeft);
        }
    }

    // curvas
    const int N = (int) out.size();
    auto drawCurve = [&] (const std::vector<float>& v, juce::Colour c, float thick, bool glow)
    {
        juce::Path p; bool open = false;
        for (int i = 0; i < N; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (N - 1);
            const float y = v[(size_t) i] > 0.0f ? yFor (v[(size_t) i]) : -1.0e6f;
            if (y < plot.getY() - 2.0f || y > plot.getBottom() + 2.0f) { open = false; continue; }
            if (! open) { p.startNewSubPath (x, y); open = true; }
            else p.lineTo (x, y);
        }
        if (glow)
        {
            g.setColour (c.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (thick * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    if (N > 1)
    {
        drawCurve (in,  col::dim.withAlpha (0.8f), 1.4f, false);
        drawCurve (out, col::accent2, 2.4f, true);
    }

    g.setColour (col::border);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);

    // nota actual + corrección
    const bool voiced = det > 0.0f && tgt > 0.0f;
    auto box = juce::Rectangle<float> (plot.getX() + 6, r.getY() + 10, 116, 66);
    g.setColour (juce::Colour (0xcc07030c));
    g.fillRoundedRectangle (box, 10.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (box, 10.0f, 1.0f);
    g.setFont (boldFont (34.0f));
    g.setColour (voiced ? col::accent2 : col::dim.withAlpha (0.5f));
    g.drawText (voiced ? noteName ((int) tgt) : juce::String ("--"), box.withTrimmedBottom (20), juce::Justification::centred);
    g.setFont (plainFont (11.0f));
    g.setColour (col::dim);
    juce::String info = voiced ? juce::String ((int) std::lround (corr)) + "c corr." : juce::String ("sin voz");
    if (voiced && corr > 0) info = "+" + info;
    g.drawText (info, box.withTrimmedTop (44), juce::Justification::centred);

    // leyenda
    g.setFont (boldFont (10.0f));
    g.setColour (col::dim);
    g.drawText ("ENTRADA", (int) plot.getX() + 6, (int) r.getBottom() - 18, 60, 12, juce::Justification::centredLeft);
    g.setColour (col::accent2);
    g.drawText ("AFINADO", (int) plot.getX() + 66, (int) r.getBottom() - 18, 60, 12, juce::Justification::centredLeft);
}

void GRMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto bar = r.withTrimmedLeft (34).reduced (0, 4);
    g.setFont (boldFont (11.0f));
    g.setColour (col::dim);
    g.drawText ("GR", r.withWidth (30), juce::Justification::centredLeft);
    g.setColour (col::track);
    g.fillRoundedRectangle (bar, 4.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, gr / 20.0f);
    auto fill = bar.withWidth (bar.getWidth() * frac);
    g.setGradientFill (juce::ColourGradient (col::accent, bar.getX(), 0, col::accent2, bar.getRight(), 0, false));
    g.fillRoundedRectangle (fill, 4.0f);
    g.setColour (col::text);
    g.setFont (plainFont (11.0f));
    g.drawText (juce::String (-gr, 1) + " dB", bar.reduced (6, 0), juce::Justification::centredRight);
}


//==============================================================================
BlezzTuneEditor::BlezzTuneEditor (BlezzTuneProcessor& p) : AudioProcessorEditor (&p), proc (p), graph (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < numPresets; ++i)
        presetBox.addItem (juce::String::fromUTF8 (presets[i].name), i + 1);
    presetBox.setSelectedId (proc.currentPresetAtomic.load() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) { proc.applyPreset (idx); lastPreset = idx; }
    };
    addAndMakeVisible (presetBox);
    lastPreset = proc.currentPresetAtomic.load();

    setupToggle (tuneOn, tuneOnAtt, "tuneOn", "TUNE");
    setupToggle (fetOn, fetOnAtt, "fetOn", "ON");
    setupToggle (optoOn, optoOnAtt, "optoOn", "ON");

    setupCombo (modeBox, modeAtt, "mode", { "Estudio", "Tiempo Real" });
    setupCombo (keyBox, keyAtt, "key", BlezzTuneProcessor::keyNames());
    setupCombo (scaleBox, scaleAtt, "scale", BlezzTuneProcessor::scaleNames());
    setupCombo (ratioBox, ratioAtt, "fetRatio", { "4:1", "8:1", "12:1", "20:1", "ALL" });
    setupCombo (optoModeBox, optoModeAtt, "optoMode", { "Compress", "Limit" });

    auto setupSmallLabel = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (boldFont (11.0f));
        l.setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (l);
    };
    setupSmallLabel (modeLbl, "MODO");
    setupSmallLabel (keyLbl, "TONALIDAD");
    setupSmallLabel (scaleLbl, "ESCALA");
    setupSmallLabel (ratioLbl, "RATIO");
    setupSmallLabel (optoModeLbl, "MODO");
    setupSmallLabel (latencyLbl, "");
    latencyLbl.setFont (plainFont (10.5f));
    latencyLbl.setJustificationType (juce::Justification::centredRight);

    auto ms  = [] (double v) { return v < 0.5 ? juce::String ("0 ms") : juce::String ((int) v) + " ms"; };
    auto pct = [] (double v) { return juce::String ((int) std::lround (v)) + "%"; };
    auto db  = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto one = [] (double v) { return juce::String (v, 1); };

    setupKnob (retune,   "retune",   "RETUNE SPEED", ms, true);
    setupKnob (humanize, "humanize", "HUMANIZE", pct);
    setupKnob (flex,     "flex",     "FLEX-TUNE", pct);
    setupKnob (vibrato,  "vibrato",  "VIBRATO", pct);
    setupKnob (fetIn,    "fetIn",    "INPUT", db);
    setupKnob (fetOut,   "fetOut",   "OUTPUT", db);
    setupKnob (fetAtk,   "fetAtk",   "ATTACK", one);
    setupKnob (fetRel,   "fetRel",   "RELEASE", one);
    setupKnob (optoPeak, "optoPeak", "PEAK REDUCTION", pct);
    setupKnob (optoGain, "optoGain", "GAIN", db);

    brillo.setSliderStyle (juce::Slider::LinearVertical);
    brillo.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    brilloAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "brillo", brillo);
    brillo.textFromValueFunction = pct;
    brillo.updateText();
    addAndMakeVisible (brillo);

    addAndMakeVisible (graph);
    addAndMakeVisible (fetMeter);
    addAndMakeVisible (optoMeter);

    sendLookAndFeelChange();   // recrea las cajas de valor con el estilo trap
    setSize (980, 660);
    startTimerHz (30);
}

BlezzTuneEditor::~BlezzTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}
void BlezzTuneEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name,
                                 std::function<juce::String (double)> fmt, bool big)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.slider.textFromValueFunction = std::move (fmt);
    k.slider.updateText();
    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (boldFont (big ? 13.0f : 11.0f));
    k.label.setColour (juce::Label::textColourId, big ? col::text : col::dim);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void BlezzTuneEditor::setupCombo (juce::ComboBox& box, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                                  const juce::String& id, const juce::StringArray& items)
{
    box.addItemList (items, 1);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
    addAndMakeVisible (box);
}

void BlezzTuneEditor::setupToggle (juce::ToggleButton& b, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                   const juce::String& id, const juce::String& text)
{
    b.setButtonText (text);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
    addAndMakeVisible (b);
}

void BlezzTuneEditor::placeKnob (Knob& k, juce::Rectangle<int> a)
{
    k.label.setBounds (a.removeFromTop (18));
    k.slider.setBounds (a);
}

//==============================================================================
static void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    auto r = area.toFloat();
    g.setColour (col::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.0f), r.getX() + 14, 0,
                                             col::accent2, r.getCentreX(), 0, true));
    g.fillRect (r.getX() + 20, r.getY(), r.getWidth() - 40, 2.0f);

    g.setColour (col::accent2);
    g.fillEllipse (r.getX() + 16, r.getY() + 17, 7, 7);
    g.setColour (col::text);
    g.setFont (boldFont (14.0f));
    g.drawText (title, (int) r.getX() + 30, (int) r.getY() + 10, 220, 20, juce::Justification::centredLeft);
}


void BlezzTuneEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::bg1, 0, 0, col::bg0, 0, b.getHeight(), false));
    g.fillAll();

    // brillos morados de fondo
    g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.28f), 120, 40,
                                             col::accent.withAlpha (0.0f), 560, 300, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (col::accent2.withAlpha (0.12f), b.getRight() - 80, b.getBottom() - 40,
                                             col::accent2.withAlpha (0.0f), b.getRight() - 520, b.getBottom() - 320, true));
    g.fillAll();

    // marca de agua 777
    g.setColour (col::accent.withAlpha (0.05f));
    g.setFont (boldFont (200.0f));
    g.drawText ("777", getLocalBounds().withTrimmedTop (300).translated (250, 0), juce::Justification::centred);

    // logo
    g.setFont (boldFont (36.0f));
    const juce::String a = "BLEZZ", t = " TUNE";
    const int x0 = 22, y0 = 14;
    const int aw = juce::GlyphArrangement::getStringWidthInt (boldFont (36.0f), a);
    const int tw = juce::GlyphArrangement::getStringWidthInt (boldFont (36.0f), t);
    g.setColour (col::accent.withAlpha (0.35f));
    g.drawText (a + t, x0 + 1, y0 + 2, 400, 42, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white);
    g.drawText (a, x0, y0, 300, 40, juce::Justification::centredLeft);
    g.setGradientFill (juce::ColourGradient (col::accent2, (float) (x0 + aw), 0, col::accent, (float) (x0 + aw + 120), 0, false));
    g.drawText (t, x0 + aw, y0, 300, 40, juce::Justification::centredLeft);

    // insignia V2
    auto badge = juce::Rectangle<float> ((float) (x0 + aw + tw + 10), (float) y0 + 9, 36, 22);
    g.setGradientFill (juce::ColourGradient (col::accent, badge.getX(), 0, col::accent2, badge.getRight(), 0, false));
    g.fillRoundedRectangle (badge, 6.0f);
    g.setColour (juce::Colours::white);
    g.setFont (boldFont (14.0f));
    g.drawText ("V2", badge, juce::Justification::centred);

    g.setFont (boldFont (10.5f));
    g.setColour (col::dim);
    g.drawText ("PSOLA VOCAL SUITE  /  TRAP EDITION  /  777", x0 + 2, y0 + 40, 420, 16, juce::Justification::centredLeft);

    g.drawText ("PRESET", getWidth() - 330, 30, 60, 20, juce::Justification::centredLeft);

    drawPanel (g, tunePanel,   "AUTO TUNE");
    drawPanel (g, fetPanel,    "FET 76  /  COMPRESOR");
    drawPanel (g, optoPanel,   "OPTO 2A  /  LEVELER");
    drawPanel (g, brilloPanel, "BRILLO");
}

void BlezzTuneEditor::resized()
{
    presetBox.setBounds (getWidth() - 270, 24, 250, 32);

    tunePanel   = { 16, 88, 948, 282 };
    fetPanel    = { 16, 382, 440, 262 };
    optoPanel   = { 466, 382, 320, 262 };
    brilloPanel = { 796, 382, 168, 262 };

    // AUTO TUNE
    {
        const int x = tunePanel.getX(), y = tunePanel.getY();
        tuneOn.setBounds (x + 16, y + 40, 110, 24);
        modeLbl.setBounds  (x + 14, y + 74, 150, 16);
        modeBox.setBounds  (x + 16, y + 92, 156, 30);
        keyLbl.setBounds   (x + 14, y + 130, 150, 16);
        keyBox.setBounds   (x + 16, y + 148, 156, 30);
        scaleLbl.setBounds (x + 14, y + 186, 150, 16);
        scaleBox.setBounds (x + 16, y + 204, 156, 30);
        latencyLbl.setBounds (x + 14, y + 240, 160, 30);
        latencyLbl.setJustificationType (juce::Justification::centredLeft);

        graph.setBounds (x + 190, y + 14, 450, 254);

        const int kx = x + 650, kw = 145, kh = 130;
        placeKnob (retune,   { kx,      y + 10,      kw, kh });
        placeKnob (humanize, { kx + kw, y + 10,      kw, kh });
        placeKnob (flex,     { kx,      y + 12 + kh, kw, kh });
        placeKnob (vibrato,  { kx + kw, y + 12 + kh, kw, kh });
    }
    // FET 76
    {
        const int x = fetPanel.getX(), y = fetPanel.getY();
        fetOn.setBounds (fetPanel.getRight() - 90, y + 9, 80, 24);
        Knob* ks[] = { &fetIn, &fetOut, &fetAtk, &fetRel };
        for (int i = 0; i < 4; ++i) placeKnob (*ks[i], { x + 8 + i * 106, y + 42, 104, 140 });
        ratioLbl.setBounds (x + 14, y + 190, 80, 16);
        ratioBox.setBounds (x + 16, y + 208, 118, 30);
        fetMeter.setBounds (x + 152, y + 211, fetPanel.getWidth() - 170, 24);
    }
    // OPTO 2A
    {
        const int x = optoPanel.getX(), y = optoPanel.getY();
        optoOn.setBounds (optoPanel.getRight() - 90, y + 9, 80, 24);
        placeKnob (optoPeak, { x + 8,   y + 42, 150, 140 });
        placeKnob (optoGain, { x + 162, y + 42, 150, 140 });
        optoModeLbl.setBounds (x + 14, y + 190, 80, 16);
        optoModeBox.setBounds (x + 16, y + 208, 118, 30);
        optoMeter.setBounds (x + 146, y + 211, optoPanel.getWidth() - 162, 24);
    }
    // BRILLO
    brillo.setBounds (brilloPanel.getX() + 40, brilloPanel.getY() + 42, 88, 206);
}

void BlezzTuneEditor::timerCallback()
{
    graph.update();
    fetMeter.setGR (proc.apvts.getRawParameterValue ("fetOn")->load() > 0.5f ? proc.uiFetGR.load() : 0.0f);
    optoMeter.setGR (proc.apvts.getRawParameterValue ("optoOn")->load() > 0.5f ? proc.uiOptoGR.load() : 0.0f);

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    const bool rt = proc.apvts.getRawParameterValue ("mode")->load() > 0.5f;
    latencyLbl.setText (rt ? "Baja latencia: para grabar\nescuchandote en vivo"
                           : "Maxima calidad  (" + juce::String (juce::roundToInt (proc.getLatencySamples() * 1000.0 / sr)) + " ms,\nFL lo compensa solo)",
                        juce::dontSendNotification);

    const int p = proc.currentPresetAtomic.load();
    if (p != lastPreset)
    {
        lastPreset = p;
        presetBox.setSelectedId (p + 1, juce::dontSendNotification);
    }
}
