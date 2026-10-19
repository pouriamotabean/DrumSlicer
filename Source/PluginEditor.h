#pragma once
#include "PluginProcessor.h"
#ifndef HAS_ASSETS
 #define HAS_ASSETS 0
#endif
#if HAS_ASSETS
 #include <BinaryData.h>
#endif

// =====================================================================================================
//  REKIT UI  -  dark graphite, one amber accent.   by Pouria Motabean
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
    if (name.startsWith("TOM"))    return juce::Colour(0xffff7a45);
    if (name.startsWith("RIM"))    return juce::Colour(0xffb6e35a);
    if (name.startsWith("SHAKER")) return juce::Colour(0xffd9b38c);
    return juce::Colour(0xffff6b8a);                                  // PERC
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
// sound editor of the selected pad: START / END flags, FADE IN / FADE OUT squares.
// Drag a square sideways = fade length, up / down = its curve. Double-click a handle = reset it.
// Click the sound = listen.  Right-click = reset menu.  FIT / FULL (top right) = zoom to the used part.
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
        refreshPeaks(*res, sp.best, sp.user);
        const auto col = ui::padColour(res->pads[(size_t) pad].name);
        const auto w = wave(); const float mid = w.getCentreY(), hh = w.getHeight() * 0.47f;
        const float endMs = std::min(sp.end, totalMs);
        const float xs = msToX(sp.start), xe = msToX(endMs);
        // the sound: as heard (shaped) in colour, the untouched take faint behind it
        for (int i = 0; i < (int) peaks.size(); ++i)
        {
            const float x = w.getX() + (float) i, ms = xToMs(x + 0.5f);
            const auto pk = peaks[(size_t) i];
            g.setColour(ui::faint.withAlpha(0.55f));
            g.drawVerticalLine((int) x, mid - pk.second * hh, mid - pk.first * hh + 1.f);
            if (ms < sp.start || ms > endMs) continue;
            const float e = level(sp, ms);
            g.setColour(col.withAlpha(0.95f));
            g.drawVerticalLine((int) x, mid - pk.second * hh * e, mid - pk.first * hh * e + 1.f);
        }
        // envelope: filled + line
        const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        juce::Path env, fill;
        fill.startNewSubPath(xs, bot);
        for (float x = xs; x <= xe + 0.5f; x += 1.f)
        {
            const float y = bot - (bot - top) * level(sp, xToMs(std::min(x, xe)));
            if (x == xs) env.startNewSubPath(x, y); else env.lineTo(x, y);
            fill.lineTo(x, y);
        }
        fill.lineTo(xe, bot); fill.closeSubPath();
        g.setColour(col.withAlpha(0.10f)); g.fillPath(fill);
        g.setColour(ui::text.withAlpha(0.85f)); g.strokePath(env, juce::PathStrokeType(1.4f));
        // START / END flags
        const bool hs = hot == 1 || drag == 1, he = hot == 5 || drag == 5;
        g.setColour((hs ? ui::amber : ui::text).withAlpha(0.9f)); g.drawLine(xs, w.getY(), xs, w.getBottom(), hs ? 2.f : 1.4f);
        juce::Path fs; fs.addTriangle(xs, w.getY(), xs + 11.f, w.getY() + 6.f, xs, w.getY() + 12.f); g.fillPath(fs);
        g.setColour((he ? ui::amber : ui::text).withAlpha(0.9f)); g.drawLine(xe, w.getY(), xe, w.getBottom(), he ? 2.f : 1.4f);
        juce::Path fe; fe.addTriangle(xe, w.getBottom(), xe - 11.f, w.getBottom() - 6.f, xe, w.getBottom() - 12.f); g.fillPath(fe);
        // fade squares
        auto square = [&](juce::Point<float> c, bool on, bool rising)
        {
            const auto q = juce::Rectangle<float>(13.f, 13.f).withCentre(c);
            g.setColour(on ? ui::amber : ui::text.withAlpha(0.9f)); g.fillRoundedRectangle(q, 2.f);
            g.setColour(ui::well); g.drawLine(q.getX() + 3, rising ? q.getBottom() - 3 : q.getY() + 3, q.getRight() - 3, rising ? q.getY() + 3 : q.getBottom() - 3, 1.4f);
        };
        square(fadeInHandle(sp), hot == 2 || drag == 2, true);
        square(fadeOutHandle(sp), hot == 6 || drag == 6, false);
        // values
        g.setFont(ui::font(8.5f, true, 0.15f));
        auto label = [&](int id, const juce::String& t, juce::Rectangle<float> rr, juce::Justification j)
        { g.setColour(drag == id || hot == id ? ui::amber : ui::dim); g.drawText(t, rr, j); };
        const float ly = (float) getHeight() - 15.f, lw = ((float) getWidth() - 20.f) / 4.f;
        label(1, "START  " + ms(sp.start), { 10.f, ly, lw, 12.f }, juce::Justification::centredLeft);
        label(2, "FADE IN  " + ms(sp.attack) + curveText(sp.attackCurve), { 10.f + lw, ly, lw, 12.f }, juce::Justification::centred);
        label(6, "FADE OUT  " + ms(sp.fadeOut) + curveText(sp.fadeOutCurve), { 10.f + 2 * lw, ly, lw, 12.f }, juce::Justification::centred);
        label(5, "END  " + ms(endMs), { 10.f + 3 * lw, ly, lw, 12.f }, juce::Justification::centredRight);
        g.setColour(ui::dim); g.setFont(ui::font(9.f, false, 0.1f));
        g.drawText(ms(endMs - sp.start), juce::Rectangle<float>(w.getCentreX() - 40, w.getY() + 1, 80, 12).toNearestInt(), juce::Justification::centred);
        // zoom
        const auto zb = zoomRect();
        g.setColour(zoomFit ? ui::amber : ui::dim); g.setFont(ui::font(8.5f, true, 0.25f));
        g.drawRoundedRectangle(zb, 6.f, 1.f); g.drawText(zoomFit ? "FULL" : "FIT", zb, juce::Justification::centred);
    }

    void mouseMove(const juce::MouseEvent& e) override { const int h = hit(e.position); if (h != hot) { hot = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { if (hot) { hot = 0; repaint(); } }
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto res = proc.getResult(); if (!res || !res->pads[(size_t) pad].valid) return;
        if (zoomRect().contains(e.position)) { zoomFit = !zoomFit; peakKey = -1; repaint(); return; }
        if (e.mods.isPopupMenu()) { resetMenu(); return; }
        drag = hit(e.position);
        if (drag && e.getNumberOfClicks() > 1) { resetHandle(drag); drag = 0; }
        else if (drag) { beginGesture(); startSp = proc.shapeParams(pad); startPos = e.position; }
        else proc.previewPad[(size_t) pad] = 1;            // click on the sound = listen
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!drag) return;
        const auto sp = proc.shapeParams(pad);
        const float m = juce::jlimit(0.f, totalMs, xToMs(e.position.x)), endMs = std::min(sp.end, totalMs), len = endMs - sp.start;
        const auto w = wave(); const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        // moves are relative to where the drag started: grabbing a square never makes it jump
        const float m0 = juce::jlimit(0.f, totalMs, xToMs(startPos.x)), dy = e.position.y - startPos.y;
        auto curveFrom = [&](float c0) {                    // the square sits on the curve's middle: its height sets the curve
            const float L0 = (float) std::pow(0.5, std::pow(2.0, -3.0 * c0));
            const float L = juce::jlimit(0.03f, 0.97f, L0 - dy / (bot - top));
            const float pw = std::log(L) / std::log(0.5f);
            return juce::jlimit(-1.f, 1.f, -std::log2(pw) / 3.f); };
        switch (drag)
        {
            case 1: set("st", juce::jlimit(0.f, endMs - 5.f - sp.attack - sp.fadeOut, m)); break;
            case 5: set("en", m >= totalMs - 0.5f ? 2000.f : std::max(m, sp.start + 5.f + sp.attack + sp.fadeOut)); break;
            case 2: set("atk", juce::jlimit(0.f, std::max(0.f, len - sp.fadeOut), startSp.attack + 2.f * (m - m0)));
                    if (std::abs(dy) > 1.f) set("fic", curveFrom(startSp.attackCurve));
                    break;
            case 6: set("fo", juce::jlimit(0.f, std::max(0.f, len - sp.attack), startSp.fadeOut - 2.f * (m - m0)));
                    if (std::abs(dy) > 1.f) set("foc", curveFrom(startSp.fadeOutCurve));
                    break;
            default: break;
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (drag) { endGesture(); proc.previewPad[(size_t) pad] = 1; } drag = 0; repaint(); }
    void invalidate() { peakKey = -1; repaint(); }

private:
    // gain of the shaped sound at a time (ms in the take) - the same maths as shapeShot()
    float fadeOutLevel(const ShapeParams& sp, float msIn) const
    {
        const float endMs = std::min(sp.end, totalMs);
        if (sp.fadeOut <= 0 || msIn < endMs - sp.fadeOut) return 1.f;
        return (float) std::pow(std::max(0.0, (double) (endMs - msIn) / sp.fadeOut), std::pow(2.0, -3.0 * sp.fadeOutCurve));
    }
    float level(const ShapeParams& sp, float msIn) const;
    juce::Point<float> fadeInHandle(const ShapeParams& sp) const
    {
        const auto w = wave(); const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        const float L = (float) std::pow(0.5, std::pow(2.0, -3.0 * sp.attackCurve));
        return { std::max(msToX(sp.start) + 10.f, msToX(sp.start + 0.5f * sp.attack)), bot - (bot - top) * L };   // never under the flag
    }
    juce::Point<float> fadeOutHandle(const ShapeParams& sp) const
    {
        const auto w = wave(); const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        const float L = (float) std::pow(0.5, std::pow(2.0, -3.0 * sp.fadeOutCurve));
        return { std::min(msToX(std::min(sp.end, totalMs)) - 10.f, msToX(std::min(sp.end, totalMs) - 0.5f * sp.fadeOut)), bot - (bot - top) * L };
    }
    int hit(juce::Point<float> p) const
    {
        auto res = proc.getResult(); if (!res || !res->pads[(size_t) pad].valid) return 0;
        const auto sp = proc.shapeParams(pad); const auto w = wave();
        if (p.getDistanceFrom(fadeInHandle(sp)) < 10.f) return 2;
        if (p.getDistanceFrom(fadeOutHandle(sp)) < 10.f) return 6;
        const float xs = msToX(sp.start), xe = msToX(std::min(sp.end, totalMs));
        const float ds = std::abs(p.x - xs), de = std::abs(p.x - xe);
        if (std::min(ds, de) < 7.f && p.y >= w.getY() - 4 && p.y <= w.getBottom() + 4) return ds <= de ? 1 : 5;
        return 0;
    }
    juce::Rectangle<float> wave() const { return getLocalBounds().toFloat().reduced(10.f, 0).withTrimmedTop(8.f).withTrimmedBottom(20.f); }
    juce::Rectangle<float> zoomRect() const { return { (float) getWidth() - 50.f, 4.f, 40.f, 14.f }; }
    float viewMs() const
    {
        if (!zoomFit) return totalMs;
        const auto sp = proc.shapeParams(pad);
        return juce::jlimit(20.f, totalMs, std::min(sp.end, totalMs) * 1.12f + 5.f);
    }
    float msToX(float m) const { const auto w = wave(); return w.getX() + juce::jlimit(0.f, 1.f, m / std::max(1.f, viewMs())) * w.getWidth(); }
    float xToMs(float x) const { const auto w = wave(); return (x - w.getX()) / w.getWidth() * viewMs(); }
    static juce::String ms(float v) { return v >= 100.f ? juce::String(juce::roundToInt(v)) + " ms" : juce::String(v, 1) + " ms"; }
    static juce::String curveText(float c) { return std::abs(c) < 0.02f ? juce::String() : (c > 0 ? "  +" : "  ") + juce::String(juce::roundToInt(c * 100)); }
    juce::RangedAudioParameter* prm(const char* id) { return proc.apvts.getParameter(juce::String(id) + juce::String(pad)); }
    void set(const char* id, float v)
    {
        if (auto* p = prm(id)) { const auto& nr = p->getNormalisableRange(); p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(nr.start, nr.end, v))); }
    }
    static std::vector<const char*> idsOf(int d)
    {
        switch (d) { case 1: return { "st" }; case 5: return { "en" }; case 2: return { "atk", "fic" }; case 6: return { "fo", "foc" }; default: return {}; }
    }
    void beginGesture() { for (auto* id : idsOf(drag)) if (auto* p = prm(id)) p->beginChangeGesture(); }
    void endGesture() { for (auto* id : idsOf(drag)) if (auto* p = prm(id)) p->endChangeGesture(); }
    void resetIds(std::initializer_list<const char*> ids) { for (auto* id : ids) if (auto* p = prm(id)) p->setValueNotifyingHost(p->getDefaultValue()); repaint(); }
    void resetHandle(int d) { for (auto* id : idsOf(d)) if (auto* p = prm(id)) p->setValueNotifyingHost(p->getDefaultValue()); }
    void resetMenu()
    {
        juce::PopupMenu m;
        m.addItem(1, "RESET FADES"); m.addItem(2, "RESET START / END"); m.addItem(3, "RESET THE WHOLE SHAPE");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this, sp = juce::Component::SafePointer<ShapeView>(this)](int id)
        {
            if (sp == nullptr) return;
            if (id == 1) resetIds({ "atk", "fic", "fo", "foc", "hold", "dcy" });
            if (id == 2) resetIds({ "st", "en" });
            if (id == 3) resetIds({ "st", "en", "atk", "fic", "fo", "foc", "hold", "dcy" });
        });
    }
    void refreshPeaks(const ds::Result& r, bool best, bool user)
    {
        const auto& a = r.pads[(size_t) pad].get(best, user);
        const int n = a.getNumSamples(); totalMs = (float) (n / r.sr * 1000.0);
        const float vm = viewMs();
        const int key = pad * 4 + (best ? 1 : 0) + (user ? 2 : 0) + 100 * proc.getResultVersion();
        const int W = (int) wave().getWidth();
        if (key == peakKey && (int) peaks.size() == W && vm == peakView) return;
        peakKey = key; peakView = vm; peaks.clear();
        const int nv = std::min(n, (int) (vm * 0.001 * r.sr) + 1);
        float mx = 1e-6f; for (int c = 0; c < 2; ++c) mx = std::max(mx, a.getMagnitude(c, 0, n));
        for (int x = 0; x < W; ++x)
        {
            const int s0 = (int) ((juce::int64) x * nv / W), s1 = std::max(s0 + 1, (int) ((juce::int64) (x + 1) * nv / W));
            float lo = 0, hi = 0;
            for (int s = s0; s < std::min(n, s1); ++s) { const float v = 0.5f * (a.getSample(0, s) + a.getSample(1, s)) / mx; lo = std::min(lo, v); hi = std::max(hi, v); }
            peaks.push_back({ lo, hi });
        }
    }
    DrumSlicerProcessor& proc;
    std::vector<std::pair<float, float>> peaks;
    int peakKey = -1, drag = 0, hot = 0;
    float totalMs = 800.f, peakView = -1.f;
    bool zoomFit = false;
    ShapeParams startSp; juce::Point<float> startPos;
};
inline float ShapeView::level(const ShapeParams& sp, float msIn) const
{
    const float t = msIn - sp.start;
    double env = 1.0;
    if (t < sp.attack) env = std::pow(std::max(0.f, t) / std::max(0.001f, sp.attack), std::pow(2.0, -3.0 * sp.attackCurve));
    else if (t > sp.attack + sp.hold) env = std::pow(10.0, -3.0 * (t - sp.attack - sp.hold) / std::max(1.f, sp.decay));
    return (float) juce::jlimit(0.0, 1.0, env * fadeOutLevel(sp, msIn));
}

