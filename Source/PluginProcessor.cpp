#include "PluginProcessor.h"
#include "PluginEditor.h"

// background workers (MATCH, the after-match ranking): counted, so the plugin never closes while one still runs
static std::atomic<int> gWorkers { 0 };
struct WorkerScope { WorkerScope() { ++gWorkers; } ~WorkerScope() { --gWorkers; } };


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
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "sync", 1 }, "Sync MIDI", false));
    for (int i = 0; i < ds::kPads; ++i)
    {
        const juce::String n("Pad " + juce::String(i + 1));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "vol" + juce::String(i), 1 }, n + " Vol",   NR(0.f, 1.5f, 0.001f), 1.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "pan" + juce::String(i), 1 }, n + " Pan",   NR(-1.f, 1.f, 0.001f), 0.f));
        const juce::String s(i);
        p.push_back(std::make_unique<F>(juce::ParameterID{ "st" + s, 1 },    n + " Start",   NR(0.f, 300.f, 0.1f, 0.5f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "en" + s, 1 },    n + " End",     NR(5.f, 2000.f, 0.1f, 0.5f), 2000.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "atk" + s, 1 },   n + " Fade In", NR(0.f, 500.f, 0.1f, 0.4f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "fic" + s, 1 },   n + " Fade In Curve", NR(-1.f, 1.f, 0.001f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "fo" + s, 1 },    n + " Fade Out", NR(0.f, 2000.f, 0.1f, 0.4f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "foc" + s, 1 },   n + " Fade Out Curve", NR(-1.f, 1.f, 0.001f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "hold" + s, 1 },  n + " Hold",    NR(0.f, 2000.f, 0.1f, 0.5f), 2000.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "dcy" + s, 1 },   n + " Decay",   NR(5.f, 2000.f, 0.1f, 0.5f), 2000.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "punch" + s, 1 }, n + " Punch",   NR(-12.f, 12.f, 0.1f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "sus" + s, 1 },   n + " Sustain", NR(-12.f, 12.f, 0.1f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "pitch" + s, 1 }, n + " Pitch",   NR(-12.f, 12.f, 0.01f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "tone" + s, 1 },  n + " Tone",    NR(-12.f, 12.f, 0.1f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "gain" + s, 1 },  n + " Gain",    NR(-24.f, 12.f, 0.1f), 0.f));
        p.push_back(std::make_unique<F>(juce::ParameterID{ "mamt" + s, 1 },  n + " Match Amount", NR(0.f, 100.f, 0.1f), 100.f));
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "best" + juce::String(i), 1 }, n + " Best hit", false));
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "usec" + juce::String(i), 1 }, n + " Your sample", false));
    }
    return { p.begin(), p.end() };
}

DrumSlicerProcessor::DrumSlicerProcessor()
    : AudioProcessor(makeBuses()), apvts(*this, nullptr, "STATE", makeLayout())
{
    for (auto& a : previewPad) a = 0;
    for (auto& a : padLevel) a = 0.f;
    pMaster = apvts.getRawParameterValue("master"); pBpm = apvts.getRawParameterValue("bpm");
    for (int i = 0; i < ds::kPads; ++i)
    {
        const juce::String n(i);
        pVol[(size_t) i] = apvts.getRawParameterValue("vol" + n); pPan[(size_t) i] = apvts.getRawParameterValue("pan" + n);
        pPitch[(size_t) i] = apvts.getRawParameterValue("pitch" + n);
    }
    fineAt.fill(std::numeric_limits<juce::int64>::min() / 2);
    startTimerHz(20);
}

ShapeParams DrumSlicerProcessor::shapeParams(int i) const
{
    auto v = [&](const char* id) { return apvts.getRawParameterValue(juce::String(id) + juce::String(i))->load(); };
    ShapeParams p;
    p.start = v("st"); p.end = v("en"); p.attack = v("atk"); p.hold = v("hold"); p.decay = v("dcy");
    p.punch = v("punch"); p.sustain = v("sus"); p.best = v("best") > 0.5f;
    p.attackCurve = v("fic"); p.fadeOut = v("fo"); p.fadeOutCurve = v("foc"); p.user = v("usec") > 0.5f;
    p.tone = v("tone"); p.gainDb = v("gain");
    p.matchAmount = v("mamt") * 0.01f;
    { std::lock_guard<std::mutex> g(matchLock); p.matchVersion = matchVer[(size_t) i] + (match[(size_t) i].on ? 100000 : 0); }
    return p;
}

// ---------------------------------------------------------------- a shape per source (A / B / C)
const char* const* DrumSlicerProcessor::shapeIds()
{
    static const char* const ids[kShapeN] = { "st", "en", "atk", "fic", "fo", "foc", "hold", "dcy", "punch", "sus", "pitch", "tone", "gain" };
    return ids;
}
std::array<float, DrumSlicerProcessor::kShapeN> DrumSlicerProcessor::shownShape(int pad) const
{
    std::array<float, kShapeN> v {};
    for (int k = 0; k < kShapeN; ++k) v[(size_t) k] = apvts.getRawParameterValue(juce::String(shapeIds()[k]) + juce::String(pad))->load();
    return v;
}
void DrumSlicerProcessor::showShape(int pad, const std::array<float, kShapeN>& v)
{
    for (int k = 0; k < kShapeN; ++k)
        if (auto* p = apvts.getParameter(juce::String(shapeIds()[k]) + juce::String(pad))) p->setValueNotifyingHost(p->convertTo0to1(v[(size_t) k]));
}
void DrumSlicerProcessor::switchSlot(int pad, int src)
{
    if (pad < 0 || pad >= ds::kPads || src < 0 || src > 2) return;
    std::array<float, kShapeN> load {}; bool doLoad = false;
    const int before = padSource(pad);               // (called before the A / B / C switch itself changes)
    {
        std::lock_guard<std::mutex> g(slotLock);
        int own = slotOwner[(size_t) pad];
        if (own < 0) { own = before; slotOwner[(size_t) pad] = before; }   // not known yet: what is shown belongs to what is chosen
        if (own == src) return;
        slots[(size_t) pad][(size_t) own].valid = true; slots[(size_t) pad][(size_t) own].v = shownShape(pad);
        // a slot never set yet takes the shape shown now (projects of older versions: all three alike)
        if (slots[(size_t) pad][(size_t) src].valid) { load = slots[(size_t) pad][(size_t) src].v; doLoad = true; }
        slotOwner[(size_t) pad] = src;
    }
    if (doLoad) showShape(pad, load);
}
void DrumSlicerProcessor::resetShape(int pad, int src)
{
    if (pad < 0 || pad >= ds::kPads || src < 0 || src > 2) return;
    std::array<float, kShapeN> v {};
    for (int k = 0; k < kShapeN; ++k)
        if (auto* p = apvts.getParameter(juce::String(shapeIds()[k]) + juce::String(pad))) v[(size_t) k] = p->convertFrom0to1(p->getDefaultValue());
    bool shown;
    {
        std::lock_guard<std::mutex> g(slotLock);
        slots[(size_t) pad][(size_t) src] = { true, v };
        shown = slotOwner[(size_t) pad] == src;
    }
    if (shown) showShape(pad, v);
}
juce::ValueTree DrumSlicerProcessor::slotsTree()
{
    juce::ValueTree t("SLOTS");
    std::lock_guard<std::mutex> g(slotLock);
    for (int p = 0; p < ds::kPads; ++p)
    {
        t.setProperty("own" + juce::String(p), slotOwner[(size_t) p], nullptr);
        for (int s = 0; s < 3; ++s)
        {
            if (!slots[(size_t) p][(size_t) s].valid || s == slotOwner[(size_t) p]) continue;     // the shown one is in the parameters
            juce::StringArray a; for (float x : slots[(size_t) p][(size_t) s].v) a.add(juce::String(x, 4));
            t.setProperty("p" + juce::String(p) + "s" + juce::String(s), a.joinIntoString(","), nullptr);
        }
    }
    return t;
}
void DrumSlicerProcessor::slotsFromTree(const juce::ValueTree& t)
{
    std::lock_guard<std::mutex> g(slotLock);
    for (int p = 0; p < ds::kPads; ++p)
    {
        slotOwner[(size_t) p] = t.isValid() ? (int) t.getProperty("own" + juce::String(p), -1) : -1;
        for (int s = 0; s < 3; ++s)
        {
            auto& sl = slots[(size_t) p][(size_t) s];
            sl.valid = false;
            const auto str = t.isValid() ? t.getProperty("p" + juce::String(p) + "s" + juce::String(s)).toString() : juce::String();
            juce::StringArray a; a.addTokens(str, ",", "");
            if (a.size() == kShapeN || a.size() == kShapeN - 1)          // (1.10 had no gain: it is 0 dB)
            { for (int k = 0; k < kShapeN; ++k) sl.v[(size_t) k] = k < a.size() ? a[k].getFloatValue() : 0.f; sl.valid = true; }
        }
    }
}

// trim -> transient shaper (punch on the attack, sustain on the rest) -> attack / hold / decay envelope
void applyMatchChain(juce::AudioBuffer<float>& x, double sr, const MatchData& m, float amount)
{   // the same order it was measured in: pitch drop -> saturation -> EQ -> room -> shape -> width
    if (m.useGlide) mt::applyGlide(x, sr, m.glide, amount);
    if (m.useDrive) mt::applyDrive(x, m.driveDb, amount);
    if (m.useEq) mt::applyEq(x, sr, m.eq, amount);
    if (m.useRoom && m.roomMix > 0) mt::addRoom(x, sr, m.roomRt, m.roomMix * amount, m.roomPre);
    if (m.useShape) mt::applyShape(x, sr, m.shape, amount);
    if (m.useWidth) mt::applyWidth(x, m.widthDb, amount);
}

