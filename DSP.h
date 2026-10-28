#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace blezz
{
inline float dbToGain (float db)  { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g)   { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coeffForTime (double seconds, double fs)
{
    return seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * fs)));
}

//==============================================================================
// Detector de tono YIN v2: diezmado a ~24 kHz, histéresis de sonoridad,
// filtro de mediana y protección contra saltos de octava.
class PitchTracker
{
public:
    static constexpr double minFreq = 80.0, maxFreq = 1000.0;

    void prepare (double sampleRate)
    {
        D = std::max (1, (int) std::lround (sampleRate / 24000.0));
        fs = sampleRate / D;
        tauMin = std::max (2, (int) (fs / maxFreq));
        tauMax = (int) std::ceil (fs / minFreq) + 2;
        W = std::max ((int) std::lround (0.030 * fs), tauMax * 2 + 8);
        hop = std::max (16, (int) std::lround (0.0025 * fs));   // nueva estimación cada 2.5 ms
        ring.assign ((size_t) W, 0.0f);
        frame.assign ((size_t) W, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        lp1 = lp2 = 0.0f;
        lpA = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 1400.0 / (fs * D)));
        writePos = 0; counter = 0; acc = 0.0f; accN = 0;
        freq = 0.0f; voiced = false; hist[0] = hist[1] = hist[2] = 0.0f;
    }

    bool push (float x)
    {
        // pasa-bajos suave antes de diezmar: quita sibilancia y estabiliza la detección
        lp1 += lpA * (x - lp1);
        lp2 += lpA * (lp1 - lp2);
        acc += lp2;
        if (++accN < D) return false;
        ring[(size_t) writePos] = acc / (float) D;
        acc = 0.0f; accN = 0;
        writePos = (writePos + 1) % W;
        if (++counter < hop) return false;
        counter = 0;
        analyse();
        return true;
    }

    bool  isVoiced() const     { return voiced; }
    float getFrequency() const { return freq; }

private:
    void analyse()
    {
        double energy = 0.0;
        for (int i = 0; i < W; ++i)
        {
            const float v = ring[(size_t) ((writePos + i) % W)];
            frame[(size_t) i] = v;
            energy += v * v;
        }
        if (std::sqrt (energy / W) < 0.0025) { setUnvoiced(); return; }

        const int len = W - tauMax;
        const float* f = frame.data();
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const float* b = f + tau;
            for (int j = 0; j < len; ++j) { const float d = f[j] - b[j]; s += d * d; }
            running += s;
            diff[(size_t) tau] = (float) (running > 0.0 ? s * tau / running : 1.0);
        }

        const float thr = voiced ? 0.22f : 0.12f;   // histéresis
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < thr)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0) { setUnvoiced(); return; }

        // verificación de octava: si el doble del periodo encaja mucho mejor, la nota real es una octava abajo
        // (pasa en vocales como "e" o "i", donde un formante cae justo en el 2º armónico)
        for (int pass = 0; pass < 2; ++pass)
        {
            const int t2 = best * 2;
            if (t2 + 2 >= tauMax) break;
            int b2 = t2;
            for (int k = t2 - 2; k <= t2 + 2; ++k) if (diff[(size_t) k] < diff[(size_t) b2]) b2 = k;
            if (diff[(size_t) best] > 0.025f && diff[(size_t) b2] < 0.5f * diff[(size_t) best]) best = b2;
            else break;
        }

        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const float den = s0 + s2 - 2.0f * s1;
            if (std::abs (den) > 1.0e-9f) t += 0.5f * (s0 - s2) / den;
        }
        float fNew = (float) (fs / t);

        // protección de octava: si salta justo una octava respecto a lo anterior, se descarta 1 vez
        if (voiced && freq > 0.0f)
        {
            const float r = fNew / freq;
            if ((std::abs (r - 2.0f) < 0.12f || std::abs (r - 0.5f) < 0.03f) && ++octaveStrikes < 3)
                fNew = freq;
            else if (std::abs (r - 2.0f) >= 0.12f && std::abs (r - 0.5f) >= 0.03f)
                octaveStrikes = 0;
        }

        if (! voiced) { hist[0] = hist[1] = hist[2] = fNew; }
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = fNew;
        const float a = hist[0], b = hist[1], c = hist[2];
        freq = std::max (std::min (a, b), std::min (std::max (a, b), c));   // mediana de 3
        voiced = true;
    }

    void setUnvoiced() { voiced = false; freq = 0.0f; octaveStrikes = 0; }

    double fs = 24000.0;
    int D = 2, W = 720, hop = 60, tauMin = 24, tauMax = 302, writePos = 0, counter = 0, accN = 0;
    int octaveStrikes = 0;
    float acc = 0.0f, lp1 = 0.0f, lp2 = 0.0f, lpA = 0.3f;
    std::vector<float> ring, frame, diff;
    float freq = 0.0f, hist[3] {};
    bool voiced = false;
};

