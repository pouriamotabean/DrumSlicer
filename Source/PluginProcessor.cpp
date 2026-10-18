#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties DrumSlicerProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < ds::kPads; ++i)
        b = b.withOutput("PAD " + juce::String(i + 1), juce::AudioChannelSet::stereo(), false);
    return b;
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using F = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<F>(juce::ParameterID{ "master", 1 }, "Master", NR(0.f, 1.5f, 0.001f), 1.f));
    p.push_back(std::make_unique<F>(juce::ParameterID{ "sens", 1 },   "Sensitivity", NR(0.f, 1.f, 0.001f), 0.5f));
    p.push_back(std::make_unique<F>(juce::ParameterID{ "bpm", 1 },    "BPM", NR(40.f, 240.f, 0.01f), 120.f));
    p.push_back(std::make_unique<F>(juce::ParameterID{ "quant", 3 },  "Grid (tight MIDI)", NR(0.f, 1.f, 0.001f), 1.f));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "tight", 1 }, "Tight MIDI", false));
    for (int i = 0; i < ds::kPads; ++i)
    {
        const juce::String n("Pad " + juce::String(i + 1));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "vol" + juce::String(i), 1 }, n + " Vol",   NR(0.f, 1.5f, 0.001f), 1.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "pan" + juce::String(i), 1 }, n + " Pan",   NR(-1.f, 1.f, 0.001f), 0.f));
        const juce::String s(i);
        p.push_back(std::make_unique<F>(juce::ParameterID{ "st" + s, 1 },    n + " Start",   NR(0.f, 300.f, 0.1f, 0.5f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "en" + s, 1 },    n + " End",     NR(5.f, 2000.f, 0.1f, 0.5f), 2000.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "atk" + s, 1 },   n + " Attack",  NR(0.f, 100.f, 0.1f, 0.5f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "hold" + s, 1 },  n + " Hold",    NR(0.f, 2000.f, 0.1f, 0.5f), 2000.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "dcy" + s, 1 },   n + " Decay",   NR(5.f, 2000.f, 0.1f, 0.5f), 2000.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "punch" + s, 1 }, n + " Punch",   NR(-12.f, 12.f, 0.1f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "sus" + s, 1 },   n + " Sustain", NR(-12.f, 12.f, 0.1f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "pitch" + s, 1 }, n + " Pitch",   NR(-12.f, 12.f, 0.01f), 0.f));
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "best" + juce::String(i), 1 }, n + " Best hit", false));
    }
    return { p.begin(), p.end() };
}

DrumSlicerProcessor::DrumSlicerProcessor()
    : AudioProcessor(makeBuses()), apvts(*this, nullptr, "STATE", makeLayout())
{
    for (auto& a : previewPad) a = 0;
    for (auto& a : padLevel) a = 0.f;
    fineAt.fill(std::numeric_limits<juce::int64>::min() / 2);
    startTimerHz(20);
}

ShapeParams DrumSlicerProcessor::shapeParams(int i) const
{
    auto v = [&](const char* id) { return apvts.getRawParameterValue(juce::String(id) + juce::String(i))->load(); };
    ShapeParams p;
    p.start = v("st"); p.end = v("en"); p.attack = v("atk"); p.hold = v("hold"); p.decay = v("dcy");
    p.punch = v("punch"); p.sustain = v("sus"); p.best = v("best") > 0.5f;
    return p;
}

