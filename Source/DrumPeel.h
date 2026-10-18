#pragma once
// =====================================================================================================
//  DrumPeel - drum loop  ->  dry one-shots + MIDI that rebuilds the loop
//
//  Analysis by synthesis ("learn the sound, subtract it, learn the next one"):
//      loop  ~=  sum over sounds c, hits i :  g_i * T_c(t - tau_i)
//  T_c = coherent part (least squares over the WHOLE loop, all sounds jointly: a kick under a snare is
//        subtracted with its phase, not filtered away)
//      + noise part (power profile learned with a convolutive NMF on what is left: hats / snare noise
//        that is different on every hit - the Brusfri idea of "learn the noise profile").
//  The one-shots ARE T_c and the MIDI IS (tau_i, g_i), so the rebuild error is what gets minimised.
//
//  Pure C++17 / std only (no JUCE) so the same file runs in the plugin and in the CLI test harness.
//  Reference implementation and verifier: tools/drumpeel.py
// =====================================================================================================
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <functional>
#include <atomic>
#include <string>
#include <cstdint>
#include <memory>
#include <map>
#include <limits>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace dp
{
using cd = std::complex<double>;
using cf = std::complex<float>;
static constexpr double PI = 3.14159265358979323846;

// ------------------------------------------------------------------ small containers
struct Buf                                    // stereo signal, double
{
    int n = 0;
    std::vector<double> c[2];
    Buf() = default;
    explicit Buf(int len) { resize(len); }
    void resize(int len) { n = len; c[0].assign((size_t) len, 0.0); c[1].assign((size_t) len, 0.0); }
    double energy() const { double e = 0; for (int ch = 0; ch < 2; ++ch) for (double v : c[ch]) e += v * v; return e; }
    double peak() const { double p = 0; for (int ch = 0; ch < 2; ++ch) for (double v : c[ch]) p = std::max(p, std::abs(v)); return p; }
    bool any() const { for (int ch = 0; ch < 2; ++ch) for (double v : c[ch]) if (v != 0.0) return true; return false; }
    void scale(double s) { for (int ch = 0; ch < 2; ++ch) for (double& v : c[ch]) v *= s; }
    void zero() { for (int ch = 0; ch < 2; ++ch) std::fill(c[ch].begin(), c[ch].end(), 0.0); }
};

struct Spec                                   // stereo STFT, layout [ch][frame][bin]
{
    int F = 0, B = 0;
    std::vector<cf> d;
    void init(int frames, int bins) { F = frames; B = bins; d.assign((size_t) 2 * F * B, cf()); }
    cf* at(int ch, int f) { return d.data() + ((size_t) ch * F + f) * B; }
    const cf* at(int ch, int f) const { return d.data() + ((size_t) ch * F + f) * B; }
};

struct Pow                                    // stereo power spectrogram, layout [ch][frame][bin]
{
    int F = 0, B = 0;
    std::vector<float> d;
    void init(int frames, int bins, float v = 0.f) { F = frames; B = bins; d.assign((size_t) 2 * F * B, v); }
    float* at(int ch, int f) { return d.data() + ((size_t) ch * F + f) * B; }
    const float* at(int ch, int f) const { return d.data() + ((size_t) ch * F + f) * B; }
};

// ------------------------------------------------------------------ FFT (iterative radix 2)
class FFT
{
public:
    explicit FFT(int size) : n(size)
    {
        lg = 0; while ((1 << lg) < n) ++lg;
        rev.resize((size_t) n);
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < lg; ++b) if (i & (1 << b)) r |= 1 << (lg - 1 - b); rev[(size_t) i] = r; }
        tw.resize((size_t) n / 2 + 1);
        for (int i = 0; i <= n / 2; ++i) tw[(size_t) i] = std::polar(1.0, -2.0 * PI * i / n);
    }
    // in place, forward (e^-i) or inverse (e^+i, unnormalised)
    void run(cd* a, bool inverse) const
    {
        for (int i = 0; i < n; ++i) if (i < rev[(size_t) i]) std::swap(a[i], a[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    cd w = tw[(size_t) (j * step)]; if (inverse) w = std::conj(w);
                    const cd u = a[i + j], v = a[i + j + half] * w;
                    a[i + j] = u + v; a[i + j + half] = u - v;
                }
        }
    }
    int size() const { return n; }
private:
    int n, lg;
    std::vector<int> rev;
    std::vector<cd> tw;
};

inline const FFT& fftOf(int n)
{
    thread_local std::map<int, std::unique_ptr<FFT>> cache;
    auto& p = cache[n];
    if (!p) p = std::make_unique<FFT>(n);
    return *p;
}
inline int nextPow2(int v) { int n = 1; while (n < v) n <<= 1; return n; }

// ------------------------------------------------------------------ STFT (Hann, centred frames)
static constexpr int STN = 1024, STH = 256, STB = STN / 2 + 1;

inline const std::vector<double>& hannPeriodic(int N)
{
    thread_local std::map<int, std::vector<double>> cache;
    auto& w = cache[N];
    if (w.empty()) { w.resize((size_t) N); for (int i = 0; i < N; ++i) w[(size_t) i] = 0.5 - 0.5 * std::cos(2.0 * PI * i / N); }
    return w;
}
inline int nframes(int len, int hop = STH) { return (len + hop - 1) / hop + 1; }

// frame j covers samples [j*hop - N/2, j*hop + N/2). Both channels in one complex FFT.
inline void stft(const Buf& x, Spec& S, int frames = -1, int N = STN, int hop = STH)
{
    const int F = frames > 0 ? frames : nframes(x.n, hop), B = N / 2 + 1;
    S.init(F, B);
    const auto& w = hannPeriodic(N);
    const auto& fft = fftOf(N);
    std::vector<cd> z((size_t) N);
    for (int j = 0; j < F; ++j)
    {
        const int s0 = j * hop - N / 2;
        for (int i = 0; i < N; ++i)
        {
            const int t = s0 + i;
            z[(size_t) i] = (t >= 0 && t < x.n) ? cd(x.c[0][(size_t) t] * w[(size_t) i], x.c[1][(size_t) t] * w[(size_t) i]) : cd();
        }
        fft.run(z.data(), false);
        cf* L = S.at(0, j); cf* R = S.at(1, j);
        for (int k = 0; k < B; ++k)
        {
            const cd a = z[(size_t) k], b = std::conj(z[(size_t) ((N - k) % N)]);
            L[k] = cf((a + b) * 0.5);
            R[k] = cf((a - b) * cd(0, -0.5));
        }
    }
}

inline void istft(const Spec& S, Buf& y, int len, int N = STN, int hop = STH)
{
    y.resize(len);
    const auto& w = hannPeriodic(N);
    const auto& fft = fftOf(N);
    std::vector<double> norm((size_t) len, 0.0);
    std::vector<cd> z((size_t) N);
    for (int j = 0; j < S.F; ++j)
    {
        const cf* L = S.at(0, j); const cf* R = S.at(1, j);
        for (int k = 0; k < S.B; ++k)
        {
            const cd a(L[k]), b(R[k]);
            z[(size_t) k] = a + cd(0, 1) * b;
            if (k > 0 && k < N / 2) z[(size_t) (N - k)] = std::conj(a) + cd(0, 1) * std::conj(b);
        }
        fft.run(z.data(), true);
        const int s0 = j * hop - N / 2;
        for (int i = 0; i < N; ++i)
        {
            const int t = s0 + i;
            if (t < 0 || t >= len) continue;
            const double wi = w[(size_t) i];
            y.c[0][(size_t) t] += z[(size_t) i].real() / N * wi;
            y.c[1][(size_t) t] += z[(size_t) i].imag() / N * wi;
            norm[(size_t) t] += wi * wi;
        }
    }
    for (int t = 0; t < len; ++t)
        if (norm[(size_t) t] > 1e-9) { y.c[0][(size_t) t] /= norm[(size_t) t]; y.c[1][(size_t) t] /= norm[(size_t) t]; }
}

// box filter with 'reflect' edges over a [F][B] float plane (sizes fb bins x ff frames)
inline void boxFilter(std::vector<float>& p, int F, int B, int fb, int ff)
{
    auto refl = [](int i, int n) { if (n == 1) return 0; while (i < 0 || i >= n) { if (i < 0) i = -i - 1; if (i >= n) i = 2 * n - i - 1; } return i; };
    std::vector<float> tmp(p.size());
    const int hb = fb / 2, hf = ff / 2;
    for (int f = 0; f < F; ++f)
        for (int k = 0; k < B; ++k)
        {
            double s = 0; for (int d = -hb; d <= hb; ++d) s += p[(size_t) f * B + refl(k + d, B)];
            tmp[(size_t) f * B + k] = (float) (s / fb);
        }
    for (int f = 0; f < F; ++f)
        for (int k = 0; k < B; ++k)
        {
            double s = 0; for (int d = -hf; d <= hf; ++d) s += tmp[(size_t) refl(f + d, F) * B + k];
            p[(size_t) f * B + k] = (float) (s / ff);
        }
}

// ------------------------------------------------------------------ cross-correlation helper
// cc[k] = sum_ch sum_n r[k+n] * t[n],  k = 0 .. lags-1   (r has length >= lags-1+tlen)
inline void xcorr(const Buf& r, int r0, int rlen, const Buf& t, int tlen, int lags, std::vector<double>& cc)
{
    const int n = nextPow2(rlen + tlen);
    std::vector<cd> a((size_t) n), b((size_t) n);
    for (int i = 0; i < rlen; ++i) { const int s = r0 + i; if (s >= 0 && s < r.n) a[(size_t) i] = cd(r.c[0][(size_t) s], r.c[1][(size_t) s]); }
    for (int i = 0; i < tlen && i < t.n; ++i) b[(size_t) i] = cd(t.c[0][(size_t) i], t.c[1][(size_t) i]);
    const auto& fft = fftOf(n);
    fft.run(a.data(), false); fft.run(b.data(), false);
    for (int i = 0; i < n; ++i) a[(size_t) i] *= std::conj(b[(size_t) i]);
    fft.run(a.data(), true);
    cc.assign((size_t) lags, 0.0);
    for (int k = 0; k < lags; ++k) cc[(size_t) k] = a[(size_t) k].real() / n;   // real part = L.L + R.R
}

// ------------------------------------------------------------------ public result
struct PadOut
{
    std::string name; int note = 36; int hits = 0;
    Buf audio;                                 // "learned" one-shot: the sound averaged over all its hits (velocity 100 = this level)
    Buf best;                                  // "best hit": the cleanest real hit, other drums subtracted (same velocity scale)
    double bestTime = -1;                      // where that hit is in the loop (samples)
};
struct NoteOut { double time = 0; int pad = 0; int velocity = 100; };   // time in samples (whole samples)

struct Output
{
    double sr = 44100, bpm = 120;
    std::vector<PadOut> pads;
    std::vector<NoteOut> notes;
    double nullMatch = 0;                      // waveform null test  1 - E(diff)/E(loop)
    double hearing[5] {};                      // 1/3-octave level match per range
    std::string log;
};

static const double BANDS[5][2] = { { 20, 100 }, { 100, 300 }, { 300, 1000 }, { 1000, 4000 }, { 4000, 20000 } };
[[maybe_unused]] static const char* BAND_NAMES[5] = { "SUB", "LOW", "LOW-MID", "MID", "HIGH" };

using Progress = std::function<void(float, const std::string&)>;

// ------------------------------------------------------------------ the analysis
class Peeler
{
public:
    Peeler(const Buf& loop, double sampleRate, int maxPads = 8, int passes = 9)
        : x(loop), sr(sampleRate), nPads(std::max(2, std::min(8, maxPads))), iters(std::max(5, passes)) {}

    double onsetThreshold = 0.03;               // spectral-flux peak height (lower = more hits)
    bool kitMode = true;                        // one pad per instrument (variants of a kick are merged)

