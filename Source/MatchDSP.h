#pragma once
// MATCH TO A (v1.10): bring your sample C near the pad's own sound A.
//   EQ    - 26 third-octave bands (the Suno Chain match EQ: iterative solving, so the cascade really hits the curve)
//   SHAPE - the loudness over time in 8 bands, 10 moments over the first 400 ms (a time-varying EQ: the crack of
//           the attack and the colour of the body each get their own)
//   ROOM  - a short room (8-line feedback delay network) when A has more room than C
// All of it is measured from the two sounds and checked again after rendering (closed loop).
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace mt
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kBands = 26, kEnvB = 8, kEnvT = 10;
inline const std::array<float, kBands>& centres()
{
    static const std::array<float, kBands> c { 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
                                               1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000 };
    return c;
}
inline const double* envEdges() { static const double e[kEnvT + 1] = { 0, 0.005, 0.012, 0.022, 0.036, 0.055, 0.08, 0.12, 0.18, 0.26, 0.40 }; return e; }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float process(float x, int ch) { double y = b0 * x + z1[ch]; z1[ch] = b1 * x - a1 * y + z2[ch]; z2[ch] = b2 * x - a2 * y; return (float) y; }
    void setNorm(double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void setPeak(double fs, double f, double q, double db)
    {
        f = std::min(f, fs * 0.45);
        const double A = std::pow(10.0, db / 40.0), w = 2 * kPi * f / fs, al = std::sin(w) / (2 * q), c = std::cos(w);
        setNorm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLPF(double fs, double f) { f = std::min(f, fs * 0.45); const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * 0.7071), c = std::cos(w); setNorm((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void setHPF(double fs, double f) { f = std::min(f, fs * 0.45); const double w = 2 * kPi * f / fs, al = std::sin(w) / (2 * 0.7071), c = std::cos(w); setNorm((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    double magDb(double fs, double f) const
    {
        const std::complex<double> z = std::polar(1.0, -2 * kPi * f / fs);
        return 20 * std::log10(std::max(1e-12, std::abs((b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z))));
    }
};

// gains that make the 26-band cascade really give the wanted curve. The bands overlap a lot (each one also
// moves its neighbours), so the gains are solved as a linear system (what 1 dB of each band does at every
// centre), then corrected a few times against the real response
inline std::array<float, kBands> solveEq(const std::array<float, kBands>& desired, double fs)
{
    constexpr double kQ = 2.2;
    const auto& fc = centres();
    int nb = 0; while (nb < kBands && fc[(size_t) nb] <= fs * 0.45) ++nb;      // bands that exist at this rate
    std::array<std::array<double, kBands>, kBands> M {};
    for (int k = 0; k < nb; ++k) { Biquad t; t.setPeak(fs, fc[(size_t) k], kQ, 1.0); for (int b = 0; b < nb; ++b) M[(size_t) b][(size_t) k] = t.magDb(fs, fc[(size_t) b]); }
    // (M^T M + lambda I) x = M^T y : a little smoothing keeps neighbouring gains from fighting
    auto solve = [&](const std::array<double, kBands>& y) {
        std::vector<std::vector<double>> A((size_t) nb, std::vector<double>((size_t) nb + 1, 0.0));
        for (int i = 0; i < nb; ++i)
        {
            for (int j = 0; j < nb; ++j) { double v = 0; for (int b = 0; b < nb; ++b) v += M[(size_t) b][(size_t) i] * M[(size_t) b][(size_t) j]; A[(size_t) i][(size_t) j] = v + (i == j ? 0.02 : 0.0); }
            double v = 0; for (int b = 0; b < nb; ++b) v += M[(size_t) b][(size_t) i] * y[(size_t) b]; A[(size_t) i][(size_t) nb] = v;
        }
        for (int c = 0; c < nb; ++c)
        {
            int piv = c; for (int r = c + 1; r < nb; ++r) if (std::abs(A[(size_t) r][(size_t) c]) > std::abs(A[(size_t) piv][(size_t) c])) piv = r;
            std::swap(A[(size_t) c], A[(size_t) piv]);
            const double d = A[(size_t) c][(size_t) c]; if (std::abs(d) < 1e-12) continue;
            for (int r = 0; r < nb; ++r) if (r != c) { const double f = A[(size_t) r][(size_t) c] / d; for (int k = c; k <= nb; ++k) A[(size_t) r][(size_t) k] -= f * A[(size_t) c][(size_t) k]; }
        }
        std::array<double, kBands> x {};
        for (int i = 0; i < nb; ++i) x[(size_t) i] = std::abs(A[(size_t) i][(size_t) i]) > 1e-12 ? A[(size_t) i][(size_t) nb] / A[(size_t) i][(size_t) i] : 0.0;
        return x; };
    std::array<double, kBands> y {}; for (int b = 0; b < nb; ++b) y[(size_t) b] = desired[(size_t) b];
    auto x = solve(y);
    std::array<float, kBands> g {};
    for (int k = 0; k < nb; ++k) g[(size_t) k] = (float) std::clamp(x[(size_t) k], -30.0, 18.0);
    for (int it = 0; it < 6; ++it)
    {   // the real response is not exactly linear in the gains: correct what is left
        std::array<Biquad, kBands> tmp;
        for (int k = 0; k < nb; ++k) tmp[(size_t) k].setPeak(fs, fc[(size_t) k], kQ, g[(size_t) k]);
        std::array<double, kBands> err {};
        for (int b = 0; b < nb; ++b) { double r = 0; for (int k = 0; k < nb; ++k) r += tmp[(size_t) k].magDb(fs, fc[(size_t) b]); err[(size_t) b] = desired[(size_t) b] - r; }
        const auto dx = solve(err);
        for (int k = 0; k < nb; ++k) g[(size_t) k] = (float) std::clamp(g[(size_t) k] + 0.8 * dx[(size_t) k], -30.0, 18.0);
    }
    return g;
}
inline void applyEq(juce::AudioBuffer<float>& x, double fs, const std::array<float, kBands>& curve, float amount)
{
    std::array<float, kBands> want {}; bool any = false;
    for (int k = 0; k < kBands; ++k) { want[(size_t) k] = curve[(size_t) k] * amount; any = any || std::abs(want[(size_t) k]) > 0.05f; }
    if (!any) return;
    const auto g = solveEq(want, fs);
    std::array<Biquad, kBands> bq;
    for (int k = 0; k < kBands; ++k)
        if (centres()[(size_t) k] > fs * 0.45 || std::abs(g[(size_t) k]) < 0.01f) bq[(size_t) k].setIdentity(); else bq[(size_t) k].setPeak(fs, centres()[(size_t) k], 2.2, g[(size_t) k]);
    for (int c = 0; c < std::min(2, x.getNumChannels()); ++c)
    {
        auto* d = x.getWritePointer(c);
        for (int i = 0; i < x.getNumSamples(); ++i) { float v = d[i]; for (auto& b : bq) v = b.process(v, c); d[i] = v; }
    }
}

// first sample of the sound (3 % of its peak)
inline int onsetOf(const juce::AudioBuffer<float>& x)
{
    const int n = x.getNumSamples(); if (n == 0) return 0;
    float pk = 0; for (int c = 0; c < x.getNumChannels(); ++c) pk = std::max(pk, x.getMagnitude(c, 0, n));
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(x.getNumChannels() > 1 ? 1 : 0);
    for (int i = 0; i < n; ++i) if (std::abs(0.5f * (L[i] + R[i])) >= 0.03f * pk) return i;
    return 0;
}

// level in 26 third-octave bands over the first 300 ms (dB, the average of 4096-point frames)
inline std::array<float, kBands> bandLevels(const juce::AudioBuffer<float>& x, double fs)
{
    std::array<float, kBands> out {}; out.fill(-120.f);
    const int n = x.getNumSamples(); if (n < 64) return out;
    const int on = onsetOf(x), end = std::min(n, on + (int) (0.30 * fs));
    constexpr int order = 12, N = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> win((size_t) N), buf((size_t) (2 * N));
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * (float) kPi * i / (N - 1));
    std::vector<double> pw((size_t) (N / 2), 0.0);
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(x.getNumChannels() > 1 ? 1 : 0);
    for (int st = on - N / 2; st < end; st += N / 4)
    {
        std::fill(buf.begin(), buf.end(), 0.f);
        for (int i = 0; i < N; ++i) { const int j = st + i; buf[(size_t) i] = (j >= on && j < end) ? 0.5f * (L[j] + R[j]) * win[(size_t) i] : 0.f; }
        fft.performFrequencyOnlyForwardTransform(buf.data());
        for (int b = 0; b < N / 2; ++b) pw[(size_t) b] += (double) buf[(size_t) b] * buf[(size_t) b];
    }
    for (int k = 0; k < kBands; ++k)
    {
        const double lo = centres()[(size_t) k] / std::pow(2.0, 1.0 / 6), hi = centres()[(size_t) k] * std::pow(2.0, 1.0 / 6);
        const int k0 = std::max(1, (int) std::ceil(lo * N / fs)), k1 = std::min(N / 2 - 1, (int) std::floor(hi * N / fs));
        double s = 0; int cnt = 0;
        for (int b = k0; b <= k1; ++b) { s += pw[(size_t) b]; ++cnt; }
        if (cnt == 0) { const int b = juce::jlimit(1, N / 2 - 1, (int) std::lround(centres()[(size_t) k] * N / fs)); s = pw[(size_t) b]; cnt = 1; }
        out[(size_t) k] = (float) (10.0 * std::log10(s / cnt + 1e-20));
    }
    return out;
}

// the 8-band split used for SHAPE: low-passes at the 7 edges, each band = the difference of two of them,
// so the bands always sum back to the input (gain 1 everywhere = untouched)
struct SplitN
{
    static constexpr float edges[kEnvB - 1] = { 100.f, 250.f, 600.f, 1200.f, 2500.f, 5000.f, 9000.f };
    std::array<Biquad, kEnvB - 1> lp;
    void prepare(double fs) { for (int i = 0; i < kEnvB - 1; ++i) { lp[(size_t) i].setLPF(fs, edges[i]); lp[(size_t) i].reset(); } }
    inline void process(float x, int ch, float* band)
    {
        float prev = 0.f;
        for (int i = 0; i < kEnvB - 1; ++i) { const float l = lp[(size_t) i].process(x, ch); band[i] = l - prev; prev = l; }
        band[kEnvB - 1] = x - prev;
    }
};

// level of each band in each moment (dB of the sound's own peak moment)
inline std::array<float, kEnvB * kEnvT> envelopes(const juce::AudioBuffer<float>& x, double fs)
{
    std::array<float, kEnvB * kEnvT> out {}; out.fill(-90.f);
    const int n = x.getNumSamples(); if (n < 64) return out;
    const int on = onsetOf(x);
    SplitN sp; sp.prepare(fs);
    std::array<double, kEnvB * kEnvT> e {}; std::array<int, kEnvT> cnt {};
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(x.getNumChannels() > 1 ? 1 : 0);
    const double* edge = envEdges();
    int t = 0;
    for (int i = 0; i < n; ++i)
    {
        float bnd[kEnvB]; sp.process(0.5f * (L[i] + R[i]), 0, bnd);
        if (i < on) continue;
        const double ts = (i - on) / fs;
        if (ts >= edge[kEnvT]) break;
        while (t < kEnvT - 1 && ts >= edge[t + 1]) ++t;
        for (int b = 0; b < kEnvB; ++b) e[(size_t) (t * kEnvB + b)] += (double) bnd[b] * bnd[b];
        ++cnt[(size_t) t];
    }
    double mx = 1e-20;
    for (int k = 0; k < kEnvT; ++k) for (int b = 0; b < kEnvB; ++b) { auto& v = e[(size_t) (k * kEnvB + b)]; v /= std::max(1, cnt[(size_t) k]); mx = std::max(mx, v); }
    for (int k = 0; k < kEnvB * kEnvT; ++k) out[(size_t) k] = (float) std::max(-90.0, 10.0 * std::log10(e[(size_t) k] / mx + 1e-20));
    return out;
}

// a gain over time per band (dB at the middle of each moment, in between: a line in log time)
inline void applyShape(juce::AudioBuffer<float>& x, double fs, const std::array<float, kEnvB * kEnvT>& gainDb, float amount)
{
    bool any = false; for (float g : gainDb) any = any || std::abs(g * amount) > 0.05f;
    if (!any) return;
    const int n = x.getNumSamples(), on = onsetOf(x);
    const double* edge = envEdges();
    std::array<double, kEnvT> mid {}; for (int k = 0; k < kEnvT; ++k) mid[(size_t) k] = std::log(1e-4 + 0.5 * (edge[k] + edge[k + 1]));
    auto gainAt = [&](int band, double ts) {
        const double lt = std::log(1e-4 + std::max(0.0, ts));
        if (lt <= mid[0]) return gainDb[(size_t) band];
        if (lt >= mid[kEnvT - 1]) return gainDb[(size_t) ((kEnvT - 1) * kEnvB + band)];
        int k = 0; while (k < kEnvT - 2 && lt > mid[(size_t) (k + 1)]) ++k;
        const double f = (lt - mid[(size_t) k]) / (mid[(size_t) (k + 1)] - mid[(size_t) k]);
        return (float) (gainDb[(size_t) (k * kEnvB + band)] * (1 - f) + gainDb[(size_t) ((k + 1) * kEnvB + band)] * f); };
    for (int c = 0; c < std::min(2, x.getNumChannels()); ++c)
    {
        SplitN sp; sp.prepare(fs);
        auto* d = x.getWritePointer(c);
        float gs[kEnvB]; for (auto& g : gs) g = 1.f;
        for (int i = 0; i < n; ++i)
        {
            float bnd[kEnvB]; sp.process(d[i], 0, bnd);
            if ((i & 31) == 0)
            {
                const double ts = (i - on) / fs;
                for (int b = 0; b < kEnvB; ++b) gs[b] = juce::Decibels::decibelsToGain(gainAt(b, ts) * amount);
            }
            float v = 0.f; for (int b = 0; b < kEnvB; ++b) v += bnd[b] * gs[b];
            d[i] = v;
        }
    }
}

// ROOM (ReKit 2.3): the reverb of Suno Chain - an 8-line FDN with Hadamard feedback, two-band decay (the highs die
// sooner), diffused input (no slap), early reflections and a slow line drift - sized down for drums.
// Wet power ~ dry power at mix 1 (the same calibration as Suno Chain), so the measured mix keeps its meaning.
struct RoomLine
{
    std::vector<float> buf; int w = 0, mask = 0;
    void allocate(int maxSamples) { int n = 1; while (n < maxSamples + 4) n <<= 1; buf.assign((size_t) n, 0.f); mask = n - 1; w = 0; }
    inline void push(float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read(int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    inline float readFrac(float d) const { const int i = (int) d; const float f = d - (float) i; return read(i) * (1 - f) + read(i + 1) * f; }
};
inline void addRoom(juce::AudioBuffer<float>& x, double fs, float rt60, float mix, float preMs)
{
    if (mix <= 0.001f || rt60 <= 0.02f) return;
    const int tail = (int) (std::min(3.5f, rt60 * 1.4f) * fs);
    const int n0 = x.getNumSamples(), n = n0 + tail;
    x.setSize(2, n, true, true, false);
    constexpr int N = 8, kAp = 6;
    static const float base[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 73.1f };
    static const float apMs[kAp] = { 4.7f, 7.3f, 11.1f, 16.9f, 23.3f, 31.7f };
    static const float erT[8] = { 0.35f, 0.5f, 0.62f, 0.75f, 0.88f, 1.0f, 1.15f, 1.3f };
    const float size = juce::jlimit(0.28f, 1.0f, 0.22f + rt60 * 0.38f);          // a drum room: small and dense
    const float rtL = std::max(0.1f, rt60), rtH = std::max(0.1f, rt60 * 0.55f);
    std::array<RoomLine, N> lines; std::array<RoomLine, kAp> ap; RoomLine pre;
    std::array<float, N> len {}, gLow {}, gHigh {}, lpState {}, modPhase {}, modRate {};
    std::array<int, kAp> apLen {};
    double gMidSum = 0;
    for (int i = 0; i < N; ++i)
    {
        len[(size_t) i] = (float) (base[i] * size * 0.001 * fs);
        lines[(size_t) i].allocate((int) (0.25 * fs));
        gLow[(size_t) i]  = (float) std::pow(10.0, -3.0 * len[(size_t) i] / (rtL * fs));
        gHigh[(size_t) i] = (float) std::pow(10.0, -3.0 * len[(size_t) i] / (rtH * fs));
        const double gm = std::pow(10.0, -3.0 * len[(size_t) i] / (rt60 * fs)); gMidSum += gm * gm;
        modPhase[(size_t) i] = (float) i * 0.7f; modRate[(size_t) i] = 0.37f + 0.11f * (float) i;
    }
    for (int i = 0; i < kAp; ++i) { ap[(size_t) i].allocate((int) (0.06 * fs)); apLen[(size_t) i] = std::max(1, (int) (apMs[i] * size * 0.001 * fs)); }
    pre.allocate((int) (0.6 * fs));
    const float lpA = (float) (1.0 - std::exp(-2 * kPi * 4000.0 / fs));
    const float modDepth = (float) (0.3 * 0.0006 * fs);
    const float outScale = (float) (std::sqrt(1.0 - gMidSum / N) * 2.72);
    std::array<float, 8> erTime {}, erGain {};
    for (int i = 0; i < 8; ++i) { erTime[(size_t) i] = (float) (erT[i] * 8.0 * size * 0.001 * fs); erGain[(size_t) i] = std::pow(0.85f, (float) i) * 0.5f; }
    const float erLevel = 0.4f, apG = 0.6f;
    const float preS = std::max(0.f, (float) (preMs * 0.001 * fs));
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(1);
    std::vector<float> wetL((size_t) n, 0.f), wetR((size_t) n, 0.f);
    for (int t = 0; t < n; ++t)
    {
        pre.push(t < n0 ? 0.5f * (L[t] + R[t]) : 0.f);
        float v0 = pre.readFrac(preS);
        for (int i = 0; i < kAp; ++i)
        {
            const float d = ap[(size_t) i].read(apLen[(size_t) i] - 1);
            const float v = v0 + apG * d; ap[(size_t) i].push(v); v0 = d - apG * v;
        }
        float eL = 0, eR = 0;
        for (int i = 0; i < 8; ++i) { const float v = pre.readFrac(preS + erTime[(size_t) i]) * erGain[(size_t) i]; if (i & 1) eR += v; else eL += v; }
        float y[N];
        for (int i = 0; i < N; ++i)
        {
            float d = len[(size_t) i];
            if (i < 4)
            {
                modPhase[(size_t) i] += (float) (2 * kPi * modRate[(size_t) i] / fs);
                if (modPhase[(size_t) i] > 2 * kPi) modPhase[(size_t) i] -= (float) (2 * kPi);
                d += modDepth * (1.0f + std::sin(modPhase[(size_t) i]));
            }
            const float v = lines[(size_t) i].readFrac(d);
            lpState[(size_t) i] += lpA * (v - lpState[(size_t) i]);
            y[i] = gLow[(size_t) i] * lpState[(size_t) i] + gHigh[(size_t) i] * (v - lpState[(size_t) i]);
        }
        wetL[(size_t) t] = (y[0] - y[2] + y[4] - y[6] + y[1] * 0.5f - y[5] * 0.5f) * outScale * 0.9f + eL * erLevel;
        wetR[(size_t) t] = (y[1] - y[3] + y[5] - y[7] + y[2] * 0.5f - y[6] * 0.5f) * outScale * 0.9f + eR * erLevel;
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a2 = y[j], b2 = y[j + h]; y[j] = a2 + b2; y[j + h] = a2 - b2; }
        static const float inSign[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
        for (int i = 0; i < N; ++i) lines[(size_t) i].push(y[i] * 0.35355339f + v0 * inSign[i] * 0.35355339f);
    }
    for (int t = 0; t < n; ++t) { x.addSample(0, t, mix * wetL[(size_t) t]); x.addSample(1, t, mix * wetR[(size_t) t]); }
}

// how much tail: energy 120 - 500 ms vs the first 60 ms (dB), and how fast it falls (dB / s, from 80 to 400 ms)
inline void tailOf(const juce::AudioBuffer<float>& x, double fs, float& roomDb, float& slope)
{
    const int n = x.getNumSamples(), on = onsetOf(x);
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(x.getNumChannels() > 1 ? 1 : 0);
    auto e = [&](double a, double b) { const int i0 = std::min(n, on + (int) (a * fs)), i1 = std::min(n, on + (int) (b * fs)); double s = 0;
        for (int i = i0; i < i1; ++i) { const double v = 0.5 * (L[i] + R[i]); s += v * v; } return i1 > i0 ? s / (i1 - i0) : 1e-20; };
    roomDb = (float) (10.0 * std::log10((e(0.12, 0.5) + 1e-20) / (e(0, 0.06) + 1e-20)));
    // straight line through the 20 ms levels from 80 to 400 ms
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int c = 0;
    for (double t = 0.08; t < 0.40; t += 0.02)
    {
        const double y = 10.0 * std::log10(e(t, t + 0.02) + 1e-20);
        if (y < -200) continue;
        sx += t; sy += y; sxx += t * t; sxy += t * y; ++c;
    }
    slope = c >= 3 ? (float) ((c * sxy - sx * sy) / std::max(1e-9, c * sxx - sx * sx)) : -100.f;
}
// ---------------------------------------------------------------- MATCH 3 (v1.12)
// PITCH DROP: a kick / 808 / tom falls in pitch over its first ~120 ms; how fast and how far is a big part of
// what it is. The pitch is followed at 8 moments and the sample is played faster / slower along the way.
constexpr int kGlideN = 8;
inline const float* glideTimes() { static const float t[kGlideN] = { 0.002f, 0.006f, 0.012f, 0.020f, 0.032f, 0.050f, 0.080f, 0.120f }; return t; }

// pitch (Hz) around a moment: normalised autocorrelation over 30 ms, 30 - 500 Hz, the first real peak
inline float pitchAt(const juce::AudioBuffer<float>& x, double fs, int centre, float& conf)
{
    conf = 0;
    const int n = x.getNumSamples();
    const int half = (int) (0.015 * fs), a = std::max(0, centre - half / 2), b = std::min(n, centre + half * 2);
    const int lagMin = (int) (fs / 500.0), lagMax = std::min((int) (fs / 30.0), (b - a) / 2);
    if (lagMax <= lagMin + 2) return 0;
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(x.getNumChannels() > 1 ? 1 : 0);
    const int d = std::max(1, (int) (fs / 22050.0));
    std::vector<float> rs; std::vector<int> lags;
    for (int lag = d; lag <= lagMax; lag += d)
    {
        double num = 0, e0 = 0, e1 = 0;
        for (int i = a; i + lag < b; i += d) { const double u = 0.5 * (L[i] + R[i]), v = 0.5 * (L[i + lag] + R[i + lag]); num += u * v; e0 += u * u; e1 += v * v; }
        rs.push_back((float) (num / std::sqrt(e0 * e1 + 1e-18))); lags.push_back(lag);
    }
    size_t i0 = 0; while (i0 < rs.size() && rs[i0] > 0.2f) ++i0;
    float best = 0; int bestLag = 0;
    for (size_t i = i0; i < rs.size(); ++i)
        if (lags[i] >= lagMin && rs[i] > best && (i == 0 || rs[i] >= rs[i - 1]) && (i + 1 >= rs.size() || rs[i] >= rs[i + 1])) { best = rs[i]; bestLag = lags[i]; }
    conf = best;
    return bestLag > 0 ? (float) (fs / bestLag) : 0.f;
}
inline std::array<float, kGlideN> pitchTrack(const juce::AudioBuffer<float>& x, double fs, std::array<float, kGlideN>& conf)
{
    std::array<float, kGlideN> f {};
    const int on = onsetOf(x);
    for (int k = 0; k < kGlideN; ++k) f[(size_t) k] = pitchAt(x, fs, on + (int) (glideTimes()[k] * fs), conf[(size_t) k]);
    return f;
}
// played faster / slower along the way: semitones at the 8 moments (in between: a line), after them the last
inline void applyGlide(juce::AudioBuffer<float>& x, double fs, const std::array<float, kGlideN>& semis, float amount)
{
    bool any = false; for (float v : semis) any = any || std::abs(v * amount) > 0.02f;
    if (!any) return;
    const int n = x.getNumSamples(), on = onsetOf(x);
    auto semiAt = [&](double t) {
        const float* gt = glideTimes();
        if (t <= gt[0]) return semis[0];
        for (int k = 0; k + 1 < kGlideN; ++k)
            if (t < gt[k + 1]) { const double f = (t - gt[k]) / (gt[k + 1] - gt[k]); return (float) (semis[(size_t) k] * (1 - f) + semis[(size_t) k + 1] * f); }
        return semis[kGlideN - 1]; };
    juce::AudioBuffer<float> o(2, n); o.clear();
    double pos = 0;
    for (int i = 0; i < n; ++i)
    {
        const int i0 = (int) pos; if (i0 + 1 >= n) break;
        const float fr = (float) (pos - i0);
        for (int c = 0; c < 2; ++c) { const float* s = x.getReadPointer(std::min(c, x.getNumChannels() - 1)); o.setSample(c, i, s[i0] + (s[i0 + 1] - s[i0]) * fr); }
        const double t = (pos - on) / fs;
        pos += pos < on ? 1.0 : std::pow(2.0, semiAt(t) * amount / 12.0);
    }
    x = std::move(o);
}

// SATURATION: a soft clipper; the level stays (peak kept), only the shape gets rounder / louder inside
inline void applyDrive(juce::AudioBuffer<float>& x, float driveDb, float amount)
{
    const float d = driveDb * amount;
    if (d <= 0.05f) return;
    const int n = x.getNumSamples();
    float pk = 1e-9f; for (int c = 0; c < x.getNumChannels(); ++c) pk = std::max(pk, x.getMagnitude(c, 0, n));
    const float g = juce::Decibels::decibelsToGain(d) / pk, norm = 1.f / std::tanh(g * pk);
    for (int c = 0; c < x.getNumChannels(); ++c) { auto* p = x.getWritePointer(c); for (int i = 0; i < n; ++i) p[i] = std::tanh(g * p[i]) * norm * pk; }
}
// how "dense" the attack is: peak vs loudness of the first 30 ms (dB); saturation lowers it
inline float crestOf(const juce::AudioBuffer<float>& x, double fs)
{
    const int n = x.getNumSamples(), on = onsetOf(x), e = std::min(n, on + (int) (0.03 * fs));
    if (e <= on) return 0;
    const float* L = x.getReadPointer(0); const float* R = x.getReadPointer(x.getNumChannels() > 1 ? 1 : 0);
    double s = 0; float pk = 0;
    for (int i = on; i < e; ++i) { const float v = 0.5f * (L[i] + R[i]); s += (double) v * v; pk = std::max(pk, std::abs(v)); }
    return (float) (20.0 * std::log10(pk / std::sqrt(s / (e - on) + 1e-20) + 1e-9));
}

// STEREO WIDTH: side vs mid of the first 300 ms (dB), and a side gain to change it
// how loud a hit SOUNDS (dB): weighted like the ear (little sub, more presence), and the ear adds up a short
// hit over ~50 ms - a 5 ms click with the same peak as a long hat sounds much quieter. Peak level says nothing of it.
inline float loudnessOf(const juce::AudioBuffer<float>& x, double fs)
{
    const int n = x.getNumSamples();
    if (n < 16) return -120.f;
    Biquad hp1, hp2, pr; hp1.setHPF(fs, 45.0); hp2.setHPF(fs, 45.0); pr.setPeak(fs, 3200.0, 0.5, 4.0);
    const double k = 1.0 / (0.05 * fs);
    double y = 0, top = 0;
    for (int i = 0; i < n; ++i)
    {
        double e = 0;
        for (int c = 0; c < 2; ++c)
        {
            const float v = x.getSample(std::min(c, x.getNumChannels() - 1), i);
            const float w = pr.process(hp2.process(hp1.process(v, c), c), c);
            e += 0.5 * (double) w * w;
        }
        y += (e - y) * k; top = std::max(top, y);
    }
    return (float) (10.0 * std::log10(std::max(1.0e-12, top)));
}

inline float widthOf(const juce::AudioBuffer<float>& x, double fs)
{
    if (x.getNumChannels() < 2) return -60.f;
    const int n = x.getNumSamples(), on = onsetOf(x), e = std::min(n, on + (int) (0.3 * fs));
    double m = 0, sd = 0;
    for (int i = on; i < e; ++i) { const double l = x.getSample(0, i), r = x.getSample(1, i); m += (l + r) * (l + r); sd += (l - r) * (l - r); }
    return (float) std::max(-60.0, 10.0 * std::log10((sd + 1e-20) / (m + 1e-20)));
}
inline void applyWidth(juce::AudioBuffer<float>& x, float sideDb, float amount)
{
    const float g = juce::Decibels::decibelsToGain(sideDb * amount);
    if (std::abs(g - 1.f) < 0.01f || x.getNumChannels() < 2) return;
    auto* L = x.getWritePointer(0); auto* R = x.getWritePointer(1);
    for (int i = 0; i < x.getNumSamples(); ++i) { const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * g; L[i] = m + s; R[i] = m - s; }
}
} // namespace mt
