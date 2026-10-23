#pragma once
// JUCE glue around the DrumPeel engine: audio loading, result for the plugin, cache on disk, MIDI bytes.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "DrumPeel.h"
#include "MatchDSP.h"
#include <array>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <cstring>

namespace ds
{
static constexpr int kPads = 8;
static const char* kEngineVersion = "drumpeel-5";

struct Pad
{
    juce::String name { "-" };
    int note = 36, hits = 0;
    juce::AudioBuffer<float> audio;          // "learned": the sound averaged over all its hits (velocity 100 = this level)
    juce::AudioBuffer<float> best;           // "best hit": the cleanest real hit, others subtracted, learned tail
    juce::AudioBuffer<float> user;           // "C": a sample of yours dropped on the pad (level-matched to A)
    juce::String userName, userPath;       // (path: so a search never offers you the sample you already chose)
    // LAYER MATCH: "user" is then your sample + a second one, layered; your own sample alone is kept to go back to
    juce::AudioBuffer<float> userBase;
    juce::String layerName, layerPath;
    bool layered() const { return userBase.getNumSamples() > 0; }
    void clearLayer() { userBase.setSize(0, 0); layerName.clear(); layerPath.clear(); }
    bool valid = false;
    const juce::AudioBuffer<float>& get(bool useBest, bool useUser = false) const
    {
        if (useUser && user.getNumSamples() > 0) return user;
        return useBest && best.getNumSamples() > 0 ? best : audio;
    }
};

struct Hit { double time = 0; int pad = 0; int velocity = 100; };   // time in samples of the loop (whole samples)

struct Result
{
    double sr = 44100.0, bpm = 120.0;
    juce::AudioBuffer<float> loop;           // original loop (display)
    std::array<Pad, kPads> pads;
    std::vector<Hit> hits;
    double nullMatch = 0, hearing[5] {};
    std::array<int, 128> noteToPad {};
    bool fromCache = false;
    juce::int64 regionStart = 0;             // where "loop" starts in the source file (a selection of it was analysed)
    juce::uint64 sourceId = 0;               // which loaded source (see Source) it came from
    juce::String region;                     // cache key of the part: "" = the whole file, else "|r<start>-<end>"

    void buildNoteMap()
    {
        noteToPad.fill(-1);
        for (int p = 0; p < kPads; ++p) if (pads[(size_t) p].valid) noteToPad[(size_t) pads[(size_t) p].note] = p;
        // GM fallback, so a hand-played kit also works: kick / snare / hats / toms / cymbals
        auto firstWith = [&](const char* prefix) { for (int p = 0; p < kPads; ++p) if (pads[(size_t) p].valid && pads[(size_t) p].name.startsWith(prefix)) return p; return -1; };
        auto fallback = [&](int note, std::initializer_list<const char*> prefs) {
            if (noteToPad[(size_t) note] >= 0) return;
            for (auto* pr : prefs) { const int p = firstWith(pr); if (p >= 0) { noteToPad[(size_t) note] = p; return; } } };
        for (int n : { 35, 36 }) fallback(n, { "KICK" });
        for (int n : { 38, 40 }) fallback(n, { "SNARE", "CLAP", "RIM" });
        for (int n : { 39 }) fallback(n, { "CLAP", "SNARE" });
        for (int n : { 37 }) fallback(n, { "RIM", "SNARE" });
        for (int n : { 42, 44 }) fallback(n, { "C-HAT", "HAT", "SHAKER", "O-HAT" });
        for (int n : { 46 }) fallback(n, { "O-HAT", "C-HAT", "HAT" });
        for (int n : { 41, 43, 45, 47, 48, 50 }) fallback(n, { "TOM", "PERC", "SNARE", "KICK" });
        for (int n : { 60, 61, 62, 63, 64, 56, 76, 77 }) fallback(n, { "PERC", "TOM", "RIM" });
        for (int n : { 69, 70, 82 }) fallback(n, { "SHAKER", "C-HAT" });
        for (int n : { 49, 51, 52, 55, 57 }) fallback(n, { "CRASH", "RIDE", "O-HAT", "C-HAT" });
    }
    double meanHearing() const { double s = 0; for (double h : hearing) s += h; return s / 5.0; }
    double minHearing() const { double m = 1; for (double h : hearing) m = std::min(m, h); return m; }
};

inline bool loadAudio(const juce::File& file, juce::AudioBuffer<float>& out, double& sr, double maxSeconds = 300.0)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(file));
    if (rd == nullptr || rd->lengthInSamples < 4096 || rd->sampleRate <= 0) return false;
    sr = rd->sampleRate;
    const int n = (int) std::min<juce::int64>(rd->lengthInSamples, (juce::int64) (sr * maxSeconds));
    out.setSize(2, n);
    out.clear();
    // read in pieces: the length an MP3 says it has can be far off (VBR files without a length header - Suno
    // exports: 121 s of audio said to be 579 s). Reading past the real end fails: what was read up to there is kept.
    int B = 1 << 16, got = 0;
    juce::AudioBuffer<float> tmp(2, B);
    while (got < n)
    {
        const int k = std::min(B, n - got);
        tmp.clear();
        if (!rd->read(&tmp, 0, k, got, true, true))
        {   // near the real end: smaller pieces, down to about one MP3 frame
            if (k <= 1024) break;
            B = std::max(512, k / 2); continue;
        }
        for (int c = 0; c < 2; ++c) out.copyFrom(c, got, tmp, std::min(c, (int) rd->numChannels - 1), 0, k);
        got += k;
    }
    if (got < 4096) return false;
    if (got < n) out.setSize(2, got, true);
    return true;
}