    Output run(const Progress& progress = {}, const std::atomic<bool>* cancel = nullptr)
    {
        Output out; out.sr = sr;
        auto prog = [&](float p, const std::string& s) { if (progress) progress(p, s); out.log += s + "\n"; };
        auto cancelled = [&] { return cancel != nullptr && cancel->load(); };
        N = x.n;
        mono.resize((size_t) N);
        for (int i = 0; i < N; ++i) mono[(size_t) i] = 0.5 * (x.c[0][(size_t) i] + x.c[1][(size_t) i]);
        xEnergy = x.energy();

        prog(0.01f, "finding hits");
        on = onsets(onsetThreshold);
        if (on.size() < 2) return out;
        std::vector<std::array<double, 10>> F; for (double o : on) F.push_back(feats(o));
        const int K = std::min<int>(std::max(2, nPads - 3), (int) on.size());
        auto lab = kmeans(F, K);
        Lw = (int) (0.8 * sr); Lf = nframes(Lw);
        const int h0 = (int) (0.06 * sr);
        for (int j = 0; j < K; ++j)
        {
            std::vector<int> g; for (size_t i = 0; i < on.size(); ++i) if (lab[i] == j) g.push_back((int) i);
            if (g.empty()) continue;
            Snd s; s.T.resize(Lw); s.Tn.resize(Lw);
            std::vector<double> pk;
            std::vector<std::vector<double>> segs[2];
            for (int i : g)
            {
                const int o = (int) on[(size_t) i];
                double p = 1e-9;
                for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < h0; ++k) if (o + k < N) p = std::max(p, std::abs(x.c[ch][(size_t) (o + k)]));
                pk.push_back(p);
                for (int ch = 0; ch < 2; ++ch)
                {
                    std::vector<double> v((size_t) h0, 0.0);
                    for (int k = 0; k < h0; ++k) if (o + k < N) v[(size_t) k] = x.c[ch][(size_t) (o + k)] / p;
                    segs[ch].push_back(std::move(v));
                }
                s.t.push_back(on[(size_t) i]);
            }
            s.g = pk;
            std::vector<double> col(g.size());
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < h0; ++k)
                {
                    for (size_t m = 0; m < g.size(); ++m) col[m] = segs[ch][m][(size_t) k];
                    s.T.c[ch][(size_t) k] = median(col);
                }
            // hats / cymbals (mostly > 4 kHz in their own hits) may not hold low end
            std::vector<double> hr, lr;
            for (int i : g)
            {
                hr.push_back(std::pow(10.0, F[(size_t) i][7]) + std::pow(10.0, F[(size_t) i][8]) + std::pow(10.0, F[(size_t) i][9]));   // > 4 kHz
                lr.push_back(std::pow(10.0, F[(size_t) i][0]) + std::pow(10.0, F[(size_t) i][1]) + std::pow(10.0, F[(size_t) i][2]) + std::pow(10.0, F[(size_t) i][3]));  // < 500 Hz
            }
            if (median(hr) > 0.5 && median(lr) < 0.03)
            {
                s.mask.resize(STB);
                for (int k = 0; k < STB; ++k) { const double f = k * sr / STN; const double m = std::clamp((f - 300.0) / 700.0, 0.0, 1.0); s.mask[(size_t) k] = m * m; }
            }
            snd.push_back(std::move(s));
        }
        qlow.resize(STB);
        for (int k = 0; k < STB; ++k) { const double f = k * sr / STN; qlow[(size_t) k] = std::clamp((f - 100.0) / 50.0, 0.0, 1.0); }
        {   Spec Sx; stft(x, Sx); Vx.init(Sx.F, Sx.B); for (size_t i = 0; i < Sx.d.size(); ++i) Vx.d[i] = std::norm(Sx.d[i]) + 1e-14f; }
        thirdOct(x, Bx);

        const int lastSplit = iters - 4;
        double bestScore = -1e9; std::vector<Snd> best;
        for (int it = 0; it < iters; ++it)
        {
            if (cancelled()) return out;
            const float p0 = 0.05f + 0.85f * (float) it / (float) iters;
            prog(p0, "pass " + std::to_string(it + 1) + "/" + std::to_string(iters) + ": learning " + std::to_string(snd.size()) + " sounds");
            solveCoherent(15, 1e-3);
            for (auto& s : snd) { if (s.noisy) s.T.zero(); else decayProject(s.T, {}); }
            Buf yc = synthCoherent();
            std::vector<double> rho = refine(yc, it < 2 ? 400 : 64);
            int moved = 0;
            if (it >= 1)
            {
                yc = synthCoherent();
                moved = reassign(yc);
                for (size_t c = 0; c < snd.size(); ++c) { if (rho[c] < 0.3 && !snd[c].mask.empty()) snd[c].noisy = true; if (snd[c].noisy) snd[c].T.zero(); }
            }
            if (cancelled()) return out;
            yc = synthCoherent();
            // noise part from the residual, all sounds jointly
            Buf R(N); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < N; ++i) R.c[ch][(size_t) i] = x.c[ch][(size_t) i] - yc.c[ch][(size_t) i];
            Spec S; stft(R, S);
            Pow V; V.init(S.F, S.B); for (size_t i = 0; i < S.d.size(); ++i) V.d[i] = std::norm(S.d[i]) + 1e-14f;
            noiseNMF(V, 25);
            for (auto& s : snd) qProject(s);
            capNoisy(V);
            int nadded = 0;
            if (it >= 1 && it < iters - 2) { nadded = addNoiseLayers(V); if (nadded) { noiseNMF(V, 10); for (auto& s : snd) qProject(s); } }
            if (cancelled()) return out;
            auto pg = powerGains(30);
            {
                // coherent sounds (kick, snare body): least-squares gains are exact; noisy ones: power gains
                for (size_t c = 0; c < snd.size(); ++c) if (rho[c] <= 0.6) snd[c].g = pg[c];
            }
            for (auto& s : snd) noiseWave(s, S);
            for (auto& s : snd)                                  // scale ambiguity template <-> gain
            {
                double pk = 1e-12;
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) pk = std::max(pk, std::abs(s.T.c[ch][(size_t) i] + s.Tn.c[ch][(size_t) i]));
                s.T.scale(1 / pk); s.Tn.scale(1 / pk); for (auto& q : s.Q.d) q = (float) (q / (pk * pk)); for (auto& g : s.g) g *= pk;
            }
            int added = 0;
            if (it < iters - 2) added = addLayers();
            mergeClose(0.008 * sr);
            Buf y = synthFull();
            double hm[5]; const double nm = nullMatch(y); hearingMatch(y, hm);
            std::string msg = "  pass " + std::to_string(it + 1) + ": " + std::to_string(snd.size()) + " sounds, null " + pct(nm) + "  hearing";
            for (double h : hm) msg += " " + pct(h);
            msg += "  +" + std::to_string(added) + " layered, +" + std::to_string(nadded) + " noise layered, " + std::to_string(moved) + " moved";
            bool split = false;
            if ((int) snd.size() < nPads && it >= 1 && it <= lastSplit) split = splitWorst(y, msg);
            prog(p0 + 0.85f / iters, msg);
            const double score = 0.5 * nm + 0.5 * *std::min_element(hm, hm + 5);
            if (split) { best.clear(); bestScore = -1e9; }
            else if (score > bestScore) { bestScore = score; best = snd; }
        }
        if (!best.empty()) snd = best;
        if (kitMode)
        {
            // two ways to pick the leading variant of each instrument; both are polished, the better one stays
            const std::vector<Snd> pre = snd;
            std::vector<Snd> winner; double winScore = -1e9; bool any = false;
            for (int mode = 0; mode < 2 && !cancelled(); ++mode)
            {
                snd = pre;
                std::vector<size_t> leaders;
                if (!mergeByCategory(mode == 0, &leaders)) break;
                if (mode == 1 && leaders == lastLeaders) break;          // same choice: nothing new to try
                lastLeaders = leaders; any = true;
                prog(0.90f + 0.015f * mode, "one pad per instrument: " + std::to_string(snd.size()) + " sounds, polishing");
                for (int k = 0; k < 3 && !cancelled(); ++k) polishPass();
                Buf y = synthFull(); double hm[5]; const double nm = nullMatch(y); hearingMatch(y, hm);
                const double sc = 0.25 * nm + 0.75 * *std::min_element(hm, hm + 5);   // what you hear first
                if (std::getenv("DP_DEBUG")) std::fprintf(stderr, "merge mode %d score %.4f null %.3f min hearing %.3f\n", mode, sc, nm, *std::min_element(hm, hm + 5));
                if (sc > winScore) { winScore = sc; winner = snd; }
            }
            if (any) snd = winner; else snd = pre;
        }
        if (cancelled()) return out;
        prog(0.93f, "building one-shots + MIDI");
        finish(out);
        prog(1.f, "done: null " + pct(out.nullMatch) + ", hearing " + pct(out.hearing[0]) + " " + pct(out.hearing[1]) + " " + pct(out.hearing[2]) + " " + pct(out.hearing[3]) + " " + pct(out.hearing[4]));
        return out;
    }

    // ---------- helpers also used by the plugin / CLI
    static double estimateBpm(const std::vector<double>& times, int n, double sr)
    {
        double bestS = 1e300, bestB = 120;
        for (double b = 60; b < 200; b += 0.05)
        {
            const double g = 60.0 / b / 4 * sr; double s = 0;
            for (double t : times) { const double ph = std::fmod(t / g, 1.0); const double d = std::min(ph, 1 - ph); s += d * d; }
            s = s / std::max<size_t>(1, times.size()) + 1e-4 * std::abs(b - 120) / 60;
            if (s < bestS) { bestS = s; bestB = b; }
        }
        const double L = n / sr; const double bars = std::round(L * bestB / 240.0);
        if (bars >= 1 && std::abs(bars * 240.0 / L - bestB) / bestB < 0.01) bestB = bars * 240.0 / L;
        return bestB;
    }

    // plugin playback, identical maths: start at the nearest whole sample, gain = velocity / 100, additive
    static void render(const std::vector<PadOut>& pads, const std::vector<NoteOut>& notes, Buf& y, int n)
    {
        y.resize(n);
        for (const auto& nt : notes)
        {
            const auto& s = pads[(size_t) nt.pad].audio;
            const int i0 = (int) std::floor(nt.time + 0.5); const double g = nt.velocity / 100.0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < s.n; ++i) { const int t = i0 + i; if (t >= 0 && t < n) y.c[ch][(size_t) t] += g * s.c[ch][(size_t) i]; }
        }
    }

