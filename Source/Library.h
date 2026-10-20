#pragma once
// Your one-shot archive: scanned once (then only new / changed files), a small fingerprint per sound,
// and the sounds nearest to a pad. Nothing is copied: the index keeps paths + ~150 bytes per sound.
//   Documents/ReKit/Index/library.bin
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Extractor.h"
#include <mutex>
#include <map>
#include <set>

namespace ds
{
// ---------------------------------------------------------------- fingerprint
// what a drum sound is like, measured the way it is heard (the level is taken out: peak = 1):
enum FeatIdx
{
    fAttack = 0,     // rise time, log2 ms
    fPunch,          // first 15 ms vs the body (15 - 150 ms), dB
    fLen,            // HEARD LENGTH: time until the body is 20 dB down, log2 ms (what you call short / long)
    fLen10,          // time until 10 dB down, log2 ms (how fast the body goes)
    fTail,           // time until 40 dB down, log2 ms (the quiet end: tail / room)
    fRoom,           // energy 150 - 500 ms vs the first 50 ms, dB (dry vs roomy)
    fHard,           // HARDNESS: 2 - 12 kHz energy in the attack, dB of the whole sound (crack, stick; soft = low)
    fBright,         // 2 - 8 kHz of the whole sound, dB (bright vs dull / muffled)
    fAir,            // above 8 kHz, dB
    fSub,            // below 80 Hz, dB
    fNoise,          // flatness of the body: noise (wires, hats) vs tone (808, tom)
    fPitch,          // pitch of the body, log2 Hz
    fPitchConf,      // how clearly it has one (0..1)
    fGlide,          // pitch at the start minus pitch of the body, semitones (a kick's drop, an 808 glide)
    fCentA,          // spectral centre of the attack, log2 Hz
    fCentB,          // spectral centre of the body, log2 Hz
    fTimbre,         // 8 timbre coefficients (cepstrum of the 24 bands)
    kTimbre = 8,
    fBands = fTimbre + kTimbre,      // 24 bands 30 Hz - 16 kHz, dB relative to the loudest
    kBands = 24,
    fShape = fBands + kBands,        // the sound's picture: 8 bands x 10 moments (0 - 400 ms), dB of its loudest cell
    kShapeB = 8, kShapeT = 10,
    kFeat = fShape + kShapeB * kShapeT
};
using Feat = std::array<float, kFeat>;

inline bool fingerprint(const juce::AudioBuffer<float>& in, double sr, Feat& f)
{
    f.fill(0.f);
    const int n0 = in.getNumSamples();
    if (n0 < 64 || sr <= 0) return false;
    std::vector<float> m((size_t) n0);
    const float* L = in.getReadPointer(0); const float* R = in.getReadPointer(in.getNumChannels() > 1 ? 1 : 0);
    float pk = 0;
    for (int i = 0; i < n0; ++i) { m[(size_t) i] = 0.5f * (L[i] + R[i]); pk = std::max(pk, std::abs(m[(size_t) i])); }
    if (pk < 1.0e-5f) return false;
    // start at the attack (leading silence of a file does not count), 2 s at most; silence after it adds nothing
    int s0 = 0; while (s0 < n0 && std::abs(m[(size_t) s0]) < 0.03f * pk) ++s0;
    s0 = std::max(0, s0 - (int) (0.001 * sr));
    const int n = std::min(n0 - s0, (int) (2.0 * sr));
    if (n < 64) return false;
    for (int i = 0; i < n; ++i) m[(size_t) i] = m[(size_t) (i + s0)] / pk;
    m.resize((size_t) n);

    // envelope: rms over 2 ms (closer to loudness than the raw peak)
    const int H = std::max(1, (int) (0.001 * sr)), K = std::max(1, n / H);
    std::vector<float> env((size_t) K);
    for (int k = 0; k < K; ++k)
    {
        double e = 0; int c = 0;
        for (int i = std::max(0, k * H - H / 2); i < std::min(n, (k + 1) * H + H / 2); ++i) { e += (double) m[(size_t) i] * m[(size_t) i]; ++c; }
        env[(size_t) k] = (float) std::sqrt(e / std::max(1, c));
    }
    int kp = 0; for (int k = 1; k < std::min(K, 80); ++k) if (env[(size_t) k] > env[(size_t) kp]) kp = k;
    const float ep = std::max(1.0e-6f, env[(size_t) kp]);
    int k10 = 0; while (k10 < kp && env[(size_t) k10] < 0.1f * ep) ++k10;
    int k90 = k10; while (k90 < kp && env[(size_t) k90] < 0.9f * ep) ++k90;
    f[fAttack] = (float) std::log2(1.0 + (k90 - k10));
    // the first time it stays below a level (a later bump of the tail does not make the body longer)
    auto fallTo = [&](float db, int holdMs) { const float t = ep * std::pow(10.f, db / 20.f);
        for (int k = kp; k < K; ++k)
        {
            if (env[(size_t) k] >= t) continue;
            bool stays = true;
            for (int j = k; j < std::min(K, k + holdMs); ++j) if (env[(size_t) j] >= t) { stays = false; break; }
            if (stays) return (float) std::log2(1.0 + (k - kp));
        }
        return (float) std::log2(1.0 + (K - kp)); };
    f[fLen10] = fallTo(-10.f, 8); f[fLen] = fallTo(-20.f, 15); f[fTail] = fallTo(-40.f, 30);
    auto energy = [&](double a, double b) { const int i0 = std::min(n, (int) (a * sr)), i1 = std::min(n, (int) (b * sr)); double e = 0;
        for (int i = i0; i < i1; ++i) e += (double) m[(size_t) i] * m[(size_t) i]; return i1 > i0 ? e / (i1 - i0) : 0.0; };
    f[fPunch] = (float) (10.0 * std::log10((energy(0, 0.015) + 1e-12) / (energy(0.015, 0.15) + 1e-12)));
    f[fRoom] = (float) (10.0 * std::log10((energy(0.15, 0.5) + 1e-12) / (energy(0, 0.05) + 1e-12)));

    // spectra: 2048-point frames for the colour, 512-point frames for the picture in time
    constexpr int order = 11, N = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> win((size_t) N), buf((size_t) (2 * N));
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * juce::MathConstants<float>::pi * i / (N - 1));
    auto spectrum = [&](int start, std::vector<double>& pw) {
        std::fill(buf.begin(), buf.end(), 0.f);
        for (int i = 0; i < N; ++i) { const int j = start + i; buf[(size_t) i] = (j >= 0 && j < n) ? m[(size_t) j] * win[(size_t) i] : 0.f; }
        fft.performFrequencyOnlyForwardTransform(buf.data());
        for (int b = 0; b < N / 2; ++b) pw[(size_t) b] += (double) buf[(size_t) b] * buf[(size_t) b]; };
    const double binHz = sr / N;
    std::vector<double> pa((size_t) (N / 2), 0.0), pb((size_t) (N / 2), 0.0);
    spectrum(-N / 2 + (int) (0.012 * sr), pa);                       // the attack: the window ends ~12 ms in
    int frames = 0;
    for (int st = (int) (0.04 * sr) - N / 2; st < std::min(n, (int) (0.30 * sr)); st += N / 2) { spectrum(st, pb); ++frames; }
    if (frames == 0) spectrum(0, pb);
    auto centre = [&](const std::vector<double>& p) { double num = 0, den = 0;
        for (int b = 1; b < N / 2; ++b) { const double hz = b * binHz; if (hz < 20) continue; num += p[(size_t) b] * std::log2(hz); den += p[(size_t) b]; }
        return den > 0 ? (float) (num / den) : 7.f; };
    f[fCentA] = centre(pa); f[fCentB] = centre(pb);
    std::vector<double> all((size_t) (N / 2)); double tot = 0, totA = 0;
    for (int b = 0; b < N / 2; ++b) { all[(size_t) b] = pa[(size_t) b] + pb[(size_t) b]; tot += all[(size_t) b]; totA += pa[(size_t) b]; }
    tot = std::max(tot, 1e-18);
    auto bandDb = [&](const std::vector<double>& p, double lo, double hi, double ref) { double e = 0;
        for (int b = 1; b < N / 2; ++b) { const double hz = b * binHz; if (hz >= lo && hz < hi) e += p[(size_t) b]; }
        return (float) (10.0 * std::log10(e / ref + 1e-9)); };
    f[fHard] = bandDb(pa, 2000, 12000, tot);
    f[fBright] = bandDb(all, 2000, 8000, tot);
    f[fAir] = bandDb(all, 8000, 20000, tot);
    f[fSub] = bandDb(all, 0, 80, tot);
    {   // flatness 300 Hz - 8 kHz of the body
        double lg = 0, ar = 0; int c = 0;
        for (int b = 1; b < N / 2; ++b) { const double hz = b * binHz; if (hz < 300 || hz > 8000) continue; const double p = pb[(size_t) b] + 1e-14; lg += std::log(p); ar += p; ++c; }
        f[fNoise] = c > 0 ? (float) (lg / c - std::log(ar / c)) : -10.f;
    }
    {   // 24 log bands + their cepstrum (timbre coefficients)
        std::array<double, kBands> bands {};
        for (int b = 1; b < N / 2; ++b)
        {
            const double hz = b * binHz;
            if (hz < 30 || hz >= 16000) continue;
            const int k = juce::jlimit(0, kBands - 1, (int) (std::log2(hz / 30.0) / std::log2(16000.0 / 30.0) * kBands));
            bands[(size_t) k] += all[(size_t) b];
        }
        double mx = 1e-18; for (double v : bands) mx = std::max(mx, v);
        std::array<float, kBands> db {};
        for (int k = 0; k < kBands; ++k) { db[(size_t) k] = (float) std::max(-60.0, 10.0 * std::log10(bands[(size_t) k] / mx + 1e-12)); f[(size_t) (fBands + k)] = db[(size_t) k]; }
        for (int c = 0; c < kTimbre; ++c)
        {
            double v = 0;
            for (int k = 0; k < kBands; ++k) v += db[(size_t) k] * std::cos(juce::MathConstants<double>::pi * (c + 1) * (k + 0.5) / kBands);
            f[(size_t) (fTimbre + c)] = (float) (v / kBands);
        }
    }
    {   // the picture: 8 bands x 10 moments, 512-point frames every ~3 ms
        constexpr int o2 = 9, N2 = 1 << o2;
        juce::dsp::FFT fft2(o2);
        std::vector<float> w2((size_t) N2), b2((size_t) (2 * N2));
        for (int i = 0; i < N2; ++i) w2[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * juce::MathConstants<float>::pi * i / (N2 - 1));
        static const double tEdge[kShapeT + 1] = { 0, 0.005, 0.012, 0.022, 0.036, 0.055, 0.08, 0.12, 0.18, 0.26, 0.40 };
        std::array<double, kShapeB * kShapeT> cell {}; std::array<int, kShapeT> cnt {};
        const int hop2 = std::max(1, (int) (0.003 * sr));
        for (int st = -N2 / 2; st + N2 / 2 < std::min(n, (int) (0.40 * sr)); st += hop2)
        {
            const double tc = (st + N2 / 2) / sr;
            int ti = 0; while (ti < kShapeT - 1 && tc >= tEdge[ti + 1]) ++ti;
            std::fill(b2.begin(), b2.end(), 0.f);
            for (int i = 0; i < N2; ++i) { const int j = st + i; b2[(size_t) i] = (j >= 0 && j < n) ? m[(size_t) j] * w2[(size_t) i] : 0.f; }
            fft2.performFrequencyOnlyForwardTransform(b2.data());
            for (int b = 1; b < N2 / 2; ++b)
            {
                const double hz = b * sr / N2;
                if (hz < 40 || hz >= 16000) continue;
                const int k = juce::jlimit(0, kShapeB - 1, (int) (std::log2(hz / 40.0) / std::log2(16000.0 / 40.0) * kShapeB));
                cell[(size_t) (ti * kShapeB + k)] += (double) b2[(size_t) b] * b2[(size_t) b];
            }
            ++cnt[(size_t) ti];
        }
        double mx = 1e-18;
        for (int t = 0; t < kShapeT; ++t) for (int k = 0; k < kShapeB; ++k) { auto& c = cell[(size_t) (t * kShapeB + k)]; c /= std::max(1, cnt[(size_t) t]); mx = std::max(mx, c); }
        for (int i = 0; i < kShapeB * kShapeT; ++i) f[(size_t) (fShape + i)] = (float) std::max(-60.0, 10.0 * std::log10(cell[(size_t) i] / mx + 1e-12));
    }
    // pitch: normalised autocorrelation, 30 Hz - 1 kHz; of the body (40 - 250 ms) and of the start (8 - 40 ms)
    auto pitchOf = [&](double t0, double t1, float& conf) {
        const int a = std::min(n, (int) (t0 * sr)), b = std::min(n, (int) (t1 * sr));
        const int lagMin = (int) (sr / 1000.0), lagMax = std::min((int) (sr / 30.0), (b - a) / 2);
        float best = 0; int bestLag = 0;
        if (lagMax > lagMin + 2)
        {
            // a low sound is alike to itself at a tiny shift too: look only after the curve has first gone down
            // (below 0.2), then take the highest peak - the real period, not the shortest lag
            const int d = std::max(1, (int) (sr / 11025.0));
            std::vector<float> rs; std::vector<int> lags;
            for (int lag = d; lag <= lagMax; lag += d)
            {
                double num = 0, e0 = 0, e1 = 0;
                for (int i = a; i + lag < b; i += d) { const double x = m[(size_t) i], y = m[(size_t) (i + lag)]; num += x * y; e0 += x * x; e1 += y * y; }
                rs.push_back((float) (num / std::sqrt(e0 * e1 + 1e-18))); lags.push_back(lag);
            }
            size_t i0 = 0; while (i0 < rs.size() && rs[i0] > 0.2f) ++i0;
            for (size_t i = i0; i < rs.size(); ++i)
                if (lags[i] >= lagMin && rs[i] > best && (i == 0 || rs[i] >= rs[i - 1]) && (i + 1 >= rs.size() || rs[i] >= rs[i + 1])) { best = rs[i]; bestLag = lags[i]; }
            // a peak at twice the period is often as high: prefer the shorter one when it is nearly as good
            if (bestLag > 0)
                for (size_t i = i0; i < rs.size(); ++i)
                    if (std::abs(lags[i] * 2 - bestLag) <= 2 * d && rs[i] > 0.9f * best && lags[i] >= lagMin) { best = rs[i]; bestLag = lags[i]; break; }
        }
        conf = std::max(0.f, best);
        return bestLag > 0 ? (float) std::log2(sr / bestLag) : 7.f; };
    float cb = 0, cs = 0;
    f[fPitch] = pitchOf(0.04, 0.25, cb); f[fPitchConf] = cb;
    const float ps = pitchOf(0.008, 0.04, cs);
    f[fGlide] = (cb > 0.5f && cs > 0.5f) ? juce::jlimit(-24.f, 24.f, (ps - f[fPitch]) * 12.f) : 0.f;
    return true;
}