// ---------------------------------------------------------------- the loaded file, before analysis
// Attacks for the selection handles to snap to: the first sample of each hit (just after the last zero
// crossing before it), with a strength (peak right after it, 0..1 of the file's peak).
struct Onset { juce::int64 pos = 0; float strength = 0; };
inline std::vector<Onset> findOnsets(const juce::AudioBuffer<float>& a, double sr)
{
    std::vector<Onset> out;
    const int n = a.getNumSamples();
    if (n < 2048 || sr <= 0) return out;
    const float* L = a.getReadPointer(0); const float* R = a.getReadPointer(a.getNumChannels() > 1 ? 1 : 0);
    auto m = [&](juce::int64 i) { return 0.5f * (L[i] + R[i]); };
    const int H = 64, K = n / H;
    std::vector<float> e((size_t) K), d((size_t) K);         // peak envelope of the signal and of its slope (highs)
    float prev = 0, gmax = 1e-9f, dmax = 1e-9f;
    for (int k = 0; k < K; ++k)
    {
        float ek = 0, dk = 0;
        for (int i = k * H; i < (k + 1) * H; ++i) { const float v = m(i); ek = std::max(ek, std::abs(v)); dk = std::max(dk, std::abs(v - prev)); prev = v; }
        e[(size_t) k] = ek; d[(size_t) k] = dk; gmax = std::max(gmax, ek); dmax = std::max(dmax, dk);
    }
    const int W = std::max(2, (int) (0.016 * sr / H)), gap = std::max(1, (int) (0.035 * sr / H)), tail = std::max(1, (int) (0.03 * sr / H));
    const float fe = gmax * 0.015f, fd = dmax * 0.02f;
    int last = -gap - 1;
    for (int k = W; k < K; ++k)
    {
        if (k - last <= gap) continue;
        float pe = 0, pd = 0;
        for (int j = k - W; j < k; ++j) { pe = std::max(pe, e[(size_t) j]); pd = std::max(pd, d[(size_t) j]); }
        const float re = e[(size_t) k] > fe ? e[(size_t) k] / (pe + 0.25f * fe) : 0.f;
        const float rd = d[(size_t) k] > fd ? d[(size_t) k] / (pd + 0.25f * fd) : 0.f;
        if (std::max(re, rd) < 2.0f) continue;
        last = k;
        // the exact first sample: above the noise just before and 8% of the attack's peak, then back to the zero crossing
        const juce::int64 s0 = (juce::int64) k * H;
        const juce::int64 lo = std::max<juce::int64>(1, s0 - H), hi = std::min<juce::int64>(n - 1, s0 + H);
        float peak = 0; for (juce::int64 i = s0; i < std::min<juce::int64>(n, s0 + (juce::int64) (0.005 * sr)); ++i) peak = std::max(peak, std::abs(m(i)));
        float noise = 0; for (juce::int64 i = std::max<juce::int64>(0, lo - (juce::int64) (0.01 * sr)); i < lo; ++i) noise = std::max(noise, std::abs(m(i)));
        const float thr = std::max(noise * 1.5f, peak * 0.08f);
        juce::int64 at = s0;
        for (juce::int64 i = lo; i < hi; ++i) if (std::abs(m(i)) > thr) { at = i; break; }
        const juce::int64 back = std::max<juce::int64>(1, at - (juce::int64) (0.001 * sr));
        for (juce::int64 i = at; i > back; --i) if ((m(i) >= 0) != (m(i - 1) >= 0)) { at = i; break; }
        float st = 0; for (int j = k; j < std::min(K, k + tail); ++j) st = std::max(st, e[(size_t) j]);
        out.push_back({ at, st / gmax });
    }
    return out;
}

struct Source
{
    juce::AudioBuffer<float> audio;          // the whole file (up to 5 minutes)
    double sr = 44100;
    juce::File file;                         // {} = only the sound (a kit's loop, a project whose file is gone)
    std::vector<Onset> onsets;
    juce::uint64 id = 0;
    juce::int64 length() const { return audio.getNumSamples(); }
};
inline std::shared_ptr<Source> makeSource(juce::AudioBuffer<float> audio, double sr, const juce::File& f)
{
    static std::atomic<juce::uint64> counter { 0 };
    auto s = std::make_shared<Source>();
    s->audio = std::move(audio); s->sr = sr; s->file = f; s->id = ++counter;
    s->onsets = findOnsets(s->audio, sr);
    return s;
}