// trim -> transient shaper (punch on the attack, sustain on the rest) -> attack / hold / decay envelope
juce::AudioBuffer<float> shapeShot(const juce::AudioBuffer<float>& raw, double sr, const ShapeParams& p, int& offset)
{
    const int len = raw.getNumSamples();
    const int s0 = juce::jlimit(0, std::max(0, len - 1), (int) (p.start * 0.001 * sr));
    const int e = juce::jlimit(s0 + 1, len, (int) (p.end * 0.001 * sr));
    const int n = std::max(1, e - s0);
    offset = s0;
    juce::AudioBuffer<float> o(2, n);
    for (int c = 0; c < 2; ++c) o.copyFrom(c, 0, raw, std::min(c, raw.getNumChannels() - 1), s0, n);
    const bool shapeT = std::abs(p.punch) > 0.05f || std::abs(p.sustain) > 0.05f;
    auto coef = [&](double ms) { return (float) std::exp(-1.0 / (ms * 0.001 * sr)); };
    const float fa = coef(0.5), fr = coef(30), sa = coef(15), sr2 = coef(150), gsm = coef(2);
    float fast = 0, slow = 0, gs = 1;
    const double atk = p.attack * 0.001 * sr, hold = p.hold * 0.001 * sr, dec = p.decay * 0.001 * sr;
    auto* L = o.getWritePointer(0); auto* R = o.getWritePointer(1);
    for (int i = 0; i < n; ++i)
    {
        float g = 1.f;
        if (shapeT)
        {
            const float a = std::max(std::abs(L[i]), std::abs(R[i]));
            fast = a > fast ? fa * fast + (1 - fa) * a : fr * fast + (1 - fr) * a;
            slow = a > slow ? sa * slow + (1 - sa) * a : sr2 * slow + (1 - sr2) * a;
            const float w = juce::jlimit(0.f, 1.f, (fast - slow) / (fast + 1e-9f) * 2.f);
            const float target = juce::Decibels::decibelsToGain(p.punch * w + p.sustain * (1.f - w));
            gs = gsm * gs + (1 - gsm) * target; g = gs;
        }
        double env = 1.0;
        if (i < atk) env = i / atk;
        else if (i > atk + hold) env = std::pow(10.0, -3.0 * (i - atk - hold) / std::max(1.0, dec));   // -60 dB after DECAY
        g *= (float) env;
        L[i] *= g; R[i] *= g;
    }
    const int fin = s0 > 0 ? std::min(n, (int) (0.001 * sr)) : 0, fout = e < len ? std::min(n, (int) (0.003 * sr)) : 0;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = o.getWritePointer(c);
        for (int i = 0; i < fin; ++i) d[i] *= (float) i / fin;
        for (int i = 0; i < fout; ++i) d[n - 1 - i] *= (float) i / fout;
    }
    return o;
}

void DrumSlicerProcessor::timerCallback()
{
    auto r = getResult();
    if (!r) return;
    std::array<ShapeParams, ds::kPads> now;
    bool changed = resultVersion.load() != lastShapedVersion;
    for (int i = 0; i < ds::kPads; ++i) { now[(size_t) i] = shapeParams(i); if (now[(size_t) i] != lastShape[(size_t) i]) changed = true; }
    if (!changed) return;
    auto sh = std::make_shared<Shaped>();
    for (int i = 0; i < ds::kPads; ++i)
        if (r->pads[(size_t) i].valid)
            sh->buf[(size_t) i] = shapeShot(r->pads[(size_t) i].get(now[(size_t) i].best), r->sr, now[(size_t) i], sh->offset[(size_t) i]);
    { juce::SpinLock::ScopedLockType l(lock); shaped = sh; }
    lastShape = now; lastShapedVersion = resultVersion.load();
}

DrumSlicerProcessor::~DrumSlicerProcessor()
{
    *alive = false;
    cancel = true;
    if (worker.joinable()) worker.join();
}