// ---------------------------------------------------------------- what kind of drum (for weighting)
enum Family { famKick = 0, famSnare, famHat, famTom, famAny };
inline Family familyOf(const juce::String& padName)
{
    const auto t = padName.toUpperCase();
    if (t.startsWith("KICK")) return famKick;
    if (t.startsWith("SNARE") || t.startsWith("CLAP") || t.startsWith("RIM")) return famSnare;
    if (t.contains("HAT") || t.startsWith("CRASH") || t.startsWith("RIDE") || t.startsWith("SHAKER")) return famHat;
    if (t.startsWith("TOM") || t.startsWith("PERC")) return famTom;
    return famAny;
}
// what the file name says it is ("" = it does not say)
inline juce::String nameHint(const juce::String& pathLower)
{
    auto has = [&](std::initializer_list<const char*> w) { for (auto* x : w) if (pathLower.contains(x)) return true; return false; };
    const auto file = pathLower.fromLastOccurrenceOf("/", false, false).fromLastOccurrenceOf("\\", false, false);
    auto fileHas = [&](std::initializer_list<const char*> w) { for (auto* x : w) if (file.contains(x)) return true; return false; };
    // the file name first, then its folders
    for (int pass = 0; pass < 2; ++pass)
    {
        auto h = [&](std::initializer_list<const char*> w) { return pass == 0 ? fileHas(w) : has(w); };
        if (h({ "kick", "kik", "bassdrum", "bass drum" })) return "KICK";
        if (h({ "snare", "snr", "clap", "rim" })) return "SNARE";
        if (h({ "hat", "hh", "cymbal", "crash", "ride", "shaker", "tamb" })) return "HAT";
        if (h({ "tom", "perc", "conga", "bongo", "cowbell", "wood" })) return "TOM";
    }
    return {};
}
inline Family familyOfHint(const juce::String& h)
{
    return h == "KICK" ? famKick : h == "SNARE" ? famSnare : h == "HAT" ? famHat : h == "TOM" ? famTom : famAny;
}