// ---------------------------------------------------------------- folders
// Documents/ReKit  (the folder of the old name "DrumSlicer" is moved once, so caches and kits stay)
// where ReKit keeps its data (analyses, kits, exports, the sample index). Chosen once (ReKit 2.4: asked the first
// time), written to <user app data>/ReKit/location.txt. Not chosen yet: Documents/ReKit as before.
inline juce::File locationFile() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ReKit").getChildFile("location.txt"); }
inline bool rootChosen() { return locationFile().existsAsFile(); }
inline juce::File& rootRef()
{
    static juce::File r = []
    {
        const auto t = locationFile().loadFileAsString().trim();
        if (juce::File::isAbsolutePath(t) && (juce::File(t).isDirectory() || juce::File(t).createDirectory())) return juce::File(t);
        const auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto now = docs.getChildFile("ReKit"), old = docs.getChildFile("DrumSlicer");
        if (!now.exists() && old.isDirectory() && !old.moveFileTo(now)) return old;   // could not move: keep using it
        return now;
    }();
    return r;
}
inline juce::File root() { return rootRef(); }
// a new place: what is in the old one is moved there (copied, then the old one removed only when all arrived)
inline bool setRoot(const juce::File& d, bool moveData, juce::String& why)
{
    const auto old = root();
    if (d == old) { locationFile().getParentDirectory().createDirectory(); locationFile().replaceWithText(d.getFullPathName()); return true; }
    if (d.isAChildOf(old)) { why = "a folder inside the current one"; return false; }
    if (!d.isDirectory() && !d.createDirectory()) { why = "cannot create " + d.getFullPathName(); return false; }
    if (moveData && old.isDirectory())
    {
        for (const auto& e : juce::RangedDirectoryIterator(old, false, "*", juce::File::findFilesAndDirectories))
        {
            const auto src = e.getFile(), dst = d.getChildFile(src.getFileName());
            if (dst.exists()) continue;                                   // never overwrite what is there
            const bool ok = src.isDirectory() ? src.copyDirectoryTo(dst) : src.copyFileTo(dst);
            if (!ok) { why = "could not copy " + src.getFileName(); return false; }
        }
        old.deleteRecursively();
    }
    locationFile().getParentDirectory().createDirectory();
    if (!locationFile().replaceWithText(d.getFullPathName())) { why = "cannot write " + locationFile().getFullPathName(); return false; }
    rootRef() = d;
    return true;
}
// cache + exports of one loop: Documents/ReKit/<loop>_<id>/ - the id comes from the full path, so two
// stems both called "Drums.wav" from different projects never share (or overwrite) a folder
inline juce::String pathId(const juce::File& f) { return juce::String::toHexString(f.getFullPathName().toLowerCase().hashCode64()).paddedLeft('0', 16).substring(0, 6); }
inline juce::File cacheDir(const juce::File& loop) { return root().getChildFile(loop.getFileNameWithoutExtension() + "_" + pathId(loop)); }
inline juce::File legacyCacheDir(const juce::File& loop) { return root().getChildFile(loop.getFileNameWithoutExtension()); }
inline juce::File shotFileIn(const juce::File& dir, const juce::File& loop, const juce::String& padName, bool best = false)
{
    return dir.getChildFile(loop.getFileNameWithoutExtension() + "_" + padName.removeCharacters(" ") + (best ? "_BEST" : "") + ".wav");
}
inline juce::File cacheInfoIn(const juce::File& dir, const juce::File& loop) { return dir.getChildFile(loop.getFileNameWithoutExtension() + "_drumslicer.txt"); }
inline juce::File cacheInfo(const juce::File& loop) { return cacheInfoIn(cacheDir(loop), loop); }
// same file = same size + same modification time. The engine version is written too, but an older analysis
// is still used: a project keeps sounding the way it did (RE-ANALYSE uses the new engine)
inline juce::String sourceKey(const juce::File& loop)
{
    return juce::String(loop.getSize()) + "|" + juce::String(loop.getLastModificationTime().toMilliseconds());
}
inline bool keyMatches(const juce::String& line, const juce::File& loop, const juce::String& region = {})
{
    if (!line.startsWith("key=")) return false;
    auto k = line.fromFirstOccurrenceOf("=", false, false);
    if (k.startsWith("drumpeel-")) k = k.fromFirstOccurrenceOf("|", false, false);     // older files carried the version here
    return k == sourceKey(loop) + region;
}

// written to a temporary file first: an existing file is only replaced by a complete new one
inline bool writeWav24(const juce::File& f, const juce::AudioBuffer<float>& a, double sr)
{
    if (a.getNumSamples() <= 0 || sr <= 0) return false;
    if (!f.getParentDirectory().createDirectory()) return false;
    juce::TemporaryFile tmp(f);
    {
        auto fos = std::make_unique<juce::FileOutputStream>(tmp.getFile());
        if (!fos->openedOk()) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(fos.get(), sr, 2, 24, {}, 0));
        if (w == nullptr) return false;
        fos.release();                                  // the writer owns the stream now
        if (!w->writeFromAudioSampleBuffer(a, 0, a.getNumSamples())) return false;
    }
    return tmp.overwriteTargetFileWithTemporary();
}
inline bool writeData(const juce::File& f, const void* d, size_t n)
{
    if (!f.getParentDirectory().createDirectory()) return false;
    juce::TemporaryFile tmp(f);
    return tmp.getFile().replaceWithData(d, n) && tmp.overwriteTargetFileWithTemporary();
}
inline bool writeText(const juce::File& f, const juce::String& t) { const auto u = t.toUTF8(); return writeData(f, u.getAddress(), std::strlen(u.getAddress())); }