// -----------------------------------------------------------------------------------------------------
// the whole UI at its design size 1000 x 600; the editor scales it (resizable window)
// ---------------------------------------------------------------- kit browser (overlay)
class KitBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void(const juce::File&)> onLoad;
    std::function<void()> onSave, onClose;

    explicit KitBrowser(DrumSlicerProcessor& p) : proc(p)
    {
        addAndMakeVisible(list);
        list.setModel(this); list.setRowHeight(40); list.setOutlineThickness(0);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        for (auto* b : { &saveBtn, &folderBtn, &deleteBtn, &closeBtn }) addAndMakeVisible(b);
        saveBtn.setButtonText("SAVE CURRENT KIT"); saveBtn.getProperties().set("primary", true);
        folderBtn.setButtonText("OPEN FOLDER"); deleteBtn.setButtonText("DELETE"); closeBtn.setButtonText("CLOSE");
        saveBtn.onClick = [this] { if (onSave) onSave(); };
        folderBtn.onClick = [] { ds::kitsRoot().createDirectory(); ds::kitsRoot().startAsProcess(); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        deleteBtn.onClick = [this] { deleteSelected(); };
        saveBtn.setTooltip("Save the sounds, settings and MIDI you have now as a kit");
        deleteBtn.setTooltip("Move the selected kit to the Recycle Bin");
    }
    void refresh()
    {
        kits.clear();
        for (const auto& d : ds::kitsRoot().findChildFiles(juce::File::findDirectories, false))
            if (ds::isKit(d) && !d.getFileName().startsWithChar('.')) kits.add(d);
        std::sort(kits.begin(), kits.end(), [](const juce::File& a, const juce::File& b) {
            return ds::kitData(a).getChildFile("kit.txt").getLastModificationTime() > ds::kitData(b).getChildFile("kit.txt").getLastModificationTime(); });
        saveBtn.setEnabled(proc.getResult() != nullptr && !proc.isBusy());
        list.updateContent(); list.deselectAllRows();
        const int cur = kits.indexOf(proc.getKitDir()); if (cur >= 0) list.selectRow(cur);
        repaint();
    }
    void visibilityChanged() override { if (isVisible()) startTimerHz(10); else { stopTimer(); proc.stopPreview(); } }

    const juce::Rectangle<int> card { 230, 64, 540, 480 };
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.62f));
        const auto c = card.toFloat();
        g.setColour(juce::Colour(0xff15161a)); g.fillRoundedRectangle(c, 12.f);
        g.setColour(ui::panelEdge.brighter(0.3f)); g.drawRoundedRectangle(c.reduced(0.5f), 12.f, 1.f);
        g.setColour(ui::text); g.setFont(ui::font(16.f, true, 0.35f));
        g.drawText("KITS", card.getX() + 22, card.getY() + 16, 200, 22, juce::Justification::centredLeft);
        g.setColour(ui::dim); g.setFont(ui::font(9.5f, true, 0.2f));
        g.drawText(juce::String(kits.size()) + (kits.size() == 1 ? " KIT" : " KITS") + "   |   DOCUMENTS / REKIT / KITS", card.getX() + 90, card.getY() + 16, 400, 22, juce::Justification::centredLeft);
        if (kits.isEmpty())
        {
            g.setColour(ui::dim); g.setFont(ui::font(11.f, true, 0.25f));
            g.drawText("NO SAVED KITS YET  -  ANALYSE A LOOP, THEN SAVE CURRENT KIT", list.getBounds(), juce::Justification::centred);
        }
    }
    void resized() override
    {
        list.setBounds(card.getX() + 12, card.getY() + 50, card.getWidth() - 24, card.getHeight() - 112);
        const int y = card.getBottom() - 50;
        saveBtn.setBounds(card.getX() + 20, y, 170, 32);
        folderBtn.setBounds(card.getX() + 200, y, 110, 32);
        deleteBtn.setBounds(card.getX() + 320, y, 90, 32);
        closeBtn.setBounds(card.getRight() - 110, y, 90, 32);
    }
    void mouseDown(const juce::MouseEvent& e) override { if (!card.contains(e.getPosition()) && onClose) onClose(); }