// ---------------------------------------------------------------- the index
struct LibEntry
{
    juce::String path;
    juce::int64 size = 0, mtime = 0;
    juce::uint8 status = 0;          // 0 = one-shot, 1 = longer than the limit, 2 = loop (several hits), 3 = name says loop, 4 = unreadable
    Feat feat {};
    float seconds = 0;
    juce::uint8 hint = (juce::uint8) famAny;
};
struct LibIndex
{
    std::vector<LibEntry> items;
    std::array<float, kFeat> mean {}, sd {};      // over the one-shots: features are compared in these units
    int oneShots = 0, loops = 0, longs = 0, bad = 0, byName = 0;
    void stats()
    {
        oneShots = loops = longs = bad = byName = 0;
        std::array<double, kFeat> s {}, s2 {};
        for (auto& e : items)
        {
            if (e.status == 0) { ++oneShots; for (int i = 0; i < kFeat; ++i) { s[(size_t) i] += e.feat[(size_t) i]; s2[(size_t) i] += (double) e.feat[(size_t) i] * e.feat[(size_t) i]; } }
            else if (e.status == 1) ++longs; else if (e.status == 4) ++bad; else if (e.status == 3) { ++loops; ++byName; } else ++loops;
        }
        // typical spreads (also the floor: one odd archive must not make a feature count for everything)
        static const float def[fTimbre] = { 1.5f, 5.f, 1.2f, 1.2f, 1.5f, 8.f, 6.f, 6.f, 8.f, 8.f, 1.2f, 1.f, 0.3f, 4.f, 0.8f, 0.8f };
        for (int i = 0; i < kFeat; ++i)
        {
            const double c = std::max(1, oneShots), mu = s[(size_t) i] / c, var = std::max(0.0, s2[(size_t) i] / c - mu * mu);
            const float d0 = i < fTimbre ? def[i] : i < fBands ? 4.f : 10.f;
            mean[(size_t) i] = (float) mu;
            sd[(size_t) i] = oneShots < 50 ? d0 : (float) juce::jlimit(0.5 * d0, 2.0 * d0, std::sqrt(var));
        }
    }
};