private:
    struct Snd
    {
        Buf T, Tn;                                  // coherent part, noise part
        Pow Q;                                      // noise power profile [ch][frame][bin]
        std::vector<double> t, g;                   // hit times (samples, whole) and gains
        std::vector<double> mask;                   // optional frequency support (hats)
        bool noisy = false;
    };

    const Buf& x; double sr; int nPads, iters;
    int N = 0, Lw = 0, Lf = 0;
    std::vector<double> mono, on, qlow;
    double xEnergy = 1;
    std::vector<Snd> snd;
    Pow Vx;
    std::vector<std::vector<double>> Bx;          // third-octave envelope of the loop
    std::vector<double> thirdCents;

    static std::string pct(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f%%", v * 100); return b; }
    static double median(std::vector<double> v)
    {
        if (v.empty()) return 0;
        const size_t m = v.size() / 2; std::nth_element(v.begin(), v.begin() + (long) m, v.end());
        double r = v[m];
        if (v.size() % 2 == 0) { r = (r + *std::max_element(v.begin(), v.begin() + (long) m)) * 0.5; }
        return r;
    }
    static int rnd(double t) { return (int) std::floor(t + 0.5); }

    // ---------------------------------------------------------------- onsets (spectral flux)
    std::vector<double> onsets(double thr = 0.03, int hop = 128, double mind = 0.02)
    {
        const int Nw = 1024, pad = 1024;
        const int Np = N + pad;
        auto mp = [&](int i) { return i < pad ? 0.0 : mono[(size_t) (i - pad)]; };
        const int F = Np >= Nw ? (Np - Nw) / hop + 1 : 0;
        if (F < 3) return {};
        const auto& w = hannPeriodic(Nw); const auto& fft = fftOf(Nw);
        double wsum = 0; for (double v : w) wsum += v;
        std::vector<double> prev((size_t) Nw / 2 + 1, 0.0), flux((size_t) F, 0.0);
        std::vector<cd> z((size_t) Nw);
        for (int j = 0; j < F; ++j)
        {
            for (int i = 0; i < Nw; ++i) z[(size_t) i] = cd(mp(j * hop + i) * w[(size_t) i], 0);
            fft.run(z.data(), false);
            double f = 0;
            for (int k = 0; k <= Nw / 2; ++k)
            {
                const double m = std::log1p(1000.0 * std::abs(z[(size_t) k]) / wsum);
                if (j > 0) f += std::max(0.0, m - prev[(size_t) k]);
                prev[(size_t) k] = m;
            }
            flux[(size_t) j] = f;
        }
        const double mx = *std::max_element(flux.begin(), flux.end()) + 1e-12;
        for (auto& v : flux) v /= mx;
        std::vector<double> d((size_t) F);
        for (int j = 0; j < F; ++j)                                        // minus median (31), zero padded
        {
            std::vector<double> win; for (int k = j - 15; k <= j + 15; ++k) win.push_back(k >= 0 && k < F ? flux[(size_t) k] : 0.0);
            std::nth_element(win.begin(), win.begin() + 15, win.end());
            d[(size_t) j] = flux[(size_t) j] - win[15];
        }
        std::vector<int> pk;
        for (int j = 1; j < F - 1; ++j)
            if (d[(size_t) j] > d[(size_t) j - 1] && d[(size_t) j] >= d[(size_t) j + 1] && d[(size_t) j] >= thr) pk.push_back(j);
        const int dist = std::max(1, (int) (mind * sr / hop));
        std::vector<int> order(pk.size()); std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return d[(size_t) pk[(size_t) a]] > d[(size_t) pk[(size_t) b]]; });
        std::vector<char> keep(pk.size(), 1);
        for (int oi : order)
        {
            if (!keep[(size_t) oi]) continue;
            for (size_t k = 0; k < pk.size(); ++k)
                if ((int) k != oi && keep[k] && std::abs(pk[k] - pk[(size_t) oi]) < dist) keep[k] = 0;
        }
        std::vector<int> outv;
        for (size_t k = 0; k < pk.size(); ++k)
        {
            if (!keep[k]) continue;
            const int c = pk[k] * hop + 512;
            const int a = std::max(24, c - 900), b = std::min(Np, c + 600);
            int q = a; double pv = 0;
            for (int i = a; i < b; ++i) if (std::abs(mp(i)) > pv) { pv = std::abs(mp(i)); q = i; }
            int o = q;
            auto mx24 = [&](int e) { double m = 0; for (int i = e - 24; i < e; ++i) m = std::max(m, std::abs(mp(i))); return m; };
            while (o > a + 24 && mx24(o) > 0.1 * pv) --o;
            outv.push_back(std::max(0, o - pad));
        }
        std::sort(outv.begin(), outv.end());
        std::vector<double> res;
        for (int o : outv) if (res.empty() || o - res.back() > (int) (0.008 * sr)) res.push_back(o);
        return res;
    }

    std::array<double, 10> feats(double onset) const
    {
        const int L = 2048; const int o = rnd(onset);
        std::vector<cd> z((size_t) L);
        for (int i = 0; i < L; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos(2 * PI * i / (L - 1));
            z[(size_t) i] = (o + i < N && o + i >= 0) ? cd(mono[(size_t) (o + i)] * w, 0) : cd();
        }
        fftOf(L).run(z.data(), false);
        static const double edges[11] = { 20, 60, 120, 250, 500, 1000, 2000, 4000, 8000, 16000, 22050 };
        std::array<double, 10> e {}; double tot = 0;
        for (int k = 0; k <= L / 2; ++k)
        {
            const double f = k * sr / L, p = std::norm(z[(size_t) k]);
            for (int b = 0; b < 10; ++b) if (f >= edges[b] && f < edges[b + 1]) { e[(size_t) b] += p; break; }
        }
        for (double v : e) tot += v;
        for (auto& v : e) v = std::log10(v / (tot + 1e-20) + 1e-6);
        return e;
    }

    template <size_t D>
    static std::vector<int> kmeansT(const std::vector<std::array<double, D>>& F, int k, int iters = 60)
    {
        auto d2 = [](const std::array<double, D>& a, const std::array<double, D>& b) { double s = 0; for (size_t i = 0; i < D; ++i) s += (a[i] - b[i]) * (a[i] - b[i]); return s; };
        std::vector<std::array<double, D>> C { F[0] };
        while ((int) C.size() < k)
        {
            double bd = -1; size_t bi = 0;
            for (size_t i = 0; i < F.size(); ++i) { double m = 1e300; for (auto& c : C) m = std::min(m, d2(F[i], c)); if (m > bd) { bd = m; bi = i; } }
            C.push_back(F[bi]);
        }
        std::vector<int> lab(F.size(), 0);
        for (int it = 0; it < iters; ++it)
        {
            for (size_t i = 0; i < F.size(); ++i) { double m = 1e300; for (int j = 0; j < k; ++j) { const double v = d2(F[i], C[(size_t) j]); if (v < m) { m = v; lab[i] = j; } } }
            for (int j = 0; j < k; ++j)
            {
                std::array<double, D> s {}; int n = 0;
                for (size_t i = 0; i < F.size(); ++i) if (lab[i] == j) { for (size_t q = 0; q < D; ++q) s[q] += F[i][q]; ++n; }
                if (n) { for (auto& v : s) v /= n; C[(size_t) j] = s; }
            }
        }
        return lab;
    }
    static std::vector<int> kmeans(const std::vector<std::array<double, 10>>& F, int k) { return kmeansT<10>(F, k); }

    // ---------------------------------------------------------------- coherent model (whole samples)
    void addShifted(Buf& y, const Buf& T, int i0, double g) const
    {
        if (g == 0) return;
        const int a = std::max(0, i0), b = std::min(y.n, i0 + T.n);
        for (int ch = 0; ch < 2; ++ch) { double* yp = y.c[ch].data(); const double* tp = T.c[ch].data(); for (int t = a; t < b; ++t) yp[t] += g * tp[t - i0]; }
    }
    Buf synthCoherent(int only = -1) const
    {
        Buf y(N);
        for (size_t c = 0; c < snd.size(); ++c)
        {
            if (only >= 0 && (int) c != only) continue;
            for (size_t i = 0; i < snd[c].t.size(); ++i) addShifted(y, snd[c].T, rnd(snd[c].t[i]), snd[c].g[i]);
        }
        return y;
    }
    Buf synthFull() const
    {
        Buf y(N);
        for (const auto& s : snd)
        {
            Buf T(Lw); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) T.c[ch][(size_t) i] = s.T.c[ch][(size_t) i] + s.Tn.c[ch][(size_t) i];
            for (size_t i = 0; i < s.t.size(); ++i) addShifted(y, T, rnd(s.t[i]), s.g[i]);
        }
        return y;
    }
    void solveCoherent(int cgIters, double lam)
    {
        const size_t C = snd.size();
        auto synthOf = [&](const std::vector<Buf>& T) {
            Buf y(N);
            for (size_t c = 0; c < C; ++c) for (size_t i = 0; i < snd[c].t.size(); ++i) addShifted(y, T[c], rnd(snd[c].t[i]), snd[c].g[i]);
            return y; };
        auto adjOf = [&](const Buf& r) {
            std::vector<Buf> o(C, Buf(Lw));
            for (size_t c = 0; c < C; ++c)
                for (size_t i = 0; i < snd[c].t.size(); ++i)
                {
                    const double g = snd[c].g[i]; if (g == 0) continue;
                    const int i0 = rnd(snd[c].t[i]); const int a = std::max(0, i0), b = std::min(N, i0 + Lw);
                    for (int ch = 0; ch < 2; ++ch) { double* op = o[c].c[ch].data(); const double* rp = r.c[ch].data(); for (int t = a; t < b; ++t) op[t - i0] += g * rp[t]; }
                }
            return o; };
        auto op = [&](const std::vector<Buf>& T) { auto o = adjOf(synthOf(T)); for (size_t c = 0; c < C; ++c) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) o[c].c[ch][(size_t) i] += lam * T[c].c[ch][(size_t) i]; return o; };
        auto dot = [&](const std::vector<Buf>& a, const std::vector<Buf>& b) { double s = 0; for (size_t c = 0; c < C; ++c) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) s += a[c].c[ch][(size_t) i] * b[c].c[ch][(size_t) i]; return s; };
        std::vector<Buf> T(C); for (size_t c = 0; c < C; ++c) T[c] = snd[c].T;
        auto b = adjOf(x); auto AT = op(T);
        std::vector<Buf> r(C, Buf(Lw)), p(C, Buf(Lw));
        for (size_t c = 0; c < C; ++c) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) r[c].c[ch][(size_t) i] = b[c].c[ch][(size_t) i] - AT[c].c[ch][(size_t) i];
        p = r; double rr = dot(r, r);
        for (int k = 0; k < cgIters && rr > 1e-30; ++k)
        {
            auto Ap = op(p); const double a = rr / std::max(1e-30, dot(p, Ap));
            for (size_t c = 0; c < C; ++c) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i)
            { T[c].c[ch][(size_t) i] += a * p[c].c[ch][(size_t) i]; r[c].c[ch][(size_t) i] -= a * Ap[c].c[ch][(size_t) i]; }
            const double rn = dot(r, r);
            for (size_t c = 0; c < C; ++c) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) p[c].c[ch][(size_t) i] = r[c].c[ch][(size_t) i] + rn / rr * p[c].c[ch][(size_t) i];
            rr = rn;
        }
        for (size_t c = 0; c < C; ++c) snd[c].T = T[c];
    }

    // a drum only decays: per STFT bin, after its peak (first 50 ms) it may not rise above its running minimum
    void decayProject(Buf& T, const std::vector<double>& mask, double slackDb = 3.0, double hold = 0.012, double peakWin = 0.05) const
    {
        Spec S; stft(T, S, Lf);
        const double sl = std::pow(10.0, slackDb / 20.0);
        const int pw = std::max(2, (int) (peakWin * sr / STH) + 2), hf = std::max(1, (int) (hold * sr / STH));
        for (int ch = 0; ch < 2; ++ch)
        {
            std::vector<float> M((size_t) S.F * S.B);
            for (int f = 0; f < S.F; ++f) for (int k = 0; k < S.B; ++k) M[(size_t) f * S.B + k] = std::norm(S.at(ch, f)[k]);
            boxFilter(M, S.F, S.B, 5, 3);
            for (auto& v : M) v = std::sqrt(std::max(0.f, v)) + 1e-12f;
            for (int k = 0; k < S.B; ++k)
            {
                int p = 0; for (int f = 1; f < std::min(pw, S.F); ++f) if (M[(size_t) f * S.B + k] > M[(size_t) p * S.B + k]) p = f;
                double run = 1e300;
                for (int f = p + hf; f < S.F; ++f)
                {
                    const double m = M[(size_t) f * S.B + k];
                    run = std::min(run, m); const double lim = run * sl; double G = 1;
                    if (m > lim) G = lim / m;
                    run = std::min(run, m * G);
                    S.at(ch, f)[k] *= (float) G;
                }
                if (!mask.empty()) for (int f = 0; f < S.F; ++f) S.at(ch, f)[k] *= (float) mask[(size_t) k];
            }
        }
        istft(S, T, Lw);
    }

    // ---------------------------------------------------------------- per hit: time + gain
    std::vector<double> refine(Buf& y, int maxlag)
    {
        std::vector<double> rho(snd.size(), 0.0);
        const int Lc = std::min(Lw, (int) (0.15 * sr));
        std::vector<double> cc;
        Buf r;                                          // residual in a window
        for (size_t c = 0; c < snd.size(); ++c)
        {
            auto& s = snd[c];
            if (!s.T.any()) continue;
            double tw2 = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lc; ++i) tw2 += s.T.c[ch][(size_t) i] * s.T.c[ch][(size_t) i];
            double T2 = s.T.energy();
            double cnum = 1e-20, cden = 1e-20;
            for (size_t i = 0; i < s.t.size(); ++i)
            {
                addShifted(y, s.T, rnd(s.t[i]), -s.g[i]);
                const int base = rnd(s.t[i]) - maxlag, rl = Lc + 2 * maxlag;
                Buf res(rl);
                for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < rl; ++k) { const int t = base + k; if (t >= 0 && t < N) res.c[ch][(size_t) k] = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; }
                xcorr(res, 0, rl, s.T, Lc, 2 * maxlag + 1, cc);
                int kb = (int) (std::max_element(cc.begin(), cc.end()) - cc.begin());
                double e = 0; for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < Lc; ++k) e += res.c[ch][(size_t) (kb + k)] * res.c[ch][(size_t) (kb + k)];
                const double ncc = cc[(size_t) kb] / std::sqrt(e * tw2 + 1e-20);
                if (ncc > 0.5) s.t[i] = std::max(0, base + kb);
                const int i0 = rnd(s.t[i]); const int a = std::max(0, i0), b = std::min(N, i0 + Lw);
                double num = 0, rr = 0;
                for (int ch = 0; ch < 2; ++ch) for (int t = a; t < b; ++t) { const double rv = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; num += rv * s.T.c[ch][(size_t) (t - i0)]; rr += rv * rv; }
                double ss = 0; for (int ch = 0; ch < 2; ++ch) for (int t = a; t < b; ++t) ss += s.T.c[ch][(size_t) (t - i0)] * s.T.c[ch][(size_t) (t - i0)];
                (void) T2;
                const double gl = std::max(0.0, num / std::max(1e-12, ss));
                s.g[i] = gl; addShifted(y, s.T, i0, gl);
                cnum += gl * gl * ss; cden += rr;
            }
            rho[c] = cnum / cden;
        }
        return rho;
    }

    // hits their own sound explains badly are tried on every other coherent sound
    int reassign(Buf& y, int maxlag = 400, double Wsec = 0.25)
    {
        const int W = (int) (Wsec * sr); int moved = 0;
        std::vector<double> cc;
        struct Add { size_t c; double t, g; };
        std::vector<Add> adds;
        std::vector<double> T2(snd.size(), 0.0);
        for (size_t d = 0; d < snd.size(); ++d) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < W; ++i) T2[d] += snd[d].T.c[ch][(size_t) i] * snd[d].T.c[ch][(size_t) i];
        auto hasHitNear = [&](size_t d, double t) {
            for (double u : snd[d].t) if (std::abs(u - t) < 0.006 * sr) return true;
            for (auto& a : adds) if (a.c == d && std::abs(a.t - t) < 0.006 * sr) return true;
            return false; };
        for (size_t c = 0; c < snd.size(); ++c)
        {
            auto& s = snd[c];
            if (!s.T.any())
            {   // noise-only sound (hat): its hits may hide a coherent sound (a snare under the hat) -> layer it
                for (size_t i = 0; i < s.t.size(); ++i)
                {
                    const int a0 = rnd(s.t[i]);
                    if (a0 - maxlag < 0 || a0 + W + maxlag > N) continue;
                    const int rl = W + 2 * maxlag;
                    Buf r(rl); for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < rl; ++k) { const int t = a0 - maxlag + k; r.c[ch][(size_t) k] = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; }
                    double bex = 0.5, bg = 0, bt = 0; int bc = -1;
                    for (size_t d = 0; d < snd.size(); ++d)
                    {
                        if (!snd[d].T.any() || hasHitNear(d, s.t[i])) continue;
                        xcorr(r, 0, rl, snd[d].T, W, 2 * maxlag + 1, cc);
                        const int k = (int) (std::max_element(cc.begin(), cc.end()) - cc.begin());
                        const double gg = cc[(size_t) k] / std::max(1e-12, T2[d]);
                        double e = 0, nn = 0;
                        for (int ch = 0; ch < 2; ++ch) for (int q = 0; q < W; ++q) { const double rv = r.c[ch][(size_t) (k + q)]; const double dd = rv - gg * snd[d].T.c[ch][(size_t) q]; e += dd * dd; nn += rv * rv; }
                        const double ex = 1 - e / std::max(1e-12, nn);
                        if (gg > 0 && ex > bex) { bex = ex; bc = (int) d; bg = gg; bt = std::max(0, a0 - maxlag + k); }
                    }
                    if (bc >= 0) { adds.push_back({ (size_t) bc, bt, bg }); ++moved; addShifted(y, snd[(size_t) bc].T, rnd(bt), bg); }
                }
                continue;
            }
            for (size_t i = 0; i < s.t.size(); ++i)
            {
                const int a0 = rnd(s.t[i]); const double g = s.g[i];
                if (a0 - maxlag < 0 || a0 + W + maxlag > N) continue;
                addShifted(y, s.T, a0, -g);
                const int rl = W + 2 * maxlag;
                Buf r(rl); for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < rl; ++k) { const int t = a0 - maxlag + k; r.c[ch][(size_t) k] = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; }
                double e0 = 0, n0 = 0;
                for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < W; ++k) { const double rv = r.c[ch][(size_t) (maxlag + k)]; const double d = rv - g * s.T.c[ch][(size_t) k]; e0 += d * d; n0 += rv * rv; }
                const double ex0 = 1 - e0 / std::max(1e-12, n0);
                if (ex0 > 0.5) { addShifted(y, s.T, a0, g); continue; }
                double bex = ex0, bg = g, bt = s.t[i]; size_t bc = c;
                for (size_t d = 0; d < snd.size(); ++d)
                {
                    if (!snd[d].T.any()) continue;
                    xcorr(r, 0, rl, snd[d].T, W, 2 * maxlag + 1, cc);
                    const int k = (int) (std::max_element(cc.begin(), cc.end()) - cc.begin());
                    const double gg = cc[(size_t) k] / std::max(1e-12, T2[d]);
                    double e = 0, nn = 0;
                    for (int ch = 0; ch < 2; ++ch) for (int q = 0; q < W; ++q) { const double rv = r.c[ch][(size_t) (k + q)]; const double dd = rv - gg * snd[d].T.c[ch][(size_t) q]; e += dd * dd; nn += rv * rv; }
                    const double ex = 1 - e / std::max(1e-12, nn);
                    if (gg > 0 && ex > bex + 0.15) { bex = ex; bc = d; bg = gg; bt = std::max(0, a0 - maxlag + k); }
                }
                if (bc != c || bt != s.t[i])
                {
                    s.g[i] = 0; adds.push_back({ bc, bt, bg }); ++moved;
                    addShifted(y, snd[bc].T, rnd(bt), bg);
                }
                else addShifted(y, s.T, a0, g);
            }
        }
        for (auto& a : adds) { snd[a.c].t.push_back(a.t); snd[a.c].g.push_back(a.g); }
        for (auto& s : snd)
        {
            std::vector<std::pair<double, double>> v;
            for (size_t i = 0; i < s.t.size(); ++i) if (s.g[i] > 0) v.push_back({ s.t[i], s.g[i] });
            if (v.empty()) continue;
            std::sort(v.begin(), v.end());
            s.t.clear(); s.g.clear(); for (auto& p : v) { s.t.push_back(p.first); s.g.push_back(p.second); }
        }
        return moved;
    }

    // ---------------------------------------------------------------- noise model (power domain)
    static int frameOf(double t) { return std::max(0, (int) std::floor(t / STH + 0.5)); }

    void noiseNMF(const Pow& V, int nIter)
    {
        const int Fn = V.F, B = V.B;
        for (auto& s : snd)
            if (s.Q.d.empty())
            {
                s.Q.init(Lf, B, 0.f); double hs = 0;
                std::vector<double> acc((size_t) 2 * Lf * B, 0.0);
                for (size_t i = 0; i < s.t.size(); ++i)
                {
                    const double h = s.g[i] * s.g[i]; if (h <= 0) continue;
                    const int tau = frameOf(s.t[i]);
                    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && tau + f < Fn; ++f)
                    { const float* v = V.at(ch, tau + f); double* a = acc.data() + ((size_t) ch * Lf + f) * B; for (int k = 0; k < B; ++k) a[k] += v[k]; }
                    hs += h;
                }
                for (size_t q = 0; q < acc.size(); ++q) s.Q.d[q] = (float) (acc[q] / std::max(hs, 1e-9) * 0.3 + 1e-14);
            }
        Pow Vh; Vh.init(Fn, B);
        std::vector<float> num, den;
        for (int it = 0; it < nIter; ++it)
        {
            std::fill(Vh.d.begin(), Vh.d.end(), 1e-14f);
            for (auto& s : snd)
                for (size_t i = 0; i < s.t.size(); ++i)
                {
                    const float h = (float) (s.g[i] * s.g[i]); if (h <= 0) continue;
                    const int tau = frameOf(s.t[i]);
                    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && tau + f < Fn; ++f)
                    { float* o = Vh.at(ch, tau + f); const float* q = s.Q.at(ch, f); for (int k = 0; k < B; ++k) o[k] += h * q[k]; }
                }
            for (size_t q = 0; q < Vh.d.size(); ++q) Vh.d[q] = V.d[q] / Vh.d[q];           // Vh := V / Vhat
            for (auto& s : snd)
            {
                num.assign(s.Q.d.size(), 0.f); den.assign(s.Q.d.size(), 1e-14f);
                for (size_t i = 0; i < s.t.size(); ++i)
                {
                    const float h = (float) (s.g[i] * s.g[i]); if (h <= 0) continue;
                    const int tau = frameOf(s.t[i]);
                    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && tau + f < Fn; ++f)
                    {
                        const float* rt = Vh.at(ch, tau + f); float* nu = num.data() + ((size_t) ch * Lf + f) * B; float* de = den.data() + ((size_t) ch * Lf + f) * B;
                        for (int k = 0; k < B; ++k) { nu[k] += h * rt[k]; de[k] += h; }
                    }
                }
                for (size_t q = 0; q < s.Q.d.size(); ++q) s.Q.d[q] *= num[q] / den[q];
            }
        }
    }

    // noise profile may only decay after its peak (same rule as the coherent part), hats keep no lows,
    // and the sub range has no noise model at all (drums repeat exactly there)
    void qProject(Snd& s, double slackDb = 3.0, double peakWin = 0.05)
    {
        const int B = s.Q.B, F = s.Q.F;
        const double sl = std::pow(10.0, slackDb / 10.0);
        const int pw = std::max(2, (int) (peakWin * sr / STH) + 2);
        for (int ch = 0; ch < 2; ++ch)
        {
            std::vector<float> P((size_t) F * B);
            for (int f = 0; f < F; ++f) for (int k = 0; k < B; ++k) P[(size_t) f * B + k] = s.Q.at(ch, f)[k];
            boxFilter(P, F, B, 9, 3);
            for (auto& v : P) v = std::max(v, 0.f) + 1e-20f;
            for (int k = 0; k < B; ++k)
            {
                int p = 0; for (int f = 1; f < std::min(pw, F); ++f) if (P[(size_t) f * B + k] > P[(size_t) p * B + k]) p = f;
                double run = 1e300;
                for (int f = p + 1; f < F; ++f)
                {
                    const double v = P[(size_t) f * B + k]; run = std::min(run, v);
                    if (v > run * sl) s.Q.at(ch, f)[k] *= (float) (run * sl / v);
                }
                double m = qlow[(size_t) k]; if (s.noisy && !s.mask.empty()) m *= s.mask[(size_t) k] * s.mask[(size_t) k];
                for (int f = 0; f < F; ++f) { float& q = s.Q.at(ch, f)[k]; q = std::max(0.f, (float) (q * m)); }
            }
        }
    }

    // per-hit gains in the power domain, all sounds jointly (KL-NMF with fixed templates)
    std::vector<std::vector<double>> powerGains(int nIter)
    {
        const int Fn = Vx.F, B = Vx.B;
        // coherent power of each sound at 4 sub-frame offsets
        std::vector<std::array<Pow, 4>> W(snd.size());
        for (size_t c = 0; c < snd.size(); ++c)
            for (int o = 0; o < 4; ++o)
            {
                Buf seg(Lw + o * 64); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) seg.c[ch][(size_t) (i + o * 64)] = snd[c].T.c[ch][(size_t) i];
                Spec S; stft(seg, S, Lf);
                W[c][(size_t) o].init(Lf, B);
                for (size_t q = 0; q < S.d.size(); ++q) W[c][(size_t) o].d[q] = std::norm(S.d[q]) + snd[c].Q.d[q];
            }
        struct It { size_t c, i; int f0, o; double h; };
        std::vector<It> items;
        for (size_t c = 0; c < snd.size(); ++c)
            for (size_t i = 0; i < snd[c].t.size(); ++i)
            {
                int f0 = std::max(0, (int) std::floor(snd[c].t[i] / STH)); int o = (int) std::floor((snd[c].t[i] - f0 * STH) / 64.0 + 0.5);
                if (o >= 4) { ++f0; o = 0; }
                if (f0 >= Fn) continue;
                items.push_back({ c, i, f0, o, std::pow(std::max(snd[c].g[i], 1e-3), 2) });
            }
        Pow R; R.init(Fn, B);
        std::vector<double> wsum(items.size(), 0.0);
        for (size_t q = 0; q < items.size(); ++q)
        {
            const auto& w = W[items[q].c][(size_t) items[q].o];
            for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && items[q].f0 + f < Fn; ++f) { const float* wp = w.at(ch, f); for (int k = 0; k < B; ++k) wsum[q] += wp[k]; }
        }
        for (int it = 0; it < nIter; ++it)
        {
            std::fill(R.d.begin(), R.d.end(), 1e-14f);
            for (const auto& m : items)
            {
                const auto& w = W[m.c][(size_t) m.o]; const float h = (float) m.h;
                for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && m.f0 + f < Fn; ++f) { float* rp = R.at(ch, m.f0 + f); const float* wp = w.at(ch, f); for (int k = 0; k < B; ++k) rp[k] += h * wp[k]; }
            }
            for (size_t q = 0; q < R.d.size(); ++q) R.d[q] = Vx.d[q] / R.d[q];
            for (size_t q = 0; q < items.size(); ++q)
            {
                auto& m = items[q]; const auto& w = W[m.c][(size_t) m.o]; double s = 0;
                for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && m.f0 + f < Fn; ++f) { const float* rp = R.at(ch, m.f0 + f); const float* wp = w.at(ch, f); for (int k = 0; k < B; ++k) s += (double) wp[k] * rp[k]; }
                m.h *= s / std::max(1e-20, wsum[q]);
            }
        }
        std::vector<std::vector<double>> g(snd.size());
        for (size_t c = 0; c < snd.size(); ++c) g[c] = snd[c].g;
        for (const auto& m : items) g[m.c][m.i] = std::sqrt(m.h);
        return g;
    }

    // waveform with magnitude sqrt(Q): phase from the real hit whose noise looks most like Q, then
    // Griffin-Lim. Where the coherent part is present the noise is kept orthogonal to it (bin by bin),
    // so the two parts add up in power instead of cancelling each other.
    void noiseWave(Snd& s, const Spec& S, int gl = 8, double orthThr = 0.1)
    {
        const int B = S.B;
        int bestTau = -1; double bestD = 1e300;
        std::vector<float> lq(s.Q.d.size()); for (size_t q = 0; q < lq.size(); ++q) lq[q] = std::log(s.Q.d[q] + 1e-14f);
        for (size_t i = 0; i < s.t.size(); ++i)
        {
            const double g = s.g[i]; const int tau = frameOf(s.t[i]);
            if (g <= 0 || tau + Lf > S.F) continue;
            const float lg2 = (float) std::log(g * g); double d = 0;
            for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf; f += 2)
            {
                const cf* sp = S.at(ch, tau + f); const float* l = lq.data() + ((size_t) ch * Lf + f) * B;
                for (int k = 0; k < B; k += 2) { const float v = std::log(std::norm(sp[k]) + 1e-14f) - lg2 - l[k]; d += (double) v * v; }
            }
            if (d < bestD) { bestD = d; bestTau = tau; }
        }
        s.Tn.resize(Lw);
        if (bestTau < 0) return;
        const bool orth = s.T.any();
        Spec Sc; if (orth) stft(s.T, Sc, Lf);
        Spec Z; Z.init(Lf, B);
        for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf; ++f)
        {
            const cf* sp = S.at(ch, bestTau + f); const float* q = s.Q.at(ch, f); cf* z = Z.at(ch, f);
            for (int k = 0; k < B; ++k) { const float m = std::abs(sp[k]); z[k] = (m > 1e-15f ? sp[k] / m : cf(1, 0)) * std::sqrt(std::max(0.f, q[k])); }
        }
        istft(Z, s.Tn, Lw);
        for (int it = 0; it < gl; ++it)
        {
            Spec Y; stft(s.Tn, Y, Lf);
            for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf; ++f)
            {
                cf* y = Y.at(ch, f); const float* q = s.Q.at(ch, f);
                const cf* c = orth ? Sc.at(ch, f) : nullptr;
                for (int k = 0; k < B; ++k)
                {
                    cf v = y[k];
                    if (c != nullptr)
                    {
                        const float cc = std::norm(c[k]);
                        if (cc > orthThr * q[k]) v -= (float) ((v.real() * c[k].real() + v.imag() * c[k].imag()) / (cc + 1e-20f)) * c[k];
                    }
                    const float m = std::abs(v); y[k] = (m > 1e-15f ? v / m : cf(1, 0)) * std::sqrt(std::max(0.f, q[k]));
                }
            }
            istft(Y, s.Tn, Lw);
        }
        // Griffin-Lim / orthogonalisation lose a little power: calibrate per bin to the learned profile
        Spec Y; stft(s.Tn, Y, Lf);
        std::vector<double> num((size_t) B, 0.0), den((size_t) B, 0.0);
        for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf; ++f) { const cf* y = Y.at(ch, f); const float* q = s.Q.at(ch, f); for (int k = 0; k < B; ++k) { num[(size_t) k] += q[k]; den[(size_t) k] += std::norm(y[k]); } }
        std::vector<double> gk((size_t) B);
        for (int k = 0; k < B; ++k)
        {
            double a = 0, b = 0; for (int d = -4; d <= 4; ++d) { const int kk = std::clamp(k + d, 0, B - 1); a += num[(size_t) kk]; b += den[(size_t) kk]; }
            gk[(size_t) k] = b > 1e-20 ? std::clamp(std::sqrt(a / b), 0.5, 2.0) : 1.0;
        }
        for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf; ++f) { cf* y = Y.at(ch, f); for (int k = 0; k < B; ++k) y[k] *= (float) gk[(size_t) k]; }
        istft(Y, s.Tn, Lw);
    }

    // ---------------------------------------------------------------- structure
    // a sound may also play on an onset given to another sound (hat on top of a kick)
    int addLayers()
    {
        Buf y = synthCoherent(); int added = 0;
        const int Lc = std::min(Lw, (int) (0.08 * sr));
        std::vector<double> cc;
        for (auto& s : snd)
        {
            if (!s.T.any()) continue;
            const std::vector<double> times = s.t;
            const double ss = s.T.energy();
            for (double o : on)
            {
                double md = 1e300; for (double t : times) md = std::min(md, std::abs(t - o));
                if (md < 0.006 * sr) continue;
                const int base = (int) o - 48, rl = Lc + 96;
                Buf r(rl); for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < rl; ++k) { const int t = base + k; if (t >= 0 && t < N) r.c[ch][(size_t) k] = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; }
                xcorr(r, 0, rl, s.T, Lc, 97, cc);
                const int k = (int) (std::max_element(cc.begin(), cc.end()) - cc.begin());
                const int i0 = base + k; const int a = std::max(0, i0), b = std::min(N, i0 + Lw);
                double num = 0, loc = 0, sp = 0;
                for (int ch = 0; ch < 2; ++ch) for (int t = a; t < b; ++t) { const double rv = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; const double tv = s.T.c[ch][(size_t) (t - i0)]; num += rv * tv; loc += rv * rv; sp += tv * tv; }
                (void) ss;
                const double g = num / std::max(1e-12, sp);
                if (g <= 0) continue;
                if (i0 >= 0 && g * g * sp / (loc + 1e-12) > 0.15) { addShifted(y, s.T, i0, g); s.t.push_back(i0); s.g.push_back(g); ++added; }
            }
            std::vector<std::pair<double, double>> v; for (size_t i = 0; i < s.t.size(); ++i) v.push_back({ s.t[i], s.g[i] });
            std::sort(v.begin(), v.end()); s.t.clear(); s.g.clear(); for (auto& p : v) { s.t.push_back(p.first); s.g.push_back(p.second); }
        }
        return added;
    }

    // Brusfri-like: the noise profile of a hat may not exceed what its quieter hits show (25th percentile,
    // gain normalised): energy only some hits have is somebody else's leakage
    void capNoisy(const Pow& V, double pctl = 0.25, double headroom = 2.0)
    {
        const int Fn = V.F, B = V.B;
        for (auto& s : snd)
        {
            if (!s.noisy || s.t.size() < 2) continue;
            std::vector<int> taus; std::vector<float> hs;
            for (size_t i = 0; i < s.t.size(); ++i) { const int tau = frameOf(s.t[i]); if (s.g[i] > 0 && tau + Lf <= Fn) { taus.push_back(tau); hs.push_back((float) (s.g[i] * s.g[i])); } }
            if (taus.size() < 2) continue;
            std::vector<float> v(taus.size());
            const size_t kq = (size_t) std::floor(pctl * (taus.size() - 1));
            for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf; ++f)
            {
                float* q = s.Q.at(ch, f);
                for (int k = 0; k < B; ++k)
                {
                    for (size_t i = 0; i < taus.size(); ++i) v[i] = V.at(ch, taus[i] + f)[k] / hs[i];
                    std::nth_element(v.begin(), v.begin() + (long) kq, v.end());
                    q[k] = std::min(q[k], (float) (headroom * v[kq]));
                }
            }
        }
    }

    // a noise-only sound (hat) may also sound on an onset given to another sound (hat on top of a kick):
    // decided in the power domain, on what the current model leaves unexplained
    int addNoiseLayers(const Pow& V)
    {
        const int Fn = V.F, B = V.B, Ld = std::min(Lf, (int) (0.1 * sr / STH) + 1);
        Pow Vh; Vh.init(Fn, B, 1e-14f);
        for (auto& s : snd)
            for (size_t i = 0; i < s.t.size(); ++i)
            {
                const float h = (float) (s.g[i] * s.g[i]); if (h <= 0) continue;
                const int tau = frameOf(s.t[i]);
                for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && tau + f < Fn; ++f) { float* o = Vh.at(ch, tau + f); const float* q = s.Q.at(ch, f); for (int k = 0; k < B; ++k) o[k] += h * q[k]; }
            }
        int added = 0;
        for (auto& s : snd)
        {
            if (!s.noisy || s.t.empty()) continue;
            const double gmed = median(s.g);
            float qmax = 0; for (float q : s.Q.d) qmax = std::max(qmax, q);
            std::vector<double> nt;
            for (double o : on)
            {
                bool near = false; for (double t : s.t) if (std::abs(t - o) < 0.006 * sr) { near = true; break; }
                if (near) continue;
                const int tau = frameOf(o);
                double num = 0, den = 0, ee = 0;
                for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Ld && tau + f < Fn; ++f)
                {
                    const float* v = V.at(ch, tau + f); const float* vh = Vh.at(ch, tau + f); const float* q = s.Q.at(ch, f);
                    for (int k = 0; k < B; ++k) { if (q[k] < 1e-3f * qmax) continue; const double e = std::max(0.f, v[k] - vh[k]); num += q[k] * e; den += (double) q[k] * q[k]; ee += e * e; }
                }
                if (den <= 0 || ee <= 0) continue;
                const double h = num / den; if (h <= 0) continue;
                const double expl = (num * num / den) / ee;          // share of the unexplained power this sound explains
                if (expl > 0.3 && std::sqrt(h) > 0.2 * gmed) nt.push_back(o), s.g.push_back(std::sqrt(h));
                if (!nt.empty() && nt.back() == o)
                {
                    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < Lf && tau + f < Fn; ++f) { float* vv = Vh.at(ch, tau + f); const float* q = s.Q.at(ch, f); for (int k = 0; k < B; ++k) vv[k] += (float) h * q[k]; }
                    ++added;
                }
            }
            for (double t : nt) s.t.push_back(t);
            std::vector<std::pair<double, double>> v; for (size_t i = 0; i < s.t.size(); ++i) v.push_back({ s.t[i], s.g[i] });
            std::sort(v.begin(), v.end()); s.t.clear(); s.g.clear(); for (auto& p : v) { s.t.push_back(p.first); s.g.push_back(p.second); }
        }
        return added;
    }

    // one pad cannot play twice within a few ms (MIDI could not even carry it)
    void mergeClose(double mind)
    {
        for (auto& s : snd)
        {
            std::vector<std::pair<double, double>> v; for (size_t i = 0; i < s.t.size(); ++i) v.push_back({ s.t[i], s.g[i] });
            std::sort(v.begin(), v.end());
            std::vector<std::pair<double, double>> o;
            for (auto& p : v)
            {
                if (!o.empty() && p.first - o.back().first < mind)
                {
                    const double gs = o.back().second + p.second;
                    if (gs > 0) o.back().first = (o.back().first * o.back().second + p.first * p.second) / gs;
                    o.back().second = gs;
                }
                else o.push_back(p);
            }
            s.t.clear(); s.g.clear(); for (auto& p : o) { s.t.push_back(std::floor(p.first + 0.5)); s.g.push_back(p.second); }
        }
    }

    // give a free pad to the sound reproduced worst: split it in two variants (Otsu split along PC1)
    bool splitWorst(const Buf& y, std::string& msg)
    {
        std::vector<std::vector<double>> By; thirdOct(y, By);
        const int nb = (int) thirdCents.size(), nf = (int) Bx[0].size();
        std::vector<double> w((size_t) nb, 0.0);
        for (auto& band : BANDS)
        {
            double tot = 1e-20; for (int b = 0; b < nb; ++b) if (thirdCents[(size_t) b] >= band[0] && thirdCents[(size_t) b] < band[1]) for (double v : Bx[(size_t) b]) tot += v;
            for (int b = 0; b < nb; ++b) if (thirdCents[(size_t) b] >= band[0] && thirdCents[(size_t) b] < band[1]) w[(size_t) b] = 1.0 / tot;
        }
        std::vector<double> e((size_t) nf, 0.0);
        for (int b = 0; b < nb; ++b) for (int f = 0; f < nf; ++f) { const double d = std::sqrt(Bx[(size_t) b][(size_t) f]) - std::sqrt(By[(size_t) b][(size_t) f]); e[(size_t) f] += d * d * w[(size_t) b]; }
        const int win = (int) (0.15 * sr / 441);
        double bestE = -1; int bestC = -1;
        for (size_t c = 0; c < snd.size(); ++c)
        {
            if (snd[c].t.size() < 6) continue;
            double s = 0; for (double t : snd[c].t) { const int f0 = (int) (t / 441); for (int f = f0; f < std::min(nf, f0 + win); ++f) s += e[(size_t) f]; }
            if (s > bestE) { bestE = s; bestC = (int) c; }
        }
        if (bestC < 0) return false;
        auto& s = snd[(size_t) bestC];
        const size_t n = s.t.size();
        std::vector<std::array<double, 11>> Fh(n);
        for (size_t i = 0; i < n; ++i) { auto f = feats(s.t[i]); for (int q = 0; q < 10; ++q) Fh[i][(size_t) q] = f[(size_t) q]; Fh[i][10] = 2 * std::log10(s.g[i] + 1e-3); }
        // PC1 by power iteration
        std::array<double, 11> mu {}; for (auto& f : Fh) for (int q = 0; q < 11; ++q) mu[(size_t) q] += f[(size_t) q] / n;
        std::array<double, 11> u {}; u.fill(1.0 / std::sqrt(11.0));
        for (int it = 0; it < 100; ++it)
        {
            std::array<double, 11> v {};
            for (auto& f : Fh) { double p = 0; for (int q = 0; q < 11; ++q) p += (f[(size_t) q] - mu[(size_t) q]) * u[(size_t) q]; for (int q = 0; q < 11; ++q) v[(size_t) q] += p * (f[(size_t) q] - mu[(size_t) q]); }
            double nn = 0; for (double vv : v) nn += vv * vv; nn = std::sqrt(nn) + 1e-30; for (int q = 0; q < 11; ++q) u[(size_t) q] = v[(size_t) q] / nn;
        }
        std::vector<double> p(n); for (size_t i = 0; i < n; ++i) { double s2 = 0; for (int q = 0; q < 11; ++q) s2 += (Fh[i][(size_t) q] - mu[(size_t) q]) * u[(size_t) q]; p[i] = s2; }
        std::vector<size_t> o(n); std::iota(o.begin(), o.end(), 0); std::sort(o.begin(), o.end(), [&](size_t a, size_t b) { return p[a] < p[b]; });
        const int mn = std::max(3, (int) std::ceil(0.15 * n)); double bv = 1e300; int bk = -1;
        for (int k = mn; k <= (int) n - mn; ++k)
        {
            auto var = [&](int a, int b) { double m = 0; for (int i = a; i < b; ++i) m += p[o[(size_t) i]]; m /= (b - a); double v = 0; for (int i = a; i < b; ++i) v += (p[o[(size_t) i]] - m) * (p[o[(size_t) i]] - m); return v; };
            const double v = var(0, k) + var(k, (int) n);
            if (v < bv) { bv = v; bk = k; }
        }
        if (bk < 0) return false;
        Snd ns = s; ns.t.clear(); ns.g.clear();
        std::vector<double> kt, kg;
        for (int i = 0; i < (int) n; ++i)
        {
            const size_t idx = o[(size_t) i];
            if (i >= bk) { ns.t.push_back(s.t[idx]); ns.g.push_back(s.g[idx]); } else { kt.push_back(s.t[idx]); kg.push_back(s.g[idx]); }
        }
        auto sortTG = [](std::vector<double>& t, std::vector<double>& g) { std::vector<std::pair<double, double>> v; for (size_t i = 0; i < t.size(); ++i) v.push_back({ t[i], g[i] }); std::sort(v.begin(), v.end()); t.clear(); g.clear(); for (auto& q : v) { t.push_back(q.first); g.push_back(q.second); } };
        sortTG(kt, kg); sortTG(ns.t, ns.g);
        s.t = kt; s.g = kg;
        snd.push_back(std::move(ns));
        msg += "  -> split sound " + std::to_string(bestC) + " (" + std::to_string(n) + " hits)";
        return true;
    }

    // ---------------------------------------------------------------- metrics
    void thirdOct(const Buf& y, std::vector<std::vector<double>>& Bo)
    {
        const int Nn = 2048, hop = 441;
        if (thirdCents.empty()) for (int i = 0; i < 30; ++i) { const double c = 25 * std::pow(2.0, i / 3.0); if (c < 18000) thirdCents.push_back(c); }
        Spec S; stft(y, S, nframes(y.n, hop), Nn, hop);
        Bo.assign(thirdCents.size(), std::vector<double>((size_t) S.F, 0.0));
        std::vector<int> bandOf((size_t) S.B, -1);
        for (int k = 0; k < S.B; ++k)
        {
            const double f = k * sr / Nn;
            for (size_t b = 0; b < thirdCents.size(); ++b) if (f >= thirdCents[b] / std::pow(2.0, 1 / 6.0) && f < thirdCents[b] * std::pow(2.0, 1 / 6.0)) { bandOf[(size_t) k] = (int) b; break; }
        }
        for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < S.F; ++f) { const cf* sp = S.at(ch, f); for (int k = 0; k < S.B; ++k) if (bandOf[(size_t) k] >= 0) Bo[(size_t) bandOf[(size_t) k]][(size_t) f] += std::norm(sp[k]); }
    }
    double nullMatch(const Buf& y) const
    {
        double e = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < N; ++i) { const double d = x.c[ch][(size_t) i] - y.c[ch][(size_t) i]; e += d * d; }
        return 1 - e / std::max(1e-20, xEnergy);
    }
    void hearingMatch(const Buf& y, double* out)
    {
        std::vector<std::vector<double>> By; thirdOct(y, By);
        for (int r = 0; r < 5; ++r)
        {
            double num = 0, den = 1e-20;
            for (size_t b = 0; b < thirdCents.size(); ++b)
            {
                if (thirdCents[b] < BANDS[r][0] || thirdCents[b] >= BANDS[r][1]) continue;
                for (size_t f = 0; f < Bx[b].size(); ++f) { const double d = std::sqrt(Bx[b][f]) - std::sqrt(By[b][f]); num += d * d; den += Bx[b][f]; }
            }
            out[r] = 1 - num / den;
        }
    }

    // ---------------------------------------------------------------- naming + output
    std::string category(const Buf& T) const
    {
        const int n = 1 << 16; std::vector<cd> z((size_t) n);
        for (int i = 0; i < std::min(n, T.n); ++i) z[(size_t) i] = cd(0.5 * (T.c[0][(size_t) i] + T.c[1][(size_t) i]), 0);
        fftOf(n).run(z.data(), false);
        double tot = 1e-20, low = 0, high = 0;
        for (int k = 0; k <= n / 2; ++k) { const double f = k * sr / n, p = std::norm(z[(size_t) k]); tot += p; if (f < 150) low += p; if (f > 5000) high += p; }
        low /= tot; high /= tot;
        const int w = std::max(1, (int) (0.01 * sr));
        std::vector<double> e((size_t) T.n, 0.0); double run = 0;
        for (int i = 0; i < T.n; ++i)
        {
            const double v = 0.5 * (T.c[0][(size_t) i] + T.c[1][(size_t) i]); run += v * v;
            if (i >= w) { const double u = 0.5 * (T.c[0][(size_t) (i - w)] + T.c[1][(size_t) (i - w)]); run -= u * u; }
            e[(size_t) i] = run;
        }
        const int pk = (int) (std::max_element(e.begin(), e.end()) - e.begin());
        int k = pk; while (k < T.n && e[(size_t) k] >= e[(size_t) pk] * 1e-3) ++k;
        const double dec = (k - pk) / sr;
        if (high > 0.5) return dec < 0.15 ? "chat" : "ohat";
        if (low > 0.45) return "kick";
        return "snare";
    }

    // variants of the same instrument (KICK, KICK 2 ...) become one sound: a drum machine kit
    std::vector<size_t> lastLeaders;
    bool mergeByCategory(bool byWeight = true, std::vector<size_t>* leaders = nullptr)
    {
        std::map<std::string, std::vector<size_t>> groups;
        for (size_t c = 0; c < snd.size(); ++c)
        {
            Buf f(Lw); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) f.c[ch][(size_t) i] = snd[c].T.c[ch][(size_t) i] + snd[c].Tn.c[ch][(size_t) i];
            const std::string cat = category(f);
            groups[cat == "other" ? "other" + std::to_string(c) : cat].push_back(c);
        }
        if (groups.size() == snd.size()) return false;
        if (std::getenv("DP_DEBUG"))
            for (auto& kv : groups) for (size_t c : kv.second)
            {
                double g2 = 0, gm = 0; for (double g : snd[c].g) { g2 += g * g; gm = std::max(gm, g); }
                double q = 0; for (float v : snd[c].Q.d) q += v;
                std::fprintf(stderr, "%-8s sound %zu hits %zu noisy %d Tcoh %.3f Tn %.3f Qsum %.3g gmax %.4f sum g2 %.4f\n", kv.first.c_str(), c, snd[c].t.size(), (int) snd[c].noisy, snd[c].T.energy(), snd[c].Tn.energy(), q, gm, g2);
            }
        std::vector<Snd> merged;
        for (auto& kv : groups)
        {
            auto& v = kv.second;
            // the variant that carries most of the instrument's sound (not just the most hits) leads
            auto weight = [&](size_t c) { double g2 = 0; for (double g : snd[c].g) g2 += g * g; return g2 * (snd[c].T.energy() + snd[c].Tn.energy()); };
            size_t dom = v[0];
            for (size_t c : v) if (byWeight ? weight(c) > weight(dom) : snd[c].t.size() > snd[dom].t.size()) dom = c;
            if (leaders) leaders->push_back(dom);
            Snd m = snd[dom];
            for (size_t c : v) if (c != dom) { m.t.insert(m.t.end(), snd[c].t.begin(), snd[c].t.end()); m.g.insert(m.g.end(), snd[c].g.begin(), snd[c].g.end()); }
            std::vector<std::pair<double, double>> tg; for (size_t i = 0; i < m.t.size(); ++i) tg.push_back({ m.t[i], m.g[i] });
            std::sort(tg.begin(), tg.end()); m.t.clear(); m.g.clear(); for (auto& p : tg) { m.t.push_back(p.first); m.g.push_back(p.second); }
            merged.push_back(std::move(m));
        }
        snd = std::move(merged);
        mergeClose(0.008 * sr);
        return true;
    }

    // a pass without structural changes (no layering / splitting)
    void polishPass()
    {
        solveCoherent(15, 1e-3);
        for (auto& s : snd) { if (s.noisy) s.T.zero(); else decayProject(s.T, {}); }
        Buf yc = synthCoherent();
        std::vector<double> rho = refine(yc, 64);
        yc = synthCoherent();
        Buf R(N); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < N; ++i) R.c[ch][(size_t) i] = x.c[ch][(size_t) i] - yc.c[ch][(size_t) i];
        Spec S; stft(R, S);
        Pow V; V.init(S.F, S.B); for (size_t i = 0; i < S.d.size(); ++i) V.d[i] = std::norm(S.d[i]) + 1e-14f;
        noiseNMF(V, 20);
        for (auto& s : snd) qProject(s);
        capNoisy(V);
        auto pg = powerGains(30);
        for (size_t c = 0; c < snd.size(); ++c) if (rho[c] <= 0.6) snd[c].g = pg[c];
        for (auto& s : snd) noiseWave(s, S);
        for (auto& s : snd)
        {
            double pk = 1e-12;
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) pk = std::max(pk, std::abs(s.T.c[ch][(size_t) i] + s.Tn.c[ch][(size_t) i]));
            s.T.scale(1 / pk); s.Tn.scale(1 / pk); for (auto& q : s.Q.d) q = (float) (q / (pk * pk)); for (auto& g : s.g) g *= pk;
        }
        mergeClose(0.008 * sr);
    }

    // noise floor of the stem (hiss / separation hash): mean spectrum of the quietest 10 % of frames
    std::vector<double> stemFloor() const
    {
        std::vector<double> fe((size_t) Vx.F, 0.0);
        for (int f = 0; f < Vx.F; ++f) for (int ch = 0; ch < 2; ++ch) { const float* v = Vx.at(ch, f); for (int k = 0; k < Vx.B; ++k) fe[(size_t) f] += v[k]; }
        std::vector<double> srt = fe; std::sort(srt.begin(), srt.end());
        const double thr = srt[(size_t) (0.1 * (srt.size() - 1))];
        std::vector<double> nf((size_t) Vx.B, 0.0); int n = 0;
        for (int f = 0; f < Vx.F; ++f) if (fe[(size_t) f] <= thr) { ++n; for (int ch = 0; ch < 2; ++ch) { const float* v = Vx.at(ch, f); for (int k = 0; k < Vx.B; ++k) nf[(size_t) k] += 0.5 * v[k]; } }
        for (auto& v : nf) v /= std::max(1, n);
        return nf;
    }

    // Brusfri-like gate: what is not clearly above the learned stem floor fades out (transient untouched)
    void denoise(Buf& s, const std::vector<double>& floorP, double gainOfShot) const
    {
        if (s.n < STN || floorP.empty()) return;
        Spec S; stft(s, S);
        const double g2 = gainOfShot * gainOfShot;                 // floor of the loop, at the level of this shot
        const int keep = std::max(1, (int) (0.012 * sr / STH));
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < S.B; ++k)
            {
                double gs = 1.0;
                for (int f = 0; f < S.F; ++f)
                {
                    cf& v = S.at(ch, f)[k];
                    const double p = std::norm(v) + 1e-30, nn = 3.0 * floorP[(size_t) k] * g2;
                    double g = std::clamp(1.0 - nn / p, 0.05, 1.0);
                    if (f < keep) g = 1.0;
                    gs = g < gs ? g : gs + 0.1 * (g - gs);     // closes fast, opens slowly: a tail does not come back
                    v *= (float) gs;
                }
            }
        Buf y; istft(S, y, s.n); s = std::move(y);
    }

    double nextEventAfter(double t) const
    {
        double nx = N;
        for (const auto& s : snd) for (size_t i = 0; i < s.t.size(); ++i) if (s.g[i] > 0 && s.t[i] > t + 0.004 * sr) nx = std::min(nx, s.t[i]);
        for (double o : on) if (o > t + 0.004 * sr) nx = std::min(nx, o);
        return nx;
    }

    // hats ring on top of most kicks: above 5 kHz a kick keeps only its click (first ~8 ms), then fades out
    void tameHighTail(Buf& s) const
    {
        if (s.n < STN) return;
        Spec S; stft(s, S);
        const int f0 = std::max(1, (int) (0.008 * sr / STH)), f1 = f0 + std::max(1, (int) (0.015 * sr / STH));
        for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < S.F; ++f)
        {
            const double t = std::clamp((double) (f - f0) / (f1 - f0), 0.0, 1.0), gl = std::pow(10.0, -36.0 * t / 20.0);
            cf* v = S.at(ch, f);
            for (int k = 0; k < S.B; ++k)
            {
                const double fr = k * sr / STN;
                if (fr < 3500) continue;
                const double w = std::min(1.0, (fr - 3500) / 1500.0);
                const double wa = std::getenv("DP_NO_AIR") ? 0.0 : std::clamp((fr - 6000) / 2000.0, 0.0, 1.0);        // air: -18 dB from the start
                v[k] *= (float) ((1.0 - w * (1.0 - gl)) * (1.0 - wa * (1.0 - 0.125)));
            }
        }
        Buf y; istft(S, y, s.n); s = std::move(y);
    }

    // the cleanest real hit of sound c: explained well by its own model, little of the others around it,
    // and a long gap before the next hit (the real part of the shot ends there)
    int bestHit(size_t c, const Buf& yAll, const Buf& full) const
    {
        const auto& s = snd[c]; double bestS = -1e300; int bi = -1;
        const int W = std::min(Lw, (int) (0.3 * sr));
        double gmax = 1e-12; for (double g : s.g) gmax = std::max(gmax, g);
        for (size_t i = 0; i < s.t.size(); ++i)
        {
            const double g = s.g[i]; if (g < 0.5 * gmax) continue;      // only strong hits: a ghost note blown up is noise
            const int i0 = (int) std::floor(s.t[i] + 0.5); if (i0 < 0 || i0 + W > N) continue;
            double eo = 0, ei = 0, er = 0, ex = 0;
            for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < W; ++k)
            {
                const double own = g * full.c[ch][(size_t) k];
                const double others = yAll.c[ch][(size_t) (i0 + k)] - own;
                const double r = x.c[ch][(size_t) (i0 + k)] - others;            // the loop minus everybody else
                eo += own * own; ei += others * others; er += r * r; ex += (r - own) * (r - own);
            }
            const double fit = 1 - ex / std::max(1e-20, er), clean = eo / (eo + 4 * ei + 1e-20);
            const double gap = std::min(1.0, (nextEventAfter(s.t[i]) - s.t[i]) / (0.25 * sr));
            const double sc = std::max(0.0, fit) * clean * std::sqrt(g / gmax) * (0.3 + 0.7 * gap);
            if (sc > bestS) { bestS = sc; bi = (int) i; }
        }
        return bi;
    }

    // the hat that rang on top of a kick lived inside the kick's sound; with the kick cleaned it must be
    // played by the hat pad: add a hat note where the loop has clearly more highs than the model explains
    void moveHatsOffKicks()
    {
        int hat = -1, kick = -1;
        std::vector<Buf> full(snd.size());
        for (size_t c = 0; c < snd.size(); ++c)
        {
            full[c].resize(Lw); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) full[c].c[ch][(size_t) i] = snd[c].T.c[ch][(size_t) i] + snd[c].Tn.c[ch][(size_t) i];
            const auto cat = category(full[c]);
            if (cat == "chat" && (hat < 0 || snd[c].t.size() > snd[(size_t) hat].t.size())) hat = (int) c;
            if (cat == "kick" && (kick < 0 || snd[c].t.size() > snd[(size_t) kick].t.size())) kick = (int) c;
        }
        if (hat < 0 || kick < 0) return;
        const int W = (int) (0.06 * sr);
        auto hp = [&](const Buf& b, int i0) {        // energy above 5 kHz in [i0, i0+W) - simple 2nd-order difference filter
            double e = 0;
            for (int ch = 0; ch < 2; ++ch) for (int i = std::max(2, i0); i < std::min(b.n, i0 + W); ++i)
            { const double d = b.c[ch][(size_t) i] - 2 * b.c[ch][(size_t) (i - 1)] + b.c[ch][(size_t) (i - 2)]; e += d * d; }
            return e; };
        Buf hatShot = full[(size_t) hat]; const double eh = hp(hatShot, 0);
        Buf kickTamed = full[(size_t) kick]; tameHighTail(kickTamed);
        if (eh <= 0) return;
        std::vector<double> hg = snd[(size_t) hat].g; const double gmed = median(hg);
        Buf yOthers = synthFull();                    // everything as modelled now
        auto& K = snd[(size_t) kick]; auto& H = snd[(size_t) hat];
        std::vector<std::pair<double, double>> add;
        for (size_t i = 0; i < K.t.size(); ++i)
        {
            const double t = K.t[i], g = K.g[i];
            int near = -1; for (size_t j = 0; j < H.t.size(); ++j) if (std::abs(H.t[j] - t) < 0.006 * sr) { near = (int) j; break; }
            if (g <= 0) continue;
            const int i0 = rnd(t);
            // model without the kick's own highs that the cleanup removes
            Buf kOld(W), kNew(W);
            for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < W; ++k) { kOld.c[ch][(size_t) k] = g * full[(size_t) kick].c[ch][(size_t) k]; kNew.c[ch][(size_t) k] = g * kickTamed.c[ch][(size_t) k]; }
            Buf loopW(W), modW(W);
            for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < W && i0 + k < N; ++k)
            { loopW.c[ch][(size_t) k] = x.c[ch][(size_t) (i0 + k)]; modW.c[ch][(size_t) k] = yOthers.c[ch][(size_t) (i0 + k)] - kOld.c[ch][(size_t) k] + kNew.c[ch][(size_t) k]; }
            const double missing = hp(loopW, 0) - hp(modW, 0);
            if (near >= 0)
            {   // the hat is already there: it takes over the highs the kick gives up
                if (missing > 0) { double& hg2 = H.g[(size_t) near]; hg2 = std::min(std::sqrt(hg2 * hg2 + missing / eh), 2.0 * std::max(gmed, hg2)); }
            }
            else if (missing > 0.25 * gmed * gmed * eh) add.push_back({ t, std::sqrt(missing / eh) });
        }
        for (auto& a : add) { H.t.push_back(a.first); H.g.push_back(a.second); }
        std::vector<std::pair<double, double>> v; for (size_t i = 0; i < H.t.size(); ++i) v.push_back({ H.t[i], H.g[i] });
        std::sort(v.begin(), v.end()); H.t.clear(); H.g.clear(); for (auto& p : v) { H.t.push_back(p.first); H.g.push_back(p.second); }
    }

    // a clear onset that no sound plays (an isolated hat the clustering gave to a dead sound) gets the
    // sound whose spectrum fits it best
    void rescueOrphans()
    {
        if (snd.empty()) return;
        const Buf y = synthFull();
        const int W = (int) (0.1 * sr), nf = 8192;
        auto bands = [&](auto get, std::vector<double>& e) {
            std::vector<cd> z((size_t) nf);
            for (int i = 0; i < std::min(W, nf); ++i) { const double w = 0.5 + 0.5 * std::cos(PI * i / W); z[(size_t) i] = cd(get(i) * w, 0); }
            fftOf(nf).run(z.data(), false);
            e.assign(26, 0.0);
            for (int k = 1; k <= nf / 2; ++k)
            {
                const double f = k * sr / nf; if (f < 40) continue;
                const int b = std::min(25, (int) std::floor(3.0 * std::log2(f / 40.0)));
                e[(size_t) b] += std::norm(z[(size_t) k]);
            } };
        std::vector<std::vector<double>> tb(snd.size()); std::vector<double> et(snd.size(), 0.0), gmed(snd.size(), 0.0);
        for (size_t c = 0; c < snd.size(); ++c)
        {
            const auto& s = snd[c];
            bands([&](int i) { return i < Lw ? 0.5 * (s.T.c[0][(size_t) i] + s.Tn.c[0][(size_t) i] + s.T.c[1][(size_t) i] + s.Tn.c[1][(size_t) i]) : 0.0; }, tb[c]);
            for (double v : tb[c]) et[c] += v;
            std::vector<double> gp; for (double g : s.g) if (g > 0) gp.push_back(g);
            gmed[c] = median(gp);
        }
        struct Add { size_t c; double t, g; }; std::vector<Add> adds;
        for (double o : on)
        {
            bool covered = false;
            for (const auto& s : snd) { for (size_t i = 0; i < s.t.size(); ++i) if (s.g[i] > 0 && std::abs(s.t[i] - o) < 0.010 * sr) { covered = true; break; } if (covered) break; }
            const int i0 = rnd(o);
            if (covered || i0 + W > N) continue;
            double er = 0, ex = 0;
            for (int ch = 0; ch < 2; ++ch) for (int i = i0; i < i0 + W; ++i) { const double r = x.c[ch][(size_t) i] - y.c[ch][(size_t) i]; er += r * r; ex += x.c[ch][(size_t) i] * x.c[ch][(size_t) i]; }
            if (er < 0.3 * ex || ex <= 0) continue;                      // the model already plays what is there
            {   // a real attack in what is left: clearly louder right after the onset than right before it
                const int w15 = (int) (0.015 * sr); if (i0 - w15 < 0) continue;
                double pre = 0, post = 0;
                for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < w15; ++k)
                {
                    const double a = x.c[ch][(size_t) (i0 - w15 + k)] - y.c[ch][(size_t) (i0 - w15 + k)], b2 = x.c[ch][(size_t) (i0 + k)] - y.c[ch][(size_t) (i0 + k)];
                    pre += a * a; post += b2 * b2;
                }
                if (post < 6.0 * pre) continue;
            }
            std::vector<double> rb;
            bands([&](int i) { return 0.5 * (x.c[0][(size_t) (i0 + i)] - y.c[0][(size_t) (i0 + i)] + x.c[1][(size_t) (i0 + i)] - y.c[1][(size_t) (i0 + i)]); }, rb);
            double erb = 0; for (double v : rb) erb += v;
            double bc = 0; size_t bi = 0;
            for (size_t c = 0; c < snd.size(); ++c)
            {
                if (et[c] <= 0 || gmed[c] <= 0) continue;
                double num = 0; for (size_t b = 0; b < rb.size(); ++b) num += std::sqrt(rb[b] * tb[c][b]);
                const double cs = num / std::sqrt(erb * et[c] + 1e-30);
                if (cs > bc) { bc = cs; bi = c; }
            }
            if (bc < 0.8) continue;
            const double g = std::sqrt(erb / et[bi]);                    // level of the residual vs the shot (same window)
            if (g < 0.15 * gmed[bi]) continue;
            bool flam = false;                                          // right after its own hit: a ring / flam, not a new hit
            for (size_t i = 0; i < snd[bi].t.size(); ++i) if (snd[bi].g[i] > 0 && o - snd[bi].t[i] > 0 && o - snd[bi].t[i] < 0.045 * sr) flam = true;
            if (flam) continue;
            adds.push_back({ bi, (double) i0, std::min(g, 2.0 * gmed[bi]) });
        }
        for (auto& a : adds)
        {
            auto& s = snd[a.c]; s.t.push_back(a.t); s.g.push_back(a.g);
            std::vector<std::pair<double, double>> v; for (size_t i = 0; i < s.t.size(); ++i) v.push_back({ s.t[i], s.g[i] });
            std::sort(v.begin(), v.end()); s.t.clear(); s.g.clear(); for (auto& p : v) { s.t.push_back(p.first); s.g.push_back(p.second); }
        }
        if (std::getenv("DP_DEBUG")) for (auto& a : adds) std::fprintf(stderr, "rescued onset %.0f -> sound %zu g %.3f\n", a.t, a.c, a.g);
    }

    // a loop cut right on (or just after) its first hit: that hit starts somewhere inside its sound. Each
    // edge hit is matched with the part of its sound that fits (an offset of 0 - 60 ms), strongest first;
    // what is then not needed (a ghost snare explaining a cut kick) is dropped
    void edgeRefit()
    {
        const double edge = 0.02 * sr; const int Wn = std::min(N, (int) (0.25 * sr));
        if (Wn < (int) (0.05 * sr)) return;
        struct H { size_t c, i; int t0; }; std::vector<H> hs;
        for (size_t c = 0; c < snd.size(); ++c) for (size_t i = 0; i < snd[c].t.size(); ++i) if (snd[c].t[i] < edge && snd[c].g[i] > 0) hs.push_back({ c, i, rnd(snd[c].t[i]) });
        if (hs.empty() || hs.size() > 6) return;
        Buf y = synthFull();
        auto full = [&](size_t c, int ch, int k) { return k >= 0 && k < Lw ? snd[c].T.c[ch][(size_t) k] + snd[c].Tn.c[ch][(size_t) k] : 0.0; };
        for (const auto& h : hs) for (int ch = 0; ch < 2; ++ch) for (int t = 0; t < Wn; ++t) y.c[ch][(size_t) t] -= snd[h.c].g[h.i] * full(h.c, ch, t - h.t0);
        Buf r(Wn); double er0 = 0;
        for (int ch = 0; ch < 2; ++ch) for (int t = 0; t < Wn; ++t) { r.c[ch][(size_t) t] = x.c[ch][(size_t) t] - y.c[ch][(size_t) t]; er0 += r.c[ch][(size_t) t] * r.c[ch][(size_t) t]; }
        if (er0 <= 0) return;
        std::sort(hs.begin(), hs.end(), [&](const H& p, const H& q) { return snd[p.c].g[p.i] > snd[q.c].g[q.i]; });
        const int maxOff = (int) (0.06 * sr), step = 8;
        for (const auto& h : hs)
        {
            auto& s = snd[h.c];
            double bestE = 0, bestG = 0; int bestD = 0;
            for (int d = 0; d <= maxOff; d += step)
            {
                double num = 0, den = 0;
                for (int ch = 0; ch < 2; ++ch) for (int t = h.t0; t < Wn; ++t) { const double tv = full(h.c, ch, t - h.t0 + d); num += tv * r.c[ch][(size_t) t]; den += tv * tv; }
                if (den <= 0 || num <= 0) continue;
                const double e = num * num / den;                       // energy this placement explains
                if (e > bestE) { bestE = e; bestG = num / den; bestD = d; }
            }
            std::vector<double> gp; for (size_t i = 0; i < s.t.size(); ++i) if (i != h.i && s.g[i] > 0) gp.push_back(s.g[i]);
            const double gm = gp.empty() ? bestG : median(gp);
            double ng = bestE > 0.1 * er0 ? bestG : 0.0;                // must explain a real share of what is there
            if (ng < 0.15 * gm) ng = 0;
            if (!gp.empty()) ng = std::min(ng, 1.2 * *std::max_element(gp.begin(), gp.end()));
            if (std::getenv("DP_DEBUG")) std::fprintf(stderr, "edge hit sound %zu t %d g %.3f -> %.3f offset %.1f ms share %.2f (median %.3f)\n", h.c, h.t0, s.g[h.i], ng, 1000.0 * bestD / sr, bestE / er0, gm);
            s.g[h.i] = ng;
            if (ng > 0) for (int ch = 0; ch < 2; ++ch) for (int t = h.t0; t < Wn; ++t) r.c[ch][(size_t) t] -= ng * full(h.c, ch, t - h.t0 + bestD);
        }
    }

    // leftovers: a note far below its pad's normal level, right next to a stronger note of another pad
    static void dropJunkNotes(Output& out, double sr)
    {
        std::map<int, int> p90;
        { std::map<int, std::vector<int>> v; for (auto& n : out.notes) v[n.pad].push_back(n.velocity);
          for (auto& kv : v) { std::sort(kv.second.begin(), kv.second.end()); p90[kv.first] = kv.second[(size_t) (0.9 * (kv.second.size() - 1))]; } }
        std::vector<NoteOut> keep;
        for (size_t i = 0; i < out.notes.size(); ++i)
        {
            const auto& n = out.notes[i];
            bool junk = n.velocity <= 2;
            if (!junk && n.velocity < 0.1 * p90[n.pad])
                for (const auto& m : out.notes) if (m.pad != n.pad && std::abs(m.time - n.time) < 0.012 * sr && m.velocity > n.velocity) { junk = true; break; }
            if (!junk) keep.push_back(n);
        }
        std::vector<int> cnt(out.pads.size(), 0); for (auto& n : keep) cnt[(size_t) n.pad]++;
        for (size_t p = 0; p < out.pads.size(); ++p) out.pads[p].hits = cnt[p];
        out.notes = std::move(keep);
    }

    void dbgHear(const char* tag)
    {
        if (!std::getenv("DP_DEBUG")) return;
        Buf y = synthFull(); double hm[5]; hearingMatch(y, hm);
        std::fprintf(stderr, "HEAR %-10s %.1f %.1f %.1f %.1f %.1f\n", tag, hm[0] * 100, hm[1] * 100, hm[2] * 100, hm[3] * 100, hm[4] * 100);
    }

    void finish(Output& out)
    {
        dbgHear("start");
        moveHatsOffKicks();
        dbgHear("movehats");
        if (!std::getenv("DP_NO_RESCUE")) rescueOrphans();
        dbgHear("rescue");
        if (!std::getenv("DP_NO_EDGE")) edgeRefit();
        dbgHear("edge");
        const Buf yAll = synthFull();
        const auto floorP = stemFloor();
        static const std::map<std::string, std::vector<std::pair<int, const char*>>> slots = {
            { "kick",  { { 36, "KICK" }, { 35, "KICK 2" }, { 41, "KICK 3" } } },
            { "snare", { { 38, "SNARE" }, { 40, "SNARE 2" }, { 39, "SNARE 3" }, { 37, "SNARE 4" } } },
            { "chat",  { { 42, "C-HAT" }, { 44, "HAT 2" }, { 54, "HAT 3" } } },
            { "ohat",  { { 46, "O-HAT" }, { 49, "CRASH" }, { 51, "RIDE" } } },
            { "other", { { 45, "PERC" }, { 47, "PERC 2" }, { 48, "PERC 3" }, { 50, "PERC 4" }, { 43, "PERC 5" }, { 56, "PERC 6" }, { 60, "PERC 7" }, { 61, "PERC 8" } } } };
        std::vector<Buf> full(snd.size());
        for (size_t c = 0; c < snd.size(); ++c) { full[c].resize(Lw); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < Lw; ++i) full[c].c[ch][(size_t) i] = snd[c].T.c[ch][(size_t) i] + snd[c].Tn.c[ch][(size_t) i]; }
        std::vector<size_t> order(snd.size()); std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return snd[a].t.size() > snd[b].t.size(); });
        std::map<std::string, size_t> used; std::vector<int> taken;
        for (size_t c : order)
        {
            bool anyHit = false; double gmax = 0;
            for (double g : snd[c].g) if (g > 0) { anyHit = true; gmax = std::max(gmax, g); }
            if (!anyHit) continue;
            std::string cat = category(full[c]);
            auto pick = [&](const std::string& k) -> int {
                const auto& v = slots.at(k);
                while (used[k] < v.size() && std::find(taken.begin(), taken.end(), v[used[k]].first) != taken.end()) ++used[k];
                return used[k] < v.size() ? (int) used[k] : -1; };
            int idx = pick(cat); if (idx < 0) { cat = "other"; idx = pick(cat); }
            if (idx < 0) continue;
            PadOut p; p.note = slots.at(cat)[(size_t) idx].first; p.name = slots.at(cat)[(size_t) idx].second; used[cat]++; taken.push_back(p.note);
            double scale = gmax / 1.2;
            if (std::getenv("DP_DEBUG")) std::fprintf(stderr, "finish %s gmax %.4f fullpeak %.4f\n", cat.c_str(), gmax, full[c].peak());
            p.audio = full[c]; p.audio.scale(scale);
            trimShot(p.audio);
            const double pk = p.audio.peak();
            if (pk > 0.99) { p.audio.scale(0.99 / pk); scale *= 0.99 / pk; }
            // "best hit" version on the same velocity scale: real hit = g_i * T  ->  shot = hit * scale / g_i
            const int bi = bestHit(c, yAll, full[c]);
            if (bi >= 0)
            {
                const double g = snd[c].g[(size_t) bi]; const int i0 = (int) std::floor(snd[c].t[(size_t) bi] + 0.5);
                p.best.resize(Lw); p.bestTime = i0;
                // real audio (others subtracted) until the next hit, then the learned tail (cross-fade 6 ms)
                const int cut = std::max((int) (0.02 * sr), (int) (nextEventAfter(i0) - i0) - (int) (0.001 * sr));
                const int xf = (int) (0.006 * sr);
                for (int ch = 0; ch < 2; ++ch) for (int k = 0; k < Lw; ++k)
                {
                    const double learned = full[c].c[ch][(size_t) k] * scale;
                    double real = learned;
                    if (i0 + k < N) real = (x.c[ch][(size_t) (i0 + k)] - (yAll.c[ch][(size_t) (i0 + k)] - g * full[c].c[ch][(size_t) k])) * scale / g;
                    const double w = k < cut - xf ? 1.0 : (k >= cut ? 0.0 : (double) (cut - k) / xf);
                    p.best.c[ch][(size_t) k] = w * real + (1 - w) * learned;
                }
                denoise(p.best, floorP, scale / g);
                if (!snd[c].mask.empty())                       // a hat keeps no low end (leftovers of kick / snare)
                {
                    Spec S; stft(p.best, S);
                    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < S.F; ++f) { cf* v = S.at(ch, f); for (int k = 0; k < S.B; ++k) v[k] *= (float) snd[c].mask[(size_t) k]; }
                    Buf y; istft(S, y, p.best.n); p.best = std::move(y);
                }
                trimShot(p.best);
            }
            {   std::vector<double> gp; for (double g : snd[c].g) if (g > 0) gp.push_back(g);
                if (std::getenv("DP_DEBUG")) std::fprintf(stderr, "  before denoise peak %.4f  ratio %.3f\n", p.audio.peak(), scale / std::max(1e-9, median(gp)));
            denoise(p.audio, floorP, scale / std::max(1e-9, median(gp))); trimShot(p.audio);
            if (std::getenv("DP_DEBUG")) std::fprintf(stderr, "  after denoise peak %.4f\n", p.audio.peak()); }
            if (p.best.n == 0) p.best = p.audio;
            {   // B sits at the same level as A (same velocity scale); a B far off is a broken take
                auto e50 = [&](const Buf& b) { double e = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < std::min(b.n, (int) (0.1 * sr)); ++i) e += b.c[ch][(size_t) i] * b.c[ch][(size_t) i]; return e; };
                const double ea = e50(p.audio), eb = e50(p.best);
                if (eb > 0 && ea > 0) { const double r = std::sqrt(ea / eb); if (r < 0.5 || r > 2.0) p.best.scale(r); }
            }
            if (cat == "kick") { tameHighTail(p.audio); tameHighTail(p.best); }
            const double pk2 = std::max(p.audio.peak(), p.best.peak());
            if (pk2 > 0.99) { p.audio.scale(0.99 / pk2); p.best.scale(0.99 / pk2); scale *= 0.99 / pk2; }
            const int pi = (int) out.pads.size();
            for (size_t i = 0; i < snd[c].t.size(); ++i)
                if (snd[c].g[i] > 0)
                {
                    NoteOut nt; nt.time = std::floor(snd[c].t[i] + 0.5); nt.pad = pi;
                    nt.velocity = std::clamp((int) std::lround(100.0 * snd[c].g[i] / scale), 1, 127);
                    out.notes.push_back(nt); ++p.hits;
                }
            out.pads.push_back(std::move(p));
        }
        std::sort(out.notes.begin(), out.notes.end(), [](const NoteOut& a, const NoteOut& b) { return a.time < b.time; });
        if (!std::getenv("DP_NO_JUNK")) dropJunkNotes(out, sr);
        std::vector<double> times; for (auto& n : out.notes) times.push_back(n.time);
        out.bpm = estimateBpm(times, N, sr);
        Buf y; render(out.pads, out.notes, y, N);
        out.nullMatch = nullMatch(y); hearingMatch(y, out.hearing);
    }

    void trimShot(Buf& s, double floorDb = -70) const
    {
        const int w = std::max(1, (int) (0.01 * sr));
        std::vector<double> e((size_t) s.n, 0.0); double run = 0, mx = 0;
        for (int i = 0; i < s.n; ++i)
        {
            const double v = 0.5 * (s.c[0][(size_t) i] * s.c[0][(size_t) i] + s.c[1][(size_t) i] * s.c[1][(size_t) i]); run += v;
            if (i >= w) run -= 0.5 * (s.c[0][(size_t) (i - w)] * s.c[0][(size_t) (i - w)] + s.c[1][(size_t) (i - w)] * s.c[1][(size_t) (i - w)]);
            e[(size_t) i] = std::sqrt(std::max(0.0, run / w)); mx = std::max(mx, e[(size_t) i]);
        }
        const double thr = mx * std::pow(10.0, floorDb / 20); int last = s.n - 1;
        while (last > 0 && e[(size_t) last] <= thr) --last;
        const int n = std::min(s.n, last + w);
        Buf o(n); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) o.c[ch][(size_t) i] = s.c[ch][(size_t) i];
        const int fo = std::min(n, (int) (0.005 * sr));
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < fo; ++i) o.c[ch][(size_t) (n - 1 - i)] *= (double) i / fo;
        s = std::move(o);
    }
};