juce::AudioBuffer<float> shapeShot(const juce::AudioBuffer<float>& rawIn, double sr, const ShapeParams& p, int& offset, const MatchData* match)
{
    juce::AudioBuffer<float> matched;
    if (match != nullptr && match->on && p.matchAmount > 0.001f)
    {
        matched.makeCopyOf(rawIn);
        if (matched.getNumChannels() < 2) { matched.setSize(2, matched.getNumSamples(), true); matched.copyFrom(1, 0, matched, 0, 0, matched.getNumSamples()); }
        applyMatchChain(matched, sr, *match, p.matchAmount);
    }
    const juce::AudioBuffer<float>& raw = matched.getNumSamples() > 0 ? matched : rawIn;
    const int len = raw.getNumSamples();
    const int s0 = juce::jlimit(0, std::max(0, len - 1), (int) (p.start * 0.001 * sr));
    const int e = juce::jlimit(s0 + 1, len, (int) (p.end * 0.001 * sr));
    const int n = std::max(1, e - s0);
    offset = s0;
    juce::AudioBuffer<float> o(2, n);
    for (int c = 0; c < 2; ++c) o.copyFrom(c, 0, raw, std::min(c, raw.getNumChannels() - 1), s0, n);
    if (std::abs(p.tone) > 0.05f)
    {   // TONE: a tilt around ~800 Hz, + = brighter (highs up, lows down by half each), - = darker / duller
        const float k = (float) std::exp(-2.0 * juce::MathConstants<double>::pi * 800.0 / sr);
        const float gl = juce::Decibels::decibelsToGain(-p.tone * 0.5f), gh = juce::Decibels::decibelsToGain(p.tone * 0.5f);
        for (int c = 0; c < 2; ++c)
        {
            auto* d = o.getWritePointer(c); float lp = 0;
            for (int i = 0; i < n; ++i) { lp = (1.f - k) * d[i] + k * lp; d[i] = lp * gl + (d[i] - lp) * gh; }
        }
    }
    const bool shapeT = std::abs(p.punch) > 0.05f || std::abs(p.sustain) > 0.05f;
    auto coef = [&](double ms) { return (float) std::exp(-1.0 / (ms * 0.001 * sr)); };
    const float fa = coef(0.5), fr = coef(30), sa = coef(15), sr2 = coef(150), gsm = coef(2);
    float fast = 0, slow = 0, gs = 1;
    const double atk = p.attack * 0.001 * sr, hold = p.hold * 0.001 * sr, dec = p.decay * 0.001 * sr;
    const double fo = std::min((double) n, p.fadeOut * 0.001 * sr);
    const double pin = std::pow(2.0, -3.0 * p.attackCurve), pout = std::pow(2.0, -3.0 * p.fadeOutCurve);
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
        if (i < atk) env = std::pow(i / atk, pin);                                                     // fade in
        else if (i > atk + hold) env = std::pow(10.0, -3.0 * (i - atk - hold) / std::max(1.0, dec));   // (older projects: decay)
        if (fo > 0 && i >= n - fo) env *= std::pow(std::max(0.0, (n - i) / fo), pout);                // fade out to the end
        g *= (float) env;
        L[i] *= g; R[i] *= g;
    }
    if (std::abs(p.gainDb) > 0.01f) o.applyGain(juce::Decibels::decibelsToGain(p.gainDb));   // GAIN of this source (A / B / C)
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
    for (int p = 0; p < ds::kPads; ++p) switchSlot(p, padSource(p));   // A / B / C changed from anywhere (host, project)
    reshape();
    rebuildSeq();
}

void DrumSlicerProcessor::rebuildSeq()
{
    auto r = getResult();
    const int mode = midiMode();
    const double bpm = midiBpm(), quant = apvts.getRawParameterValue("quant")->load();
    const juce::String key = r ? juce::String(resultVersion.load()) + "|" + juce::String(mode) + "|" + juce::String(quant, 3) + "|" + juce::String(bpm, 3) : juce::String();
    if (key == seqKey) return;
    seqKey = key;
    std::shared_ptr<Seq> s;
    if (r && !r->hits.empty() && bpm > 0)
    {
        // exactly the MIDI file you would drag, read back
        const auto bytes = ds::midiBytes(*r, bpm, mode, quant);
        juce::MemoryInputStream in(bytes, false);
        juce::MidiFile mf;
        if (mf.readFrom(in))
        {
            s = std::make_shared<Seq>();
            const double ppq = mf.getTimeFormat() > 0 ? mf.getTimeFormat() : 480.0;
            std::array<std::pair<double, int>, 128> fine; fine.fill({ -10.0, 0 });
            for (int t = 0; t < mf.getNumTracks(); ++t)
                if (auto* tr = mf.getTrack(t))
                    for (const auto* e : *tr)
                    {
                        const auto& m = e->message;
                        const double tick = m.getTimeStamp() * 480.0 / ppq;
                        if (m.isAftertouch()) fine[(size_t) m.getNoteNumber()] = { tick, m.getAfterTouchValue() };   // sub-tick timing
                        else if (m.isNoteOn())
                        {
                            const auto& f = fine[(size_t) m.getNoteNumber()];
                            const double exact = tick + (std::abs(f.first - (tick - 1.0)) < 0.01 ? f.second / 127.0 : 0.0);
                            s->ev.push_back({ exact, m.getNoteNumber(), m.getVelocity() });
                        }
                    }
            std::sort(s->ev.begin(), s->ev.end(), [](const SeqEv& a, const SeqEv& b) { return a.tick < b.tick; });
            // one turn = the analysed part: EXACT in seconds (at this tempo), SYNC / TIGHT in whole beats of the loop
            const double secs = r->loop.getNumSamples() / r->sr;
            const double beats = mode == ds::kExact ? secs * bpm / 60.0 : std::max(1.0, std::round(secs * r->bpm / 60.0));
            s->loopTicks = beats * 480.0;
        }
    }
    std::shared_ptr<const Seq> old;
    { juce::SpinLock::ScopedLockType l(lock); old = seq; seq = s; }
    retire(std::move(old));
}

void DrumSlicerProcessor::playMidi(bool on)
{
    if (on)
    {
        rebuildSeq();
        srcPlay = false; stopPreview();
        midiRestart = true;
    }
    midiPlay = on;
}

void DrumSlicerProcessor::reshape()
{
    {   // results / shaped sounds nobody else uses any more are freed here, never on the audio thread
        std::lock_guard<std::mutex> g(retiredLock);
        retired.erase(std::remove_if(retired.begin(), retired.end(), [](const std::shared_ptr<const void>& p) { return p.use_count() <= 1; }), retired.end());
    }
    auto r = getResult();
    if (!r) return;
    std::array<ShapeParams, ds::kPads> now;
    bool changed = resultVersion.load() != lastShapedVersion;
    for (int i = 0; i < ds::kPads; ++i) { now[(size_t) i] = shapeParams(i); if (now[(size_t) i] != lastShape[(size_t) i]) changed = true; }
    if (!changed) return;
    auto sh = std::make_shared<Shaped>();
    std::array<MatchData, ds::kPads> md;
    { std::lock_guard<std::mutex> g(matchLock); md = match; }
    for (int i = 0; i < ds::kPads; ++i)
        if (r->pads[(size_t) i].valid)
        {
            const bool c = now[(size_t) i].user && r->pads[(size_t) i].user.getNumSamples() > 0;
            sh->buf[(size_t) i] = shapeShot(r->pads[(size_t) i].get(now[(size_t) i].best, now[(size_t) i].user), r->sr, now[(size_t) i], sh->offset[(size_t) i],
                                            c ? &md[(size_t) i] : nullptr);
        }
    std::shared_ptr<Shaped> old;
    { juce::SpinLock::ScopedLockType l(lock); old = shaped; shaped = sh; }
    retire(std::move(old));
    lastShape = now; lastShapedVersion = resultVersion.load();
}

DrumSlicerProcessor::~DrumSlicerProcessor()
{
    *alive = false;
    {   // stop the rankings, then wait (a few hundred ms at most) for every background worker of this plugin
        std::lock_guard<std::mutex> g(rankLock);
        for (auto& w : ranks) if (auto st = w.lock()) st->cancel = true;
    }
    for (int i = 0; i < 300 && gWorkers.load() > 0; ++i) juce::Thread::sleep(10);
    stopTimer();
    stopWorker();
}

void DrumSlicerProcessor::stopWorker()
{
    cancel = true;
    if (worker.joinable()) worker.join();
    cancel = false;
}

void DrumSlicerProcessor::publish(std::shared_ptr<ds::Result> r, const juce::File& kit, const juce::File& loop, const juce::String& orphanName)
{
    if (r != nullptr)
    {   // sounds that did not come from the loaded file (a kit, a project): their loop becomes the loaded file
        auto s = getSource();
        if (s == nullptr || r->sourceId != s->id || r->regionStart + r->loop.getNumSamples() > s->length())
        {
            juce::AudioBuffer<float> a; a.makeCopyOf(r->loop);
            auto ns = ds::makeSource(std::move(a), r->sr, loop);
            r->sourceId = ns->id; r->regionStart = 0;
            setSource(ns, 0, ns->length());
        }
    }
    std::shared_ptr<ds::Result> old;
    { juce::SpinLock::ScopedLockType l(lock); old = result; result = std::move(r); }
    setFiles(kit, loop, orphanName);
    retire(std::move(old));
    ++resultVersion;
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
void DrumSlicerProcessor::startVoice(int pad, float g, int wait, bool audition)
{
    if (pad < 0 || pad >= ds::kPads || !audioRes || !audioShaped || audioShaped->buf[(size_t) pad].getNumSamples() == 0) return;
    const double ratio = audioRes->sr / hostSR;
    const double step = ratio * std::pow(2.0, pPitch[(size_t) pad]->load() / 12.0);
    wait += (int) std::lround(audioShaped->offset[(size_t) pad] / step);    // a trimmed start keeps the hit on time
    if (audition)                                     // a new click fades the last one out (5 ms) instead of stacking on it
        for (auto& vv : voices)
            if (vv.on && vv.audition && vv.fade < 0) { vv.fadeLen = std::max(1, (int) (0.005 * hostSR)); vv.fade = vv.fadeLen; }
    Voice* v = nullptr;
    for (auto& vv : voices) if (!vv.on) { v = &vv; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& vv : voices) if (vv.age < v->age) v = &vv; }   // steal the oldest
    v->on = true; v->pad = pad; v->wait = std::max(0, wait); v->pos = 0.0; v->step = step; v->gain = g; v->age = ++voiceAge;
    v->sh = audioShaped; v->audition = audition; v->fade = -1; v->fadeLen = 1;
    padLevel[(size_t) pad] = g;
}

double DrumSlicerProcessor::midiBpm() const
{
    const double h = hostBpm.load();
    return h > 0 ? h : (double) pBpm->load();
}

void DrumSlicerProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buf.clear();
    bool fresh = false;
    {
        juce::SpinLock::ScopedTryLockType l(lock);
        fresh = l.isLocked();
        if (fresh)
        {
            if (audioRes != result)
            {   // hits still ringing keep their own sound; they fade out quickly (no hard cut = no click)
                audioRes = result;
                for (auto& v : voices)
                    if (v.on && v.fade < 0)
                    {
                        if (result == nullptr) v.on = false;   // nothing renders without sounds (a new file is loading)
                        else { v.fadeLen = std::max(1, (int) (0.005 * hostSR)); v.fade = v.fadeLen; }
                    }
            }
            audioShaped = shaped;
            audioSeq = seq;
            if (audioSource != source) { audioSource = source; srcPos = (double) selS.load(); }
            if (audioPreview != preview)
            {   // a new preview: the one still sounding fades out (5 ms) instead of being cut
                if (audioPreview && previewPlaying.load() && previewPos > 0) startPreviewFade();
                audioPreview = preview; audioPreviewSR = previewSR; previewPos = 0;
            }
        }
    }
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) hostBpm = *b;
    const int numSamples = buf.getNumSamples();
    if (previewStop.exchange(false)) { if (audioPreview && previewPlaying.load()) startPreviewFade(); audioPreview.reset(); previewPlaying = false; }
    if (fadePrev && fadeLeft > 0)
    {
        const auto& pb = *fadePrev;
        const double step = fadeSR / hostSR; const float g = pMaster->load();
        auto* oL = buf.getWritePointer(0); auto* oR = buf.getWritePointer(1);
        const int len = pb.getNumSamples(), c1 = pb.getNumChannels() > 1 ? 1 : 0;
        for (int i = 0; i < numSamples && fadeLeft > 0; ++i, --fadeLeft)
        {
            const int i0 = (int) fadePos;
            if (i0 + 1 >= len) { fadeLeft = 0; break; }
            const float fr = (float) (fadePos - i0), k = g * (float) fadeLeft / (float) fadeLen;
            oL[i] += k * (pb.getSample(0, i0) + (pb.getSample(0, i0 + 1) - pb.getSample(0, i0)) * fr);
            oR[i] += k * (pb.getSample(c1, i0) + (pb.getSample(c1, i0 + 1) - pb.getSample(c1, i0)) * fr);
            fadePos += step;
        }
        if (fadeLeft <= 0) fadePrev.reset();         // never the last owner: replaced previews are retired
    }   // "preview" itself is retired on the message thread
    if (audioPreview && previewPlaying.load())
    {   // a saved kit playing its pattern: straight to the main output
        const auto& pb = *audioPreview;
        const double step = audioPreviewSR / hostSR; const float g = pMaster->load();
        auto* oL = buf.getWritePointer(0); auto* oR = buf.getWritePointer(1);
        const int len = pb.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            const int i0 = (int) previewPos;
            if (i0 + 1 >= len) { previewPlaying = false; break; }
            const float fr = (float) (previewPos - i0);
            oL[i] += g * (pb.getSample(0, i0) + (pb.getSample(0, i0 + 1) - pb.getSample(0, i0)) * fr);
            oR[i] += g * (pb.getSample(1, i0) + (pb.getSample(1, i0 + 1) - pb.getSample(1, i0)) * fr);
            previewPos += step;
        }
    }
    if (audioSource && srcPlay.load())
    {   // the loaded file: the selection, looped, from the cursor
        const auto& sb = audioSource->audio;
        const juce::int64 a = selS.load(), b = std::min<juce::int64>(selE.load(), sb.getNumSamples() - 1);
        const juce::int64 sk = srcSeek.exchange(-1);
        if (sk >= 0) srcPos = (double) sk;
        if (srcPos < (double) a || srcPos >= (double) b) srcPos = (double) a;
        const double step = audioSource->sr / hostSR; const float g = pMaster->load();
        const int c1 = sb.getNumChannels() > 1 ? 1 : 0;
        auto* oL = buf.getWritePointer(0); auto* oR = buf.getWritePointer(1);
        const int fadeN = std::max(1, (int) (0.002 * audioSource->sr));
        for (int i = 0; i < numSamples && b > a; ++i)
        {
            if (srcPos >= (double) b) srcPos = (double) a;
            const int i0 = (int) srcPos; const float fr = (float) (srcPos - i0);
            const int i1 = std::min(i0 + 1, sb.getNumSamples() - 1);
            // 2 ms fades at the selection's edges: the loop point never clicks
            const double fromA = srcPos - (double) a, toB = (double) b - srcPos;
            const float edge = (float) std::min(1.0, std::min(fromA, toB) / fadeN);
            oL[i] += g * edge * (sb.getSample(0, i0) + (sb.getSample(0, i1) - sb.getSample(0, i0)) * fr);
            oR[i] += g * edge * (sb.getSample(c1, i0) + (sb.getSample(c1, i1) - sb.getSample(c1, i0)) * fr);
            srcPos += step;
        }
        srcPosShown = srcPos;
    }
    else if (audioSource) { const juce::int64 sk = srcSeek.exchange(-1); if (sk >= 0) srcPos = (double) sk; srcPosShown = srcPos; }
    if (!audioRes || !audioShaped) { clock += numSamples; return; }
    const auto& res = *audioRes;

    const float master = pMaster->load();
    float vol[ds::kPads], pl[ds::kPads], pr[ds::kPads];
    for (int i = 0; i < ds::kPads; ++i)
    {
        vol[i] = pVol[(size_t) i]->load();
        const float pan = pPan[(size_t) i]->load();
        const float a = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
        pl[i] = 1.41421356f * std::cos(a);           // pan 0 -> 1.0 / 1.0 (unity)
        pr[i] = 1.41421356f * std::sin(a);
        if (fresh && previewPad[(size_t) i].exchange(0)) startVoice(i, 1.f, 0, true);   // (else: next block, with the new sound)
    }
    const double spt = 60.0 / midiBpm() / 480.0 * hostSR;   // samples per tick of the exported MIDI

    auto render = [&](int s0, int s1)
    {
        for (auto& v : voices)
        {
            if (!v.on || s1 <= s0 || !v.sh) continue;
            const auto& sh = v.sh->buf[(size_t) v.pad];
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
                float gf = g;
                if (v.fade >= 0)
                {
                    if (v.fade == 0) { v.on = false; break; }
                    gf *= (float) v.fade / (float) v.fadeLen; --v.fade;
                }
                oL[i] += l * gf * pl[v.pad];
                oR[i] += r * gf * pr[v.pad];
                v.pos += v.step;
            }
        }
    };

    if (midiPlay.load() && audioSeq && audioSeq->loopTicks > 0)
    {   // the MIDI preview: notes started like host notes, sample-exact inside the block
        if (midiRestart.exchange(false)) seqPos = 0;
        const auto& sq = *audioSeq;
        const double spt = 60.0 / midiBpm() / 480.0 * hostSR;
        double remaining = numSamples, offset = 0;
        for (int guard = 0; guard < 64 && remaining > 0.5; ++guard)
        {
            const double endTick = std::min(sq.loopTicks, seqPos + remaining / spt);
            auto it = std::lower_bound(sq.ev.begin(), sq.ev.end(), seqPos, [](const SeqEv& e, double t) { return e.tick < t; });
            for (; it != sq.ev.end() && it->tick < endTick; ++it)
                if (it->note >= 0 && it->note < 128)
                    startVoice(res.noteToPad[(size_t) it->note], (float) it->vel / 100.f, (int) std::floor(offset + (it->tick - seqPos) * spt + 0.5));
            const double used = (endTick - seqPos) * spt;
            offset += used; remaining -= used; seqPos = endTick;
            if (seqPos >= sq.loopTicks - 1e-9) seqPos = 0;
            if (used <= 0 && seqPos != 0) break;
        }
    }
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
// All starts come from the message thread; a new job stops the running one first. A job that fails leaves
// the current sounds (and their files) as they were and says so.
void DrumSlicerProcessor::setSource(std::shared_ptr<ds::Source> s, juce::int64 a, juce::int64 b)
{
    std::shared_ptr<const ds::Source> old;
    {
        juce::SpinLock::ScopedLockType l(lock);
        old = source; source = std::move(s);
        const juce::int64 n = source ? source->length() : 0;
        a = juce::jlimit<juce::int64>(0, n, a); b = juce::jlimit<juce::int64>(a, n, b);
        if (b <= a) { a = 0; b = n; }
        selS = a; selE = b; srcSeek = a;
    }
    retire(std::move(old));
}

void DrumSlicerProcessor::setSelection(juce::int64 a, juce::int64 b)
{
    auto s = getSource(); if (!s) return;
    const juce::int64 n = s->length(), minLen = (juce::int64) (0.05 * s->sr);
    a = juce::jlimit<juce::int64>(0, std::max<juce::int64>(0, n - minLen), a);
    b = juce::jlimit<juce::int64>(std::min(n, a + minLen), n, b);
    selS = a; selE = b;
}

bool DrumSlicerProcessor::selectionAnalysed() const
{
    auto r = getResult(); auto s = getSource();
    return r && s && r->sourceId == s->id && r->regionStart == selS.load() && r->regionStart + r->loop.getNumSamples() == selE.load();
}

void DrumSlicerProcessor::loadSource(const juce::File& f)
{
    stopWorker();
    srcPlay = false;
    const int token = ++jobToken;
    busy = true; progress = 0.f; setStatus("LOADING...");
    worker = std::thread([this, f, token]
    {
        juce::AudioBuffer<float> a; double sr = 0;
        if (!ds::loadAudio(f, a, sr)) { if (token == jobToken.load()) setStatus("COULD NOT READ " + f.getFileName()); busy = false; return; }
        if (cancel.load() || token != jobToken.load()) { busy = false; return; }
        auto s = ds::makeSource(std::move(a), sr, f);
        if (cancel.load() || token != jobToken.load()) { busy = false; return; }
        setSource(s, 0, s->length());
        publish(nullptr, {}, f);                     // the old sounds go: these pads will come from this file
        setStatus("CHOOSE THE PART, THEN ANALYSE");
        busy = false;
    });
}

void DrumSlicerProcessor::analyseSelection(bool force)
{
    auto s = getSource();
    if (!s) return;
    const juce::int64 a = selS.load(), b = selE.load();
    if (b - a < (juce::int64) (0.5 * s->sr)) { setStatus("SELECTION TOO SHORT"); return; }
    const bool whole = a == 0 && b == s->length();
    const juce::String region = whole ? juce::String() : "|r" + juce::String(a) + "-" + juce::String(b);
    stopWorker();
    if (force && s->file.existsAsFile()) { ds::cacheInfo(s->file).deleteFile(); ds::cacheInfoIn(ds::legacyCacheDir(s->file), s->file).deleteFile(); }
    const int token = ++jobToken;
    busy = true; progress = 0.f; setStatus("ANALYSING...");
    const float sens = apvts.getRawParameterValue("sens")->load();
    const auto orphanName = orphan();
    worker = std::thread([this, s, a, b, region, sens, token, orphanName]
    {
        juce::AudioBuffer<float> part(2, (int) (b - a));
        for (int c = 0; c < 2; ++c) part.copyFrom(c, 0, s->audio, std::min(c, s->audio.getNumChannels() - 1), (int) a, (int) (b - a));
        auto r = ds::analyseBuffer(std::move(part), s->sr, s->file, region, sens, [this](float p, const juce::String& st) { progress = p; setStatus(st.trim()); }, &cancel);
        if (cancel.load() || token != jobToken.load()) { busy = false; return; }
        if (r == nullptr) { setStatus("COULD NOT ANALYSE THIS PART"); busy = false; return; }
        r->regionStart = a; r->sourceId = s->id;
        publish(r, {}, s->file, s->file == juce::File() ? orphanName : juce::String());
        setStatus(r->fromCache ? "LOADED FROM CACHE" : "DONE");
        const double bpm = r->bpm;
        juce::MessageManager::callAsync([this, bpm, a = alive, v = resultVersion.load()]
        {
            if (!a->load() || resultVersion.load() != v) return;
            if (auto* pb = apvts.getParameter("bpm")) pb->setValueNotifyingHost(pb->convertTo0to1((float) bpm));
        });
        busy = false;
    });
}

void DrumSlicerProcessor::playSource(bool on)
{
    if (on) { stopPreview(); midiPlay = false; auto s = getSource(); if (!s) return; }
    srcPlay = on;
}

juce::File DrumSlicerProcessor::makeMidiFile()
{
    auto r = getResult();
    if (!r || r->hits.empty()) return {};
    const int mode = midiMode();
    const auto bytes = ds::midiBytes(*r, midiBpm(), mode, apvts.getRawParameterValue("quant")->load());
    // a new file for every drag (two instances never hand Cubase the same file); the previous one is removed
    static std::atomic<int> counter { 0 };
    if (lastTempMidi != juce::File()) lastTempMidi.deleteFile();
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("ReKit_" + juce::File::createLegalFileName(baseName()) + "_" + juce::String(++counter) + ".mid");
    if (!f.replaceWithData(bytes.getData(), bytes.getSize())) return {};
    lastTempMidi = f;
    ds::writeData(exportFolder().getChildFile(baseName() + ds::midiSuffix(mode) + ".mid"), bytes.getData(), bytes.getSize());
    return f;
}

