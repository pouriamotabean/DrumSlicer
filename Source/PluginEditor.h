#pragma once
#include "PluginProcessor.h"
#ifndef HAS_ASSETS
 #define HAS_ASSETS 0
#endif
#if HAS_ASSETS
 #include <BinaryData.h>
#endif

// =====================================================================================================
//  DRUM SLICER UI  -  dark graphite, one amber accent.   by Pouria Motabean
//  1000 x 600.  Text, waveforms, arcs and buttons are drawn in code; PNG skin (optional) in /Assets:
//  background.png, knob.png, pad_off.png, pad_on.png (pad_on is tinted to each pad's colour).
// =====================================================================================================
namespace ui
{
static const juce::Colour amber(0xffffb347), text(0xffe9e6e1), dim(0xff8a8f98), faint(0xff4a4e56),
                          panelEdge(0xff262930), well(0xff0c0d10);

inline juce::Colour padColour(const juce::String& name)
{
    if (name.startsWith("KICK"))  return juce::Colour(0xffffb347);
    if (name.startsWith("SNARE")) return juce::Colour(0xff3da5ff);
    if (name.startsWith("CLAP"))  return juce::Colour(0xff2ed47a);
    if (name.startsWith("C-HAT") || name.startsWith("HAT")) return juce::Colour(0xff2ec4d6);
    if (name.startsWith("O-HAT")) return juce::Colour(0xffa66bff);
    if (name.startsWith("CRASH") || name.startsWith("RIDE")) return juce::Colour(0xffe8d44d);
    return juce::Colour(0xffff6b8a);
}

inline juce::Font font(float size, bool bold = true, float kerning = 0.f)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor(kerning));
}

struct Skin
{
    juce::Image background, knob, padOff, padOn;
    std::map<juce::uint32, juce::Image> tinted;
    Skin()
    {
#if HAS_ASSETS
        auto ld = [](const char* n) { int sz = 0; auto* d = BinaryData::getNamedResource(n, sz);
                                      return d ? juce::ImageFileFormat::loadFrom(d, (size_t) sz) : juce::Image(); };
        background = ld("background_png"); knob = ld("knob_png"); padOff = ld("pad_off_png"); padOn = ld("pad_on_png");
#endif
    }
    // lit pad = off + (on - off) * colour   (the glow of pad_on.png is neutral, so any colour works)
    const juce::Image& litPad(juce::Colour c)
    {
        auto& im = tinted[c.getARGB()];
        if (im.isNull() && padOn.isValid() && padOff.isValid() && padOn.getBounds() == padOff.getBounds())
        {
            im = padOff.createCopy().convertedToFormat(juce::Image::ARGB);
            juce::Image::BitmapData on(padOn, juce::Image::BitmapData::readOnly), out(im, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < im.getHeight(); ++y)
                for (int x = 0; x < im.getWidth(); ++x)
                {
                    const auto a = out.getPixelColour(x, y), b = on.getPixelColour(x, y);
                    auto ch = [&](float o, float n, float t) { return juce::jlimit(0.f, 1.f, o + std::max(0.f, n - o) * t * 1.6f); };
                    out.setPixelColour(x, y, juce::Colour::fromFloatRGBA(ch(a.getFloatRed(), b.getFloatRed(), c.getFloatRed()),
                                                                         ch(a.getFloatGreen(), b.getFloatGreen(), c.getFloatGreen()),
                                                                         ch(a.getFloatBlue(), b.getFloatBlue(), c.getFloatBlue()), a.getFloatAlpha()));
                }
        }
        return im;
    }
};

