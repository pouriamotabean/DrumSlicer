#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Extractor.h"
#include <thread>
#include <cstring>

// per-pad sound shaping (start / end trim, attack-hold-decay, punch / sustain), rendered off the audio thread
struct ShapeParams { float start = 0, end = 2000, attack = 0, hold = 2000, decay = 2000, punch = 0, sustain = 0; bool best = false;
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
    const juce::String getName() const override { return "Drum Slicer"; }
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

    // --- used by the editor ---
    void startAnalysis(const juce::File& f, bool updateBpm, bool forceNew = false);   // runs on a background thread
    void reanalyse() { if (loopFile.existsAsFile()) startAnalysis(loopFile, false, true); }
    bool isBusy() const { return busy.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getStatus() const { juce::SpinLock::ScopedLockType l(statusLock); return status; }
    juce::File getLoopFile() const { return loopFile; }
    double midiBpm() const;                          // host tempo when known, else the BPM knob
    double getHostBpm() const { return hostBpm.load(); }
    juce::File makeMidiFile();                       // .mid built from the analysis (for dragging into the DAW)
    juce::File exportFolder();                       // Documents/DrumSlicer/<loop>/ with one-shots + rebuild + .mid
    juce::File shotFile(int pad);
    std::shared_ptr<ds::Result> getResult() const { juce::SpinLock::ScopedLockType l(lock); return result; }
    std::shared_ptr<Shaped> getShaped() const { juce::SpinLock::ScopedLockType l(lock); return shaped; }
    ShapeParams shapeParams(int pad) const;
    juce::File exportFinal();                        // shaped, chosen one-shots + MIDI into <loop>/EXPORT
    int getResultVersion() const { return resultVersion.load(); }

    // kits (presets): Documents/DrumSlicer/Kits/<name>/
    juce::File saveKit(const juce::String& name);    // current sounds + settings + MIDI; returns the kit folder
    void loadKit(const juce::File& kit, bool applySettings = true);
    juce::File getKitDir() const { return kitDir; }
    juce::String displayName() const { return kitDir != juce::File() ? "KIT  " + kitDir.getFileName() : loopFile.getFileName(); }
    juce::String baseName() const { return kitDir != juce::File() ? kitDir.getFileName() : loopFile.getFileNameWithoutExtension(); }

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

    std::array<Voice, 64> voices;
    juce::uint64 voiceAge = 0;
    std::array<int, 128> fineVal {};                 // sub-tick timing from Poly Pressure (see DrumPeel.h)
    std::array<juce::int64, 128> fineAt {};
    juce::int64 clock = 0;
    std::atomic<double> hostBpm { 0.0 };

    std::shared_ptr<ds::Result> result, audioRes;
    mutable juce::SpinLock lock, statusLock;
    std::atomic<int> resultVersion { 0 };
    juce::String status;
    juce::File loopFile, kitDir;
    double hostSR = 44100.0;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    std::thread worker;
    std::atomic<bool> busy { false }, cancel { false };
    std::atomic<float> progress { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerProcessor)
};
