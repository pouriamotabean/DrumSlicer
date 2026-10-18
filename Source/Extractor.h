#pragma once
// JUCE glue around the DrumPeel engine: audio loading, result for the plugin, cache on disk, MIDI bytes.
#include <juce_audio_formats/juce_audio_formats.h>
#include "DrumPeel.h"
#include <array>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

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
    bool valid = false;
    const juce::AudioBuffer<float>& get(bool useBest) const { return useBest && best.getNumSamples() > 0 ? best : audio; }
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
        for (int n : { 37, 38, 39, 40 }) fallback(n, { "SNARE", "PERC" });
        for (int n : { 42, 44 }) fallback(n, { "C-HAT", "HAT", "O-HAT" });
        for (int n : { 46 }) fallback(n, { "O-HAT", "C-HAT", "HAT" });
        for (int n : { 41, 43, 45, 47, 48, 50 }) fallback(n, { "PERC", "SNARE", "KICK" });
        for (int n : { 49, 51, 52, 55, 57 }) fallback(n, { "CRASH", "RIDE", "O-HAT", "C-HAT" });
    }
    double meanHearing() const { double s = 0; for (double h : hearing) s += h; return s / 5.0; }
    double minHearing() const { double m = 1; for (double h : hearing) m = std::min(m, h); return m; }
};

inline bool loadAudio(const juce::File& file, juce::AudioBuffer<float>& out, double& sr, double maxSeconds = 60.0)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(file));
    if (rd == nullptr || rd->lengthInSamples < 4096) return false;
    sr = rd->sampleRate;
    const int n = (int) std::min<juce::int64>(rd->lengthInSamples, (juce::int64) (sr * maxSeconds));
    out.setSize(2, n);
    rd->read(&out, 0, n, 0, true, true);
    if (rd->numChannels < 2) out.copyFrom(1, 0, out, 0, 0, n);
    return true;
}

inline juce::File cacheDir(const juce::File& loop)
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("DrumSlicer").getChildFile(loop.getFileNameWithoutExtension());
}
inline juce::File shotFile(const juce::File& loop, const juce::String& padName, bool best = false)
{
    return cacheDir(loop).getChildFile(loop.getFileNameWithoutExtension() + "_" + padName.removeCharacters(" ") + (best ? "_BEST" : "") + ".wav");
}
inline juce::File cacheInfo(const juce::File& loop)
{
    return cacheDir(loop).getChildFile(loop.getFileNameWithoutExtension() + "_drumslicer.txt");
}
inline juce::String sourceKey(const juce::File& loop)
{
    return juce::String(kEngineVersion) + "|" + juce::String(loop.getSize()) + "|" + juce::String(loop.getLastModificationTime().toMilliseconds());
}