juce::File DrumSlicerProcessor::shotFile(int pad)
{
    auto r = getResult();
    if (!r || pad < 0 || pad >= ds::kPads || !r->pads[(size_t) pad].valid) return {};
    // the sound as you hear it: chosen version (A / B) with its shaping
    auto sh = getShaped();
    auto f = exportFolder().getChildFile("EXPORT").getChildFile(baseName() + "_" + r->pads[(size_t) pad].name.removeCharacters(" ") + ".wav");
    const bool ok = (sh && sh->buf[(size_t) pad].getNumSamples() > 0) ? ds::writeWav24(f, sh->buf[(size_t) pad], r->sr)
                                                                      : ds::writeWav24(f, r->pads[(size_t) pad].audio, r->sr);
    if (!ok) { setStatus("COULD NOT WRITE " + f.getFileName()); return {}; }
    return f;
}

juce::File DrumSlicerProcessor::exportFinal()
{
    auto r = getResult();
    if (!r) return {};
    bool ok = true;
    for (int p = 0; p < ds::kPads; ++p) if (r->pads[(size_t) p].valid) ok = shotFile(p).existsAsFile() && ok;
    const int mode = midiMode();
    if (auto m = makeMidiFile(); m.existsAsFile())
        ok = m.copyFileTo(exportFolder().getChildFile("EXPORT").getChildFile(baseName() + ds::midiSuffix(mode) + ".mid")) && ok;
    else ok = false;
    if (!ok) setStatus("SOME EXPORT FILES COULD NOT BE WRITTEN");
    return exportFolder().getChildFile("EXPORT");
}

juce::File DrumSlicerProcessor::exportFolder()
{
    auto r = getResult();
    const auto kit = getKitDir(), loop = getLoopFile();
    if (kit != juce::File()) { kit.createDirectory(); return kit; }
    if (!loop.existsAsFile()) { auto d = ds::root().getChildFile(juce::File::createLegalFileName(baseName()) + "_project"); d.createDirectory(); return d; }
    auto dir = ds::cacheDir(loop);
    if (r && !ds::cacheInfo(loop).existsAsFile() && loop.existsAsFile()) ds::saveCache(loop, *r);
    dir.createDirectory();
    return dir;
}

// ---------------------------------------------------------------- project state
// settings + where the sounds came from + the sounds themselves (see ds::embedResult)
void DrumSlicerProcessor::getStateInformation(juce::MemoryBlock& d)
{
    auto st = apvts.copyState();
    st.setProperty("loopPath", getLoopFile().getFullPathName(), nullptr);
    st.setProperty("kitPath", getKitDir().getFullPathName(), nullptr);
    st.setProperty("snap", snap.load(), nullptr);
    st.appendChild(slotsTree(), nullptr);
    st.appendChild(matchTree(), nullptr);
    if (auto r = getResult())
    {
        std::lock_guard<std::mutex> g(embedLock);
        const int v = resultVersion.load();
        if (embedVersion != v || !embedCache.isValid()) { embedCache = ds::embedResult(*r); embedVersion = v; }
        st.appendChild(embedCache.createCopy(), nullptr);
    }
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, d);
}

void DrumSlicerProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml(*xml);
    auto emb = st.getChildWithName("EMBED");
    if (emb.isValid()) { emb = emb.createCopy(); st.removeChild(st.getChildWithName("EMBED"), nullptr); }
    const juce::String kp = st.getProperty("kitPath").toString(), lp = st.getProperty("loopPath").toString();
    snap = (bool) st.getProperty("snap", true);
    { auto sl = st.getChildWithName("SLOTS"); slotsFromTree(sl.createCopy()); if (sl.isValid()) st.removeChild(sl, nullptr); }
    { auto mm = st.getChildWithName("MATCH"); matchFromTree(mm.createCopy()); if (mm.isValid()) st.removeChild(mm, nullptr); }
    apvts.replaceState(st);
    restoreAsync(emb, kp.isNotEmpty() ? juce::File(kp) : juce::File(), lp.isNotEmpty() ? juce::File(lp) : juce::File());
}

// the sounds saved in the project win (they are exactly what was heard); then the kit (also when it was
// moved: found by name), then the loop and its analysis. Nothing found: no old sounds keep playing.
void DrumSlicerProcessor::restoreAsync(juce::ValueTree embedded, juce::File kitIn, juce::File loopIn)
{
    stopWorker();
    const int token = ++jobToken;
    busy = true; progress = 0.5f; setStatus("OPENING...");
    const float sens = apvts.getRawParameterValue("sens")->load();
    worker = std::thread([this, embedded, kitIn, loopIn, token, sens]
    {
        auto stale = [&] { return cancel.load() || token != jobToken.load(); };
        const auto kit = kitIn != juce::File() ? ds::findKit(kitIn) : juce::File();
        juce::File loop = loopIn;
        if (!loop.existsAsFile() && kit != juce::File()) loop = ds::kitLoop(kit);
        if (auto r = ds::unembedResult(embedded))
        {
            if (loop.existsAsFile())
            {   // the whole file is shown again, with the part that was analysed selected
                juce::AudioBuffer<float> a; double sr = 0;
                const juce::int64 rs = r->regionStart, len = r->loop.getNumSamples();
                if (ds::loadAudio(loop, a, sr) && std::abs(sr - r->sr) < 1.0)
                {
                    if (rs > 0 || len < a.getNumSamples())
                    {
                        if (rs + len <= a.getNumSamples())
                        {
                            juce::AudioBuffer<float> part(2, (int) len);
                            for (int c = 0; c < 2; ++c) part.copyFrom(c, 0, a, c, (int) rs, (int) len);
                            r->loop = std::move(part);
                            auto src = ds::makeSource(std::move(a), sr, loop);
                            r->sourceId = src->id;
                            r->region = "|r" + juce::String(rs) + "-" + juce::String(rs + len);
                            if (stale()) { busy = false; return; }
                            setSource(src, rs, rs + len);
                        }
                    }
                    else
                    {
                        auto src = ds::makeSource(std::move(a), sr, loop);
                        r->loop.makeCopyOf(src->audio); r->sourceId = src->id;
                        if (stale()) { busy = false; return; }
                        setSource(src, 0, src->length());
                    }
                }
            }
            if (stale()) { busy = false; return; }
            if (kit != juce::File() || loop.existsAsFile()) publish(r, kit, loop);
            else publish(r, {}, {}, kitIn != juce::File() ? kitIn.getFileName() : loopIn.getFileNameWithoutExtension());
            setStatus("OPENED");
            busy = false; return;
        }
        if (kit != juce::File())
            if (auto r = ds::loadKitData(kit))
            {
                if (stale()) { busy = false; return; }
                publish(r, kit, ds::kitLoop(kit)); setStatus("KIT LOADED"); busy = false; return;
            }
        if (loop.existsAsFile())
        {
            auto r = ds::analyse(loop, sens, [this](float p, const juce::String& s) { progress = p; setStatus(s.trim()); }, &cancel);
            if (stale()) { busy = false; return; }
            if (r != nullptr) { publish(r, {}, loop); setStatus("DONE"); busy = false; return; }
        }
        if (stale()) { busy = false; return; }
        const bool hadSomething = loopIn != juce::File() || kitIn != juce::File();
        publish(nullptr, {}, {});
        setStatus(hadSomething ? "COULD NOT FIND " + (kitIn != juce::File() ? kitIn.getFileName() : loopIn.getFileName()) : juce::String());
        busy = false;
    });
}

// ---------------------------------------------------------------- pad type / merge
const std::vector<juce::String>& DrumSlicerProcessor::padTypes()
{
    static const std::vector<juce::String> t { "KICK", "SNARE", "CLAP", "RIM", "TOM", "PERC", "SHAKER", "C-HAT", "O-HAT", "CRASH", "RIDE" };
    return t;
}

// name + MIDI note of a pad for an instrument: the first free note of its kind (KICK, KICK 2 ...)
static bool assignType(ds::Result& r, int pad, const juce::String& type)
{
    static const std::map<juce::String, std::vector<int>> notes {
        { "KICK", { 36, 35 } }, { "SNARE", { 38, 40 } }, { "CLAP", { 39, 54 } }, { "RIM", { 37, 75 } },
        { "TOM", { 45, 47, 48, 43, 50, 41 } }, { "PERC", { 62, 63, 64, 60, 61, 56, 76, 77 } }, { "SHAKER", { 70, 82 } },
        { "C-HAT", { 42, 44 } }, { "O-HAT", { 46, 53 } }, { "CRASH", { 49, 57 } }, { "RIDE", { 51, 59 } } };
    auto it = notes.find(type); if (it == notes.end()) return false;
    int k = 0;
    for (int note : it->second)
    {
        bool used = false;
        for (int q = 0; q < ds::kPads; ++q) if (q != pad && r.pads[(size_t) q].valid && r.pads[(size_t) q].note == note) used = true;
        if (!used) break;
        ++k;
    }
    int note = -1;
    if (k < (int) it->second.size()) note = it->second[(size_t) k];
    else
    {   // all notes of this kind taken: the first free one from 65 up (never a note another pad plays)
        for (int nn = 65; nn < 128 && note < 0; ++nn)
        {
            bool used = false;
            for (int q = 0; q < ds::kPads; ++q) if (q != pad && r.pads[(size_t) q].valid && r.pads[(size_t) q].note == nn) used = true;
            if (!used) note = nn;
        }
        if (note < 0) note = it->second[0];
    }
    int same = 0; for (int q = 0; q < ds::kPads; ++q) if (q != pad && r.pads[(size_t) q].valid && r.pads[(size_t) q].name.upToFirstOccurrenceOf(" ", false, false) == type) ++same;
    r.pads[(size_t) pad].note = note;
    r.pads[(size_t) pad].name = same == 0 ? type : type + " " + juce::String(same + 1);
    return true;
}

void DrumSlicerProcessor::setPadType(int pad, const juce::String& type)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || !cur->pads[(size_t) pad].valid) return;
    auto r = std::make_shared<ds::Result>(*cur);
    if (!assignType(*r, pad, type)) return;
    r->buildNoteMap();
    const auto loop = getLoopFile(); const auto kit = getKitDir();
    publish(r, kit, loop, orphan());
    if (kit == juce::File() && loop.existsAsFile()) ds::saveCache(loop, *r);      // the loop opens like this next time
}

void DrumSlicerProcessor::mergePad(int from, int into)
{
    auto cur = getResult();
    if (!cur || from == into || from < 0 || into < 0 || from >= ds::kPads || into >= ds::kPads) return;
    if (!cur->pads[(size_t) from].valid || !cur->pads[(size_t) into].valid) return;
    auto r = std::make_shared<ds::Result>(*cur);
    // the hits keep about their loudness: velocity scaled by how loud the two sounds are (first 100 ms), but
    // at most x0.6 .. x1.6 - a quiet hat merged into a kick must not turn into notes of velocity 1
    auto e100 = [&](const juce::AudioBuffer<float>& b) { double e = 0; const int n = std::min(b.getNumSamples(), (int) (0.1 * r->sr));
        for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < n; ++i) e += (double) b.getSample(c, i) * b.getSample(c, i); return e; };
    const double ef = e100(r->pads[(size_t) from].audio), ei = e100(r->pads[(size_t) into].audio);
    const double k = ei > 0 ? juce::jlimit(0.6, 1.6, std::sqrt(ef / ei)) : 1.0;
    for (auto& h : r->hits) if (h.pad == from) { h.pad = into; h.velocity = juce::jlimit(1, 127, (int) std::lround(h.velocity * k)); }
    std::sort(r->hits.begin(), r->hits.end(), [](const ds::Hit& a, const ds::Hit& b) { return a.time < b.time; });
    r->pads[(size_t) into].hits += r->pads[(size_t) from].hits;
    r->pads[(size_t) from] = ds::Pad();
    r->buildNoteMap();
    const auto loop = getLoopFile(); const auto kit = getKitDir();
    publish(r, kit, loop, orphan());
    if (kit == juce::File() && loop.existsAsFile()) ds::saveCache(loop, *r);
}