//==============================================================================
// Motor TD-PSOLA en tiempo real (Pitch-Synchronous Overlap-Add).
// Corta la voz en granos de 2 periodos centrados en cada ciclo glotal y los
// reubica con el nuevo periodo: cambia la afinación sin mover los formantes
// (la voz no se pone "ardilla" ni "monstruo") y sin el "phasing" de un shifter normal.
class PsolaShifter
{
public:
    void prepare (double sampleRate, int channels)
    {
        fs = sampleRate;
        numCh = std::max (1, std::min (2, channels));
        Pmax = (int) std::ceil (fs / PitchTracker::minFreq);
        Pmin = std::max (8, (int) std::floor (fs / PitchTracker::maxFreq));
        Punv = std::max (16.0, std::round (0.005 * fs));
        latency = computeLatency();

        int n = 1; while (n < (int) std::ceil (2.6 * Pmax) * 4 + Pmax * 4) n <<= 1;
        size = n; mask = n - 1;
        for (int c = 0; c < 2; ++c)
        {
            in[(size_t) c].assign ((size_t) n, 0.0f);
            ola[(size_t) c].assign ((size_t) n, 0.0f);
        }
        mono.assign ((size_t) n, 0.0f);
        wsum.assign ((size_t) n, 0.0f);

        t = 0; head = tail = cursor = 0;
        lastMark = 0.0; lastMarkVoiced = false; lastPeriod = Punv;
        sNext = 0.0;
    }

    int getLatency() const { return latency; }

    // Estudio: espera a tener el ciclo exacto (mejor calidad, ~32 ms que el DAW compensa).
    // Tiempo real: usa el último ciclo ya escuchado (latencia mínima para monitorear al grabar).
    void setRealtime (bool rt)
    {
        if (rt == realtime) return;
        realtime = rt;
        if (size > 0) prepare (fs, numCh);
    }
    bool isRealtime() const { return realtime; }

    // x: muestras de entrada (se sobrescriben con la salida, retrasada `latency` muestras)
    void process (float* x, bool voiced, float period, float ratio)
    {
        const int64_t tm = t & mask;
        float m = 0.0f;
        for (int c = 0; c < numCh; ++c) { in[(size_t) c][(size_t) tm] = x[c]; m += x[c]; }
        mono[(size_t) tm] = m / (float) numCh;

        generateMarks (voiced, period, ratio);
        synthesise();

        const int64_t o = t - latency;
        const int64_t om = o & mask;
        const float w = wsum[(size_t) om];
        const float norm = 1.0f / std::max (w, 0.35f);
        for (int c = 0; c < numCh; ++c)
        {
            x[c] = o >= 0 ? ola[(size_t) c][(size_t) om] * norm : 0.0f;
            ola[(size_t) c][(size_t) om] = 0.0f;
        }
        wsum[(size_t) om] = 0.0f;
        ++t;
    }

    // salida "dry" con la misma latencia (para el bypass sin saltos)
    float delayedDry (int c) const
    {
        const int64_t o = t - 1 - latency;
        return o >= 0 ? in[(size_t) c][(size_t) (o & mask)] : 0.0f;
    }

private:
    struct Mark { double pos; double period; float ratio; bool voiced; };

    int computeLatency() const
    {
        return realtime ? std::max (16, (int) std::lround (0.002 * fs))
                        : (int) std::ceil (2.6 * Pmax) + 6;
    }

    float monoAt (int64_t i) const { return mono[(size_t) (i & mask)]; }