bool DrumSlicerProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (!l.inputBuses.isEmpty()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
    {
        auto s = l.getChannelSet(false, i);
        if (!s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

// ---------------------------------------------------------------- playback
// Every hit rings out on its own voice (additive, like the loop itself), starts on a whole sample and
// plays the one-shot unchanged at velocity 100. That is exactly the model the analysis fitted.
void DrumSlicerProcessor::startVoice(int pad, float g, int wait)
{
    if (pad < 0 || pad >= ds::kPads || !audioRes || !audioShaped || audioShaped->buf[(size_t) pad].getNumSamples() == 0) return;
    const double ratio = audioRes->sr / hostSR;
    const double step = ratio * std::pow(2.0, *apvts.getRawParameterValue("pitch" + juce::String(pad)) / 12.0);
    wait += (int) std::lround(audioShaped->offset[(size_t) pad] / step);    // a trimmed start keeps the hit on time
    Voice* v = nullptr;
    for (auto& vv : voices) if (!vv.on) { v = &vv; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& vv : voices) if (vv.age < v->age) v = &vv; }   // steal the oldest
    *v = { true, pad, std::max(0, wait), 0.0, step, g, ++voiceAge };
    padLevel[(size_t) pad] = g;
}

double DrumSlicerProcessor::midiBpm() const
{
    const double h = hostBpm.load();
    return h > 0 ? h : (double) *apvts.getRawParameterValue("bpm");
}

void DrumSlicerProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buf.clear();
    {
        juce::SpinLock::ScopedTryLockType l(lock);
        if (l.isLocked())
        {
            if (audioRes != result) { audioRes = result; for (auto& v : voices) v.on = false; }
            audioShaped = shaped;
        }
    }
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) hostBpm = *b;
    const int numSamples = buf.getNumSamples();
    if (!audioRes || !audioShaped) { clock += numSamples; return; }
    const auto& res = *audioRes;

    const float master = *apvts.getRawParameterValue("master");
    float vol[ds::kPads], pl[ds::kPads], pr[ds::kPads];
    for (int i = 0; i < ds::kPads; ++i)
    {
        vol[i] = *apvts.getRawParameterValue("vol" + juce::String(i));
        const float pan = *apvts.getRawParameterValue("pan" + juce::String(i));
        const float a = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        pl[i] = 1.41421356f * std::cos(a);           // pan 0 -> 1.0 / 1.0 (unity)
        pr[i] = 1.41421356f * std::sin(a);
        if (previewPad[(size_t) i].exchange(0)) startVoice(i, 1.f, 0);
    }
    const double spt = 60.0 / midiBpm() / 480.0 * hostSR;   // samples per tick of the exported MIDI

    auto render = [&](int s0, int s1)
    {
        for (auto& v : voices)
        {
            if (!v.on || s1 <= s0) continue;
            const auto& sh = audioShaped->buf[(size_t) v.pad];
            const int len = sh.getNumSamples();
            auto* bus = getBus(false, v.pad + 1);
            auto out = (bus != nullptr && bus->isEnabled()) ? getBusBuffer(buf, false, v.pad + 1) : getBusBuffer(buf, false, 0);
            if (out.getNumChannels() < 2) continue;
            auto* oL = out.getWritePointer(0); auto* oR = out.getWritePointer(1);
            auto* sL = sh.getReadPointer(0);   auto* sR = sh.getReadPointer(1);
            const float g = v.gain * vol[v.pad] * master;
            int i = s0;
            if (v.wait > 0) { const int w = std::min(v.wait, s1 - s0); v.wait -= w; i += w; }
            for (; i < s1; ++i)
            {
                float l, r;
                if (v.step == 1.0)
                {
                    const int i0 = (int) v.pos;
                    if (i0 >= len) { v.on = false; break; }
                    l = sL[i0]; r = sR[i0];
                }
                else
                {
                    const int i0 = (int) v.pos;
                    if (i0 + 1 >= len) { v.on = false; break; }
                    const float fr = (float) (v.pos - i0);
                    l = sL[i0] + (sL[i0 + 1] - sL[i0]) * fr;
                    r = sR[i0] + (sR[i0 + 1] - sR[i0]) * fr;
                }
                oL[i] += l * g * pl[v.pad];
                oR[i] += r * g * pr[v.pad];
                v.pos += v.step;
            }
        }
    };

    int cur = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit(0, numSamples, meta.samplePosition);
        render(cur, pos);
        cur = pos;
        const int note = msg.getNoteNumber();
        if (msg.isAftertouch())                       // Poly Pressure one tick before a note = its sub-tick offset
        {
            fineVal[(size_t) note] = msg.getAfterTouchValue();
            fineAt[(size_t) note] = clock + pos;
        }
        else if (msg.isNoteOn())
        {
            int wait = 0;
            const double age = (double) (clock + pos - fineAt[(size_t) note]);
            if (age > 0.5 * spt && age < 1.5 * spt) wait = (int) std::floor(fineVal[(size_t) note] / 127.0 * spt + 0.5);
            fineAt[(size_t) note] = std::numeric_limits<juce::int64>::min() / 2;
            startVoice(res.noteToPad[(size_t) note], (float) msg.getVelocity() / 100.f, wait);
        }
    }
    render(cur, numSamples);
    clock += numSamples;
}