// the exact playback of the plugin (whole-sample start, gain = velocity / 100), used for the rebuild file
inline void renderRebuild(const Result& r, juce::AudioBuffer<float>& y, int length = -1)
{
    y.setSize(2, length > 0 ? length : r.loop.getNumSamples()); y.clear();
    for (const auto& h : r.hits)
    {
        const auto& s = r.pads[(size_t) h.pad].audio;
        const int i0 = (int) std::floor(h.time + 0.5);
        const int n = std::min(s.getNumSamples(), y.getNumSamples() - i0);
        if (n > 0 && i0 >= 0) for (int c = 0; c < 2; ++c) y.addFrom(c, i0, s, c, 0, n, (float) h.velocity / 100.f);
    }
}

// analysis lines shared by the cache and the kits
inline juce::StringArray infoLines(const Result& r)
{
    juce::StringArray lines;
    lines.add("sr=" + juce::String(r.sr, 3));
    lines.add("bpm=" + juce::String(r.bpm, 4));
    lines.add("null=" + juce::String(r.nullMatch, 6));
    juce::String hs = "hearing="; for (double h : r.hearing) hs << juce::String(h, 6) << " "; lines.add(hs.trim());
    for (int p = 0; p < kPads; ++p)
    {
        const auto& pd = r.pads[(size_t) p];
        if (pd.valid) lines.add("pad=" + juce::String(p) + "|" + pd.name + "|" + juce::String(pd.note) + "|" + juce::String(pd.hits));
    }
    for (const auto& h : r.hits) lines.add("hit=" + juce::String((juce::int64) h.time) + "|" + juce::String(h.pad) + "|" + juce::String(h.velocity));
    return lines;
}
// fills r from the lines; shot(pad, best) gives the file of a one-shot
inline bool parseInfo(const juce::StringArray& lines, Result& r, const std::function<juce::File(const juce::String&, bool)>& shot)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (auto& l : lines)
    {
        const auto k = l.upToFirstOccurrenceOf("=", false, false), v = l.fromFirstOccurrenceOf("=", false, false);
        juce::StringArray f; f.addTokens(v, "|", "");
        if (k == "bpm") r.bpm = v.getDoubleValue();
        else if (k == "null") r.nullMatch = v.getDoubleValue();
        else if (k == "hearing") { juce::StringArray t; t.addTokens(v, " ", ""); for (int i = 0; i < 5 && i < t.size(); ++i) r.hearing[i] = t[i].getDoubleValue(); }
        else if (k == "pad" && f.size() >= 4)
        {
            const int p = f[0].getIntValue(); if (p < 0 || p >= kPads) return false;
            auto& pd = r.pads[(size_t) p];
            pd.name = f[1]; pd.note = juce::jlimit(0, 127, f[2].getIntValue()); pd.hits = f[3].getIntValue();
            for (int b = 0; b < 2; ++b)
            {
                std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(shot(pd.name, b == 1)));
                if (rd == nullptr || rd->lengthInSamples <= 0) return false;
                auto& buf = b == 1 ? pd.best : pd.audio;
                buf.setSize(2, (int) rd->lengthInSamples);
                if (!rd->read(&buf, 0, (int) rd->lengthInSamples, 0, true, true)) return false;
            }
            pd.valid = true;
        }
        else if (k == "hit" && f.size() >= 3) r.hits.push_back({ (double) f[0].getLargeIntValue(), f[1].getIntValue(), juce::jlimit(1, 127, f[2].getIntValue()) });
    }
    for (auto& h : r.hits) if (h.pad < 0 || h.pad >= kPads || !r.pads[(size_t) h.pad].valid) return false;
    return true;
}

// one-shots, rebuild and analysis next to each other in Documents/ReKit/<loop>_<id>/  (also the cache).
// The analysis text is written last, and only when every sound was written.
inline bool saveCache(const juce::File& loop, const Result& r)
{
    const auto& region = r.region;
    auto dir = cacheDir(loop);
    if (!dir.createDirectory()) return false;
    bool ok = true;
    for (int p = 0; p < kPads; ++p)
    {
        const auto& pd = r.pads[(size_t) p];
        if (!pd.valid) continue;
        ok = writeWav24(shotFileIn(dir, loop, pd.name), pd.audio, r.sr) && ok;
        ok = writeWav24(shotFileIn(dir, loop, pd.name, true), pd.best.getNumSamples() > 0 ? pd.best : pd.audio, r.sr) && ok;
    }
    if (!ok) { cacheInfoIn(dir, loop).deleteFile(); return false; }
    auto lines = infoLines(r);
    lines.insert(0, "key=" + sourceKey(loop) + region);
    lines.insert(1, "engine=" + juce::String(kEngineVersion));
    if (!writeText(cacheInfoIn(dir, loop), lines.joinIntoString("\n"))) return false;
    juce::AudioBuffer<float> y; renderRebuild(r, y);
    writeWav24(dir.getChildFile(loop.getFileNameWithoutExtension() + "_rebuilt.wav"), y, r.sr);
    return true;
}

inline std::shared_ptr<Result> loadCacheFrom(const juce::File& dir, const juce::File& loop, const juce::AudioBuffer<float>& audio, double sr, const juce::String& region = {})
{
    auto info = cacheInfoIn(dir, loop);
    if (!info.existsAsFile()) return nullptr;
    juce::StringArray lines; lines.addLines(info.loadFileAsString());
    if (lines.isEmpty() || !keyMatches(lines[0], loop, region)) return nullptr;
    auto r = std::make_shared<Result>();
    r->loop = audio; r->sr = sr; r->fromCache = true; r->region = region;
    if (!parseInfo(lines, *r, [&](const juce::String& n, bool b) { return shotFileIn(dir, loop, n, b); })) return nullptr;
    r->buildNoteMap();
    return r;
}
inline std::shared_ptr<Result> loadCache(const juce::File& loop, const juce::AudioBuffer<float>& audio, double sr, const juce::String& region = {})
{
    if (auto r = loadCacheFrom(cacheDir(loop), loop, audio, sr, region)) return r;
    return loadCacheFrom(legacyCacheDir(loop), loop, audio, sr, region);     // analysed by an older version
}