inline bool writeWav24(const juce::File& f, const juce::AudioBuffer<float>& a, double sr)
{
    f.deleteFile();
    auto fos = std::make_unique<juce::FileOutputStream>(f);
    if (!fos->openedOk()) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(fos.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return false;
    fos.release();                                      // the writer owns the stream now
    return w->writeFromAudioSampleBuffer(a, 0, a.getNumSamples());
}

// the exact playback of the plugin (whole-sample start, gain = velocity / 100), used for the rebuild file
inline void renderRebuild(const Result& r, juce::AudioBuffer<float>& y)
{
    y.setSize(2, r.loop.getNumSamples()); y.clear();
    for (const auto& h : r.hits)
    {
        const auto& s = r.pads[(size_t) h.pad].audio;
        const int i0 = (int) std::floor(h.time + 0.5);
        const int n = std::min(s.getNumSamples(), y.getNumSamples() - i0);
        if (n > 0 && i0 >= 0) for (int c = 0; c < 2; ++c) y.addFrom(c, i0, s, c, 0, n, (float) h.velocity / 100.f);
    }
}

// one-shots, rebuild and analysis next to each other in Documents/DrumSlicer/<loop>/  (also the cache)
inline void saveCache(const juce::File& loop, const Result& r)
{
    auto dir = cacheDir(loop); dir.createDirectory();
    juce::StringArray lines;
    lines.add("key=" + sourceKey(loop));
    lines.add("sr=" + juce::String(r.sr, 3));
    lines.add("bpm=" + juce::String(r.bpm, 4));
    lines.add("null=" + juce::String(r.nullMatch, 6));
    juce::String hs = "hearing="; for (double h : r.hearing) hs << juce::String(h, 6) << " "; lines.add(hs.trim());
    for (int p = 0; p < kPads; ++p)
    {
        const auto& pd = r.pads[(size_t) p];
        if (!pd.valid) continue;
        writeWav24(shotFile(loop, pd.name), pd.audio, r.sr);
        writeWav24(shotFile(loop, pd.name, true), pd.best, r.sr);
        lines.add("pad=" + juce::String(p) + "|" + pd.name + "|" + juce::String(pd.note) + "|" + juce::String(pd.hits));
    }
    for (const auto& h : r.hits) lines.add("hit=" + juce::String((juce::int64) h.time) + "|" + juce::String(h.pad) + "|" + juce::String(h.velocity));
    cacheInfo(loop).replaceWithText(lines.joinIntoString("\n"));
    juce::AudioBuffer<float> y; renderRebuild(r, y);
    writeWav24(dir.getChildFile(loop.getFileNameWithoutExtension() + "_rebuilt.wav"), y, r.sr);
}

inline std::shared_ptr<Result> loadCache(const juce::File& loop, const juce::AudioBuffer<float>& audio, double sr)
{
    auto info = cacheInfo(loop);
    if (!info.existsAsFile()) return nullptr;
    juce::StringArray lines; lines.addLines(info.loadFileAsString());
    if (lines.isEmpty() || lines[0] != "key=" + sourceKey(loop)) return nullptr;
    auto r = std::make_shared<Result>();
    r->loop = audio; r->sr = sr; r->fromCache = true;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (auto& l : lines)
    {
        const auto k = l.upToFirstOccurrenceOf("=", false, false), v = l.fromFirstOccurrenceOf("=", false, false);
        juce::StringArray f; f.addTokens(v, "|", "");
        if (k == "bpm") r->bpm = v.getDoubleValue();
        else if (k == "null") r->nullMatch = v.getDoubleValue();
        else if (k == "hearing") { juce::StringArray t; t.addTokens(v, " ", ""); for (int i = 0; i < 5 && i < t.size(); ++i) r->hearing[i] = t[i].getDoubleValue(); }
        else if (k == "pad" && f.size() >= 4)
        {
            const int p = f[0].getIntValue(); if (p < 0 || p >= kPads) return nullptr;
            auto& pd = r->pads[(size_t) p];
            pd.name = f[1]; pd.note = f[2].getIntValue(); pd.hits = f[3].getIntValue();
            for (int b = 0; b < 2; ++b)
            {
                std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(shotFile(loop, pd.name, b == 1)));
                if (rd == nullptr) return nullptr;
                auto& buf = b == 1 ? pd.best : pd.audio;
                buf.setSize(2, (int) rd->lengthInSamples);
                rd->read(&buf, 0, (int) rd->lengthInSamples, 0, true, true);
            }
            pd.valid = true;
        }
        else if (k == "hit" && f.size() >= 3) r->hits.push_back({ (double) f[0].getLargeIntValue(), f[1].getIntValue(), f[2].getIntValue() });
    }
    for (auto& h : r->hits) if (h.pad < 0 || h.pad >= kPads || !r->pads[(size_t) h.pad].valid) return nullptr;
    r->buildNoteMap();
    return r;
}

// full analysis (or the cache when the same file was analysed before)
inline std::shared_ptr<Result> analyse(const juce::File& file, float sensitivity,
                                       const std::function<void(float, const juce::String&)>& progress,
                                       const std::atomic<bool>* cancel)
{
    juce::AudioBuffer<float> audio; double sr = 44100;
    if (!loadAudio(file, audio, sr)) return nullptr;
    if (auto c = loadCache(file, audio, sr)) return c;

    dp::Buf x(audio.getNumSamples());
    for (int c = 0; c < 2; ++c) { const float* p = audio.getReadPointer(c); for (int i = 0; i < x.n; ++i) x.c[c][(size_t) i] = p[i]; }
    dp::Peeler peel(x, sr, kPads, 9);
    peel.onsetThreshold = 0.06 - 0.06 * juce::jlimit(0.f, 1.f, sensitivity);   // SENS 0.5 -> 0.03
    auto out = peel.run([&](float p, const std::string& s) { if (progress) progress(p, juce::String(s)); }, cancel);
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
    saveCache(file, *r);
    return r;
}

// MIDI file bytes. EXACT: the loop as played, at the host tempo (timing exact in seconds).
// TIGHT: drum-machine pattern on the loop's grid (strength 1 = fully on the grid), at the loop's tempo.
inline juce::MemoryBlock midiBytes(const Result& r, double bpm, bool tight, double strength)
{
    dp::Output o; o.sr = r.sr;
    for (const auto& p : r.pads) { dp::PadOut po; po.note = p.note; po.name = p.name.toStdString(); o.pads.push_back(po); }
    for (const auto& h : r.hits) o.notes.push_back({ h.time, h.pad, h.velocity });
    o.bpm = r.bpm;
    const auto bytes = dp::makeMidiFile(o, bpm, 480, !tight, 0.0, tight, strength);
    return juce::MemoryBlock(bytes.data(), bytes.size());
}
} // namespace ds