struct Look : juce::LookAndFeel_V4
{
    juce::Image knobImg;
    Look()
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b1d22));
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff1b1d22));
        setColour(juce::TooltipWindow::textColourId, text);
        setColour(juce::TooltipWindow::outlineColourId, panelEdge);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h);
        const float d = std::min(b.getWidth(), b.getHeight()) - 2.f;
        const auto c = b.getCentre();
        const float r = d * 0.5f, kd = d * 0.80f;
        const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
        const float ang = a0 + pos * (a1 - a0), from = bipolar ? (a0 + a1) * 0.5f : a0;
        juce::Path bg, val;
        bg.addCentredArc(c.x, c.y, r - 1.5f, r - 1.5f, 0.f, a0, a1, true);
        val.addCentredArc(c.x, c.y, r - 1.5f, r - 1.5f, 0.f, std::min(from, ang), std::max(from, ang), true);
        g.setColour(juce::Colour(0xff26292f)); g.strokePath(bg, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(amber.withAlpha(0.25f));   g.strokePath(val, juce::PathStrokeType(6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(amber);                    g.strokePath(val, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (knobImg.isValid())
        {
            const float iw = (float) knobImg.getWidth();
            g.drawImageTransformed(knobImg, juce::AffineTransform::translation(-iw * 0.5f, -iw * 0.5f).scaled(kd / iw).rotated(ang).translated(c.x, c.y), false);
            return;
        }
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(c.x - kd / 2, c.y - kd / 2 + 2, kd, kd);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3d44), c.x - kd * 0.3f, c.y - kd * 0.4f, juce::Colour(0xff15161a), c.x + kd * 0.4f, c.y + kd * 0.5f, true));
        g.fillEllipse(c.x - kd / 2, c.y - kd / 2, kd, kd);
        g.setColour(juce::Colour(0xff0c0d10)); g.drawEllipse(c.x - kd / 2, c.y - kd / 2, kd, kd, 1.2f);
        juce::Path p; p.addRoundedRectangle(-1.f, -kd * 0.44f, 2.f, kd * 0.26f, 1.f);
        g.setColour(text); g.fillPath(p, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const bool primary = b.getProperties()["primary"];
        const bool on = b.getToggleState();
        if (primary)
        {
            g.setGradientFill(juce::ColourGradient(amber.brighter(0.15f), 0, r.getY(), amber.darker(0.25f), 0, r.getBottom(), false));
            g.fillRoundedRectangle(r, 8.f);
            if (over) { g.setColour(juce::Colours::white.withAlpha(down ? 0.18f : 0.10f)); g.fillRoundedRectangle(r, 8.f); }
            return;
        }
        g.setColour(on ? amber.withAlpha(0.14f) : bg.brighter(over ? 0.06f : 0.f)); g.fillRoundedRectangle(r, 6.f);
        g.setColour(on ? amber.withAlpha(0.8f) : panelEdge.brighter(0.15f)); g.drawRoundedRectangle(r, 6.f, 1.f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool primary = b.getProperties()["primary"];
        g.setFont(font(primary ? 13.f : 10.5f, true, primary ? 0.18f : 0.12f));
        g.setColour(primary ? juce::Colour(0xff1a1206) : (b.getToggleState() ? amber : text.withAlpha(b.isEnabled() ? 0.85f : 0.3f)));
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
};

struct DragButton : juce::TextButton
{
    std::function<void()> onDrag;
    bool started = false;
    void mouseDown(const juce::MouseEvent& e) override { started = false; juce::TextButton::mouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override { if (!started && e.getDistanceFromDragStart() > 6) { started = true; if (onDrag) onDrag(); } }
};
} // namespace ui

// -----------------------------------------------------------------------------------------------------
// bottom editor: the selected pad's sound with start / end trim and an attack - hold - decay envelope
// -----------------------------------------------------------------------------------------------------
class ShapeView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ShapeView(DrumSlicerProcessor& p) : proc(p) {}
    int pad = 0;

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(ui::well); g.fillRoundedRectangle(r, 8.f);
        g.setColour(ui::panelEdge); g.drawRoundedRectangle(r.reduced(0.5f), 8.f, 1.f);
        auto res = proc.getResult();
        if (!res || !res->pads[(size_t) pad].valid)
        {
            g.setColour(ui::faint); g.setFont(ui::font(11.f, true, 0.2f));
            g.drawText("SELECT A PAD", getLocalBounds(), juce::Justification::centred);
            return;
        }
        const auto sp = proc.shapeParams(pad);
        refreshPeaks(*res, sp.best);
        const auto col = ui::padColour(res->pads[(size_t) pad].name);
        const auto w = wave(); const float mid = w.getCentreY(), hh = w.getHeight() * 0.48f;
        const float xs = msToX(sp.start), xe = msToX(std::min(sp.end, totalMs));
        for (int i = 0; i < (int) peaks.size(); ++i)
        {
            const float x = w.getX() + (float) i;
            const bool in = x >= xs && x <= xe;
            g.setColour(in ? col.withAlpha(0.95f) : ui::faint);
            g.drawVerticalLine((int) x, mid - peaks[(size_t) i].second * hh, mid - peaks[(size_t) i].first * hh + 1.f);
        }
        // envelope
        const float top = w.getY() + 6.f, bot = w.getBottom() - 4.f;
        const float xa = msToX(sp.start + sp.attack), xh = msToX(sp.start + sp.attack + sp.hold);
        juce::Path env; env.startNewSubPath(xs, bot); env.lineTo(xa, top); env.lineTo(std::min(xh, xe), top);
        if (xh < xe)
            for (float x = xh; x <= xe; x += 2.f)
            {
                const float t = xToMs(x) - (sp.start + sp.attack + sp.hold);
                const float lvl = std::pow(10.f, -3.f * t / std::max(1.f, sp.decay));
                env.lineTo(x, bot - (bot - top) * std::sqrt(lvl));
            }
        g.setColour(ui::text.withAlpha(0.9f)); g.strokePath(env, juce::PathStrokeType(1.5f));
        // trims
        for (float x : { xs, xe }) { g.setColour(ui::text.withAlpha(0.85f)); g.drawLine(x, w.getY(), x, w.getBottom(), 1.5f); }
        juce::Path flag; flag.addTriangle(xs, w.getY(), xs + 9.f, w.getY() + 4.f, xs, w.getY() + 8.f); g.fillPath(flag);
        g.fillEllipse(xe - 3.f, mid - 3.f, 6.f, 6.f);
        // handles + labels
        const float xd = msToX(sp.start + sp.attack + sp.hold + sp.decay);
        float lx[3]; int nl = 0; const char* names[3]; bool act[3];
        auto handle = [&](float x, float y, const char* name, bool active)
        {
            x = juce::jlimit(w.getX() + 5.f, w.getRight() - 5.f, x);
            g.setColour(ui::well); g.fillEllipse(x - 5, y - 5, 10, 10);
            g.setColour(active ? ui::amber : ui::text); g.drawEllipse(x - 5, y - 5, 10, 10, 1.5f);
            lx[nl] = x - 30.f; names[nl] = name; act[nl] = active; ++nl;
        };
        // handles that sit at "off" (full length) wait at the right end, so they can always be grabbed
        handle(xa, top, "ATTACK", drag == 2);
        handle(std::min(xh, w.getRight() - 30.f), top, "HOLD", drag == 3);
        handle(std::min(xd, w.getRight() - 12.f), xd < w.getRight() ? bot - 2.f : top + 14.f, "DECAY", drag == 4);
        for (int i = 0; i < nl; ++i) lx[i] = std::max(i ? lx[i - 1] + 62.f : 2.f, lx[i]);      // labels never overlap
        lx[nl - 1] = std::min(lx[nl - 1], (float) getWidth() - 62.f);
        for (int i = nl - 2; i >= 0; --i) lx[i] = std::min(lx[i], lx[i + 1] - 62.f);
        g.setFont(ui::font(9.f, true, 0.18f));
        for (int i = 0; i < nl; ++i) { g.setColour(act[i] ? ui::amber : ui::dim); g.drawText(names[i], juce::Rectangle<float>(lx[i], (float) getHeight() - 15.f, 60, 12), juce::Justification::centred); }
        g.setColour(ui::dim); g.setFont(ui::font(9.f, false, 0.1f));
        g.drawText(juce::String(juce::roundToInt(std::min(sp.end, totalMs) - sp.start)) + " ms", juce::Rectangle<float>(w.getCentreX() - 40, w.getY(), 80, 12).toNearestInt(), juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto res = proc.getResult(); if (!res || !res->pads[(size_t) pad].valid) return;
        const auto sp = proc.shapeParams(pad); const auto p = e.position;
        const float xs = msToX(sp.start), xe = msToX(std::min(sp.end, totalMs));
        const auto w = wave();
        const float xa = juce::jmax(w.getX() + 5.f, msToX(sp.start + sp.attack)), xh = std::min(msToX(sp.start + sp.attack + sp.hold), w.getRight() - 30.f),
                    xd = std::min(msToX(sp.start + sp.attack + sp.hold + sp.decay), w.getRight() - 12.f);
        drag = 0; float best = 9.f;
        auto test = [&](float x, int id) { if (std::abs(p.x - x) < best) { best = std::abs(p.x - x); drag = id; } };
        test(xe, 5); test(xd, 4); test(xh, 3); test(xa, 2); test(xs, 1);
        if (drag && e.getNumberOfClicks() > 1) { resetHandle(drag); drag = 0; }
        else if (drag) beginGesture();
        else proc.previewPad[(size_t) pad] = 1;            // click on the sound = listen
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!drag) return;
        const auto sp = proc.shapeParams(pad); const float ms = juce::jlimit(0.f, totalMs, xToMs(e.position.x));
        switch (drag)
        {
            case 1: set("st", std::min(ms, std::min(sp.end, totalMs) - 5.f)); break;
            case 5: set("en", ms >= totalMs - 1.f ? 2000.f : std::max(ms, sp.start + 5.f)); break;
            case 2: set("atk", ms - sp.start); break;
            case 3: set("hold", ms >= totalMs * 0.97f ? 2000.f : ms - sp.start - sp.attack); break;
            case 4: set("dcy", ms >= totalMs * 0.99f ? 2000.f : std::max(5.f, ms - sp.start - sp.attack - std::min(sp.hold, ms))); break;
            default: break;
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (drag) { endGesture(); proc.previewPad[(size_t) pad] = 1; } drag = 0; repaint(); }
    void invalidate() { peakKey = -1; repaint(); }

private:
    juce::Rectangle<float> wave() const { return getLocalBounds().toFloat().reduced(10.f, 0).withTrimmedTop(8.f).withTrimmedBottom(18.f); }
    float msToX(float ms) const { const auto w = wave(); return w.getX() + juce::jlimit(0.f, 1.f, ms / std::max(1.f, totalMs)) * w.getWidth(); }
    float xToMs(float x) const { const auto w = wave(); return (x - w.getX()) / w.getWidth() * totalMs; }
    juce::RangedAudioParameter* prm(const char* id) { return proc.apvts.getParameter(juce::String(id) + juce::String(pad)); }
    void set(const char* id, float v)
    {
        if (auto* p = prm(id)) { const auto& nr = p->getNormalisableRange(); p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(nr.start, nr.end, v))); }
    }
    static const char* idOf(int d) { static const char* ids[6] = { "", "st", "atk", "hold", "dcy", "en" }; return ids[d]; }
    void beginGesture() { if (auto* p = prm(idOf(drag))) p->beginChangeGesture(); }
    void endGesture() { if (auto* p = prm(idOf(drag))) p->endChangeGesture(); }
    void resetHandle(int d) { if (auto* p = prm(idOf(d))) p->setValueNotifyingHost(p->getDefaultValue()); }
    void refreshPeaks(const ds::Result& r, bool best)
    {
        const int key = pad * 2 + (best ? 1 : 0) + 100 * proc.getResultVersion();
        const int W = (int) wave().getWidth();
        if (key == peakKey && (int) peaks.size() == W) return;
        peakKey = key; peaks.clear();
        const auto& a = r.pads[(size_t) pad].get(best);
        const int n = a.getNumSamples(); totalMs = (float) (n / r.sr * 1000.0);
        float mx = 1e-6f; for (int c = 0; c < 2; ++c) mx = std::max(mx, a.getMagnitude(c, 0, n));
        for (int x = 0; x < W; ++x)
        {
            const int s0 = (int) ((juce::int64) x * n / W), s1 = std::max(s0 + 1, (int) ((juce::int64) (x + 1) * n / W));
            float lo = 0, hi = 0;
            for (int s = s0; s < std::min(n, s1); ++s) { const float v = 0.5f * (a.getSample(0, s) + a.getSample(1, s)) / mx; lo = std::min(lo, v); hi = std::max(hi, v); }
            peaks.push_back({ lo, hi });
        }
    }
    DrumSlicerProcessor& proc;
    std::vector<std::pair<float, float>> peaks;
    int peakKey = -1, drag = 0;
    float totalMs = 800.f;
};