// ------------------------------------------------------------------ TIGHT pattern (drum machine)
// notes snapped to the loop's own grid (1/16, or triplets when they fit clearly better), velocities sorted
// into a few levels per instrument, one note per instrument per grid step. strength 1 = fully on the grid.
struct BeatNote { double beat; int note; int vel; };

inline std::vector<BeatNote> tightPattern(const Output& r, double strength, int* subdivOut = nullptr)
{
    std::vector<BeatNote> out;
    if (r.notes.empty()) return out;
    const double beatLen = 60.0 / r.bpm * r.sr;
    auto fit = [&](int d, double& phase) {
        const double step = beatLen / d; double bestE = 1e300; phase = 0;
        for (int k = 0; k < 64; ++k)
        {
            const double ph = step * k / 64.0; double e = 0;
            for (const auto& n : r.notes) { double q = std::fmod(n.time - ph, step); if (q < 0) q += step; const double dd = std::min(q, step - q); e += n.velocity * dd * dd; }
            if (e < bestE) { bestE = e; phase = ph; }
        }
        return bestE; };
    double ph4 = 0, ph6 = 0;
    const double e4 = fit(4, ph4), e6 = fit(6, ph6);
    const int d = e6 < 0.4 * e4 ? 6 : 4;                 // triplets only when they are clearly the grid
    const double phase = d == 6 ? ph6 : ph4;
    if (subdivOut) *subdivOut = d;
    double minBeat = 1e300;
    for (const auto& n : r.notes) minBeat = std::min(minBeat, (n.time - phase) / beatLen);
    const double b0 = std::floor(minBeat + 0.5 / d);
    // velocity levels per instrument
    std::map<int, std::vector<int>> velsOf;
    for (const auto& n : r.notes) velsOf[r.pads[(size_t) n.pad].note].push_back(n.velocity);
    std::map<int, std::vector<double>> levels;
    for (auto& kv : velsOf)
    {
        auto v = kv.second; std::sort(v.begin(), v.end());
        const int k = v.back() < 1.3 * v.front() ? 1 : (v.size() >= 6 ? 3 : 2);
        std::vector<double> c((size_t) k);
        for (int j = 0; j < k; ++j) c[(size_t) j] = v[(size_t) ((v.size() - 1) * (k == 1 ? 0.5 : (double) j / (k - 1)))];
        for (int it = 0; it < 30; ++it)
        {
            std::vector<double> s((size_t) k, 0.0), n((size_t) k, 0.0);
            for (int x : v) { size_t bj = 0; for (size_t j = 1; j < c.size(); ++j) if (std::abs(x - c[j]) < std::abs(x - c[bj])) bj = j; s[bj] += x; n[bj] += 1; }
            for (size_t j = 0; j < c.size(); ++j) if (n[j] > 0) c[j] = s[j] / n[j];
        }
        levels[kv.first] = c;
    }
    std::map<int, int> top;                              // 90th percentile velocity per instrument
    for (auto& kv : velsOf) { auto v = kv.second; std::sort(v.begin(), v.end()); top[kv.first] = v[(size_t) (0.9 * (v.size() - 1))]; }
    std::map<std::pair<int, long long>, size_t> slot;
    for (const auto& n : r.notes)
    {
        const int note = r.pads[(size_t) n.pad].note;
        if (n.velocity < 0.12 * top[note]) continue;      // leftovers of the analysis, not real hits
        const double beat = (n.time - phase) / beatLen - b0;
        const double snapped = std::round(beat * d) / d;
        const auto& c = levels[note];
        size_t bj = 0; for (size_t j = 1; j < c.size(); ++j) if (std::abs(n.velocity - c[j]) < std::abs(n.velocity - c[bj])) bj = j;
        const int vel = std::clamp((int) std::lround(c[bj]), 1, 127);
        const auto key = std::make_pair(note, (long long) std::llround(snapped * d));
        const BeatNote bn { snapped + (1.0 - strength) * (beat - snapped), note, vel };
        auto it = slot.find(key);
        if (it == slot.end()) { slot[key] = out.size(); out.push_back(bn); }
        else if (vel > out[it->second].vel) out[it->second] = bn;
    }
    std::sort(out.begin(), out.end(), [](const BeatNote& a, const BeatNote& b) { return a.beat < b.beat; });
    return out;
}