// an empty pad gets a sound of yours: no hits in the loop, you play it (MIDI note of its instrument)
juce::String DrumSlicerProcessor::guessType(const juce::File& f)
{
    const auto n = f.getFileNameWithoutExtension().toLowerCase();
    auto has = [&](std::initializer_list<const char*> w) { for (auto* x : w) if (n.contains(x)) return true; return false; };
    if (has({ "kick", "kik", "808 bd", "bassdrum", "bass drum" })) return "KICK";
    if (has({ "snare", "snr" })) return "SNARE";
    if (has({ "clap" })) return "CLAP";
    if (has({ "rim", "stick" })) return "RIM";
    if (has({ "tom" })) return "TOM";
    if (has({ "shak", "maraca", "tamb" })) return "SHAKER";
    if (has({ "open", "ohh", "ohat", "o-hat", "o_hat" })) return "O-HAT";
    if (has({ "hat", "hh", "hihat" })) return "C-HAT";
    if (has({ "crash", "cym" })) return "CRASH";
    if (has({ "ride" })) return "RIDE";
    return "PERC";
}

bool DrumSlicerProcessor::addPad(int pad, const juce::String& type, const juce::File& f)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || cur->pads[(size_t) pad].valid) return false;
    const ds::Pad none;
    juce::AudioBuffer<float> shot;
    if (!ds::loadUserShot(f, none, cur->sr, shot)) { setStatus("COULD NOT READ " + f.getFileName()); return false; }
    auto r = std::make_shared<ds::Result>(*cur);
    auto& pd = r->pads[(size_t) pad];
    pd = ds::Pad();
    pd.audio.makeCopyOf(shot); pd.user = std::move(shot); pd.userName = f.getFileNameWithoutExtension(); pd.userPath = f.getFullPathName();
    pd.hits = 0; pd.valid = true;
    if (!assignType(*r, pad, type.isNotEmpty() ? type : guessType(f))) assignType(*r, pad, "PERC");
    r->buildNoteMap();
    const auto loop = getLoopFile(); const auto kit = getKitDir();
    publish(r, kit, loop, orphan());
    if (kit == juce::File() && loop.existsAsFile()) ds::saveCache(loop, *r);
    for (int s = 0; s < 3; ++s) resetShape(pad, s);  // a new pad: nothing of the old one
    matchOff(pad);
    setPadSource(pad, 2);
    audition(pad);
    return true;
}

// ---------------------------------------------------------------- clear / delete a pad
bool DrumSlicerProcessor::padIsSilent(int pad) const
{
    auto r = getResult();
    if (!r || pad < 0 || pad >= ds::kPads || !r->pads[(size_t) pad].valid) return false;
    const auto& pd = r->pads[(size_t) pad];
    return pd.user.getNumSamples() == 0 && pd.audio.getMagnitude(0, pd.audio.getNumSamples()) < 1.0e-7f;
}

void DrumSlicerProcessor::clearPadSound(int pad)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || !cur->pads[(size_t) pad].valid) return;
    auto r = std::make_shared<ds::Result>(*cur);
    auto& pd = r->pads[(size_t) pad];
    pd.audio.setSize(2, 64); pd.audio.clear();       // silent: the hits stay in the MIDI and wait for a new sound
    pd.best.setSize(2, 64); pd.best.clear();
    pd.user.setSize(0, 0); pd.userName.clear(); pd.userPath.clear();
    matchOff(pad);
    publish(r, getKitDir(), getLoopFile(), orphan());
    for (int s = 0; s < 3; ++s) resetShape(pad, s);
    setPadSource(pad, 0);
    const auto loop = getLoopFile();
    if (getKitDir() == juce::File() && loop.existsAsFile()) ds::saveCache(loop, *r);
}

void DrumSlicerProcessor::deletePad(int pad)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || !cur->pads[(size_t) pad].valid) return;
    auto r = std::make_shared<ds::Result>(*cur);
    r->hits.erase(std::remove_if(r->hits.begin(), r->hits.end(), [pad](const ds::Hit& h) { return h.pad == pad; }), r->hits.end());
    r->pads[(size_t) pad] = ds::Pad();
    r->buildNoteMap();
    matchOff(pad);
    publish(r, getKitDir(), getLoopFile(), orphan());
    for (int s = 0; s < 3; ++s) resetShape(pad, s);
    setPadSource(pad, 0);
    const auto loop = getLoopFile();
    if (getKitDir() == juce::File() && loop.existsAsFile()) ds::saveCache(loop, *r);
}

// ---------------------------------------------------------------- TRY IN KIT
bool DrumSlicerProcessor::tryOnPad(int pad, const juce::File& f)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || !cur->pads[(size_t) pad].valid) return false;
    if (trying.on && trying.pad != pad) endTry(false);
    if (!trying.on)
    {   // what the pad had, to go back to
        const auto& pd = cur->pads[(size_t) pad];
        trying.on = true; trying.pad = pad; trying.user.makeCopyOf(pd.user); trying.name = pd.userName; trying.path = pd.userPath;
        trying.src = padSource(pad);
        trying.wasPlaying = midiPlay.load();
        trying.match = getMatch(pad);
        {
            std::lock_guard<std::mutex> g(slotLock);
            trying.cslot = slots[(size_t) pad][2];
            if (slotOwner[(size_t) pad] == 2) { trying.cslot.valid = true; trying.cslot.v = shownShape(pad); }
        }
    }
    juce::AudioBuffer<float> shot;
    if (!ds::loadUserShot(f, cur->pads[(size_t) pad], cur->sr, shot)) return false;
    auto r = std::make_shared<ds::Result>(*cur);
    r->pads[(size_t) pad].user = std::move(shot);
    r->pads[(size_t) pad].userName = f.getFileNameWithoutExtension();
    r->pads[(size_t) pad].userPath = f.getFullPathName();
    publish(r, getKitDir(), getLoopFile(), orphan());
    resetShape(pad, 2);
    matchOff(pad);
    setPadSource(pad, 2);
    trying.file = f;
    if (!midiPlay.load()) playMidi(true);            // heard in the pattern, at the project tempo
    return true;
}

void DrumSlicerProcessor::endTry(bool keep)
{
    if (!trying.on) return;
    const int pad = trying.pad;
    trying.on = false;
    if (!trying.wasPlaying) playMidi(false);
    if (keep) return;
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || !cur->pads[(size_t) pad].valid) return;
    auto r = std::make_shared<ds::Result>(*cur);
    auto& pd = r->pads[(size_t) pad];
    pd.user = std::move(trying.user); pd.userName = trying.name; pd.userPath = trying.path;
    publish(r, getKitDir(), getLoopFile(), orphan());
    {
        std::lock_guard<std::mutex> g(slotLock);
        slots[(size_t) pad][2] = trying.cslot;
    }
    { std::lock_guard<std::mutex> g(matchLock); match[(size_t) pad] = trying.match; ++matchVer[(size_t) pad]; }
    if (slotOwner[(size_t) pad] == 2 && trying.cslot.valid) showShape(pad, trying.cslot.v);
    setPadSource(pad, pd.user.getNumSamples() > 0 ? trying.src : std::min(trying.src, 1));
}

// ---------------------------------------------------------------- MATCH TO A
static ShapeParams shapeFrom(const std::array<float, DrumSlicerProcessor::kShapeN>& v)
{   // st en atk fic fo foc hold dcy punch sus pitch tone
    ShapeParams p;
    p.start = v[0]; p.end = v[1]; p.attack = v[2]; p.attackCurve = v[3]; p.fadeOut = v[4]; p.fadeOutCurve = v[5];
    p.hold = v[6]; p.decay = v[7]; p.punch = v[8]; p.sustain = v[9]; p.tone = v[11];
    return p;
}
// resample: the sound played at "rate" times its speed (rate 2 = an octave up, half as long)
static juce::AudioBuffer<float> atRate(const juce::AudioBuffer<float>& sh, double rate)
{
    if (std::abs(rate - 1.0) < 1e-4) { juce::AudioBuffer<float> c; c.makeCopyOf(sh); return c; }
    const int n = std::max(1, (int) (sh.getNumSamples() / rate));
    juce::AudioBuffer<float> o(2, n);
    for (int c = 0; c < 2; ++c)
    {
        const float* s = sh.getReadPointer(std::min(c, sh.getNumChannels() - 1)); float* d = o.getWritePointer(c);
        for (int i = 0; i < n; ++i) { const double p = i * rate; const int i0 = (int) p; const float fr = (float) (p - i0);
            d[i] = i0 + 1 < sh.getNumSamples() ? s[i0] + (s[i0 + 1] - s[i0]) * fr : (i0 < sh.getNumSamples() ? s[i0] : 0.f); }
    }
    return o;
}