// ---------------------------------------------------------------- analysis (background thread)
void DrumSlicerProcessor::startAnalysis(const juce::File& f, bool updateBpm, bool forceNew)
{
    cancel = true;
    if (worker.joinable()) worker.join();
    cancel = false;
    if (forceNew) ds::cacheInfo(f).deleteFile();
    loopFile = f;
    busy = true; progress = 0.f; setStatus("ANALYSING...");
    const float sens = *apvts.getRawParameterValue("sens");
    worker = std::thread([this, f, updateBpm, sens]
    {
        auto r = ds::analyse(f, sens, [this](float p, const juce::String& s) { progress = p; setStatus(s.trim()); }, &cancel);
        if (cancel.load()) { busy = false; return; }
        if (r == nullptr) { setStatus("COULD NOT ANALYSE THIS FILE"); busy = false; return; }
        { juce::SpinLock::ScopedLockType l(lock); result = r; }
        ++resultVersion;
        setStatus(r->fromCache ? "LOADED FROM CACHE" : "DONE");
        if (updateBpm)
        {
            const double bpm = r->bpm;
            juce::MessageManager::callAsync([this, bpm, a = alive, v = resultVersion.load()]
            {
                if (!a->load() || resultVersion.load() != v) return;
                if (auto* pb = apvts.getParameter("bpm")) pb->setValueNotifyingHost(pb->convertTo0to1((float) bpm));
            });
        }
        busy = false;
    });
}

juce::File DrumSlicerProcessor::makeMidiFile()
{
    auto r = getResult();
    if (!r || r->hits.empty()) return {};
    const bool tight = *apvts.getRawParameterValue("tight") > 0.5f;
    const auto bytes = ds::midiBytes(*r, midiBpm(), tight, *apvts.getRawParameterValue("quant"));
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("DrumSlicer_" + loopFile.getFileNameWithoutExtension() + ".mid");
    f.deleteFile();
    if (!f.replaceWithData(bytes.getData(), bytes.getSize())) return {};
    exportFolder().getChildFile(loopFile.getFileNameWithoutExtension() + (tight ? "_tight" : "_exact") + ".mid").replaceWithData(bytes.getData(), bytes.getSize());
    return f;
}

juce::File DrumSlicerProcessor::shotFile(int pad)
{
    auto r = getResult();
    if (!r || pad < 0 || pad >= ds::kPads || !r->pads[(size_t) pad].valid) return {};
    // the sound as you hear it: chosen version (A / B) with its shaping
    auto sh = getShaped();
    auto f = exportFolder().getChildFile("EXPORT").getChildFile(loopFile.getFileNameWithoutExtension() + "_" + r->pads[(size_t) pad].name.removeCharacters(" ") + ".wav");
    f.getParentDirectory().createDirectory();
    if (sh && sh->buf[(size_t) pad].getNumSamples() > 0) ds::writeWav24(f, sh->buf[(size_t) pad], r->sr);
    else ds::writeWav24(f, r->pads[(size_t) pad].audio, r->sr);
    return f;
}

juce::File DrumSlicerProcessor::exportFinal()
{
    auto r = getResult();
    if (!r) return {};
    for (int p = 0; p < ds::kPads; ++p) if (r->pads[(size_t) p].valid) shotFile(p);
    if (auto m = makeMidiFile(); m.existsAsFile())
        m.copyFileTo(exportFolder().getChildFile("EXPORT").getChildFile(loopFile.getFileNameWithoutExtension()
                        + (*apvts.getRawParameterValue("tight") > 0.5f ? "_tight" : "_exact") + ".mid"));
    return exportFolder().getChildFile("EXPORT");
}

juce::File DrumSlicerProcessor::exportFolder()
{
    auto r = getResult();
    auto dir = ds::cacheDir(loopFile);
    if (r && !ds::cacheInfo(loopFile).existsAsFile()) ds::saveCache(loopFile, *r);
    dir.createDirectory();
    return dir;
}

void DrumSlicerProcessor::getStateInformation(juce::MemoryBlock& d)
{
    auto st = apvts.copyState();
    st.setProperty("loopPath", loopFile.getFullPathName(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, d);
}

void DrumSlicerProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto st = juce::ValueTree::fromXml(*xml);
        apvts.replaceState(st);
        juce::File f(st.getProperty("loopPath").toString());
        if (f.existsAsFile()) startAnalysis(f, false);      // the cache in Documents/DrumSlicer makes this instant
    }
}

juce::AudioProcessorEditor* DrumSlicerProcessor::createEditor() { return new DrumSlicerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumSlicerProcessor(); }