// a one-shot of yours for pad "C": up to 4 s, at the kit's sample rate, as loud as the pad's own sound (A) in its
// first 100 ms, so the MIDI velocities keep meaning the same
inline bool loadUserShot(const juce::File& f, const Pad& pad, double kitSr, juce::AudioBuffer<float>& out)
{
    juce::AudioBuffer<float> a; double sr = 0;
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
        if (rd == nullptr || rd->lengthInSamples < 16 || rd->sampleRate <= 0) return false;
        sr = rd->sampleRate;
        const int n = (int) std::min<juce::int64>(rd->lengthInSamples, (juce::int64) (sr * 4.0));
        a.setSize(2, n);
        if (!rd->read(&a, 0, n, 0, true, true)) return false;
        if (rd->numChannels < 2) a.copyFrom(1, 0, a, 0, 0, n);
    }
    if (std::abs(sr - kitSr) > 0.5)
    {
        const double ratio = sr / kitSr;
        const int n2 = (int) std::floor(a.getNumSamples() / ratio);
        juce::AudioBuffer<float> b(2, n2);
        for (int c = 0; c < 2; ++c) { juce::LagrangeInterpolator li; li.process(ratio, a.getReadPointer(c), b.getWritePointer(c), n2); }
        a = std::move(b);
    }
    // as loud as the pad SOUNDS (ear-weighted, a short hit added up over ~50 ms - not its peak: a long hat with the
    // same peak as a short one is much louder), but never moved more than 12 dB: a kick dropped on a quiet hat pad
    // must stay audible
    const float pa = pad.audio.getNumSamples() > 0 ? pad.audio.getMagnitude(0, pad.audio.getNumSamples()) : 0.0f;
    const float pu = a.getMagnitude(0, a.getNumSamples());
    if (pu <= 1.0e-6f) return false;
    {
        float g;
        if (pa > 1.0e-4f) g = juce::Decibels::decibelsToGain(mt::loudnessOf(pad.audio, kitSr) - mt::loudnessOf(a, kitSr));
        else g = 0.7f / pu;
        a.applyGain(juce::jlimit(0.25f, 4.0f, g));
    }
    const float pk = a.getMagnitude(0, a.getNumSamples());
    if (pk > 0.99f) a.applyGain(0.99f / pk);
    out = std::move(a);
    return true;
}

// ---------------------------------------------------------------- kits (presets)
// Documents/ReKit/Kits/<name>/          final one-shots (as heard) + exact / tight MIDI, for any use
// Documents/ReKit/Kits/<name>/_data/    what the plugin needs to reopen the kit: loop, A / B shots, hits, settings
inline juce::File kitsRoot() { return root().getChildFile("Kits"); }
inline juce::File kitData(const juce::File& kit) { return kit.getChildFile("_data"); }
inline juce::File kitLoop(const juce::File& kit)
{
    const auto named = kitData(kit).getChildFile(kit.getFileName() + ".wav");     // kits saved by version 1.0
    return named.existsAsFile() ? named : kitData(kit).getChildFile("loop.wav");
}
inline bool isKit(const juce::File& dir) { return kitData(dir).getChildFile("kit.txt").existsAsFile(); }
// a kit saved in a project but moved since: look for one with the same name in the kits folder
inline juce::File findKit(const juce::File& saved)
{
    if (isKit(saved)) return saved;
    const auto byName = kitsRoot().getChildFile(saved.getFileName());
    return isKit(byName) ? byName : juce::File();
}

inline bool saveKitData(const juce::File& kit, const Result& r)
{
    auto d = kitData(kit);
    if (!d.createDirectory()) return false;
    bool ok = writeWav24(kitLoop(kit), r.loop, r.sr);
    for (int p = 0; p < kPads; ++p)
    {
        const auto& pd = r.pads[(size_t) p];
        if (!pd.valid) continue;
        const auto base = pd.name.removeCharacters(" ");
        ok = writeWav24(d.getChildFile(base + "_A.wav"), pd.audio, r.sr) && ok;
        ok = writeWav24(d.getChildFile(base + "_B.wav"), pd.best.getNumSamples() > 0 ? pd.best : pd.audio, r.sr) && ok;
        if (pd.user.getNumSamples() > 0) ok = writeWav24(d.getChildFile(base + "_C.wav"), pd.user, r.sr) && ok;
        if (pd.user.getNumSamples() > 0 && pd.layered()) ok = writeWav24(d.getChildFile(base + "_CBASE.wav"), pd.userBase, r.sr) && ok;   // yours alone, under the layer
    }
    auto lines = infoLines(r); lines.insert(0, "kit=1");
    for (int p = 0; p < kPads; ++p) if (r.pads[(size_t) p].valid && r.pads[(size_t) p].user.getNumSamples() > 0) lines.add("user=" + juce::String(p) + "|" + r.pads[(size_t) p].userName);
    for (int p = 0; p < kPads; ++p) if (r.pads[(size_t) p].valid && r.pads[(size_t) p].user.getNumSamples() > 0 && r.pads[(size_t) p].layered())
        lines.add("layer=" + juce::String(p) + "|" + r.pads[(size_t) p].layerName + "|" + r.pads[(size_t) p].layerPath);
    return ok && writeText(d.getChildFile("kit.txt"), lines.joinIntoString("\n"));
}

