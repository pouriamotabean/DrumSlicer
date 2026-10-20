#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Extractor.h"
#include "Library.h"
#include <thread>
#include <cstring>
#include <mutex>

// per-pad sound shaping (start / end trim, attack-hold-decay, punch / sustain), rendered off the audio thread
// fades: curve -1..1 (0 = straight line, + = stays louder, - = drops / rises later), gain = t^(2^(-3 curve))
struct ShapeParams { float start = 0, end = 2000, attack = 0, hold = 2000, decay = 2000, punch = 0, sustain = 0,
                     attackCurve = 0, fadeOut = 0, fadeOutCurve = 0; bool best = false, user = false;
                     bool operator!=(const ShapeParams& o) const { return std::memcmp(this, &o, sizeof(*this)) != 0; } };
struct Shaped
{
    std::array<juce::AudioBuffer<float>, ds::kPads> buf;
    std::array<int, ds::kPads> offset {};            // samples cut at the start: the hit keeps its time
};
juce::AudioBuffer<float> shapeShot(const juce::AudioBuffer<float>& raw, double sr, const ShapeParams& p, int& offset);

class DrumSlicerProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DrumSlicerProcessor();
    ~DrumSlicerProcessor() override;

    void prepareToPlay(double sr, int) override { hostSR = sr; for (auto& v : voices) v.on = false; }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ReKit"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- used by the editor (message thread) ---
    // a file is loaded (shown, playable) but not analysed: you choose the part, then ANALYSE
    void loadSource(const juce::File& f);            // runs on a background thread
    std::shared_ptr<const ds::Source> getSource() const { juce::SpinLock::ScopedLockType l(lock); return source; }
    juce::int64 selStart() const { return selS.load(); }
    juce::int64 selEnd() const { return selE.load(); }
    void setSelection(juce::int64 a, juce::int64 b);
    bool selectionAnalysed() const;                  // the sounds come from exactly this selection
    void analyseSelection(bool force);               // ANALYSE (the selection; cache when it was done before) / RE-ANALYSE (force)
    void reanalyse() { analyseSelection(selectionAnalysed()); }
    // the player of the loaded file: loops the selection
    void playSource(bool on);
    bool isSourcePlaying() const { return srcPlay.load(); }
    double sourcePos() const { return srcPosShown.load(); }   // samples of the source
    void seekSource(juce::int64 pos) { srcSeek = pos; }
    std::atomic<bool> snap { true };                 // selection handles snap to the attacks
    // ▶ next to DRAG MIDI: the MIDI you would drag (EXACT / SYNC / TIGHT), played with the kit as it is now,
    // at the project tempo, looped
    void playMidi(bool on);
    bool isMidiPlaying() const { return midiPlay.load(); }
    bool isBusy() const { return busy.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getStatus() const { juce::SpinLock::ScopedLockType l(statusLock); return status; }
    juce::File getLoopFile() const { juce::SpinLock::ScopedLockType l(fileLock); return loopFile; }
    juce::File getKitDir() const { juce::SpinLock::ScopedLockType l(fileLock); return kitDir; }
    double midiBpm() const;
    int midiMode() const                             // 0 EXACT, 1 TIGHT, 2 SYNC (see ds::midiBytes)
    {
        if (apvts.getRawParameterValue("tight")->load() > 0.5f) return ds::kTight;
        return apvts.getRawParameterValue("sync")->load() > 0.5f ? ds::kSync : ds::kExact;
    }
    void setMidiMode(int m)
    {
        if (auto* p = apvts.getParameter("tight")) p->setValueNotifyingHost(m == ds::kTight ? 1.f : 0.f);
        if (auto* p = apvts.getParameter("sync")) p->setValueNotifyingHost(m == ds::kSync ? 1.f : 0.f);
    }                          // host tempo when known, else the BPM knob
    double getHostBpm() const { return hostBpm.load(); }
    juce::File makeMidiFile();                       // .mid built from the analysis (for dragging into the DAW)
    juce::File exportFolder();                       // Documents/ReKit/<loop>_<id>/ (or the kit) with one-shots + .mid
    juce::File shotFile(int pad);                    // the pad as you hear it, written for a drag; {} if it failed
    std::shared_ptr<ds::Result> getResult() const { juce::SpinLock::ScopedLockType l(lock); return result; }
    std::shared_ptr<Shaped> getShaped() const { juce::SpinLock::ScopedLockType l(lock); return shaped; }
    ShapeParams shapeParams(int pad) const;
    juce::File exportFinal();                        // shaped, chosen one-shots + MIDI into <folder>/EXPORT
    int getResultVersion() const { return resultVersion.load(); }
    // listen to one pad (click): the sound is rebuilt first, so you hear the new one, and it cuts the last click
    void audition(int pad) { if (pad < 0 || pad >= ds::kPads) return; reshape(); previewPad[(size_t) pad] = 1; }

    // kits (presets): Documents/ReKit/Kits/<name>/
    juce::File saveKit(const juce::String& name);    // current sounds + settings + MIDI; returns the kit folder ({} on failure)
    void loadKit(const juce::File& kit, bool applySettings = true);
    // fix what the analysis decided: the instrument of a pad (its name and MIDI note), or merge two pads
    static const std::vector<juce::String>& padTypes();
    // "C": a sample of yours on a pad (dropped on it or chosen); the MIDI and A / B stay as they are
    bool setPadSample(int pad, const juce::File& f);
    void clearPadSample(int pad);
    void setPadSource(int pad, int src);             // 0 = A, 1 = B, 2 = C
    int padSource(int pad) const;
    void setPadType(int pad, const juce::String& type);
    bool addPad(int pad, const juce::String& type, const juce::File& f);   // an empty pad: a new sound of yours ("" = from its file name)
    static juce::String guessType(const juce::File& f);
    void mergePad(int from, int into);
    void kitRemoved(const juce::File& kit) { if (getKitDir() == kit) setFiles({}, {}, kit.getFileName()); }   // sounds stay, files gone
    // listening to a saved kit (its pattern) before loading it
    bool previewKit(const juce::File& kit);
    bool previewSample(const juce::File& f, int pad);   // a one-shot from your folders, level-matched to the pad
    void stopPreview() { previewStop = true; }
    bool isPreviewing() const { return previewPlaying.load(); }
    juce::File previewingKit() const { juce::SpinLock::ScopedLockType l(fileLock); return previewDir; }

    juce::String displayName() const
    {
        const auto k = getKitDir(); const auto o = orphan();
        return k != juce::File() ? "KIT  " + k.getFileName() : o.isNotEmpty() ? o + "  (FROM PROJECT)" : getLoopFile().getFileName();
    }
    juce::String baseName() const
    {
        const auto k = getKitDir(); const auto o = orphan();
        return k != juce::File() ? k.getFileName() : o.isNotEmpty() ? o : getLoopFile().getFileNameWithoutExtension();
    }

    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<ds::Library> library;     // your one-shot archive (one for all ReKit windows)
    std::array<std::atomic<int>, 8>   previewPad;
    std::array<std::atomic<float>, 8> padLevel;

private:
    static BusesProperties makeBuses();
    // a voice keeps the sound it started with: a sound rebuilt while it rings (A/B/C, a shape change) never jumps mid-hit.
    // It is never the last owner (the retired list holds every replaced Shaped), so it never frees on the audio thread.
    struct Voice { bool on = false; int pad = 0, wait = 0; double pos = 0, step = 1; float gain = 0; juce::uint64 age = 0;
                   std::shared_ptr<Shaped> sh; bool audition = false; int fade = -1, fadeLen = 1; };
    void timerCallback() override;                   // rebuilds the shaped shots when a shape parameter changes
    std::array<ShapeParams, ds::kPads> lastShape {};
    int lastShapedVersion = -1;
    std::shared_ptr<Shaped> shaped, audioShaped;
    void startVoice(int pad, float gain, int wait, bool audition = false);
    void reshape();                                  // rebuilds the shaped sounds now if anything changed
    void setStatus(const juce::String& s) { juce::SpinLock::ScopedLockType l(statusLock); status = s; }
    void setFiles(const juce::File& kit, const juce::File& loop, const juce::String& orphanName = {})
    { juce::SpinLock::ScopedLockType l(fileLock); kitDir = kit; loopFile = loop; orphanName_ = orphanName; }
    juce::String orphan() const { juce::SpinLock::ScopedLockType l(fileLock); return orphanName_; }   // sounds from the project, files gone
    // a new result (or none) becomes the current one. The old one is kept alive here and released later on the
    // message thread, so the audio thread never frees big buffers when it lets go of its copy.
    void publish(std::shared_ptr<ds::Result> r, const juce::File& kit, const juce::File& loop, const juce::String& orphanName = {});
    void setSource(std::shared_ptr<ds::Source> s, juce::int64 a, juce::int64 b);
    struct SeqEv { double tick; int note; int vel; };
    struct Seq { std::vector<SeqEv> ev; double loopTicks = 0; };
    std::shared_ptr<const Seq> seq, audioSeq;
    juce::String seqKey;
    void rebuildSeq();                               // message thread: when the result / mode / tempo changed
    std::atomic<bool> midiPlay { false }, midiRestart { false };
    double seqPos = 0;                               // ticks (audio thread)
    std::shared_ptr<const ds::Source> source, audioSource;
    std::atomic<juce::int64> selS { 0 }, selE { 0 }, srcSeek { -1 };
    std::atomic<bool> srcPlay { false };
    std::atomic<double> srcPosShown { 0 };
    double srcPos = 0;
    template <typename T> void retire(std::shared_ptr<T> p) { if (p) { std::lock_guard<std::mutex> g(retiredLock); retired.push_back(std::move(p)); } }
    void stopWorker();
    void restoreAsync(juce::ValueTree embedded, juce::File kit, juce::File loop);

    std::array<Voice, 64> voices;
    juce::uint64 voiceAge = 0;
    std::array<int, 128> fineVal {};                 // sub-tick timing from Poly Pressure (see DrumPeel.h)
    std::array<juce::int64, 128> fineAt {};
    juce::int64 clock = 0;
    std::atomic<double> hostBpm { 0.0 };
    // parameters read by the audio thread, looked up once
    std::atomic<float>* pMaster = nullptr; std::atomic<float>* pBpm = nullptr;
    std::array<std::atomic<float>*, ds::kPads> pVol {}, pPan {}, pPitch {};

    std::shared_ptr<ds::Result> result, audioRes;
    std::shared_ptr<juce::AudioBuffer<float>> preview, audioPreview;
    double previewSR = 44100, audioPreviewSR = 44100, previewPos = 0;
    std::shared_ptr<juce::AudioBuffer<float>> fadePrev;   // the preview that was cut, fading out
    double fadePos = 0, fadeSR = 44100; int fadeLeft = 0, fadeLen = 1;
    void startPreviewFade() { fadePrev = audioPreview; fadePos = previewPos; fadeSR = audioPreviewSR; fadeLen = std::max(1, (int) (0.005 * hostSR)); fadeLeft = fadeLen; }
    std::atomic<bool> previewStop { false }, previewPlaying { false };
    juce::File previewDir;
    mutable juce::SpinLock lock, statusLock, fileLock;
    std::mutex retiredLock;
    std::vector<std::shared_ptr<const void>> retired;
    std::atomic<int> resultVersion { 0 }, jobToken { 0 };
    juce::String status;
    juce::File loopFile, kitDir, lastTempMidi;
    juce::String orphanName_;
    double hostSR = 44100.0;
    // the sounds embedded in the project, encoded once per result
    std::mutex embedLock; juce::ValueTree embedCache; int embedVersion = -1;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    std::thread worker;
    std::atomic<bool> busy { false }, cancel { false };
    std::atomic<float> progress { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerProcessor)
};