    // interpolación Hermite de 4 puntos (posiciones fraccionarias sin perder agudos)
    float readFrac (int c, double pos) const
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float f = (float) (pos - (double) i);
        const auto& b = in[(size_t) c];
        const float xm1 = b[(size_t) ((i - 1) & mask)], x0 = b[(size_t) (i & mask)];
        const float x1 = b[(size_t) ((i + 1) & mask)], x2 = b[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    void generateMarks (bool voiced, float period, float ratio)
    {
        const double P = voiced ? juce::jlimit ((double) Pmin, (double) Pmax, (double) period) : Punv;
        const double cand = lastMark + P;
        if ((double) t < cand + std::ceil (0.6 * P) + 3) return;

        double pos = cand;
        const int64_t ci = (int64_t) std::llround (cand);
        if (voiced && ! lastMarkVoiced)
        {
            // inicio de nota: anclar la marca al pico de energía del ciclo (cierre glótico)
            const int h = std::max (2, (int) (P / 8));
            double best = -1.0; int bestTau = 0;
            const int lo = -(int) (P / 2), hi = std::min ((int) (P / 2), (int) (t - ci) - h - 2);
            for (int tau = lo; tau < hi; ++tau)
            {
                double e = 0.0;
                for (int k = -h; k <= h; ++k) { const float v = monoAt (ci + tau + k); e += v * v; }
                if (e > best) { best = e; bestTau = tau; }
            }
            pos = (double) (ci + bestTau);
        }
        else if (voiced && lastMarkVoiced)
        {
            // seguir el mismo punto del ciclo: máxima correlación con el ciclo anterior (con precisión sub-muestra)
            const int h = (int) (P / 2);
            const int range = std::max (2, (int) (P / 10));
            const int64_t li = (int64_t) std::llround (lastMark);
            const double lf = lastMark - (double) li;
            double corr[64]; const int R = std::min (range, 31);
            for (int tau = -R; tau <= R; ++tau)
            {
                double s = 0.0;
                for (int k = -h; k <= h; ++k) s += (double) monoAt (li + k) * monoAt (ci + tau + k);
                corr[tau + R] = s;
            }
            int bi = 0;
            for (int i = 1; i <= 2 * R; ++i) if (corr[i] > corr[bi]) bi = i;
            double frac = 0.0;
            if (bi > 0 && bi < 2 * R)
            {
                const double a = corr[bi - 1], b = corr[bi], c = corr[bi + 1], den = a + c - 2.0 * b;
                if (std::abs (den) > 1.0e-12) frac = juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / den);
            }
            pos = (double) ci + (bi - R) + frac + lf;
        }

        if (head - tail >= capacity) ++tail;
        if (cursor < tail) cursor = tail;
        marks[(size_t) (head % capacity)] = { pos, P, voiced ? juce::jlimit (0.5f, 2.0f, ratio) : 1.0f, voiced };
        ++head;
        lastMark = pos; lastMarkVoiced = voiced; lastPeriod = P;
    }

    bool usable (const Mark& m) const { return m.pos + m.period + 3.0 <= (double) t; }

    void synthesise()
    {
        if (head == tail) return;
        const int64_t o = t - latency;
        for (;;)
        {
            // Los granos se generan "lo más tarde posible": justo antes de que su primera muestra salga.
            const double lead = realtime ? lastPeriod : (double) Pmax;
            if (sNext > (double) o + lead) break;

            while (cursor + 1 < head && marks[(size_t) ((cursor + 1) % capacity)].pos <= sNext) ++cursor;
            int64_t pick = cursor;
            if (cursor + 1 < head)
            {
                const Mark& cur = marks[(size_t) (cursor % capacity)];
                const Mark& nx  = marks[(size_t) ((cursor + 1) % capacity)];
                if (std::abs (nx.pos - sNext) < std::abs (cur.pos - sNext)) pick = cursor + 1;
            }
            while (pick > tail && ! usable (marks[(size_t) (pick % capacity)])) --pick;
            const Mark mk = marks[(size_t) (pick % capacity)];

            while (tail < cursor && tail < pick && marks[(size_t) (tail % capacity)].pos < sNext - 3.0 * Pmax) ++tail;

            const double Pa = mk.period;
            const int64_t s0 = (int64_t) std::floor (sNext);
            const double fsh = sNext - (double) s0;               // desfase fraccionario de la salida
            const int K = (int) std::ceil (Pa);
            const int kStart = (int) std::max<int64_t> (-K, o - s0);
            const double inv = juce::MathConstants<double>::pi / Pa;
            for (int k = kStart; k <= K; ++k)
            {
                const double d = (double) k - fsh;                  // distancia al centro del grano
                if (d <= -Pa || d >= Pa) continue;
                const double src = mk.pos + d;
                if (src + 2.0 >= (double) t) break;                  // nunca leer el futuro
                const float w = (float) (0.5 + 0.5 * std::cos (inv * d));
                const size_t di = (size_t) ((s0 + k) & mask);
                for (int c = 0; c < numCh; ++c) ola[(size_t) c][di] += w * readFrac (c, src);
                wsum[di] += w;
            }
            sNext += Pa / (double) mk.ratio;
        }
    }

    static constexpr int64_t capacity = 2048;
    double fs = 44100.0;
    int numCh = 2, Pmax = 600, Pmin = 48, latency = 1600;
    double Punv = 240.0;
    int64_t size = 0, mask = 0, t = 0;
    std::array<std::vector<float>, 2> in, ola;
    std::vector<float> mono, wsum;
    std::array<Mark, (size_t) capacity> marks {};
    int64_t head = 0, tail = 0, cursor = 0;
    double lastMark = 0.0, lastPeriod = 240.0, sNext = 0.0;
    bool lastMarkVoiced = false, realtime = false;
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