inline std::shared_ptr<Result> loadKitData(const juce::File& kit)
{
    auto d = kitData(kit);
    juce::StringArray lines; lines.addLines(d.getChildFile("kit.txt").loadFileAsString());
    if (lines.isEmpty() || lines[0] != "kit=1") return nullptr;
    auto r = std::make_shared<Result>();
    double sr = 44100;
    if (!loadAudio(kitLoop(kit), r->loop, sr)) return nullptr;
    r->sr = sr; r->fromCache = true;
    if (!parseInfo(lines, *r, [&](const juce::String& n, bool b) { return d.getChildFile(n.removeCharacters(" ") + (b ? "_B.wav" : "_A.wav")); })) return nullptr;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (auto& l : lines)
        if (l.startsWith("user="))
        {
            const int p = l.fromFirstOccurrenceOf("=", false, false).upToFirstOccurrenceOf("|", false, false).getIntValue();
            if (p < 0 || p >= kPads || !r->pads[(size_t) p].valid) continue;
            auto& pd = r->pads[(size_t) p];
            std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(d.getChildFile(pd.name.removeCharacters(" ") + "_C.wav")));
            if (rd == nullptr || rd->lengthInSamples <= 0) continue;
            pd.user.setSize(2, (int) rd->lengthInSamples);
            if (rd->read(&pd.user, 0, (int) rd->lengthInSamples, 0, true, true)) pd.userName = l.fromFirstOccurrenceOf("|", false, false);
            else pd.user.setSize(0, 0);
        }
    for (auto& l : lines)
        if (l.startsWith("layer="))
        {   // a layered C: your own sample alone comes back too (REMOVE LAYER)
            juce::StringArray f; f.addTokens(l.fromFirstOccurrenceOf("=", false, false), "|", "");
            const int p = f[0].getIntValue();
            if (p < 0 || p >= kPads || !r->pads[(size_t) p].valid || r->pads[(size_t) p].user.getNumSamples() == 0) continue;
            auto& pd = r->pads[(size_t) p];
            std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(d.getChildFile(pd.name.removeCharacters(" ") + "_CBASE.wav")));
            if (rd == nullptr || rd->lengthInSamples <= 0) continue;
            pd.userBase.setSize(2, (int) rd->lengthInSamples);
            if (rd->read(&pd.userBase, 0, (int) rd->lengthInSamples, 0, true, true)) { pd.layerName = f[1]; pd.layerPath = f[2]; }
            else pd.userBase.setSize(0, 0);
        }
    r->buildNoteMap();
    return r;
}

// a few seconds of the kit playing its own pattern (its final, shaped sounds) to listen to before loading
inline bool renderKitPreview(const juce::File& kit, juce::AudioBuffer<float>& out, double& sr)
{
    juce::StringArray lines; lines.addLines(kitData(kit).getChildFile("kit.txt").loadFileAsString());
    if (lines.isEmpty() || lines[0] != "kit=1") return false;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::array<juce::AudioBuffer<float>, kPads> shots; std::vector<Hit> hits; double bpm = 120; sr = 44100;
    for (auto& l : lines)
    {
        const auto k = l.upToFirstOccurrenceOf("=", false, false), v = l.fromFirstOccurrenceOf("=", false, false);
        juce::StringArray f; f.addTokens(v, "|", "");
        if (k == "sr") sr = v.getDoubleValue();
        else if (k == "bpm") bpm = v.getDoubleValue();
        else if (k == "pad" && f.size() >= 2)
        {
            const int p = f[0].getIntValue(); if (p < 0 || p >= kPads) continue;
            const auto base = f[1].removeCharacters(" ");
            auto file = kit.getChildFile(kit.getFileName() + "_" + base + ".wav");          // as heard (shaped)
            if (!file.existsAsFile()) file = kitData(kit).getChildFile(base + "_A.wav");
            std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(file));
            if (rd == nullptr) continue;
            shots[(size_t) p].setSize(2, (int) rd->lengthInSamples);
            rd->read(&shots[(size_t) p], 0, (int) rd->lengthInSamples, 0, true, true);
        }
        else if (k == "hit" && f.size() >= 3) hits.push_back({ (double) f[0].getLargeIntValue(), f[1].getIntValue(), f[2].getIntValue() });
    }
    if (hits.empty() || sr <= 0) return false;
    const double t0 = hits.front().time;
    const int len = (int) (sr * juce::jlimit(3.0, 8.0, 2 * 240.0 / std::max(40.0, bpm)));      // two bars
    out.setSize(2, len + (int) sr); out.clear();
    for (const auto& h : hits)
    {
        const int i0 = (int) std::floor(h.time - t0 + 0.5);
        if (i0 >= len) break;
        if (h.pad < 0 || h.pad >= kPads) continue;
        const auto& s = shots[(size_t) h.pad];
        const int n = std::min(s.getNumSamples(), out.getNumSamples() - i0);
        if (n > 0) for (int c = 0; c < 2; ++c) out.addFrom(c, i0, s, c, 0, n, (float) h.velocity / 100.f);
    }
    const int fade = std::min(out.getNumSamples(), (int) (0.3 * sr));
    out.applyGainRamp(out.getNumSamples() - fade, fade, 1.f, 0.f);
    return true;
}

