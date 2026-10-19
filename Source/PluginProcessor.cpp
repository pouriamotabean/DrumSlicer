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
    reshape();
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
    for (int i = 0; i < ds::kPads; ++i)
        if (r->pads[(size_t) i].valid)
            sh->buf[(size_t) i] = shapeShot(r->pads[(size_t) i].get(now[(size_t) i].best, now[(size_t) i].user), r->sr, now[(size_t) i], sh->offset[(size_t) i]);
    std::shared_ptr<Shaped> old;
    { juce::SpinLock::ScopedLockType l(lock); old = shaped; shaped = sh; }
    retire(std::move(old));
    lastShape = now; lastShapedVersion = resultVersion.load();
}

DrumSlicerProcessor::~DrumSlicerProcessor()
{
    *alive = false;
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
    if (on) { stopPreview(); auto s = getSource(); if (!s) return; }
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
    if (k >= (int) it->second.size()) k = 0;
    r.pads[(size_t) pad].note = it->second[(size_t) k];
    r.pads[(size_t) pad].name = k == 0 ? type : type + " " + juce::String(k + 1);
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
    pd.audio.makeCopyOf(shot); pd.user = std::move(shot); pd.userName = f.getFileNameWithoutExtension();
    pd.hits = 0; pd.valid = true;
    if (!assignType(*r, pad, type.isNotEmpty() ? type : guessType(f))) assignType(*r, pad, "PERC");
    r->buildNoteMap();
    const auto loop = getLoopFile(); const auto kit = getKitDir();
    publish(r, kit, loop, orphan());
    if (kit == juce::File() && loop.existsAsFile()) ds::saveCache(loop, *r);
    setPadSource(pad, 2);
    audition(pad);
    return true;
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
    publish(r, getKitDir(), getLoopFile(), orphan());
    setPadSource(pad, 2);
    audition(pad);
    return true;
}

void DrumSlicerProcessor::clearPadSample(int pad)
{
    auto cur = getResult();
    if (!cur || pad < 0 || pad >= ds::kPads) return;
    auto r = std::make_shared<ds::Result>(*cur);
    r->pads[(size_t) pad].user.setSize(0, 0); r->pads[(size_t) pad].userName.clear();
    publish(r, getKitDir(), getLoopFile(), orphan());
    setPadSource(pad, 0);
}

void DrumSlicerProcessor::setPadSource(int pad, int src)
{
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
    srcPlay = false;
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
            publish(r, kit, ds::kitLoop(kit));
            setStatus("KIT LOADED");
            busy = false;
        });
    });
}

juce::AudioProcessorEditor* DrumSlicerProcessor::createEditor() { return new DrumSlicerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DrumSlicerProcessor(); }