// -----------------------------------------------------------------------------------------------------
class DrumSlicerEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget,
                         public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit DrumSlicerEditor(DrumSlicerProcessor& p) : juce::AudioProcessorEditor(p), proc(p), shape(p)
    {
        setSize(1000, 600);
        look.knobImg = skin.knob;
        setLookAndFeel(&look);

        auto knob = [this](juce::Slider& s, const juce::String& id, std::function<juce::String(double)> fmt)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            s.setMouseDragSensitivity(180);
            addChildComponent(s);
            atts.push_back(std::make_unique<juce::SliderParameterAttachment>(*proc.apvts.getParameter(id), s));
            s.textFromValueFunction = std::move(fmt);
            s.onValueChange = [this] { repaint(); shape.repaint(); };
        };
        auto dB = [](double v) { return v <= 0.0001 ? juce::String("-INF") : juce::String(juce::Decibels::gainToDecibels(v), 1) + " dB"; };
        auto panTxt = [](double v) { const int p = juce::roundToInt(v * 100); return p == 0 ? juce::String("C") : juce::String(p < 0 ? "L " : "R ") + juce::String(std::abs(p)); };
        auto sdB = [](double v) { return juce::String(v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; };
        for (int i = 0; i < ds::kPads; ++i)
        {
            const juce::String s(i);
            knob(vol[(size_t) i], "vol" + s, dB);
            knob(pan[(size_t) i], "pan" + s, panTxt);
            knob(punch[(size_t) i], "punch" + s, sdB);
            knob(sus[(size_t) i], "sus" + s, sdB);
            knob(pitch[(size_t) i], "pitch" + s, [](double v) { return juce::String(v > 0 ? "+" : "") + juce::String(v, std::abs(v - std::round(v)) < 0.005 ? 0 : 2) + " st"; });
        }
        knob(master, "master", dB);
        knob(sens, "sens", [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
        knob(grid, "quant", [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
        knob(bpm, "bpm", [](double v) { return juce::String(v, 1); });
        for (auto* s : { &master, &sens, &grid, &bpm }) s->setVisible(true);
        sens.onDragEnd = [this] { proc.reanalyse(); };
        sens.setTooltip("Hit detection sensitivity (re-analyses the loop when you let go)");
        grid.setTooltip("TIGHT MIDI: 100% = fully on the grid, lower keeps some of the loop's groove");
        bpm.setTooltip("Tempo used for the MIDI when the host does not report one");

        for (auto* b : { &loadBtn, &reBtn }) addAndMakeVisible(b);
        loadBtn.setButtonText("LOAD"); reBtn.setButtonText("RE-ANALYSE");
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser>("Select drum loop", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) proc.startAnalysis(f, true); });
        };
        reBtn.onClick = [this] { proc.reanalyse(); };
        reBtn.setTooltip("Analyse the loop again (ignores the cache)");

        addAndMakeVisible(dragMidi);
        dragMidi.getProperties().set("primary", true);
        dragMidi.setButtonText("DRAG MIDI");
        dragMidi.setTooltip("Drag onto a MIDI / instrument track in your DAW");
        dragMidi.onDrag = [this]
        {
            auto f = proc.makeMidiFile();
            if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
        };
        for (int b = 0; b < 2; ++b)
        {
            addAndMakeVisible(modeBtn[(size_t) b]);
            modeBtn[(size_t) b].setButtonText(b == 0 ? "EXACT" : "TIGHT");
            modeBtn[(size_t) b].setTooltip(b == 0 ? "MIDI = the loop as played: fills, groove, every hit" : "MIDI = clean drum-machine pattern on the grid, few velocity levels");
            modeBtn[(size_t) b].onClick = [this, b] { if (auto* p = proc.apvts.getParameter("tight")) p->setValueNotifyingHost(b == 1 ? 1.f : 0.f); };
        }
        addAndMakeVisible(exportBtn);
        exportBtn.setButtonText("EXPORT");
        exportBtn.setTooltip("Shaped one-shots (A or B as chosen) + MIDI into Documents/DrumSlicer/<loop>/EXPORT");
        exportBtn.onClick = [this] { auto d = proc.exportFinal(); if (d.exists()) d.revealToUser(); };

        for (int b = 0; b < 2; ++b)
        {
            addAndMakeVisible(srcBtn[(size_t) b]);
            srcBtn[(size_t) b].setButtonText(b == 0 ? "A  LEARNED" : "B  BEST HIT");
            srcBtn[(size_t) b].setTooltip(b == 0 ? "The sound averaged over all its hits: stem dirt cancels out" : "The cleanest real hit, other drums subtracted, learned tail");
            srcBtn[(size_t) b].onClick = [this, b] { setBest(selected, b == 1); };
        }
        addAndMakeVisible(dragWav);
        dragWav.setButtonText("DRAG WAV");
        dragWav.setTooltip("Drag this pad's sound (as you hear it) into the DAW");
        dragWav.onDrag = [this]
        {
            auto f = proc.shotFile(selected);
            if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
        };
        addAndMakeVisible(shape);
        shape.setTooltip("Click: listen.  Drag the flag / end line to trim, the dots for ATTACK, HOLD, DECAY.  Double-click a handle to reset.");
        updateVisibility();
        startTimerHz(30);
    }
    ~DrumSlicerEditor() override { setLookAndFeel(nullptr); }

    bool isInterestedInFileDrag(const juce::StringArray& f) override { return f.size() > 0 && juce::File(f[0]).hasFileExtension("wav;aif;aiff;flac;mp3"); }
    void fileDragEnter(const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { hover = false; repaint(); }
    void filesDropped(const juce::StringArray& f, int, int) override { hover = false; proc.startAnalysis(juce::File(f[0]), true); repaint(); }

    // ---------------------------------------------------------------- layout (1000 x 600)
    const juce::Rectangle<int> display { 20, 76, 960, 140 }, padsPanel { 20, 226, 624, 206 }, editPanel { 20, 440, 624, 140 },
                               rightPanel { 654, 226, 326, 262 }, soundPanel { 654, 496, 326, 84 };
    juce::Rectangle<int> padRect(int i) const { return { 30 + (i % 4) * 152, 236 + (i / 4) * 100, 146, 92 }; }
    juce::Rectangle<int> abRect(int i) const { auto r = padRect(i); return { r.getRight() - 50, r.getBottom() - 30, 40, 20 }; }

    void resized() override
    {
        loadBtn.setBounds(display.getRight() - 172, display.getY() + 10, 60, 22);
        reBtn.setBounds(display.getRight() - 106, display.getY() + 10, 94, 22);
        for (int i = 0; i < ds::kPads; ++i)
        {
            vol[(size_t) i].setBounds(712, 266, 64, 64); pan[(size_t) i].setBounds(858, 266, 64, 64);
            punch[(size_t) i].setBounds(690, 503, 42, 42); sus[(size_t) i].setBounds(796, 503, 42, 42); pitch[(size_t) i].setBounds(902, 503, 42, 42);
        }
        master.setBounds(669, 362, 42, 42); sens.setBounds(749, 362, 42, 42); grid.setBounds(829, 362, 42, 42); bpm.setBounds(909, 362, 42, 42);
        dragMidi.setBounds(666, 440, 136, 36);
        modeBtn[0].setBounds(808, 446, 50, 24); modeBtn[1].setBounds(857, 446, 50, 24);
        exportBtn.setBounds(914, 440, 56, 36);
        srcBtn[0].setBounds(32, 450, 102, 22); srcBtn[1].setBounds(133, 450, 102, 22);
        dragWav.setBounds(552, 450, 80, 22);
        shape.setBounds(30, 478, 604, 96);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto res = proc.getResult();
        for (int i = 0; i < ds::kPads; ++i)
        {
            if (!padRect(i).contains(e.getPosition())) continue;
            selected = i;
            if (res && res->pads[(size_t) i].valid && abRect(i).contains(e.getPosition()))
                setBest(i, e.getPosition().x > abRect(i).getCentreX());
            else proc.previewPad[(size_t) i] = 1;
            updateVisibility(); repaint();
        }
        if (display.contains(e.getPosition()) && !res && !proc.isBusy()) loadBtn.triggerClick();
    }

    void timerCallback() override
    {
        if (proc.getResultVersion() != shownVersion) { shownVersion = proc.getResultVersion(); refreshWave(); shape.invalidate(); repaint(); }
        const bool tight = *proc.apvts.getRawParameterValue("tight") > 0.5f;
        modeBtn[0].setToggleState(!tight, juce::dontSendNotification); modeBtn[1].setToggleState(tight, juce::dontSendNotification);
        const bool b = *proc.apvts.getRawParameterValue("best" + juce::String(selected)) > 0.5f;
        srcBtn[0].setToggleState(!b, juce::dontSendNotification); srcBtn[1].setToggleState(b, juce::dontSendNotification);
        bool lit = false;
        for (size_t i = 0; i < 8; ++i) { levels[i] = std::max(proc.padLevel[i].exchange(0.f) > 0.f ? 1.f : 0.f, levels[i] * 0.85f); lit |= levels[i] > 0.01f; }
        auto res = proc.getResult();
        const bool hasPad = res && res->pads[(size_t) selected].valid;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &dragMidi, &exportBtn, &modeBtn[0], &modeBtn[1], &reBtn }) c->setEnabled(res != nullptr);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &srcBtn[0], &srcBtn[1], &dragWav }) c->setEnabled(hasPad);
        if (lit || proc.isBusy() || wasBusy) repaint();
        wasBusy = proc.isBusy();
        shape.repaint();
    }

    // ---------------------------------------------------------------- drawing
    void paint(juce::Graphics& g) override
    {
        if (skin.background.isValid()) g.drawImage(skin.background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        else
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff15161a), 500, 250, juce::Colour(0xff0d0e11), 0, 600, true));
            g.fillAll();
        }
        auto res = proc.getResult();
        if (res && peaks.empty()) refreshWave();
        drawHeader(g, res);
        for (auto r : { display, padsPanel, editPanel, rightPanel, soundPanel }) drawPanel(g, r);
        drawDisplay(g, res);
        drawPads(g, res);
        drawRight(g, res);
    }