private:
    DrumSlicerProcessor& proc;
    juce::ListBox list;
    juce::TextButton saveBtn, folderBtn, deleteBtn, closeBtn;
    juce::Array<juce::File> kits;
    bool wasPlaying = false;

    juce::Rectangle<int> playZone(int w, int h) const { return { w - 128, (h - 26) / 2, 26, 26 }; }
    juce::Rectangle<int> loadZone(int w, int h) const { return { w - 92, (h - 26) / 2, 80, 26 }; }

    int getNumRows() override { return kits.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= kits.size()) return;
        const auto& k = kits[row];
        const bool loaded = k == proc.getKitDir(), playing = proc.isPreviewing() && proc.previewingKit() == k;
        const auto r = juce::Rectangle<float>(2.f, 2.f, (float) w - 4.f, (float) h - 4.f);
        g.setColour(sel ? juce::Colours::white.withAlpha(0.07f) : juce::Colours::white.withAlpha(0.025f)); g.fillRoundedRectangle(r, 7.f);
        if (loaded) { g.setColour(ui::amber.withAlpha(0.8f)); g.fillRoundedRectangle(r.withWidth(3.f), 1.5f); }
        g.setColour(loaded ? ui::amber : ui::text); g.setFont(ui::font(12.5f, true, 0.08f));
        g.drawText(k.getFileName(), 16, h / 2 - 15, w - 170, 16, juce::Justification::centredLeft);
        g.setColour(ui::dim); g.setFont(ui::font(9.f, false, 0.1f));
        const auto t = ds::kitData(k).getChildFile("kit.txt").getLastModificationTime();
        g.drawText(t.formatted("%d %b %Y  %H:%M") + (loaded ? "   -   LOADED" : ""), 16, h / 2 + 2, w - 170, 12, juce::Justification::centredLeft);
        // play / stop
        const auto pz = playZone(w, h).toFloat();
        g.setColour(playing ? ui::amber : ui::dim.withAlpha(0.9f)); g.drawEllipse(pz.reduced(1.f), 1.2f);
        juce::Path ic;
        if (playing) ic.addRectangle(pz.getCentreX() - 4.f, pz.getCentreY() - 4.f, 8.f, 8.f);
        else ic.addTriangle(pz.getCentreX() - 3.f, pz.getCentreY() - 5.f, pz.getCentreX() - 3.f, pz.getCentreY() + 5.f, pz.getCentreX() + 5.f, pz.getCentreY());
        g.fillPath(ic);
        // load
        const auto lz = loadZone(w, h).toFloat();
        g.setColour(ui::amber.withAlpha(0.9f)); g.drawRoundedRectangle(lz.reduced(0.5f), 13.f, 1.1f);
        g.setFont(ui::font(9.5f, true, 0.3f)); g.drawText("LOAD", lz.toNearestInt(), juce::Justification::centred);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= kits.size()) return;
        // the event comes from the row itself: its own size and position, exactly what paintListBoxItem drew
        const auto* rc = e.eventComponent;
        const int w = rc != nullptr ? rc->getWidth() : list.getVisibleRowWidth(), h = rc != nullptr ? rc->getHeight() : list.getRowHeight();
        const auto pos = e.getPosition();
        if (playZone(w, h).contains(pos))
        {
            if (proc.isPreviewing() && proc.previewingKit() == kits[row]) proc.stopPreview();
            else proc.previewKit(kits[row]);
            list.repaint();
        }
        else if (loadZone(w, h).contains(pos)) { if (onLoad) onLoad(kits[row]); }
    }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { if (row >= 0 && row < kits.size() && onLoad) onLoad(kits[row]); }
    void timerCallback() override { const bool p = proc.isPreviewing(); if (p || p != wasPlaying) list.repaint(); wasPlaying = p; }

    void deleteSelected()
    {
        const int row = list.getSelectedRow();
        if (row < 0 || row >= kits.size()) return;
        const auto k = kits[row];
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "DELETE KIT", "Move the kit \"" + k.getFileName() + "\" to the Recycle Bin?",
            "DELETE", "CANCEL", this, juce::ModalCallbackFunction::create([this, k, sp = juce::Component::SafePointer<KitBrowser>(this)](int ok)
            {
                if (sp == nullptr || !ok) return;
                proc.stopPreview();
                if (k.moveToTrash() || k.deleteRecursively()) proc.kitRemoved(k);
                refresh();
            }));
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KitBrowser)
};

