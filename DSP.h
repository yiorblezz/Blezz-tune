#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace blezz
{
inline float dbToGain (float db)  { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g)   { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coeffForTime (double seconds, double fs)
{
    return seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * fs)));
}

//==============================================================================
// Detector de tono YIN (monofónico, pensado para voz)
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        // se analiza a ~24 kHz (diezmado) para menos latencia y menos CPU
        D = std::max (1, (int) std::lround (sampleRate / 24000.0));
        fs = sampleRate / D;
        W = (int) std::lround (0.032 * fs);
        hop = std::max (16, W / 8);
        tauMin = std::max (2, (int) (fs / 1000.0));
        tauMax = std::min (W / 2, (int) (fs / 75.0));
        ring.assign ((size_t) W, 0.0f);
        frame.assign ((size_t) W, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        writePos = 0; counter = 0; freq = 0.0f; acc = 0.0f; accN = 0;
    }

    // devuelve true cuando hay una estimación nueva
    bool push (float x)
    {
        acc += x;
        if (++accN < D) return false;
        ring[(size_t) writePos] = acc / (float) D;
        acc = 0.0f; accN = 0;
        writePos = (writePos + 1) % W;
        if (++counter < hop) return false;
        counter = 0;
        analyse();
        return true;
    }

    float getFrequency() const { return freq; }   // 0 = sin tono (silencio / consonante)

private:
    void analyse()
    {
        double energy = 0.0;
        for (int i = 0; i < W; ++i)
        {
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % W)];
            energy += frame[(size_t) i] * frame[(size_t) i];
        }
        if (std::sqrt (energy / W) < 0.003) { freq = 0.0f; return; } // ~ -50 dBFS

        const int len = W - tauMax;
        const float* f = frame.data();
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const float* a = f;
            const float* b = f + tau;
            for (int j = 0; j < len; ++j) { const float d = a[j] - b[j]; s += d * d; }
            running += s;
            diff[(size_t) tau] = (float) (running > 0.0 ? s * tau / running : 1.0);
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0) { freq = 0.0f; return; }

        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const float den = s0 + s2 - 2.0f * s1;
            if (std::abs (den) > 1.0e-9f) t += 0.5f * (s0 - s2) / den;
        }
        freq = (float) (fs / t);
    }

    double fs = 24000.0;
    int D = 2, W = 768, hop = 96, tauMin = 24, tauMax = 320, writePos = 0, counter = 0, accN = 0;
    float acc = 0.0f;
    std::vector<float> ring, frame, diff;
    float freq = 0.0f;
};

//==============================================================================
// Pitch shifter granular de dos lecturas cruzadas (baja latencia)
class GrainShifter
{
public:
    void prepare (int numChannels, int maxLenSamples)
    {
        int n = 1; while (n < maxLenSamples + 8) n <<= 1;
        size = n; mask = n - 1;
        buffers.assign ((size_t) numChannels, std::vector<float> ((size_t) n, 0.0f));
        writePos = 0; phase = 0.0;
    }

    // avanza la fase una muestra (llamar una vez por muestra, antes de processSample)
    void advance (double ratio, double grainLen)
    {
        phase += (1.0 - ratio) / grainLen;
        phase -= std::floor (phase);
        L = grainLen;
    }

    float processSample (int ch, float x)
    {
        auto& b = buffers[(size_t) ch];
        b[(size_t) writePos] = x;
        float out = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            double fr = phase + 0.5 * k; fr -= std::floor (fr);
            const double d = 1.0 + fr * L;
            const float g = (float) std::pow (std::sin (juce::MathConstants<double>::pi * fr), 2.0);
            const double rp = (double) writePos - d;
            const int i0 = (int) std::floor (rp);
            const float frac = (float) (rp - i0);
            const float s0 = b[(size_t) (i0 & mask)], s1 = b[(size_t) ((i0 + 1) & mask)];
            out += g * (s0 + frac * (s1 - s0));
        }
        return out;
    }

    void endSample() { writePos = (writePos + 1) & mask; }

private:
    std::vector<std::vector<float>> buffers;
    int size = 0, mask = 0, writePos = 0;
    double phase = 0.0, L = 1024.0;
};

//==============================================================================
// Compresor FET estilo "1176"
class FetComp
{
public:
    void prepare (double sampleRate) { fs = sampleRate; env = 0.0f; gr = 0.0f; }

    void setParams (float inDb, float outDb, float atkKnob, float relKnob, int ratioIdx)
    {
        inGain = dbToGain (inDb); outGain = dbToGain (outDb);
        // 1176: el 7 es lo más rápido
        const double atk = 800.0e-6 * std::pow (20.0 / 800.0, (atkKnob - 1.0) / 6.0);
        const double rel = 1.100 * std::pow (0.050 / 1.100, (relKnob - 1.0) / 6.0);
        allButtons = ratioIdx == 4;
        static const float ratios[] = { 4.0f, 8.0f, 12.0f, 20.0f, 20.0f };
        ratio = ratios[juce::jlimit (0, 4, ratioIdx)];
        threshold = ratioIdx == 0 ? -16.0f : -14.0f;
        knee = allButtons ? 1.0f : 3.0f;
        aAtk = coeffForTime (allButtons ? atk * 0.5 : atk, fs);
        aRel = coeffForTime (rel, fs);
        aDet = coeffForTime (0.0005, fs);
        sat = allButtons ? 0.35f : 0.12f;
    }