struct Match { const LibEntry* e = nullptr; float dist = 0, score = 0; juce::StringArray tags; };

// how different two sounds are, weighted for the drum: what you hear first counts most
inline float distance(const Feat& q, const Feat& c, const LibIndex& ix, Family fam, juce::StringArray* tags = nullptr)
{
    struct W { float len, tail, hard, bright, sub, noise, pitch, glide, timbre, shape; };
    static const W w[] = {
        /* kick  */ { 2.0f, 0.6f, 1.6f, 1.0f, 1.4f, 0.6f, 1.8f, 0.8f, 1.0f, 1.5f },
        /* snare */ { 1.8f, 0.8f, 1.6f, 1.6f, 0.4f, 1.2f, 0.8f, 0.2f, 1.2f, 1.5f },
        /* hat   */ { 2.0f, 1.0f, 1.0f, 1.8f, 0.2f, 1.0f, 0.2f, 0.0f, 1.2f, 1.5f },
        /* tom   */ { 1.8f, 0.6f, 1.2f, 1.0f, 0.6f, 1.0f, 1.8f, 0.6f, 1.2f, 1.5f },
        /* any   */ { 1.8f, 0.8f, 1.2f, 1.2f, 0.8f, 1.0f, 1.0f, 0.4f, 1.2f, 1.5f } };
    const W& k = w[fam];
    auto z = [&](int i) { const float d = (q[(size_t) i] - c[(size_t) i]) / ix.sd[(size_t) i]; return d * d; };
    float d = 0;
    d += k.len * (0.7f * z(fLen) + 0.3f * z(fLen10));
    d += k.tail * 0.5f * (z(fTail) + z(fRoom));
    d += k.hard * (0.6f * z(fHard) + 0.25f * z(fPunch) + 0.15f * z(fAttack));
    d += k.bright * (0.5f * z(fBright) + 0.25f * z(fAir) + 0.25f * z(fCentB));
    d += k.sub * z(fSub);
    d += k.noise * z(fNoise);
    if (q[fPitchConf] > 0.5f && c[fPitchConf] > 0.5f)
    {
        float semis = std::abs(q[fPitch] - c[fPitch]) * 12.f;
        semis = std::min(semis, std::abs(semis - 12.f) + 1.f);          // an octave off is "nearly the same note"
        d += k.pitch * (semis / 3.f) * (semis / 3.f);
        d += k.glide * z(fGlide);
    }
    else if ((q[fPitchConf] > 0.5f) != (c[fPitchConf] > 0.5f)) d += k.pitch * 0.6f;
    float td = 0; for (int i = fTimbre; i < fTimbre + kTimbre; ++i) td += z(i);
    d += k.timbre * td / kTimbre;
    float sd2 = 0; for (int i = fShape; i < kFeat; ++i) { const float x = (q[(size_t) i] - c[(size_t) i]) / 10.f; sd2 += x * x; }
    d += k.shape * sd2 / (kShapeB * kShapeT) * 4.f;
    if (tags != nullptr)
    {   // how it differs from the pad, in the words you would use
        const float dl = c[fLen] - q[fLen];
        if (dl > 0.4f) tags->add("LONGER"); else if (dl < -0.4f) tags->add("SHORTER");
        const float dh = c[fHard] - q[fHard], dp = c[fPunch] - q[fPunch];
        if (dh > 3.f || (dh > 1.5f && dp > 3.f)) tags->add("HARDER"); else if (dh < -3.f || (dh < -1.5f && dp < -3.f)) tags->add("SOFTER");
        const float db = c[fBright] - q[fBright];
        if (db > 3.f) tags->add("BRIGHTER"); else if (db < -3.f) tags->add("DULLER");
        if (q[fPitchConf] > 0.5f && c[fPitchConf] > 0.5f)
        {
            const int st = (int) std::lround((c[fPitch] - q[fPitch]) * 12.f);
            if (std::abs(st) >= 1 && std::abs(st) <= 11) tags->add((st > 0 ? "+" : "") + juce::String(st) + " ST");
        }
        const float dr = c[fRoom] - q[fRoom];
        if (dr > 5.f) tags->add("ROOMIER"); else if (dr < -5.f) tags->add("DRIER");
        if (fam == famKick) { const float ds = c[fSub] - q[fSub]; if (ds > 4.f) tags->add("MORE SUB"); else if (ds < -4.f) tags->add("LESS SUB"); }
        const float dn = c[fNoise] - q[fNoise];
        if (dn > 1.2f) tags->add("NOISIER"); else if (dn < -1.2f) tags->add("CLEANER");
    }
    return d;
}
inline float scoreOf(float dist) { return 100.f / (1.f + 0.12f * dist); }