private:
    void setBest(int pad, bool b)
    {
        if (auto* p = proc.apvts.getParameter("best" + juce::String(pad))) p->setValueNotifyingHost(b ? 1.f : 0.f);
        proc.previewPad[(size_t) pad] = 1; shape.invalidate(); repaint();
    }

    void drawPanel(juce::Graphics& g, juce::Rectangle<int> r)
    {
        auto f = r.toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(f.translated(0, 2), 12.f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xf0181a1f), 0, f.getY(), juce::Colour(0xf0121317), 0, f.getBottom(), false));
        g.fillRoundedRectangle(f, 12.f);
        g.setColour(ui::panelEdge); g.drawRoundedRectangle(f.reduced(0.5f), 12.f, 1.f);
    }

    void drawHeader(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        g.setColour(ui::text); g.setFont(ui::font(25.f, true, 0.32f));
        g.drawText("DRUM SLICER", 32, 18, 400, 30, juce::Justification::centredLeft);
        g.setColour(ui::dim); g.setFont(ui::font(9.f, true, 0.55f));
        g.drawText("BY POURIA MOTABEAN", 34, 50, 400, 12, juce::Justification::centredLeft);
        const juce::Rectangle<float> pill(842.f, 27.f, 138.f, 26.f);
        const bool has = res != nullptr;
        const double m = has ? res->minHearing() : 0;
        const auto c = !has ? ui::faint : (m >= 0.9 ? ui::amber : juce::Colour(0xffff7a59));
        g.setColour(c.withAlpha(0.9f)); g.drawRoundedRectangle(pill, 13.f, 1.2f);
        g.setFont(ui::font(9.5f, true, 0.4f)); g.setColour(c.withAlpha(0.75f));
        g.drawText("MATCH", pill.withTrimmedLeft(18).toNearestInt(), juce::Justification::centredLeft);
        g.setFont(ui::font(14.f, true, 0.05f)); g.setColour(c);
        g.drawText(has ? juce::String(juce::roundToInt(100.0 * m)) + "%" : juce::String("--"), pill.withTrimmedRight(16).toNearestInt(), juce::Justification::centredRight);
    }

    void drawDisplay(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        auto d = display.toFloat().reduced(1.f);
        g.setColour(ui::well.withAlpha(0.7f)); g.fillRoundedRectangle(d.reduced(6.f), 8.f);
        if (hover) { g.setColour(ui::amber.withAlpha(0.8f)); g.drawRoundedRectangle(d.reduced(4.f), 10.f, 1.5f); }
        if (proc.isBusy())
        {
            const auto bar = juce::Rectangle<float>(d.getX() + 120, d.getCentreY() + 8, d.getWidth() - 240, 6);
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(bar, 3.f);
            g.setColour(ui::amber); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, proc.getProgress())), 3.f);
            g.setFont(ui::font(13.f, true, 0.3f));
            g.drawText("LEARNING THE SOUNDS   " + juce::String(juce::roundToInt(100.f * proc.getProgress())) + "%", d.withTrimmedBottom(40).toNearestInt(), juce::Justification::centred);
            g.setColour(ui::dim); g.setFont(ui::font(9.5f, false, 0.05f));
            g.drawText(proc.getStatus(), d.withTrimmedTop(84).reduced(24, 0).toNearestInt(), juce::Justification::centred);
            return;
        }
        if (!res)
        {
            const auto st = proc.getStatus();
            g.setColour(ui::amber.withAlpha(0.9f)); g.setFont(ui::font(14.f, true, 0.3f));
            g.drawText(st.startsWith("COULD NOT") ? st : juce::String("DROP A DRUM LOOP HERE"), d.withTrimmedBottom(16).toNearestInt(), juce::Justification::centred);
            g.setColour(ui::dim); g.setFont(ui::font(9.5f, true, 0.3f));
            g.drawText("WAV  /  AIFF  /  FLAC  /  MP3     -     OR CLICK TO BROWSE", d.withTrimmedTop(36).toNearestInt(), juce::Justification::centred);
            return;
        }
        const float x0 = d.getX() + 18, w = d.getWidth() - 36, mid = d.getY() + 58, hh = 38.f;
        juce::Path wav; wav.startNewSubPath(x0, mid);
        for (int x = 0; x < (int) peaks.size(); ++x) wav.lineTo(x0 + (float) x, mid - peaks[(size_t) x].second * hh);
        for (int x = (int) peaks.size() - 1; x >= 0; --x) wav.lineTo(x0 + (float) x, mid - peaks[(size_t) x].first * hh);
        wav.closeSubPath();
        g.setColour(ui::amber.withAlpha(0.18f)); g.strokePath(wav, juce::PathStrokeType(3.f));
        g.setColour(ui::amber); g.fillPath(wav);
        const int n = res->loop.getNumSamples();
        for (auto& h : res->hits)
        {
            const float x = x0 + (float) (h.time / n) * w;
            const auto c = ui::padColour(res->pads[(size_t) h.pad].name);
            g.setColour(c.withAlpha(h.pad == selected ? 1.f : 0.55f));
            g.fillRoundedRectangle(x - 1.f, mid + hh + 6.f, 2.f, h.pad == selected ? 10.f : 7.f, 1.f);
        }
        const double secs = n / res->sr, bars = std::max(1.0, std::round(secs * res->bpm / 240.0));
        const double hb = proc.getHostBpm();
        juce::String info = "LOOP  " + juce::String((int) bars) + " BARS   |   " + juce::String(res->bpm, 2) + " BPM   |   " + juce::String(res->sr / 1000.0, 1)
                          + " kHz   |   " + juce::String((int) res->hits.size()) + " HITS   |   MIDI @ " + (hb > 0 ? "HOST " + juce::String(hb, 2) : juce::String(proc.midiBpm(), 2) + " BPM");
        g.setColour(ui::dim.brighter(0.2f)); g.setFont(ui::font(9.f, true, 0.2f));
        g.drawText(info, juce::Rectangle<float>(x0, d.getBottom() - 22, w, 12).toNearestInt(), juce::Justification::centredLeft);
        g.drawText(proc.getLoopFile().getFileName(), juce::Rectangle<float>(x0, d.getBottom() - 22, w, 12).toNearestInt(), juce::Justification::centredRight);
    }

    void drawPads(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        for (int i = 0; i < ds::kPads; ++i)
        {
            const auto r = padRect(i).toFloat();
            const bool has = res && res->pads[(size_t) i].valid;
            const auto c = has ? ui::padColour(res->pads[(size_t) i].name) : ui::faint;
            const float lv = levels[(size_t) i];
            if (skin.padOff.isValid())
            {
                g.setOpacity(1.f);
                g.drawImage(skin.padOff, r, juce::RectanglePlacement::stretchToFit);
                if (has)
                {
                    const auto& lit = skin.litPad(c);
                    if (lit.isValid()) { g.setOpacity(std::min(1.f, (i == selected ? 0.85f : 0.45f) + 0.4f * lv)); g.drawImage(lit, r, juce::RectanglePlacement::stretchToFit); g.setOpacity(1.f); }
                }
            }
            else
            {
                g.setColour(juce::Colour(0xff1c1e23)); g.fillRoundedRectangle(r, 10.f);
                if (has) { g.setGradientFill(juce::ColourGradient(c.withAlpha(i == selected ? 0.28f : 0.14f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getBottom(), false)); g.fillRoundedRectangle(r, 10.f); }
                g.setColour(juce::Colour(0xff0c0d10)); g.drawRoundedRectangle(r.reduced(0.5f), 10.f, 1.f);
            }
            if (has && lv > 0.02f) { g.setColour(c.withAlpha(0.22f * lv)); g.fillRoundedRectangle(r.reduced(4), 9.f); }
            if (i == selected) { g.setColour(c.withAlpha(has ? 0.9f : 0.4f)); g.drawRoundedRectangle(r.reduced(1.f), 10.f, 1.5f); }
            if (has) { g.setColour(c.withAlpha(0.25f)); g.fillRoundedRectangle(r.getX() + 12, r.getY() + 8, 48, 7, 3.f); }
            g.setColour(has ? c.withAlpha(0.6f + 0.4f * lv) : juce::Colour(0xff2c2f36));
            g.fillRoundedRectangle(r.getX() + 14, r.getY() + 10, 44, 3, 1.5f);
            g.setColour(has ? ui::text : ui::faint); g.setFont(ui::font(13.f, true, 0.2f));
            g.drawText(has ? res->pads[(size_t) i].name : juce::String("-"), juce::Rectangle<float>(r.getX() + 14, r.getBottom() - 42, 90, 16).toNearestInt(), juce::Justification::centredLeft);
            if (!has) continue;
            g.setColour(ui::dim); g.setFont(ui::font(9.5f, true, 0.1f));
            g.drawText(juce::MidiMessage::getMidiNoteName(res->pads[(size_t) i].note, true, true, 3), juce::Rectangle<float>(r.getX() + 14, r.getBottom() - 24, 60, 12).toNearestInt(), juce::Justification::centredLeft);
            const auto ab = abRect(i).toFloat();
            const bool b = *proc.apvts.getRawParameterValue("best" + juce::String(i)) > 0.5f;
            g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(ab, 5.f);
            g.setColour(ui::panelEdge.brighter(0.2f)); g.drawRoundedRectangle(ab.reduced(0.5f), 5.f, 1.f);
            g.setFont(ui::font(10.f, true));
            g.setColour(!b ? ui::amber : ui::dim); g.drawText("A", ab.withWidth(ab.getWidth() / 2).toNearestInt(), juce::Justification::centred);
            g.setColour(b ? ui::amber : ui::dim);  g.drawText("B", ab.withTrimmedLeft(ab.getWidth() / 2).toNearestInt(), juce::Justification::centred);
            g.setColour(ui::panelEdge.brighter(0.2f)); g.drawVerticalLine((int) ab.getCentreX(), ab.getY() + 4, ab.getBottom() - 4);
        }
    }

    void knobLabel(juce::Graphics& g, juce::Slider& s, const juce::String& name, int dy = 4)
    {
        if (!s.isVisible()) return;
        const auto b = s.getBounds();
        g.setColour(ui::text.withAlpha(0.9f)); g.setFont(ui::font(9.5f, true, 0.22f));
        g.drawText(name, b.getX() - 30, b.getBottom() + dy, b.getWidth() + 60, 12, juce::Justification::centred);
        g.setColour(ui::dim.brighter(0.15f)); g.setFont(ui::font(9.5f, false, 0.05f));
        g.drawText(s.getTextFromValue(s.getValue()), b.getX() - 30, b.getBottom() + dy + 13, b.getWidth() + 60, 12, juce::Justification::centred);
    }

    void drawRight(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        const bool has = res && res->pads[(size_t) selected].valid;
        const ds::Pad empty;
        const auto& pd = has ? res->pads[(size_t) selected] : empty;
        g.setColour(has ? ui::text : ui::faint); g.setFont(ui::font(15.f, true, 0.25f));
        g.drawText(has ? pd.name + "  -  " + juce::MidiMessage::getMidiNoteName(pd.note, true, true, 3) : "PAD " + juce::String(selected + 1),
                   670, 236, 200, 20, juce::Justification::centredLeft);
        if (has)
        {
            g.setColour(ui::padColour(pd.name)); g.fillEllipse(958, 242, 8, 8);
            g.setColour(ui::dim); g.setFont(ui::font(9.f, true, 0.12f));
            g.drawText(juce::String(pd.hits) + " HITS", 860, 236, 90, 20, juce::Justification::centredRight);
        }
        g.setColour(ui::panelEdge);
        g.drawHorizontalLine(258, 668.f, 966.f); g.drawHorizontalLine(354, 668.f, 966.f); g.drawHorizontalLine(432, 668.f, 966.f);
        knobLabel(g, vol[(size_t) selected], "VOL"); knobLabel(g, pan[(size_t) selected], "PAN");
        knobLabel(g, master, "MASTER", 1); knobLabel(g, sens, "SENS", 1); knobLabel(g, grid, "GRID", 1); knobLabel(g, bpm, "BPM", 1);
        knobLabel(g, punch[(size_t) selected], "PUNCH", 1); knobLabel(g, sus[(size_t) selected], "SUSTAIN", 1); knobLabel(g, pitch[(size_t) selected], "PITCH", 1);
    }

    void updateVisibility()
    {
        for (int i = 0; i < ds::kPads; ++i)
            for (auto* k : { &vol[(size_t) i], &pan[(size_t) i], &punch[(size_t) i], &sus[(size_t) i], &pitch[(size_t) i] }) k->setVisible(i == selected);
        shape.pad = selected; shape.invalidate();
    }

    void refreshWave()
    {
        peaks.clear();
        auto r = proc.getResult();
        if (!r) return;
        const int w = display.getWidth() - 38, n = r->loop.getNumSamples();
        float mx = 1e-6f; for (int c = 0; c < 2; ++c) mx = std::max(mx, r->loop.getMagnitude(c, 0, n));
        const float* L = r->loop.getReadPointer(0); const float* R = r->loop.getReadPointer(1);
        for (int x = 0; x < w; ++x)
        {
            const int a = (int) ((juce::int64) x * n / w), b = (int) ((juce::int64) (x + 1) * n / w), step = std::max(1, (b - a) / 64);
            float mn = 0.f, mxv = 0.f;
            for (int s = a; s < b; s += step) { const float v = 0.5f * (L[s] + R[s]) / mx; mn = std::min(mn, v); mxv = std::max(mxv, v); }
            peaks.push_back({ mn, mxv });
        }
    }

    DrumSlicerProcessor& proc;
    ui::Skin skin;
    ui::Look look;
    int selected = 0, shownVersion = -1;
    bool hover = false, wasBusy = false;
    std::array<float, 8> levels {};
    std::vector<std::pair<float, float>> peaks;

    std::array<juce::Slider, ds::kPads> vol, pan, punch, sus, pitch;
    juce::Slider master, sens, grid, bpm;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> atts;
    juce::TextButton loadBtn, reBtn, exportBtn;
    std::array<juce::TextButton, 2> modeBtn, srcBtn;
    ui::DragButton dragMidi, dragWav;
    ShapeView shape;
    juce::TooltipWindow tips { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerEditor)
};
