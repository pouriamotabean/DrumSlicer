#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Extractor.h"
#include <thread>
#include <cstring>
#include <mutex>

// per-pad sound shaping (start / end trim, attack-hold-decay, punch / sustain), rendered off the audio thread
// fades: curve -1..1 (0 = straight line, + = stays louder, - = drops / rises later), gain = t^(2^(-3 curve))
struct ShapeParams { float start = 0, end = 2000, attack = 0, hold = 2000, decay = 2000, punch = 0, sustain = 0,
                     attackCurve = 0, fadeOut = 0, fadeOutCurve = 0; bool best = false;
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
    void startAnalysis(const juce::File& f, bool updateBpm, bool forceNew = false);   // runs on a background thread
    void reanalyse() { const auto f = getLoopFile(); if (f.existsAsFile()) startAnalysis(f, false, true); }
    bool isBusy() const { return busy.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getStatus() const { juce::SpinLock::ScopedLockType l(statusLock); return status; }
    juce::File getLoopFile() const { juce::SpinLock::ScopedLockType l(fileLock); return loopFile; }
    juce::File getKitDir() const { juce::SpinLock::ScopedLockType l(fileLock); return kitDir; }
    double midiBpm() const;                          // host tempo when known, else the BPM knob
    double getHostBpm() const { return hostBpm.load(); }
    juce::File makeMidiFile();                       // .mid built from the analysis (for dragging into the DAW)
    juce::File exportFolder();                       // Documents/ReKit/<loop>_<id>/ (or the kit) with one-shots + .mid
    juce::File shotFile(int pad);                    // the pad as you hear it, written for a drag; {} if it failed
    std::shared_ptr<ds::Result> getResult() const { juce::SpinLock::ScopedLockType l(lock); return result; }
    std::shared_ptr<Shaped> getShaped() const { juce::SpinLock::ScopedLockType l(lock); return shaped; }
    ShapeParams shapeParams(int pad) const;
    juce::File exportFinal();                        // shaped, chosen one-shots + MIDI into <folder>/EXPORT
    int getResultVersion() const { return resultVersion.load(); }

    // kits (presets): Documents/ReKit/Kits/<name>/
    juce::File saveKit(const juce::String& name);    // current sounds + settings + MIDI; returns the kit folder ({} on failure)
    void loadKit(const juce::File& kit, bool applySettings = true);
    void kitRemoved(const juce::File& kit) { if (getKitDir() == kit) setFiles({}, {}, kit.getFileName()); }   // sounds stay, files gone
    // listening to a saved kit (its pattern) before loading it
    bool previewKit(const juce::File& kit);
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
    std::array<std::atomic<int>, 8>   previewPad;
    std::array<std::atomic<float>, 8> padLevel;

private:
    static BusesProperties makeBuses();
    struct Voice { bool on = false; int pad = 0, wait = 0; double pos = 0, step = 1; float gain = 0; juce::uint64 age = 0; };
    void timerCallback() override;                   // rebuilds the shaped shots when a shape parameter changes
    std::array<ShapeParams, ds::kPads> lastShape {};
    int lastShapedVersion = -1;
    std::shared_ptr<Shaped> shaped, audioShaped;
    void startVoice(int pad, float gain, int wait);
    void setStatus(const juce::String& s) { juce::SpinLock::ScopedLockType l(statusLock); status = s; }
    void setFiles(const juce::File& kit, const juce::File& loop, const juce::String& orphanName = {})
    { juce::SpinLock::ScopedLockType l(fileLock); kitDir = kit; loopFile = loop; orphanName_ = orphanName; }
    juce::String orphan() const { juce::SpinLock::ScopedLockType l(fileLock); return orphanName_; }   // sounds from the project, files gone
    // a new result (or none) becomes the current one. The old one is kept alive here and released later on the
    // message thread, so the audio thread never frees big buffers when it lets go of its copy.
    void publish(std::shared_ptr<ds::Result> r, const juce::File& kit, const juce::File& loop, const juce::String& orphanName = {});
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