    // procesa una muestra estéreo vinculada
    void process (float& l, float& r)
    {
        l *= inGain; r *= inGain;
        const float peak = std::max (std::abs (l), std::abs (r));
        env += (peak > env ? 1.0f : aDet) * (peak - env);
        const float lvl = gainToDb (env);
        const float over = lvl - threshold;
        float target = 0.0f;
        if (over > knee) target = over * (1.0f - 1.0f / ratio);
        else if (over > -knee) { const float x = over + knee; target = (1.0f - 1.0f / ratio) * x * x / (4.0f * knee); }
        if (allButtons) target *= 1.25f;
        gr += (target > gr ? aAtk : aRel) * (target - gr);
        const float g = dbToGain (-gr);
        l = shape (l * g) * outGain;
        r = shape (r * g) * outGain;
    }

    float getGainReduction() const { return gr; }

private:
    float shape (float x) const { return x * (1.0f - sat) + std::tanh (x) * sat; }

    double fs = 44100.0;
    float inGain = 1, outGain = 1, ratio = 4, threshold = -20, knee = 3, sat = 0.12f;
    float aAtk = 0.1f, aRel = 0.001f, aDet = 0.1f, env = 0, gr = 0;
    bool allButtons = false;
};

//==============================================================================
// Compresor óptico estilo "LA-2A" (ataque fijo, release de dos etapas según programa)
class OptoComp
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; env = 0; gr = 0; memory = 0;
        aDet = coeffForTime (0.005, fs);
        aAtk = coeffForTime (0.010, fs);
        aMemUp = coeffForTime (1.5, fs);
        aMemDown = coeffForTime (4.0, fs);
    }

    void setParams (float peakReduction, float gainDb, bool limitMode)
    {
        threshold = -peakReduction * 0.5f;          // 0..100 -> 0..-50 dBFS
        ratio = limitMode ? 12.0f : 3.0f;
        makeup = dbToGain (gainDb);
    }

    void process (float& l, float& r)
    {
        const float sq = 0.5f * (l * l + r * r);
        env += aDet * (sq - env);
        const float lvl = 10.0f * std::log10 (std::max (env, 1.0e-12f));
        const float over = lvl - threshold;
        const float knee = 10.0f;
        float target = 0.0f;
        if (over > knee) target = over * (1.0f - 1.0f / ratio);
        else if (over > -knee) { const float x = over + knee; target = (1.0f - 1.0f / ratio) * x * x / (4.0f * knee); }

        memory += (target > 3.0f ? aMemUp * (1.0f - memory) : aMemDown * (0.0f - memory));
        const float relTime = 0.06f + 2.5f * memory;    // 60 ms ... ~2.5 s
        const float aRel = coeffForTime (relTime, fs);
        gr += (target > gr ? aAtk : aRel) * (target - gr);

        const float g = dbToGain (-gr) * makeup;
        l = tube (l * g); r = tube (r * g);
    }

    float getGainReduction() const { return gr; }

private:
    static float tube (float x) { return 0.9f * x + 0.1f * std::tanh (1.5f * x) / 1.5f; }

    double fs = 44100.0;
    float threshold = 0, ratio = 3, makeup = 1;
    float env = 0, gr = 0, memory = 0, aDet = 0, aAtk = 0, aMemUp = 0, aMemDown = 0;
};

//==============================================================================
// Brillo: excitador suave + dos high-shelf (presencia y aire)
class Brightness
{
public:
    void prepare (double sampleRate, int numChannels, int blockSize)
    {
        fs = sampleRate;
        juce::dsp::ProcessSpec spec { fs, (juce::uint32) blockSize, 1 };
        shelfA.resize ((size_t) numChannels); shelfB.resize ((size_t) numChannels);
        hpState.assign ((size_t) numChannels, 0.0f); hpPrev.assign ((size_t) numChannels, 0.0f);
        for (auto& f : shelfA) f.prepare (spec);
        for (auto& f : shelfB) f.prepare (spec);
        hpCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 3500.0 / fs);
        lastAmount = -1.0f;
        setAmount (0.0f);
    }

    void setAmount (float amount01)
    {
        if (std::abs (amount01 - lastAmount) < 0.002f) return;
        lastAmount = amount01;
        const double nyq = fs * 0.45;
        auto a = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, std::min (6500.0, nyq), 0.6f, dbToGain (amount01 * 8.0f));
        auto b = juce::dsp::IIR::Coefficients<float>::makeHighShelf (fs, std::min (12500.0, nyq), 0.7f, dbToGain (amount01 * 5.0f));
        for (auto& f : shelfA) *f.coefficients = *a;
        for (auto& f : shelfB) *f.coefficients = *b;
        excite = amount01 * 0.22f;
    }

    float process (int ch, float x)
    {
        // pasa-altos de 1 polo -> saturación -> armónicos de agudos
        auto& s = hpState[(size_t) ch]; auto& p = hpPrev[(size_t) ch];
        s = hpCoeff * (s + x - p); p = x;
        x += excite * std::tanh (3.0f * s) / 3.0f;
        x = shelfA[(size_t) ch].processSample (x);
        return shelfB[(size_t) ch].processSample (x);
    }

private:
    double fs = 44100.0;
    std::vector<juce::dsp::IIR::Filter<float>> shelfA, shelfB;
    std::vector<float> hpState, hpPrev;
    float hpCoeff = 0.6f, excite = 0.0f, lastAmount = -1.0f;
};
} // namespace blezz