class Library : private juce::Thread
{
public:
    Library() : juce::Thread("ReKit library") { load(); }
    ~Library() override { stopThread(4000); }

    static juce::File folder() { return root().getChildFile("Index"); }
    static juce::File indexFile() { return folder().getChildFile("library.bin"); }
    // the folders of the SAMPLES panel
    static juce::Array<juce::File> roots()
    {
        juce::Array<juce::File> r;
        juce::StringArray lines; lines.addLines(root().getChildFile("browser.txt").loadFileAsString());
        for (auto& l : lines)
            if (l.startsWith("root="))
            {
                const auto v = l.fromFirstOccurrenceOf("=", false, false);
                if (juce::File::isAbsolutePath(v) && juce::File(v).isDirectory()) r.addIfNotAlreadyThere(juce::File(v));
            }
        return r;
    }

    std::shared_ptr<const LibIndex> index() const { std::lock_guard<std::mutex> g(lock); return current; }
    bool scanning() const { return isThreadRunning(); }
    float progress() const { return prog.load(); }
    juce::String status() const { std::lock_guard<std::mutex> g(lock); return statusText; }
    float maxSeconds() const { return limit.load(); }
    void setMaxSeconds(float s) { limit = s; }

    void scan(bool full = false)
    {
        if (isThreadRunning()) { if (!full) return; stopThread(4000); }
        fullScan = full;
        startThread(juce::Thread::Priority::low);
    }