// ---------------------------------------------------------------- the sounds inside the project
// A and B of every pad (FLAC), the hits and the tempo travel with the Cubase project: it opens with the
// same sounds on another computer or after the loop / kit was deleted, and an engine update never changes it.
inline juce::ValueTree embedResult(const Result& r)
{
    juce::ValueTree e("EMBED");
    e.setProperty("sr", r.sr, nullptr); e.setProperty("bpm", r.bpm, nullptr); e.setProperty("len", r.loop.getNumSamples(), nullptr);
    e.setProperty("null", r.nullMatch, nullptr);
    e.setProperty("rs", r.regionStart, nullptr);
    juce::String hs; for (double h : r.hearing) hs << juce::String(h, 6) << " "; e.setProperty("hearing", hs.trim(), nullptr);
    juce::FlacAudioFormat flac;
    auto enc = [&](const juce::AudioBuffer<float>& b) -> juce::String {
        juce::MemoryBlock mb;
        {
            auto* mos = new juce::MemoryOutputStream(mb, false);
            std::unique_ptr<juce::AudioFormatWriter> w(flac.createWriterFor(mos, r.sr, 2, 24, {}, 5));
            if (w == nullptr) { delete mos; return {}; }
            w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
        }
        return mb.toBase64Encoding(); };
    for (int p = 0; p < kPads; ++p)
    {
        const auto& pd = r.pads[(size_t) p];
        if (!pd.valid) continue;
        juce::ValueTree t("PAD");
        t.setProperty("i", p, nullptr); t.setProperty("name", pd.name, nullptr); t.setProperty("note", pd.note, nullptr); t.setProperty("hits", pd.hits, nullptr);
        t.setProperty("a", enc(pd.audio), nullptr);
        t.setProperty("b", pd.best.getNumSamples() > 0 ? enc(pd.best) : juce::String(), nullptr);
        if (pd.user.getNumSamples() > 0) { t.setProperty("c", enc(pd.user), nullptr); t.setProperty("cname", pd.userName, nullptr); t.setProperty("cpath", pd.userPath, nullptr); }
        if (pd.user.getNumSamples() > 0 && pd.layered())
        { t.setProperty("cbase", enc(pd.userBase), nullptr); t.setProperty("lname", pd.layerName, nullptr); t.setProperty("lpath", pd.layerPath, nullptr); }
        e.appendChild(t, nullptr);
    }
    juce::String hits; for (const auto& h : r.hits) hits << (juce::int64) h.time << ":" << h.pad << ":" << h.velocity << " ";
    e.setProperty("hitlist", hits.trim(), nullptr);
    return e;
}

inline std::shared_ptr<Result> unembedResult(const juce::ValueTree& e)
{
    if (!e.isValid() || !e.hasType("EMBED")) return nullptr;
    auto r = std::make_shared<Result>();
    r->sr = (double) e.getProperty("sr", 44100.0); r->bpm = (double) e.getProperty("bpm", 120.0); r->nullMatch = (double) e.getProperty("null", 0.0);
    r->regionStart = (juce::int64) e.getProperty("rs", 0);
    { juce::StringArray t; t.addTokens(e.getProperty("hearing").toString(), " ", ""); for (int i = 0; i < 5 && i < t.size(); ++i) r->hearing[i] = t[i].getDoubleValue(); }
    juce::FlacAudioFormat flac;
    auto dec = [&](const juce::String& b64, juce::AudioBuffer<float>& out) {
        juce::MemoryBlock mb; if (b64.isEmpty() || !mb.fromBase64Encoding(b64)) return false;
        std::unique_ptr<juce::AudioFormatReader> rd(flac.createReaderFor(new juce::MemoryInputStream(mb, true), true));
        if (rd == nullptr || rd->lengthInSamples <= 0) return false;
        out.setSize(2, (int) rd->lengthInSamples);
        return rd->read(&out, 0, (int) rd->lengthInSamples, 0, true, true); };
    for (int i = 0; i < e.getNumChildren(); ++i)
    {
        const auto t = e.getChild(i); const int p = t.getProperty("i", -1);
        if (p < 0 || p >= kPads) return nullptr;
        auto& pd = r->pads[(size_t) p];
        pd.name = t.getProperty("name").toString(); pd.note = juce::jlimit(0, 127, (int) t.getProperty("note", 36)); pd.hits = t.getProperty("hits", 0);
        if (!dec(t.getProperty("a").toString(), pd.audio)) return nullptr;
        if (!dec(t.getProperty("b").toString(), pd.best)) pd.best.makeCopyOf(pd.audio);
        if (t.hasProperty("c") && dec(t.getProperty("c").toString(), pd.user)) { pd.userName = t.getProperty("cname").toString(); pd.userPath = t.getProperty("cpath").toString(); }
        if (pd.user.getNumSamples() > 0 && t.hasProperty("cbase") && dec(t.getProperty("cbase").toString(), pd.userBase))
        { pd.layerName = t.getProperty("lname").toString(); pd.layerPath = t.getProperty("lpath").toString(); }
        else pd.clearLayer();          // (2.3.3: this cleared YOUR SAMPLE itself when it had no layer - lost on reopen)
        pd.valid = true;
    }
    juce::StringArray hl; hl.addTokens(e.getProperty("hitlist").toString(), " ", "");
    for (auto& h : hl) { juce::StringArray f; f.addTokens(h, ":", ""); if (f.size() == 3) r->hits.push_back({ (double) f[0].getLargeIntValue(), f[1].getIntValue(), juce::jlimit(1, 127, f[2].getIntValue()) }); }
    for (auto& h : r->hits) if (h.pad < 0 || h.pad >= kPads || !r->pads[(size_t) h.pad].valid) return nullptr;
    r->fromCache = true;
    r->buildNoteMap();
    // the loop itself is not in the project: the display shows the rebuild until the loop file is found
    int len = (int) e.getProperty("len", 0);
    if (len <= 0) { for (auto& h : r->hits) len = std::max(len, (int) h.time); len += (int) r->sr; }
    renderRebuild(*r, r->loop, len);
    return r;
}