// everything MATCH TO A needs, measured off the message thread.
// The target is A as heard; where B (the best real hit, others subtracted) is there too, only what BOTH have
// counts as the sound (the bleed of other drums in A - hats in a snare's tail - is not copied onto your sample).
static MatchData computeMatch(const juce::AudioBuffer<float>& A, const std::array<float, DrumSlicerProcessor::kShapeN>& vA,
                              const juce::AudioBuffer<float>& C, const std::array<float, DrumSlicerProcessor::kShapeN>& neutral,
                              const std::array<float, DrumSlicerProcessor::kShapeN>& vCnow, const MatchData& before, bool hadMatch,
                              double sr, ds::Family fam, const ds::LibIndex& stats, const juce::AudioBuffer<float>* B = nullptr)
{
    MatchData m; m.on = true;
    if (hadMatch) { m.useEq = before.useEq; m.useShape = before.useShape; m.useRoom = before.useRoom; m.usePitch = before.usePitch; m.useLen = before.useLen;
                    m.useGlide = before.useGlide; m.useDrive = before.useDrive; m.useWidth = before.useWidth; }
    auto heard = [&](const juce::AudioBuffer<float>& raw, const std::array<float, DrumSlicerProcessor::kShapeN>& v, const MatchData* md) {
        int off = 0; return atRate(shapeShot(raw, sr, shapeFrom(v), off, md), std::pow(2.0, v[10] / 12.0)); };
    const auto T = heard(A, vA, nullptr);
    ds::Feat fT, fC;
    ds::fingerprint(T, sr, fT); ds::fingerprint(C, sr, fC);
    {
        ds::Feat f; MatchData cur = before; cur.on = hadMatch;
        m.before = ds::fingerprint(heard(C, vCnow, hadMatch ? &cur : nullptr), sr, f) ? ds::scoreOf(ds::distance(fT, f, stats, fam)) : 0.f;
    }
    // pitch: to A's note (an octave off counts as near)
    if (fT[ds::fPitchConf] > 0.5f && fC[ds::fPitchConf] > 0.5f)
    {
        float st = (fT[ds::fPitch] - fC[ds::fPitch]) * 12.f;
        while (st > 6.f) st -= 12.f; while (st < -6.f) st += 12.f;
        m.pitch = std::round(st * 10.f) / 10.f;
    }
    const double rate = std::pow(2.0, (m.usePitch ? m.pitch : 0.f) / 12.0);
    // A (and B) in C's own time: C will be played at "rate", so the target is compared slowed down by it
    const auto Tc = atRate(T, 1.0 / rate);
    juce::AudioBuffer<float> TcB;
    const bool haveB = B != nullptr && B->getNumSamples() > 64 && B->getMagnitude(0, B->getNumSamples()) > 1.0e-6f;
    if (haveB) TcB = atRate(heard(*B, vA, nullptr), 1.0 / rate);
    int off = 0;
    juce::AudioBuffer<float> Cw = shapeShot(C, sr, shapeFrom(neutral), off);   // the working copy, step by step
    // 1. PITCH DROP (tonal drums): the pitch curve of C bent to A's; the overall note stays with PITCH
    m.glide.fill(0.f);
    {
        std::array<float, mt::kGlideN> cT {}, cC {};
        const auto pT = mt::pitchTrack(Tc, sr, cT), pC = mt::pitchTrack(Cw, sr, cC);
        std::array<float, mt::kGlideN> semi {}; std::array<bool, mt::kGlideN> ok {}; int nOk = 0;
        for (int k = 0; k < mt::kGlideN; ++k)
            if (cT[(size_t) k] > 0.6f && cC[(size_t) k] > 0.6f && pT[(size_t) k] > 0 && pC[(size_t) k] > 0)
            { semi[(size_t) k] = 12.f * std::log2(pT[(size_t) k] / pC[(size_t) k]); ok[(size_t) k] = true; ++nOk; }
        if (nOk >= 4 && (fam == ds::famKick || fam == ds::famTom || fam == ds::famAny))
        {
            std::vector<float> v; for (int k = 0; k < mt::kGlideN; ++k) if (ok[(size_t) k]) v.push_back(semi[(size_t) k]);
            std::sort(v.begin(), v.end());
            const float med = v[v.size() / 2];                     // the note itself is PITCH's job: only the movement
            for (int k = 0; k < mt::kGlideN; ++k)
            {
                if (!ok[(size_t) k]) { int j = k; while (j >= 0 && !ok[(size_t) j]) --j; if (j < 0) { j = k; while (j < mt::kGlideN && !ok[(size_t) j]) ++j; } semi[(size_t) k] = (j >= 0 && j < mt::kGlideN) ? semi[(size_t) j] : med; }
                m.glide[(size_t) k] = std::clamp(semi[(size_t) k] - med, -7.f, 7.f);
            }
            if (m.useGlide) mt::applyGlide(Cw, sr, m.glide, 1.f);
        }
    }
    // 2. SATURATION: A's attack denser (lower peak / loudness) than C's: round C off until they are alike
    m.driveDb = 0;
    {
        const float crT = mt::crestOf(Tc, sr), crC = mt::crestOf(Cw, sr);
        if (crC - crT > 1.5f)
        {
            float bestErr = crC - crT;
            for (float d : { 1.5f, 3.f, 4.5f, 6.f, 8.f, 10.f, 13.f, 16.f })
            {
                juce::AudioBuffer<float> t; t.makeCopyOf(Cw); mt::applyDrive(t, d, 1.f);
                const float err = std::abs(mt::crestOf(t, sr) - crT);
                if (err < bestErr) { bestErr = err; m.driveDb = d; }
            }
            if (m.useDrive) mt::applyDrive(Cw, m.driveDb, 1.f);
        }
    }
    // 3. EQ: third-octave levels, target minus C, the overall level difference taken out; checked and corrected
    auto LT = mt::bandLevels(Tc, sr);
    float topT = -200; for (float v : LT) topT = std::max(topT, v);
    auto weightedMean = [&](const std::array<float, mt::kBands>& d) { double sum = 0, w = 0;
        for (int k = 3; k <= 23; ++k) { const double wk = std::pow(10.0, (LT[(size_t) k] - topT) / 20.0); sum += d[(size_t) k] * wk; w += wk; }
        return w > 0 ? (float) (sum / w) : 0.f; };
    if (haveB)
    {   // only what A and B both have (each band: the lower of the two, after their levels are lined up)
        auto LB = mt::bandLevels(TcB, sr);
        std::array<float, mt::kBands> d {}; for (int k = 0; k < mt::kBands; ++k) d[(size_t) k] = LB[(size_t) k] - LT[(size_t) k];
        const float off2 = weightedMean(d);
        for (int k = 0; k < mt::kBands; ++k) LT[(size_t) k] = std::min(LT[(size_t) k], LB[(size_t) k] - off2);
        topT = -200; for (float v : LT) topT = std::max(topT, v);
    }
    const auto LC = mt::bandLevels(Cw, sr);
    float topC = -200; for (float v : LC) topC = std::max(topC, v);
    auto diffCurve = [&](const std::array<float, mt::kBands>& lc) {
        std::array<float, mt::kBands> d {}; for (int k = 0; k < mt::kBands; ++k) d[(size_t) k] = LT[(size_t) k] - lc[(size_t) k];
        const float mean = weightedMean(d); for (auto& v : d) v -= mean; return d; };
    auto limit = [&](std::array<float, mt::kBands>& c) {
        std::array<float, mt::kBands> sm = c;
        for (int k = 0; k < mt::kBands; ++k) { float s2 = c[(size_t) k] * 2, ww = 2; if (k > 0) { s2 += c[(size_t) k - 1]; ww += 1; } if (k + 1 < mt::kBands) { s2 += c[(size_t) k + 1]; ww += 1; } sm[(size_t) k] = s2 / ww; }
        for (int k = 0; k < mt::kBands; ++k)
        {
            float v = sm[(size_t) k];
            const bool emptyC = LC[(size_t) k] < topC - 55.f, emptyT = LT[(size_t) k] < topT - 55.f;
            if (v > 0 && (emptyC || emptyT)) v = 0;                  // nothing there to bring up (only noise would grow)
            c[(size_t) k] = std::clamp(v, -18.f, mt::centres()[(size_t) k] < 80.f ? 4.f : 12.f);
        } };
    m.eq = diffCurve(LC); limit(m.eq);
    for (int it = 0; it < 3; ++it)
    {
        juce::AudioBuffer<float> t; t.makeCopyOf(Cw); mt::applyEq(t, sr, m.eq, 1.f);
        const auto res = diffCurve(mt::bandLevels(t, sr));
        for (int k = 0; k < mt::kBands; ++k) m.eq[(size_t) k] += 0.75f * res[(size_t) k];
        limit(m.eq);
    }
    if (m.useEq) mt::applyEq(Cw, sr, m.eq, 1.f);
    // 4. ROOM: added only when the target has clearly more room than C (the target's tail = the cleaner of A / B)
    float roomT, slopeT, roomC, slopeC;
    mt::tailOf(Tc, sr, roomT, slopeT);
    if (haveB) { float rb, sb; mt::tailOf(TcB, sr, rb, sb); if (rb < roomT) { roomT = rb; } slopeT = std::min(slopeT, sb); }
    mt::tailOf(Cw, sr, roomC, slopeC);
    m.roomMix = 0;
    if (roomT > roomC + 3.f)
    {
        m.roomRt = juce::jlimit(0.12f, 1.5f, slopeT < -5.f ? -60.f / slopeT : 0.5f);
        float bestErr = std::abs(roomT - roomC);
        for (float mix : { 0.02f, 0.035f, 0.05f, 0.075f, 0.1f, 0.14f, 0.2f, 0.28f, 0.4f })
        {
            juce::AudioBuffer<float> t; t.makeCopyOf(Cw); mt::addRoom(t, sr, m.roomRt, mix, 2.f);
            float rr, ss; mt::tailOf(t, sr, rr, ss);
            if (std::abs(rr - roomT) < bestErr) { bestErr = std::abs(rr - roomT); m.roomMix = mix; m.roomPre = 2.f; }
        }
        if (m.useRoom && m.roomMix > 0) mt::addRoom(Cw, sr, m.roomRt, m.roomMix, m.roomPre);
    }
    const bool cRoomier = roomC > roomT + 3.f;               // C has more room: its tail may go down further
    // 5. SHAPE: the loudness over time in 8 bands, target minus C, twice checked
    auto ET = mt::envelopes(Tc, sr);
    if (haveB) { const auto EB = mt::envelopes(TcB, sr); for (int k = 0; k < mt::kEnvB * mt::kEnvT; ++k) ET[(size_t) k] = std::min(ET[(size_t) k], EB[(size_t) k]); }
    auto shapeLimit = [&](std::array<float, mt::kEnvB * mt::kEnvT>& g, const std::array<float, mt::kEnvB * mt::kEnvT>& ec) {
        for (int t = 0; t < mt::kEnvT; ++t)
            for (int b = 0; b < mt::kEnvB; ++b)
            {
                auto& v = g[(size_t) (t * mt::kEnvB + b)];
                if (ET[(size_t) (t * mt::kEnvB + b)] < -60.f && ec[(size_t) (t * mt::kEnvB + b)] < -60.f) v = 0;
                v = std::clamp(v, (cRoomier && t >= 4) ? -24.f : -15.f, t >= 6 ? 6.f : 9.f);   // late on: a tail is not pumped up out of noise
            }
        auto sm = g;
        for (int t = 0; t < mt::kEnvT; ++t)
            for (int b = 0; b < mt::kEnvB; ++b)
            {
                float s2 = g[(size_t) (t * mt::kEnvB + b)] * 2, ww = 2;
                if (t > 0) { s2 += g[(size_t) ((t - 1) * mt::kEnvB + b)]; ww += 1; }
                if (t + 1 < mt::kEnvT) { s2 += g[(size_t) ((t + 1) * mt::kEnvB + b)]; ww += 1; }
                sm[(size_t) (t * mt::kEnvB + b)] = s2 / ww;
            }
        g = sm; };
    auto EC = mt::envelopes(Cw, sr);
    for (int k = 0; k < mt::kEnvB * mt::kEnvT; ++k) m.shape[(size_t) k] = ET[(size_t) k] - EC[(size_t) k];
    shapeLimit(m.shape, EC);
    for (int it = 0; it < 2; ++it)
    {
        juce::AudioBuffer<float> t; t.makeCopyOf(Cw); mt::applyShape(t, sr, m.shape, 1.f);
        const auto e2 = mt::envelopes(t, sr);
        for (int k = 0; k < mt::kEnvB * mt::kEnvT; ++k) m.shape[(size_t) k] += 0.7f * (ET[(size_t) k] - e2[(size_t) k]);
        shapeLimit(m.shape, e2);
    }
    if (m.useShape) mt::applyShape(Cw, sr, m.shape, 1.f);
    // 6. WIDTH: side vs mid like A's (a mono hit stays mono, a wide one gets its width)
    {
        const float wT = mt::widthOf(Tc, sr), wC = mt::widthOf(Cw, sr);
        // widening needs some side to start from (a mono hit made "wide" is only its noise made louder)
        m.widthDb = (std::abs(wT - wC) > 1.f && wC > -55.f) ? std::clamp(wT - wC, -24.f, wC > -30.f ? 6.f : 0.f) : 0.f;
        if (m.useWidth) mt::applyWidth(Cw, m.widthDb, 1.f);
    }
    // 7. LENGTH: after 400 ms the shape stops; a longer tail than the target's is ended with a fade
    {
        auto endOf = [&](const juce::AudioBuffer<float>& x) {   // ms after the onset where it stays 45 dB down
            const int n = x.getNumSamples(), on = mt::onsetOf(x), H = std::max(1, (int) (0.002 * sr));
            float pk = 1e-9f; for (int c = 0; c < x.getNumChannels(); ++c) pk = std::max(pk, x.getMagnitude(c, 0, n));
            const float thr = pk * 0.0056f; int last = on;
            for (int i = on; i < n; i += H) if (x.getMagnitude(0, i, std::min(H, n - i)) > thr) last = i;
            return std::make_pair((last - on) / sr * 1000.0, on / sr * 1000.0); };
        auto [endT, onT] = endOf(Tc);
        if (haveB) { const auto eb = endOf(TcB); endT = std::min(endT, eb.first); }
        const auto [endC, onC] = endOf(Cw);
        juce::ignoreUnused(onT);
        if (endC > endT * 1.15 && endT > 20)
        {
            m.en = (float) juce::jlimit(5.0, 2000.0, onC + endT * 1.1);
            m.fo = (float) juce::jlimit(0.0, (double) m.en, endT * 0.5);
        }
    }
    // how near it got (as heard: at its pitch, with its length)
    auto v = neutral; if (m.usePitch) v[10] = m.pitch; if (m.useLen) { v[1] = m.en; v[4] = m.fo; }
    auto scoreAt = [&](const MatchData& md, float amount) {
        int o2 = 0; auto sp = shapeFrom(v); sp.matchAmount = amount;
        ds::Feat f; return ds::fingerprint(atRate(shapeShot(C, sr, sp, o2, &md), std::pow(2.0, v[10] / 12.0)), sr, f) ? ds::scoreOf(ds::distance(fT, f, stats, fam)) : 0.f; };
    auto scoreOf = [&](const MatchData& md) { return scoreAt(md, 1.f); };
    m.after = scoreOf(m);
    // each optional step stays only if it really brings the sound nearer (else it is left out, not just switched off)
    for (int step = 0; step < 4; ++step)
    {
        MatchData t = m;
        if (step == 0) { bool any = false; for (float g : t.glide) any = any || std::abs(g) > 0.05f; if (!any) continue; t.glide.fill(0.f); }
        if (step == 1) { if (t.driveDb <= 0.05f) continue; t.driveDb = 0; }
        if (step == 2) { if (t.roomMix <= 0) continue; t.roomMix = 0; }
        if (step == 3) { if (std::abs(t.widthDb) <= 0.05f) continue; t.widthDb = 0; }
        const float s2 = scoreOf(t);
        if (s2 > m.after + 0.3f) { m = t; m.after = s2; }
    }
    // how much of it: sometimes a part of the match is nearer than all of it
    m.bestAmount = 1.f;
    for (float amt : { 0.5f, 0.75f })
    {
        const float s2 = scoreAt(m, amt);
        if (s2 > m.after + 0.3f) { m.after = s2; m.bestAmount = amt; }
    }
    return m;
}