    // nearest one-shots to a sound
    // (the matches point into "ix": keep it while using them)
    std::vector<Match> nearest(const juce::AudioBuffer<float>& sound, double sr, const juce::String& padName, int count,
                               std::shared_ptr<const LibIndex>& ix) const
    {
        std::vector<Match> out;
        ix = index();
        if (!ix || ix->oneShots == 0) return out;
        Feat q; if (!fingerprint(sound, sr, q)) return out;
        const Family fam = familyOf(padName);
        std::vector<Match> all;
        all.reserve(ix->items.size());
        for (auto& e : ix->items)
        {
            if (e.status != 0) continue;
            Match mt; mt.e = &e;
            mt.dist = distance(q, e.feat, *ix, fam);
            const Family hf = (Family) e.hint;
            if (fam != famAny && hf != famAny && hf != fam) mt.dist = mt.dist * 1.6f + 6.f;   // the name says it is another drum
            all.push_back(mt);
        }
        std::sort(all.begin(), all.end(), [](const Match& a, const Match& b) { return a.dist < b.dist; });
        for (auto& mt : all)
        {
            if ((int) out.size() >= count) break;
            // the same sound in two places (same size, same fingerprint): once
            bool dup = false;
            for (auto& o : out)
                if (o.e->size == mt.e->size)
                {
                    float dmax = 0; for (int i = 0; i < kFeat; ++i) dmax = std::max(dmax, std::abs(o.e->feat[(size_t) i] - mt.e->feat[(size_t) i]));
                    if (dmax < 0.01f) { dup = true; break; }
                }
            if (dup) continue;
            distance(q, mt.e->feat, *ix, fam, &mt.tags);
            while (mt.tags.size() > 3) mt.tags.remove(mt.tags.size() - 1);
            mt.score = scoreOf(mt.dist);
            out.push_back(mt);
        }
        return out;
    }