class DrumSlicerPanel : public juce::Component, public juce::FileDragAndDropTarget, public juce::TextDragAndDropTarget,
                        public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit DrumSlicerPanel(DrumSlicerProcessor& p) : proc(p), shape(p)
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
        addAndMakeVisible(kitsBtn);
        kitsBtn.setButtonText("KITS");
        kitsBtn.setTooltip("Save this kit (sounds, settings, MIDI) or open a saved one - Documents/ReKit/Kits");
        kitsBtn.onClick = [this] { openKits(); };
        addChildComponent(kitBrowser);
        kitBrowser.onClose = [this] { kitBrowser.setVisible(false); };
        kitBrowser.onSave = [this] { askKitName(); };
        kitBrowser.onLoad = [this](const juce::File& k) { proc.stopPreview(); proc.loadKit(k); toast("KIT  " + k.getFileName()); kitBrowser.setVisible(false); };
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
        exportBtn.setTooltip("Shaped one-shots (A or B as chosen) + MIDI into Documents/ReKit/<loop>/EXPORT");
        exportBtn.onClick = [this] { auto d = proc.exportFinal(); if (d.exists()) d.revealToUser(); };

        for (int b = 0; b < 3; ++b)
        {
            addAndMakeVisible(srcBtn[(size_t) b]);
            srcBtn[(size_t) b].setButtonText(b == 0 ? "A  LEARNED" : b == 1 ? "B  BEST HIT" : "C  YOUR SAMPLE");
            srcBtn[(size_t) b].setTooltip(b == 0 ? "The sound averaged over all its hits: stem dirt cancels out"
                                          : b == 1 ? "The cleanest real hits, other drums subtracted"
                                                   : "A sample of yours: drop a file on the pad (or right-click the pad > LOAD YOUR SAMPLE)");
            srcBtn[(size_t) b].onClick = [this, b] { setSource(selected, b); };
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
        shape.setTooltip("Click: listen.  Flags: START / END.  Squares: drag sideways = fade length, up / down = curve.  Double-click a handle = reset.  Right-click = reset menu.");
        updateVisibility();
        startTimerHz(30);
    }
    ~DrumSlicerPanel() override { setLookAndFeel(nullptr); }

    // drops: files from Explorer / MediaBay / Pool, and hosts that hand over a path or file:// URL as text
    static bool isAudio(const juce::File& f) { return f.existsAsFile() && f.hasFileExtension("wav;wave;aif;aiff;flac;mp3;ogg"); }
    static juce::File fileFromText(const juce::String& t)
    {
        juce::StringArray lines; lines.addLines(t);
        for (auto l : lines)
        {
            l = l.trim().unquoted();
            if (l.startsWithIgnoreCase("file:")) l = juce::URL(l).getLocalFile().getFullPathName();
            if (juce::File::isAbsolutePath(l) && isAudio(juce::File(l))) return juce::File(l);
        }
        return {};
    }
    bool isInterestedInFileDrag(const juce::StringArray& f) override { for (auto& p : f) if (isAudio(juce::File(p))) return true; return false; }
    // a file let go over a pad becomes that pad's "C" (your sample); anywhere else it is a new loop
    int padAt(int x, int y) const
    {
        auto res = proc.getResult(); if (!res) return -1;
        for (int i = 0; i < ds::kPads; ++i) if (res->pads[(size_t) i].valid && padRect(i).contains(x, y)) return i;
        return -1;
    }
    void dropFile(const juce::File& f, int x, int y)
    {
        const int pad = padAt(x, y);
        if (pad >= 0) { selected = pad; if (proc.setPadSample(pad, f)) toast("C  " + f.getFileNameWithoutExtension()); updateVisibility(); }
        else proc.startAnalysis(f, true);
    }
    void fileDragEnter(const juce::StringArray&, int x, int y) override { hover = true; hoverPad = padAt(x, y); repaint(); }
    void fileDragMove(const juce::StringArray&, int x, int y) override { const int p = padAt(x, y); if (p != hoverPad) { hoverPad = p; repaint(); } }
    void fileDragExit(const juce::StringArray&) override { hover = false; hoverPad = -1; repaint(); }
    void filesDropped(const juce::StringArray& f, int x, int y) override
    {
        hover = false; hoverPad = -1;
        for (auto& p : f) if (isAudio(juce::File(p))) { dropFile(juce::File(p), x, y); break; }
        repaint();
    }
    bool isInterestedInTextDrag(const juce::String& t) override { return fileFromText(t).existsAsFile(); }
    void textDragEnter(const juce::String&, int x, int y) override { hover = true; hoverPad = padAt(x, y); repaint(); }
    void textDragMove(const juce::String&, int x, int y) override { const int p = padAt(x, y); if (p != hoverPad) { hoverPad = p; repaint(); } }
    void textDragExit(const juce::String&) override { hover = false; hoverPad = -1; repaint(); }
    void textDropped(const juce::String& t, int x, int y) override { hover = false; hoverPad = -1; auto f = fileFromText(t); if (f.existsAsFile()) dropFile(f, x, y); repaint(); }

    // ---------------------------------------------------------------- layout (1000 x 600)
    const juce::Rectangle<int> display { 20, 76, 960, 140 }, padsPanel { 20, 226, 624, 206 }, editPanel { 20, 440, 624, 140 },
                               rightPanel { 654, 226, 326, 262 }, soundPanel { 654, 496, 326, 84 };
    juce::Rectangle<int> padRect(int i) const { return { 30 + (i % 4) * 152, 236 + (i / 4) * 100, 146, 92 }; }
    bool padHasC(int i) const { auto r = proc.getResult(); return r && r->pads[(size_t) i].user.getNumSamples() > 0; }
    juce::Rectangle<int> abRect(int i) const { auto r = padRect(i); const int w = padHasC(i) ? 60 : 40; return { r.getRight() - 10 - w, r.getBottom() - 30, w, 20 }; }

    void resized() override
    {
        kitsBtn.setBounds(display.getRight() - 238, display.getY() + 10, 60, 22);
        loadBtn.setBounds(display.getRight() - 172, display.getY() + 10, 60, 22);
        reBtn.setBounds(display.getRight() - 106, display.getY() + 10, 94, 22);
        for (int i = 0; i < ds::kPads; ++i)
        {
            vol[(size_t) i].setBounds(712, 266, 64, 64); pan[(size_t) i].setBounds(858, 266, 64, 64);
            punch[(size_t) i].setBounds(690, 503, 42, 42); sus[(size_t) i].setBounds(796, 503, 42, 42); pitch[(size_t) i].setBounds(902, 503, 42, 42);
        }
        master.setBounds(669, 362, 42, 42); sens.setBounds(749, 362, 42, 42); grid.setBounds(829, 362, 42, 42); bpm.setBounds(909, 362, 42, 42);
        dragMidi.setBounds(666, 440, 134, 36);
        modeBtn[0].setBounds(806, 440, 52, 36); modeBtn[1].setBounds(862, 440, 52, 36);
        exportBtn.setBounds(918, 440, 52, 36);
        srcBtn[0].setBounds(32, 450, 102, 22); srcBtn[1].setBounds(133, 450, 102, 22); srcBtn[2].setBounds(234, 450, 122, 22);
        dragWav.setBounds(552, 450, 80, 22);
        shape.setBounds(30, 478, 604, 96);
    }

    // ---------------------------------------------------------------- kits
    void toast(const juce::String& t) { toastText = t; toastUntil = juce::Time::getMillisecondCounter() + 2500; repaint(); }

    // right-click a pad: what instrument it is (name + MIDI note), or put its hits on another pad
    void padMenu(int pad)
    {
        auto res = proc.getResult(); if (!res) return;
        juce::PopupMenu types, into, m;
        const auto& list = DrumSlicerProcessor::padTypes();
        for (int t = 0; t < (int) list.size(); ++t) types.addItem(100 + t, list[(size_t) t], true, res->pads[(size_t) pad].name.upToFirstOccurrenceOf(" ", false, false) == list[(size_t) t]);
        for (int q = 0; q < ds::kPads; ++q) if (q != pad && res->pads[(size_t) q].valid) into.addItem(200 + q, res->pads[(size_t) q].name);
        m.addSectionHeader(res->pads[(size_t) pad].name + "   (MIDI " + juce::MidiMessage::getMidiNoteName(res->pads[(size_t) pad].note, true, true, 3) + ")");
        m.addSubMenu("THIS PAD IS A ...", types);
        m.addSubMenu("MERGE INTO", into, into.getNumItems() > 0);
        m.addSeparator();
        m.addItem(300, "LOAD YOUR SAMPLE (C) ...");
        m.addItem(301, "REMOVE YOUR SAMPLE", res->pads[(size_t) pad].user.getNumSamples() > 0);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(padRect(pad))),
            [this, pad, sp = juce::Component::SafePointer<DrumSlicerPanel>(this)](int id)
            {
                if (sp == nullptr || id == 0) return;
                if (id >= 100 && id < 200) { proc.setPadType(pad, DrumSlicerProcessor::padTypes()[(size_t) (id - 100)]); toast("PAD  " + proc.getResult()->pads[(size_t) pad].name); }
                else if (id == 300)
                {
                    chooser = std::make_unique<juce::FileChooser>("Your sample for this pad", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
                    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this, pad](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile() && proc.setPadSample(pad, f)) { selected = pad; toast("C  " + f.getFileNameWithoutExtension()); updateVisibility(); repaint(); } });
                }
                else if (id == 301) { proc.clearPadSample(pad); toast("BACK TO A"); }
                else if (id >= 200) { const int q = id - 200; proc.mergePad(pad, q); selected = q; toast("MERGED INTO  " + proc.getResult()->pads[(size_t) q].name); }
                updateVisibility(); repaint();
            });
    }

    void openKits() { kitBrowser.setBounds(getLocalBounds()); kitBrowser.refresh(); kitBrowser.setVisible(true); kitBrowser.toFront(false); }

    void askKitName()
    {
        auto* w = new juce::AlertWindow("SAVE KIT", "Name of the kit (a folder in Documents/ReKit/Kits):", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", proc.baseName());
        w->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, w, sp = juce::Component::SafePointer<DrumSlicerPanel>(this)](int r)
            {
                if (sp == nullptr || r != 1) return;
                const auto name = juce::File::createLegalFileName(w->getTextEditorContents("name").trim());
                if (name.isEmpty()) return;
                const auto dir = ds::kitsRoot().getChildFile(name);
                if (ds::isKit(dir) && dir != proc.getKitDir())
                {
                    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "SAVE KIT", "A kit called \"" + name + "\" exists. Replace it?",
                        "REPLACE", "CANCEL", this, juce::ModalCallbackFunction::create([this, name, sp](int ok) { if (sp != nullptr && ok) doSaveKit(name); }));
                    return;
                }
                doSaveKit(name);
            }), true);
    }

    void doSaveKit(const juce::String& name)
    {
        const auto k = proc.saveKit(name);
        toast(k.isDirectory() ? "KIT SAVED  " + k.getFileName() : juce::String("COULD NOT SAVE THE KIT"));
        if (kitBrowser.isVisible()) kitBrowser.refresh();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto res = proc.getResult();
        for (int i = 0; i < ds::kPads; ++i)
        {
            if (!padRect(i).contains(e.getPosition())) continue;
            selected = i;
            if (e.mods.isPopupMenu() && res && res->pads[(size_t) i].valid) { padMenu(i); updateVisibility(); repaint(); return; }
            if (res && res->pads[(size_t) i].valid && abRect(i).contains(e.getPosition()))
            {
                const auto ab = abRect(i); const int n = padHasC(i) ? 3 : 2;
                setSource(i, juce::jlimit(0, n - 1, (e.getPosition().x - ab.getX()) * n / ab.getWidth()));
            }
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
        const int src = proc.padSource(selected);
        for (int k = 0; k < 3; ++k) srcBtn[(size_t) k].setToggleState(src == k, juce::dontSendNotification);
        bool lit = false;
        for (size_t i = 0; i < 8; ++i) { levels[i] = std::max(proc.padLevel[i].exchange(0.f) > 0.f ? 1.f : 0.f, levels[i] * 0.85f); lit |= levels[i] > 0.01f; }
        auto res = proc.getResult();
        const bool hasPad = res && res->pads[(size_t) selected].valid;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &dragMidi, &exportBtn, &modeBtn[0], &modeBtn[1], &reBtn }) c->setEnabled(res != nullptr);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &srcBtn[0], &srcBtn[1], &dragWav }) c->setEnabled(hasPad);
        const bool hasC = hasPad && res->pads[(size_t) selected].user.getNumSamples() > 0;
        srcBtn[2].setEnabled(hasC);
        srcBtn[2].setTooltip(hasC ? "Your sample: " + res->pads[(size_t) selected].userName
                                  : juce::String("A sample of yours: drop a file on the pad (or right-click the pad > LOAD YOUR SAMPLE)"));
        const bool toasting = juce::Time::getMillisecondCounter() < toastUntil;
        if (lit || proc.isBusy() || wasBusy || toasting || wasToasting) repaint();
        wasToasting = toasting;
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
        if (juce::Time::getMillisecondCounter() < toastUntil && toastText.isNotEmpty())
        {
            const auto t = juce::Rectangle<float>(0, 0, 320, 26).withCentre({ (float) display.getCentreX(), (float) display.getY() + 22.f });
            g.setColour(ui::well.withAlpha(0.95f)); g.fillRoundedRectangle(t, 13.f);
            g.setColour(ui::amber); g.drawRoundedRectangle(t, 13.f, 1.2f);
            g.setFont(ui::font(10.f, true, 0.3f)); g.drawText(toastText, t.toNearestInt(), juce::Justification::centred);
        }
        drawPads(g, res);
        drawRight(g, res);
    }