// analysis of a piece of audio (the whole file or the selected part of it); with a file: its cache first.
// region = "" for the whole file, else "|r<start>-<end>" (part of the cache key)
inline std::shared_ptr<Result> analyseBuffer(juce::AudioBuffer<float> audio, double sr, const juce::File& file, const juce::String& region,
                                             float sensitivity, const std::function<void(float, const juce::String&)>& progress,
                                             const std::atomic<bool>* cancel)
{
    if (audio.getNumSamples() < 4096) return nullptr;
    if (file.existsAsFile()) if (auto c = loadCache(file, audio, sr, region)) return c;

    dp::Buf x(audio.getNumSamples());
    for (int c = 0; c < 2; ++c) { const float* p = audio.getReadPointer(c); for (int i = 0; i < x.n; ++i) x.c[c][(size_t) i] = p[i]; }
    const double thr = 0.06 - 0.06 * juce::jlimit(0.f, 1.f, sensitivity);       // SENS 0.5 -> 0.03
    auto out = dp::analyseLong(x, sr, thr, [&](float p, const std::string& s) { if (progress) progress(p, juce::String(s)); }, cancel);
    if (cancel != nullptr && cancel->load()) return nullptr;
    if (out.pads.empty()) return nullptr;

    auto r = std::make_shared<Result>();
    r->loop = std::move(audio); r->sr = sr; r->bpm = out.bpm; r->nullMatch = out.nullMatch;
    for (int i = 0; i < 5; ++i) r->hearing[i] = out.hearing[i];
    for (size_t p = 0; p < out.pads.size() && p < (size_t) kPads; ++p)
    {
        auto& pd = r->pads[p]; const auto& op = out.pads[p];
        pd.name = op.name; pd.note = op.note; pd.hits = op.hits; pd.valid = true;
        auto copy = [](const dp::Buf& src, juce::AudioBuffer<float>& dst) {
            dst.setSize(2, src.n);
            for (int c = 0; c < 2; ++c) { float* d = dst.getWritePointer(c); for (int i = 0; i < src.n; ++i) d[i] = (float) src.c[c][(size_t) i]; } };
        copy(op.audio, pd.audio); copy(op.best, pd.best);
    }
    for (const auto& n : out.notes) if (n.pad < kPads) r->hits.push_back({ n.time, n.pad, n.velocity });
    r->buildNoteMap();
    r->region = region;
    if (file.existsAsFile()) saveCache(file, *r);
    return r;
}

// full analysis of a file (or the cache when the same file was analysed before)
inline std::shared_ptr<Result> analyse(const juce::File& file, float sensitivity,
                                       const std::function<void(float, const juce::String&)>& progress,
                                       const std::atomic<bool>* cancel)
{
    juce::AudioBuffer<float> audio; double sr = 44100;
    if (!loadAudio(file, audio, sr)) return nullptr;
    return analyseBuffer(std::move(audio), sr, file, {}, sensitivity, progress, cancel);
}

// MIDI file bytes (mode 0 / 1 / 2):
// EXACT: the loop as played, at the host tempo (timing exact in seconds: plays at the loop's own speed).
// TIGHT: drum-machine pattern on the loop's grid (strength 1 = fully on the grid), follows the project tempo.
// SYNC:  the loop as played (every hit, its groove), but in beats of the loop: follows the project tempo.
enum MidiMode { kExact = 0, kTight = 1, kSync = 2 };
inline const char* midiSuffix(int mode) { return mode == kTight ? "_tight" : mode == kSync ? "_sync" : "_exact"; }
inline juce::MemoryBlock midiBytes(const Result& r, double bpm, int mode, double strength)
{
    const bool tight = mode == kTight;
    if (mode == kSync && r.bpm > 0) bpm = r.bpm;      // one beat of the loop = one beat of the project
    dp::Output o; o.sr = r.sr;
    for (const auto& p : r.pads) { dp::PadOut po; po.note = p.note; po.name = p.name.toStdString(); o.pads.push_back(po); }
    for (const auto& h : r.hits) o.notes.push_back({ h.time, h.pad, h.velocity });
    o.bpm = r.bpm;
    const auto bytes = dp::makeMidiFile(o, bpm, 480, !tight, 0.0, tight, strength);
    return juce::MemoryBlock(bytes.data(), bytes.size());
}
} // namespace ds