void DrumSlicerProcessor::startMatch(int pad)
{
    auto r = getResult();
    if (!r || pad < 0 || pad >= ds::kPads || !r->pads[(size_t) pad].valid || r->pads[(size_t) pad].user.getNumSamples() == 0 || matching.load()) return;
    const auto& pd = r->pads[(size_t) pad];
    if (pd.audio.getMagnitude(0, pd.audio.getNumSamples()) < 1.0e-6f)
    { std::lock_guard<std::mutex> g(matchLock); matchMsg = "A HAS NO SOUND: NOTHING TO MATCH TO"; return; }
    setPadSource(pad, 2);                            // C is the one shown (and shaped)
    std::array<float, kShapeN> vA = shownShape(pad), neutral {};
    for (int k = 0; k < kShapeN; ++k)
        if (auto* p = apvts.getParameter(juce::String(shapeIds()[k]) + juce::String(pad))) neutral[(size_t) k] = p->convertFrom0to1(p->getDefaultValue());
    { std::lock_guard<std::mutex> g(slotLock); if (slots[(size_t) pad][0].valid) vA = slots[(size_t) pad][0].v; }
    const auto vCnow = shownShape(pad);
    const auto prev = getMatch(pad);
    auto ix = library->index();
    auto stats = std::make_shared<ds::LibIndex>();
    if (ix && ix->oneShots >= 50) { stats->mean = ix->mean; stats->sd = ix->sd; stats->oneShots = ix->oneShots; } else stats->stats();
    auto A = std::make_shared<juce::AudioBuffer<float>>(); A->makeCopyOf(pd.audio);
    auto C = std::make_shared<juce::AudioBuffer<float>>(); C->makeCopyOf(pd.user);
    auto Bb = std::make_shared<juce::AudioBuffer<float>>(); Bb->makeCopyOf(pd.best);
    const double sr = r->sr; const auto fam = ds::familyOf(pd.name);
    matching = true;
    { std::lock_guard<std::mutex> g(matchLock); matchMsg = "MATCHING ..."; }
    std::thread([this, a = alive, A, Bb, C, vA, neutral, vCnow, prev, stats, sr, fam, pad]
    {
        WorkerScope ws;
        auto m = computeMatch(*A, vA, *C, neutral, vCnow, prev, prev.on, sr, fam, *stats, Bb.get());
        juce::MessageManager::callAsync([this, a, m, pad, neutral]
        {
            if (!a->load()) return;
            matching = false;
            auto r2 = getResult();
            if (!r2 || !r2->pads[(size_t) pad].valid || r2->pads[(size_t) pad].user.getNumSamples() == 0) return;
            if (m.after <= m.before + 0.5f)
            {   // a match would not bring it nearer: nothing changes
                std::lock_guard<std::mutex> g(matchLock);
                matchMsg = "ALREADY AS NEAR AS IT GETS  (" + juce::String(juce::roundToInt(m.before)) + "%)";
                return;
            }
            setPadSource(pad, 2);
            { std::lock_guard<std::mutex> g(matchLock); match[(size_t) pad] = m; ++matchVer[(size_t) pad];
              matchMsg = "MATCHED  " + juce::String(juce::roundToInt(m.before)) + "% -> " + juce::String(juce::roundToInt(m.after)) + "%"; }
            // the knobs it sets (you can still turn them): pitch, length; the amount back to 100 %
            auto v = shownShape(pad);
            v[10] = m.usePitch ? m.pitch : neutral[10];
            v[1] = m.useLen ? m.en : neutral[1]; v[4] = m.useLen ? m.fo : neutral[4];
            v[11] = neutral[11]; v[8] = neutral[8]; v[9] = neutral[9];   // tone / punch / sustain: the match does that now
            showShape(pad, v);
            if (auto* p = apvts.getParameter("mamt" + juce::String(pad))) p->setValueNotifyingHost(p->convertTo0to1(100.f * m.bestAmount));
            audition(pad);
        });
    }).detach();
}

std::shared_ptr<DrumSlicerProcessor::RankState> DrumSlicerProcessor::startRank(int pad, int src, const std::vector<juce::String>& paths)
{
    auto st = std::make_shared<RankState>();
    st->paths = paths; st->after.assign(paths.size(), -1.f);
    auto r = getResult();
    if (!r || pad < 0 || pad >= ds::kPads || !r->pads[(size_t) pad].valid || paths.empty()) { st->finished = true; return st; }
    const auto& pd = r->pads[(size_t) pad];
    auto T = std::make_shared<juce::AudioBuffer<float>>(); T->makeCopyOf(pd.get(src == 1, src == 2));
    auto Tb = std::make_shared<juce::AudioBuffer<float>>(); if (src == 0) Tb->makeCopyOf(pd.best);   // A: its clean part with B
    if (T->getMagnitude(0, T->getNumSamples()) < 1.0e-6f) { st->finished = true; return st; }
    std::array<float, kShapeN> vT = shownShape(pad), neutral {};
    for (int k = 0; k < kShapeN; ++k)
        if (auto* p = apvts.getParameter(juce::String(shapeIds()[k]) + juce::String(pad))) neutral[(size_t) k] = p->convertFrom0to1(p->getDefaultValue());
    {
        std::lock_guard<std::mutex> g(slotLock);
        const int own = slotOwner[(size_t) pad];
        if (own != src) vT = slots[(size_t) pad][(size_t) src].valid ? slots[(size_t) pad][(size_t) src].v : vT;
    }
    auto ref = std::make_shared<ds::Pad>(); ref->audio.makeCopyOf(pd.audio); ref->valid = true;   // the level a sample is brought to
    auto ix = library->index();
    auto stats = std::make_shared<ds::LibIndex>();
    if (ix && ix->oneShots >= 50) { stats->mean = ix->mean; stats->sd = ix->sd; stats->oneShots = ix->oneShots; } else stats->stats();
    const double sr = r->sr; const auto fam = ds::familyOf(pd.name);
    {
        std::lock_guard<std::mutex> g(rankLock);
        ranks.erase(std::remove_if(ranks.begin(), ranks.end(), [](const std::weak_ptr<RankState>& w) { return w.expired(); }), ranks.end());
        ranks.push_back(st);
    }
    std::thread([st, T, Tb, vT, neutral, ref, stats, sr, fam]
    {
        WorkerScope ws;
        for (size_t i = 0; i < st->paths.size() && !st->cancel.load(); ++i)
        {
            juce::AudioBuffer<float> c;
            float score = 0.f;
            if (ds::loadUserShot(juce::File(st->paths[i]), *ref, sr, c))
                score = computeMatch(*T, vT, c, neutral, neutral, MatchData(), false, sr, fam, *stats, Tb.get()).after;
            { std::lock_guard<std::mutex> g(st->lock); st->after[i] = score; }
            ++st->done;
        }
        st->finished = true;
    }).detach();
    return st;
}

void DrumSlicerProcessor::setMatchPart(int pad, int part, bool on)
{
    if (pad < 0 || pad >= ds::kPads) return;
    MatchData m;
    {
        std::lock_guard<std::mutex> g(matchLock);
        auto& md = match[(size_t) pad];
        if (!md.on) return;
        if (part == partEq) md.useEq = on; else if (part == partShape) md.useShape = on; else if (part == partRoom) md.useRoom = on;
        else if (part == partPitch) md.usePitch = on; else if (part == partLen) md.useLen = on;
        else if (part == partGlide) md.useGlide = on; else if (part == partDrive) md.useDrive = on; else if (part == partWidth) md.useWidth = on;
        ++matchVer[(size_t) pad]; m = md;
    }
    if (part == partPitch || part == partLen)
    {   // these live on the knobs
        auto v = shownShape(pad);
        if (part == partPitch) v[10] = on ? m.pitch : 0.f;
        else { v[1] = on ? m.en : 2000.f; v[4] = on ? m.fo : 0.f; }
        showShape(pad, v);
    }
    audition(pad);
}

void DrumSlicerProcessor::removeMatch(int pad)
{
    if (pad < 0 || pad >= ds::kPads) return;
    MatchData m;
    { std::lock_guard<std::mutex> g(matchLock); m = match[(size_t) pad]; match[(size_t) pad].on = false; ++matchVer[(size_t) pad]; matchMsg.clear(); }
    if (m.on)
    {
        auto v = shownShape(pad);
        if (m.usePitch) v[10] = 0.f;
        if (m.useLen) { v[1] = 2000.f; v[4] = 0.f; }
        showShape(pad, v);
    }
    audition(pad);
}

void DrumSlicerProcessor::matchOff(int pad)
{
    if (pad < 0 || pad >= ds::kPads) return;
    std::lock_guard<std::mutex> g(matchLock);
    if (match[(size_t) pad].on) { match[(size_t) pad].on = false; ++matchVer[(size_t) pad]; }
}