// ------------------------------------------------------------------ MIDI with sub-tick timing
// Tick = floor(time). The rest of the tick (0..126/127) is sent as Poly Pressure on the same key ONE TICK
// EARLIER; DrumSlicer delays that note by the fraction -> sample-accurate rebuild even through a DAW.
struct MidiEv { int tick, order; uint8_t st, a, b; };

// EXACT: the loop as played, in seconds at the given tempo (sub-tick timing).  TIGHT: tightPattern() at the
// loop's own tempo, so it plays as a pattern at whatever tempo the project has.
inline std::vector<uint8_t> makeMidiFile(const Output& r, double bpm, int ppq = 480, bool fine = true, double quant = 0.0,
                                         bool tight = false, double tightStrength = 1.0)
{
    const double spt = 60.0 / bpm / ppq * r.sr;
    std::vector<MidiEv> ev; std::map<int, int> last;
    if (tight)
    {
        bpm = r.bpm;
        for (const auto& n : tightPattern(r, tightStrength))
        {
            const int tick = std::max(0, (int) std::lround(n.beat * ppq));
            ev.push_back({ tick, 2, 0x99, (uint8_t) n.note, (uint8_t) n.vel });
            ev.push_back({ tick + ppq / 8, 0, 0x89, (uint8_t) n.note, 0 });
        }
    }
    std::vector<NoteOut> notes = tight ? std::vector<NoteOut>() : r.notes;
    std::sort(notes.begin(), notes.end(), [](const NoteOut& a, const NoteOut& b) { return a.time < b.time; });
    for (const auto& n : notes)
    {
        double exact = n.time / spt;
        if (quant > 0) { const double g = ppq / 4.0; exact += (std::round(exact / g) * g - exact) * quant; }
        int tick = (int) std::floor(exact); int f = (int) std::lround((exact - tick) * 127.0);
        if (f >= 127) { ++tick; f = 0; }
        const int note = r.pads[(size_t) n.pad].note;
        auto it = last.find(note);
        if (fine && quant <= 0 && tick >= 1 && f > 0 && (it == last.end() || tick - it->second >= 2))
            ev.push_back({ tick - 1, 1, 0xA9, (uint8_t) note, (uint8_t) f });
        last[note] = tick;
        ev.push_back({ tick, 2, 0x99, (uint8_t) note, (uint8_t) std::clamp(n.velocity, 1, 127) });
        ev.push_back({ tick + ppq / 8, 0, 0x89, (uint8_t) note, 0 });
    }
    std::stable_sort(ev.begin(), ev.end(), [](const MidiEv& a, const MidiEv& b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });
    std::vector<uint8_t> trk;
    auto vlq = [&](uint32_t v) { uint8_t b[5]; int n = 0; b[n++] = v & 0x7F; v >>= 7; while (v) { b[n++] = (uint8_t) ((v & 0x7F) | 0x80); v >>= 7; } while (n) trk.push_back(b[--n]); };
    // no tempo / time-signature events: the DAW would put them on an extra (empty) track and the notes
    // would land one track lower. Positions are already right for the project tempo.
    // Format 1 with one notes-only track: Cubase "Auto Dissolve Format 0" would spread a format-0 file
    // over newly created tracks instead of the track it is dropped on.
    int lastT = 0;
    for (const auto& e : ev) { vlq((uint32_t) (e.tick - lastT)); lastT = e.tick; trk.push_back(e.st); trk.push_back(e.a); trk.push_back(e.b); }
    vlq(0); trk.insert(trk.end(), { 0xFF, 0x2F, 0x00 });
    std::vector<uint8_t> f = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, (uint8_t) (ppq >> 8), (uint8_t) ppq, 'M', 'T', 'r', 'k' };
    const uint32_t L = (uint32_t) trk.size();
    f.push_back((uint8_t) (L >> 24)); f.push_back((uint8_t) (L >> 16)); f.push_back((uint8_t) (L >> 8)); f.push_back((uint8_t) L);
    f.insert(f.end(), trk.begin(), trk.end());
    return f;
}
} // namespace dp