private:
    void setSource(int pad, int src)
    {
        proc.setPadSource(pad, src);
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
        g.drawText("REKIT", 32, 18, 400, 30, juce::Justification::centredLeft);
        {   // version, small, next to the name
           #ifdef JucePlugin_VersionString
            const juce::String ver = "v" JucePlugin_VersionString;
           #else
            const juce::String ver = "v1.5";
           #endif
            const float tw = juce::GlyphArrangement::getStringWidth(ui::font(25.f, true, 0.32f), "REKIT");
            g.setColour(ui::dim); g.setFont(ui::font(9.5f, true, 0.2f));
            g.drawText(ver, juce::Rectangle<float>(32.f + tw + 8.f, 30.f, 60.f, 12.f).toNearestInt(), juce::Justification::centredLeft);
        }
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
            g.drawText("DRAG A FILE FROM THE POOL / MEDIABAY / EXPLORER  -  OR CLICK TO BROWSE", d.withTrimmedTop(36).toNearestInt(), juce::Justification::centred);
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
        g.drawText(proc.displayName(), juce::Rectangle<float>(x0, d.getBottom() - 22, w, 12).toNearestInt(), juce::Justification::centredRight);
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
            const int src = proc.padSource(i), nsrc = padHasC(i) ? 3 : 2;
            g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(ab, 5.f);
            g.setColour(ui::panelEdge.brighter(0.2f)); g.drawRoundedRectangle(ab.reduced(0.5f), 5.f, 1.f);
            g.setFont(ui::font(10.f, true));
            const float cw = ab.getWidth() / nsrc;
            for (int k = 0; k < nsrc; ++k)
            {
                g.setColour(src == k ? ui::amber : ui::dim);
                g.drawText(juce::String::charToString((juce::juce_wchar) ('A' + k)), juce::Rectangle<float>(ab.getX() + k * cw, ab.getY(), cw, ab.getHeight()).toNearestInt(), juce::Justification::centred);
                if (k > 0) { g.setColour(ui::panelEdge.brighter(0.2f)); g.drawVerticalLine((int) (ab.getX() + k * cw), ab.getY() + 4, ab.getBottom() - 4); }
            }
        }
        if (hoverPad >= 0 && hoverPad < ds::kPads)                        // dropping a sample here: it becomes "C"
        {
            const auto r = padRect(hoverPad).toFloat();
            g.setColour(ui::amber.withAlpha(0.15f)); g.fillRoundedRectangle(r, 10.f);
            g.setColour(ui::amber); g.drawRoundedRectangle(r.reduced(1.f), 10.f, 2.f);
            g.setFont(ui::font(10.f, true, 0.25f)); g.drawText("DROP = YOUR SAMPLE (C)", r.withTrimmedTop(r.getHeight() * 0.35f).withHeight(14.f).toNearestInt(), juce::Justification::centred);
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
    int hoverPad = -1;
    std::array<float, 8> levels {};
    std::vector<std::pair<float, float>> peaks;

    std::array<juce::Slider, ds::kPads> vol, pan, punch, sus, pitch;
    juce::Slider master, sens, grid, bpm;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> atts;
    juce::TextButton loadBtn, reBtn, exportBtn, kitsBtn;
    KitBrowser kitBrowser { proc };
    juce::String toastText; juce::uint32 toastUntil = 0; bool wasToasting = false;
    std::array<juce::TextButton, 2> modeBtn;
    std::array<juce::TextButton, 3> srcBtn;
    ui::DragButton dragMidi, dragWav;
    ShapeView shape;
    juce::TooltipWindow tips { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerPanel)
};

// resizable window (fixed 5:3), the panel is scaled - everything stays sharp because it is drawn in code
class DrumSlicerEditor : public juce::AudioProcessorEditor
{
public:
    explicit DrumSlicerEditor(DrumSlicerProcessor& p) : juce::AudioProcessorEditor(p), proc(p), panel(p)
    {
        addAndMakeVisible(panel);
        setResizable(true, true);
        setResizeLimits(700, 420, 2000, 1200);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio(1000.0 / 600.0);
        const int w = juce::jlimit(700, 2000, (int) proc.apvts.state.getProperty("uiWidth", 1000));
        setSize(w, juce::roundToInt(w * 0.6));
    }
    void resized() override
    {
        panel.setBounds(0, 0, 1000, 600);
        panel.setTransform(juce::AffineTransform::scale((float) getWidth() / 1000.f));
        proc.apvts.state.setProperty("uiWidth", getWidth(), nullptr);
    }
private:
    DrumSlicerProcessor& proc;
    DrumSlicerPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerEditor)
};