juce::ValueTree DrumSlicerProcessor::matchTree() const
{
    juce::ValueTree t("MATCH");
    std::lock_guard<std::mutex> g(matchLock);
    for (int p = 0; p < ds::kPads; ++p)
    {
        const auto& m = match[(size_t) p];
        if (!m.on) continue;
        juce::StringArray a;
        a.add(juce::String((m.useEq ? 1 : 0) | (m.useShape ? 2 : 0) | (m.useRoom ? 4 : 0) | (m.usePitch ? 8 : 0) | (m.useLen ? 16 : 0)
                           | (m.useGlide ? 32 : 0) | (m.useDrive ? 64 : 0) | (m.useWidth ? 128 : 0)));
        for (float v : m.eq) a.add(juce::String(v, 3));
        for (float v : m.shape) a.add(juce::String(v, 3));
        for (float v : { m.roomRt, m.roomMix, m.roomPre, m.pitch, m.en, m.fo, m.before, m.after }) a.add(juce::String(v, 4));
        for (float v : m.glide) a.add(juce::String(v, 3));
        a.add(juce::String(m.driveDb, 3)); a.add(juce::String(m.widthDb, 3));
        t.setProperty("p" + juce::String(p), a.joinIntoString(","), nullptr);
    }
    return t;
}

void DrumSlicerProcessor::matchFromTree(const juce::ValueTree& t)
{
    std::lock_guard<std::mutex> g(matchLock);
    for (int p = 0; p < ds::kPads; ++p)
    {
        auto& m = match[(size_t) p];
        m = MatchData(); ++matchVer[(size_t) p];
        if (!t.isValid()) continue;
        juce::StringArray a; a.addTokens(t.getProperty("p" + juce::String(p)).toString(), ",", "");
        const int base = 1 + mt::kBands + mt::kEnvB * mt::kEnvT + 8, full = base + mt::kGlideN + 2;
        if (a.size() != base && a.size() != full) continue;   // (1.11: no pitch drop / saturation / width yet)
        int i = 0;
        const int fl = a[i++].getIntValue();
        m.useEq = fl & 1; m.useShape = fl & 2; m.useRoom = fl & 4; m.usePitch = fl & 8; m.useLen = fl & 16;
        m.useGlide = a.size() == full ? (fl & 32) != 0 : true; m.useDrive = a.size() == full ? (fl & 64) != 0 : true; m.useWidth = a.size() == full ? (fl & 128) != 0 : true;
        for (auto& v : m.eq) v = a[i++].getFloatValue();
        for (auto& v : m.shape) v = a[i++].getFloatValue();
        for (float* v : { &m.roomRt, &m.roomMix, &m.roomPre, &m.pitch, &m.en, &m.fo, &m.before, &m.after }) *v = a[i++].getFloatValue();
        if (a.size() == full) { for (auto& v : m.glide) v = a[i++].getFloatValue(); m.driveDb = a[i++].getFloatValue(); m.widthDb = a[i++].getFloatValue(); }
        m.on = true;
    }
}

// ---------------------------------------------------------------- your samples ("C")
bool DrumSlicerProcessor::setPadSample(int pad, const juce::File& f)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads || !cur->pads[(size_t) pad].valid) return false;
    juce::AudioBuffer<float> shot;
    if (!ds::loadUserShot(f, cur->pads[(size_t) pad], cur->sr, shot)) { setStatus("COULD NOT READ " + f.getFileName()); return false; }
    auto r = std::make_shared<ds::Result>(*cur);
    r->pads[(size_t) pad].user = std::move(shot);
    r->pads[(size_t) pad].userName = f.getFileNameWithoutExtension();
    r->pads[(size_t) pad].userPath = f.getFullPathName();
    publish(r, getKitDir(), getLoopFile(), orphan());
    resetShape(pad, 2);                              // a new sample of yours starts unshaped
    matchOff(pad);
    setPadSource(pad, 2);
    audition(pad);
    return true;
}

void DrumSlicerProcessor::clearPadSample(int pad)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads) return;
    auto r = std::make_shared<ds::Result>(*cur);
    r->pads[(size_t) pad].user.setSize(0, 0); r->pads[(size_t) pad].userName.clear(); r->pads[(size_t) pad].userPath.clear();
    matchOff(pad);
    publish(r, getKitDir(), getLoopFile(), orphan());
    setPadSource(pad, 0);
}

void DrumSlicerProcessor::setPadSource(int pad, int src)
{
    switchSlot(pad, src);                            // the knobs now show this source's own shape
    if (auto* p = apvts.getParameter("best" + juce::String(pad))) p->setValueNotifyingHost(src == 1 ? 1.f : 0.f);
    if (auto* p = apvts.getParameter("usec" + juce::String(pad))) p->setValueNotifyingHost(src == 2 ? 1.f : 0.f);
}

int DrumSlicerProcessor::padSource(int pad) const
{
    auto r = getResult();
    const bool hasC = r && pad >= 0 && pad < ds::kPads && r->pads[(size_t) pad].user.getNumSamples() > 0;
    if (hasC && apvts.getRawParameterValue("usec" + juce::String(pad))->load() > 0.5f) return 2;
    return apvts.getRawParameterValue("best" + juce::String(pad))->load() > 0.5f ? 1 : 0;
}

// ---------------------------------------------------------------- kits
bool DrumSlicerProcessor::previewSample(const juce::File& f, int pad)
{
    auto r = getResult();
    const ds::Pad none;
    const ds::Pad& pd = (r && pad >= 0 && pad < ds::kPads && r->pads[(size_t) pad].valid) ? r->pads[(size_t) pad] : none;
    const double sr = r ? r->sr : hostSR;
    auto b = std::make_shared<juce::AudioBuffer<float>>();
    if (!ds::loadUserShot(f, pd, sr, *b)) return false;          // as loud as the pad, like it would be as "C"
    std::shared_ptr<juce::AudioBuffer<float>> old;
    { juce::SpinLock::ScopedLockType l(lock); old = preview; preview = b; previewSR = sr; }
    retire(std::move(old));
    { juce::SpinLock::ScopedLockType l(fileLock); previewDir = f; }
    srcPlay = false;
    previewStop = false; previewPlaying = true;
    return true;
}

bool DrumSlicerProcessor::previewKit(const juce::File& kit)
{
    auto b = std::make_shared<juce::AudioBuffer<float>>(); double sr = 44100;
    if (!ds::renderKitPreview(kit, *b, sr)) return false;
    std::shared_ptr<juce::AudioBuffer<float>> old;
    { juce::SpinLock::ScopedLockType l(lock); old = preview; preview = b; previewSR = sr; }
    retire(std::move(old));
    { juce::SpinLock::ScopedLockType l(fileLock); previewDir = kit; }
    srcPlay = false; midiPlay = false;
    previewStop = false; previewPlaying = true;
    return true;
}

// written to a temporary folder first; an existing kit of that name is only replaced once the new one is complete
juce::File DrumSlicerProcessor::saveKit(const juce::String& rawName)
{
    auto r = getResult();
    const auto name = juce::File::createLegalFileName(rawName.trim());
    if (!r || name.isEmpty()) return {};
    const auto rootDir = ds::kitsRoot();
    if (!rootDir.createDirectory()) { setStatus("COULD NOT CREATE " + rootDir.getFullPathName()); return {}; }
    const auto kit = rootDir.getChildFile(name), tmp = rootDir.getChildFile(".saving_" + name), old = rootDir.getChildFile(".old_" + name);
    tmp.deleteRecursively(); old.deleteRecursively();
    bool ok = tmp.createDirectory() && ds::saveKitData(tmp, *r);
    // the settings (shape, A / B, volume, pan, pitch, MIDI mode) without the window size or file paths
    auto st = apvts.copyState();
    for (auto id : { "loopPath", "kitPath", "uiWidth" }) st.removeProperty(id, nullptr);
    st.appendChild(slotsTree(), nullptr);
    st.appendChild(matchTree(), nullptr);
    if (auto xml = st.createXml()) ok = ok && ds::writeText(ds::kitData(tmp).getChildFile("settings.xml"), xml->toString());
    // ready-to-use files next to it: the sounds as you hear them + both MIDI versions
    auto sh = getShaped();
    for (int p = 0; p < ds::kPads && ok; ++p)
    {
        const auto& pd = r->pads[(size_t) p];
        if (!pd.valid) continue;
        const auto f = tmp.getChildFile(name + "_" + pd.name.removeCharacters(" ") + ".wav");
        ok = (sh && sh->buf[(size_t) p].getNumSamples() > 0) ? ds::writeWav24(f, sh->buf[(size_t) p], r->sr) : ds::writeWav24(f, pd.audio, r->sr);
    }
    for (int t = 0; t < 3 && ok; ++t)
    {
        const auto bytes = ds::midiBytes(*r, midiBpm(), t, apvts.getRawParameterValue("quant")->load());
        ok = ds::writeData(tmp.getChildFile(name + ds::midiSuffix(t) + ".mid"), bytes.getData(), bytes.getSize());
    }
    if (ok && kit.exists()) ok = kit.moveFileTo(old);
    if (ok) ok = tmp.moveFileTo(kit);
    if (!ok)
    {
        if (!kit.exists() && old.exists()) old.moveFileTo(kit);     // put the previous kit back
        tmp.deleteRecursively();
        setStatus("COULD NOT SAVE THE KIT");
        return {};
    }
    old.deleteRecursively();
    setFiles(kit, ds::kitLoop(kit));
    setStatus("KIT SAVED");
    return kit;
}

void DrumSlicerProcessor::loadKit(const juce::File& kit, bool applySettings)
{
    if (!ds::isKit(kit)) return;
    stopWorker();
    const int token = ++jobToken;
    busy = true; progress = 0.5f; setStatus("LOADING KIT...");
    worker = std::thread([this, kit, applySettings, token]
    {
        auto r = ds::loadKitData(kit);
        if (token != jobToken.load()) { busy = false; return; }
        if (r == nullptr) { setStatus("COULD NOT LOAD THIS KIT"); busy = false; return; }
        std::unique_ptr<juce::XmlElement> xml;
        if (applySettings) xml = juce::parseXML(ds::kitData(kit).getChildFile("settings.xml"));
        if (xml == nullptr) { publish(r, kit, ds::kitLoop(kit)); setStatus("KIT LOADED"); busy = false; return; }
        // its settings first, then its sounds: the new kit never plays with the old kit's shaping
        auto vt = std::make_shared<juce::ValueTree>(juce::ValueTree::fromXml(*xml));
        juce::MessageManager::callAsync([this, vt, r, kit, token, a = alive]
        {
            if (!a->load()) return;
            if (token != jobToken.load()) return;                        // a newer job owns "busy" now
            for (int i = 0; i < vt->getNumChildren(); ++i)
            {
                const auto c = vt->getChild(i);
                if (!c.hasProperty("id") || !c.hasProperty("value")) continue;
                const auto id = c.getProperty("id").toString();
                if (id == "master" || id == "sens") continue;                // your mix level stays
                if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1((float) c.getProperty("value")));
            }
            slotsFromTree(vt->getChildWithName("SLOTS"));          // the kit's other A / B / C shapes
            matchFromTree(vt->getChildWithName("MATCH"));
            publish(r, kit, ds::kitLoop(kit));
            setStatus("KIT LOADED");
            busy = false;
        });
    });
}

juce::AudioProcessorEditor* DrumSlicerProcessor::createEditor() { return new DrumSlicerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumSlicerProcessor(); }