    // whole words only: "Loops/..." or "kick_loop_120bpm" yes, a label called "Loopmasters" no
    static bool nameSaysLoop(const juce::String& pathLower)
    {
        static const juce::StringArray words { "loop", "loops", "bpm", "fill", "fills", "groove", "grooves", "break", "breaks", "breakbeat", "breakbeats" };
        juce::String w;
        for (auto ch : pathLower + " ")
        {
            if (juce::CharacterFunctions::isLetter(ch)) { w += juce::String::charToString(ch); continue; }
            if (w.isNotEmpty() && words.contains(w)) return true;
            w.clear();
        }
        return false;
    }
    // one-shot or not: two more strong hits after the first (claps / flams within 90 ms count as one)
    static bool soundsLikeLoop(const juce::AudioBuffer<float>& a, double sr)
    {
        const auto on = findOnsets(a, sr);
        if (on.empty()) return false;
        const juce::int64 first = on.front().pos;
        int strong = 0;
        for (auto& o : on) if (o.pos > first + (juce::int64) (0.09 * sr) && o.strength >= 0.35f) ++strong;
        return strong >= 2;
    }

private:
    mutable std::mutex lock;
    std::shared_ptr<const LibIndex> current = std::make_shared<LibIndex>();
    juce::String statusText;
    std::atomic<float> prog { 0.f }, limit { 8.f };
    bool fullScan = false;

    void setStatus(const juce::String& s) { std::lock_guard<std::mutex> g(lock); statusText = s; }
    void publish(std::shared_ptr<LibIndex> ix) { ix->stats(); std::lock_guard<std::mutex> g(lock); current = std::move(ix); }

    void run() override
    {
        const auto dirs = roots();
        prog = 0.f;
        if (dirs.isEmpty()) { setStatus("ADD A SAMPLE FOLDER FIRST"); return; }
        setStatus("LOOKING FOR FILES...");
        juce::Array<juce::File> files, fileRoot;
        for (auto& d : dirs)
            for (const auto& it : juce::RangedDirectoryIterator(d, true, "*.wav;*.wave;*.aif;*.aiff;*.flac;*.mp3;*.ogg", juce::File::findFiles))
            {
                if (threadShouldExit()) return;
                if (!it.getFile().getFileName().startsWithChar('.')) { files.add(it.getFile()); fileRoot.add(d); }
            }
        // what is known already: same path, size, date and limit = not read again
        std::map<juce::String, const LibEntry*> known;
        auto old = index();
        if (!fullScan && old) for (auto& e : old->items) known[e.path] = &e;
        auto ix = std::make_shared<LibIndex>();
        ix->items.reserve((size_t) files.size());
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        const float lim = limit.load();
        int fresh = 0;
        for (int i = 0; i < files.size(); ++i)
        {
            if (threadShouldExit()) break;
            const auto& f = files.getReference(i);
            LibEntry e; e.path = f.getFullPathName(); e.size = f.getSize(); e.mtime = f.getLastModificationTime().toMilliseconds();
            auto k = known.find(e.path);
            if (k != known.end() && k->second->size == e.size && k->second->mtime == e.mtime && !(k->second->status == 1 && k->second->seconds <= lim))
            {
                ix->items.push_back(*k->second);
                auto& back = ix->items.back();
                if (back.status == 0 && back.seconds > lim) back.status = 1;     // the limit was made shorter
            }
            else
            {
                analyseFile(fm, f, f.getRelativePathFrom(fileRoot.getReference(i)), e, lim);
                ix->items.push_back(e);
                ++fresh;
            }
            prog = (float) (i + 1) / (float) files.size();
            if ((i % 200) == 199)
            {
                setStatus("SCANNING  " + juce::String(i + 1) + " / " + juce::String(files.size()));
                if (fresh > 0) { auto copy = std::make_shared<LibIndex>(*ix); publish(copy); }
            }
        }
        if (threadShouldExit() && ix->items.size() < (size_t) files.size())
        {   // stopped: keep what was done and the rest of the old index
            std::set<juce::String> done; for (auto& x : ix->items) done.insert(x.path);
            for (auto& [p, e] : known) if (done.count(p) == 0) ix->items.push_back(*e);
        }
        publish(ix);
        save(*index());
        prog = 1.f;
        auto now = index();
        setStatus(summary(*now));
    }

    static juce::String summary(const LibIndex& ix)
    {
        return juce::String(ix.oneShots) + " ONE-SHOTS\nLEFT OUT: " + juce::String(ix.loops) + " LOOPS (" + juce::String(ix.byName) + " BY NAME)  "
             + juce::String(ix.longs) + " LONG" + (ix.bad > 0 ? "  " + juce::String(ix.bad) + " UNREADABLE" : juce::String());
    }
    // the name rules look only inside your sample folder (its own path above does not count: a folder
    // called "Loops & One Shots" holding everything must not throw everything out)
    static void analyseFile(juce::AudioFormatManager& fm, const juce::File& f, const juce::String& relPath, LibEntry& e, float lim)
    {
        const auto lower = relPath.toLowerCase().replaceCharacter('\\', '/');
        e.hint = (juce::uint8) familyOfHint(nameHint(lower));
        std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
        if (rd == nullptr || rd->sampleRate <= 0 || rd->lengthInSamples < 32) { e.status = 4; return; }
        e.seconds = (float) (rd->lengthInSamples / rd->sampleRate);
        if (e.seconds > lim) { e.status = 1; return; }                       // decided without reading it
        // a short file is judged by its sound (one hit or several), only a longer one by its name too
        if (e.seconds > 1.5f && nameSaysLoop(lower)) { e.status = 3; return; }
        const int n = (int) rd->lengthInSamples;
        juce::AudioBuffer<float> a(2, n);
        if (!rd->read(&a, 0, n, 0, true, true)) { e.status = 4; return; }
        if (rd->numChannels < 2) a.copyFrom(1, 0, a, 0, 0, n);
        if (soundsLikeLoop(a, rd->sampleRate)) { e.status = 2; return; }
        e.status = fingerprint(a, rd->sampleRate, e.feat) ? 0 : 4;
    }

    void load()
    {
        juce::MemoryBlock mb;
        if (!indexFile().loadFileAsData(mb) || mb.getSize() < 12) return;
        juce::MemoryInputStream in(mb, false);
        if (in.readInt() != 0x524b4c42 /* RKLB */ || in.readInt() != 4) return;     // older index: read the folders again
        const int count = in.readInt();
        if (count < 0 || count > 2000000) return;
        auto ix = std::make_shared<LibIndex>();
        ix->items.reserve((size_t) count);
        for (int i = 0; i < count && !in.isExhausted(); ++i)
        {
            LibEntry e;
            e.path = in.readString(); e.size = in.readInt64(); e.mtime = in.readInt64();
            e.status = (juce::uint8) in.readByte(); e.hint = (juce::uint8) in.readByte(); e.seconds = in.readFloat();
            if (e.status == 0) for (auto& v : e.feat) v = in.readFloat();
            ix->items.push_back(std::move(e));
        }
        publish(ix);
        auto now = index();
        setStatus(summary(*now));
    }
    static void save(const LibIndex& ix)
    {
        juce::MemoryOutputStream out;
        out.writeInt(0x524b4c42); out.writeInt(4); out.writeInt((int) ix.items.size());
        for (auto& e : ix.items)
        {
            out.writeString(e.path); out.writeInt64(e.size); out.writeInt64(e.mtime);
            out.writeByte((char) e.status); out.writeByte((char) e.hint); out.writeFloat(e.seconds);
            if (e.status == 0) for (auto v : e.feat) out.writeFloat(v);
        }
        writeData(indexFile(), out.getData(), out.getDataSize());
    }
    JUCE_DECLARE_NON_COPYABLE(Library)
};
} // namespace ds
