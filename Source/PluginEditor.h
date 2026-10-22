#pragma once
#include "PluginProcessor.h"
#ifndef HAS_ASSETS
 #define HAS_ASSETS 0
#endif
#if HAS_ASSETS
 #include <BinaryData.h>
#endif
void rekitRaiseFrame(void* nativeHandle);   // WinRaise.cpp (does nothing outside Windows)

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
        const bool compact = b.getProperties()["compact"];
        g.setFont(font(primary ? (compact ? 11.5f : 13.f) : 10.5f, true, primary ? (compact ? 0.06f : 0.18f) : 0.12f));
        g.setColour(primary ? juce::Colour(0xff1a1206) : (b.getToggleState() ? amber : text.withAlpha(b.isEnabled() ? 0.85f : 0.3f)));
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
};

// a small square play / stop button
struct PlayButton : juce::Button
{
    PlayButton() : juce::Button("play") {}
    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        const bool on = getToggleState();
        g.setColour(on ? amber.withAlpha(0.18f) : juce::Colour(0xff1c1e23).brighter(over ? 0.08f : 0.f)); g.fillRoundedRectangle(r, 8.f);
        g.setColour(on ? amber : panelEdge.brighter(0.3f)); g.drawRoundedRectangle(r, 8.f, 1.f);
        g.setColour(!isEnabled() ? faint : on ? amber : ui::text);
        const auto c = r.getCentre();
        if (on) g.fillRoundedRectangle(juce::Rectangle<float>(10.f, 10.f).withCentre(c), 1.5f);
        else { juce::Path t; t.addTriangle(c.x - 4.5f, c.y - 7.f, c.x - 4.5f, c.y + 7.f, c.x + 7.f, c.y); g.fillPath(t); }
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
            const float e = std::min(1.f / std::max(0.05f, std::max(std::abs(pk.first), std::abs(pk.second))), level(sp, ms) * gainLin(sp));   // GAIN shows on the wave
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
        // GAIN fader (this source's own level), right edge
        {
            const auto fr = gainRect();
            const bool on = hot == 7 || drag == 7;
            g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(fr, 4.f);
            g.setColour(ui::panelEdge.brighter(0.2f)); g.drawRoundedRectangle(fr, 4.f, 1.f);
            const float y0 = gainToY(0.f);
            g.setColour(ui::dim); g.drawHorizontalLine((int) y0, fr.getX() + 2.f, fr.getRight() - 2.f);
            const float y = gainToY(sp.gainDb);
            g.setColour((on ? ui::amber : col).withAlpha(0.35f));
            g.fillRect(juce::Rectangle<float>(fr.getX() + 3.f, std::min(y, y0), fr.getWidth() - 6.f, std::abs(y0 - y)));
            g.setColour(on ? ui::amber : ui::text); g.fillRoundedRectangle(juce::Rectangle<float>(fr.getWidth() + 4.f, 6.f).withCentre({ fr.getCentreX(), y }), 2.f);
            g.setFont(ui::font(8.f, true, 0.1f)); g.setColour(on ? ui::amber : ui::dim);
            g.drawText("GAIN", juce::Rectangle<float>(fr.getX() - 8.f, fr.getBottom() + 2.f, fr.getWidth() + 16.f, 10.f), juce::Justification::centred);
            if (on || std::abs(sp.gainDb) > 0.05f)
                g.drawText((sp.gainDb > 0 ? "+" : "") + juce::String(sp.gainDb, 1) + " dB", juce::Rectangle<float>(fr.getX() - 62.f, y - 6.f, 58.f, 12.f), juce::Justification::centredRight);
        }
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
        else proc.audition(pad);            // click on the sound = listen
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
            case 7: { float gdb = yToGain(gainToY(startSp.gainDb) + dy * (e.mods.isShiftDown() ? 0.25f : 1.f));   // SHIFT: fine
                      if (std::abs(gdb) < 0.25f) gdb = 0.f;                                   // 0 dB clicks in
                      set("gain", std::round(gdb * 10.f) / 10.f); break; }
            default: break;
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (drag) { endGesture(); proc.audition(pad); } drag = 0; repaint(); }
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
        if (gainRect().expanded(4.f, 4.f).contains(p)) return 7;
        if (p.getDistanceFrom(fadeInHandle(sp)) < 10.f) return 2;
        if (p.getDistanceFrom(fadeOutHandle(sp)) < 10.f) return 6;
        const float xs = msToX(sp.start), xe = msToX(std::min(sp.end, totalMs));
        const float ds = std::abs(p.x - xs), de = std::abs(p.x - xe);
        if (std::min(ds, de) < 7.f && p.y >= w.getY() - 4 && p.y <= w.getBottom() + 4) return ds <= de ? 1 : 5;
        return 0;
    }
    juce::Rectangle<float> wave() const { return getLocalBounds().toFloat().reduced(10.f, 0).withTrimmedTop(8.f).withTrimmedBottom(20.f).withTrimmedRight(26.f); }
    // GAIN: -24 .. +12 dB, 0 dB at 2/3 of the way down... a dB scale so small moves near 0 are easy
    juce::Rectangle<float> gainRect() const { const auto w = wave(); return { (float) getWidth() - 24.f, w.getY() + 10.f, 12.f, w.getHeight() - 14.f }; }
    float gainToY(float db) const { const auto r = gainRect(); return r.getY() + (12.f - juce::jlimit(-24.f, 12.f, db)) / 36.f * r.getHeight(); }
    float yToGain(float y) const { const auto r = gainRect(); return juce::jlimit(-24.f, 12.f, 12.f - (y - r.getY()) / r.getHeight() * 36.f); }
    static float gainLin(const ShapeParams& sp) { return juce::Decibels::decibelsToGain(sp.gainDb); }
    juce::Rectangle<float> zoomRect() const { return { (float) getWidth() - 76.f, 4.f, 40.f, 14.f }; }
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
        switch (d) { case 1: return { "st" }; case 5: return { "en" }; case 2: return { "atk", "fic" }; case 6: return { "fo", "foc" }; case 7: return { "gain" }; default: return {}; }
    }
    void beginGesture() { for (auto* id : idsOf(drag)) if (auto* p = prm(id)) p->beginChangeGesture(); }
    void endGesture() { for (auto* id : idsOf(drag)) if (auto* p = prm(id)) p->endChangeGesture(); }
    void resetIds(std::initializer_list<const char*> ids) { for (auto* id : ids) if (auto* p = prm(id)) p->setValueNotifyingHost(p->getDefaultValue()); repaint(); }
    void resetHandle(int d) { for (auto* id : idsOf(d)) if (auto* p = prm(id)) p->setValueNotifyingHost(p->getDefaultValue()); }
    void resetMenu()
    {
        juce::PopupMenu m;
        m.addItem(1, "RESET FADES"); m.addItem(2, "RESET START / END"); m.addItem(4, "RESET GAIN"); m.addItem(3, "RESET THE WHOLE SHAPE");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this, sp = juce::Component::SafePointer<ShapeView>(this)](int id)
        {
            if (sp == nullptr) return;
            if (id == 1) resetIds({ "atk", "fic", "fo", "foc", "hold", "dcy" });
            if (id == 2) resetIds({ "st", "en" });
            if (id == 3) resetIds({ "st", "en", "atk", "fic", "fo", "foc", "hold", "dcy", "gain" });
            if (id == 4) resetIds({ "gain" });
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

// ---------------------------------------------------------------- your samples, inside ReKit
// FOLDERS: your sample folders (added once, remembered), click = listen as loud as the pad.
// SIMILAR: the sounds of your archive nearest to the pad's sound (scanned once; loops and long files left out).
// Double-click / drag onto a pad / USE = it becomes that pad's "C". Arrow keys walk the list and play each file.
class SampleBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void()> onClose;
    std::function<void(const juce::File&)> onUse;         // set as "C" on the selected pad
    std::function<void(const juce::File&)> onKept;        // a tried one was kept
    std::function<int()> targetPad;
    std::function<juce::String()> targetName;

    explicit SampleBrowser(DrumSlicerProcessor& p) : proc(p)
    {
        addAndMakeVisible(list);
        list.setModel(this); list.setRowHeight(24); list.setOutlineThickness(0);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setWantsKeyboardFocus(true);
        for (auto* b : { &addBtn, &remBtn, &upBtn, &useBtn, &revealBtn, &closeBtn, &tabFolders, &tabSimilar, &scanBtn, &limitBtn, &moreBtn }) addAndMakeVisible(b);
        addChildComponent(firstBtn);
        firstBtn.setButtonText("ADD SAMPLE FOLDER"); firstBtn.getProperties().set("primary", true);
        firstBtn.onClick = [this] { addFolder(); };
        addBtn.setButtonText("+"); remBtn.setButtonText("-"); upBtn.setButtonText("UP");
        useBtn.setButtonText("USE ON PAD"); useBtn.getProperties().set("primary", true);
        revealBtn.setButtonText("OPEN FOLDER"); closeBtn.setButtonText("X");
        tabFolders.setButtonText("FOLDERS"); tabSimilar.setButtonText("SIMILAR");
        tabFolders.onClick = [this] { setMode(0); };
        tabSimilar.onClick = [this] { setMode(1); };
        scanBtn.setButtonText("RESCAN"); scanBtn.setTooltip("Look for new / changed files in your sample folders (RESCAN ALL with SHIFT)");
        scanBtn.onClick = [this] { proc.library->scan(juce::ModifierKeys::currentModifiers.isShiftDown()); repaint(); };
        limitBtn.setTooltip("Files longer than this are loops / long files and left out (click to change)");
        limitBtn.onClick = [this] { const float l = proc.library->maxSeconds(); const float n = l < 6 ? 8.f : l < 9 ? 10.f : l < 11 ? 15.f : 5.f;
                                    proc.library->setMaxSeconds(n); saveSettings(); updateLimit(); proc.library->scan(false); };
        moreBtn.setButtonText("MORE"); moreBtn.setTooltip("5 more");
        addAndMakeVisible(tryBtn);
        tryBtn.setButtonText("TRY IN KIT");
        tryBtn.setTooltip("On: every file you pick (click / arrow keys) sits on the pad for now and plays in the pattern. USE ON PAD keeps it, off goes back.");
        tryBtn.onClick = [this] { tryMode = !tryMode; tryBtn.setToggleState(tryMode, juce::dontSendNotification); if (!tryMode) proc.endTry(false); };
        for (int k = 0; k < 3; ++k)
        {   // what to look like: A (the loop's own sound, the default), B, or your C
            addAndMakeVisible(likeBtn[(size_t) k]);
            likeBtn[(size_t) k].setButtonText(juce::String::charToString((juce::juce_wchar) ('A' + k)));
            likeBtn[(size_t) k].setTooltip(k == 0 ? "Look for sounds like A: the loop's own sound" : k == 1 ? "Like B: the best real hit" : "Like C: the sample you put on the pad");
            likeBtn[(size_t) k].onClick = [this, k] { likeSrc = k; want = 5; findSimilar(true); };
        }
        moreBtn.onClick = [this] { want += 5; if (want > (int) matches.size()) findSimilar(true); else { list.updateContent(); list.repaint(); } };
        addBtn.setTooltip("Add a sample folder (remembered)"); remBtn.setTooltip("Remove this folder from the list (files stay)");
        revealBtn.setTooltip("Show the selected file in Explorer");
        addAndMakeVisible(roots);
        roots.onChange = [this] { const int i = roots.getSelectedItemIndex(); if (i >= 0 && i < rootDirs.size()) open(rootDirs[i]); };
        addAndMakeVisible(filter);
        filter.setTextToShowWhenEmpty("FILTER  (kick, 808, snare ...)", ui::dim);
        filter.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
        filter.setColour(juce::TextEditor::outlineColourId, ui::panelEdge.brighter(0.2f));
        filter.setColour(juce::TextEditor::textColourId, ui::text);
        filter.onTextChange = [this] { rebuild(); };
        addBtn.onClick = [this] { addFolder(); };
        remBtn.onClick = [this] { removeFolder(); };
        upBtn.onClick = [this] { if (dir.getParentDirectory() != dir && isInsideRoot(dir.getParentDirectory())) open(dir.getParentDirectory()); };
        useBtn.onClick = [this] {
            auto f = selectedFile();
            if (proc.isTrying() && proc.triedFile() == f) { proc.endTry(true); if (onKept) onKept(f); return; }   // already on the pad: keep it
            if (f.existsAsFile() && onUse) onUse(f); };
        revealBtn.onClick = [this] { if (auto f = selectedFile(); f.existsAsFile()) f.revealToUser(); else if (dir.isDirectory()) dir.revealToUser(); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        loadSettings();
        updateLimit();
    }
    ~SampleBrowser() override { if (rank) rank->cancel = true; saveSettings(); }

    void showFor() { refreshRoots(); if (!dir.isDirectory() && !rootDirs.isEmpty()) open(rootDirs[0]); else rebuild(); setMode(mode); list.grabKeyboardFocus(); repaint(); }
    void showSimilar() { setMode(1); }
    void visibilityChanged() override { if (isVisible()) startTimerHz(10); else { stopTimer(); proc.stopPreview(); stopTrying(); saveSettings(); } }

    void paint(juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(c.translated(0, 2), 12.f);
        g.setColour(juce::Colour(0xff15161a)); g.fillRoundedRectangle(c, 12.f);
        g.setColour(ui::panelEdge.brighter(0.3f)); g.drawRoundedRectangle(c.reduced(0.5f), 12.f, 1.f);
        const auto tn = targetName ? targetName() : juce::String();
        g.setColour(tn.isNotEmpty() ? ui::amber : ui::dim); g.setFont(ui::font(9.5f, true, 0.2f));
        g.drawText(tn.isNotEmpty() ? "FOR " + tn : juce::String("SELECT A PAD"), 176, 10, getWidth() - 220, 22, juce::Justification::centredLeft);
        if (rootDirs.isEmpty())
        {   // first time: say what this is
            auto r = juce::Rectangle<int>(20, 60, getWidth() - 40, 170);
            g.setColour(ui::text); g.setFont(ui::font(11.f, true, 0.15f));
            g.drawFittedText("CHOOSE A SAMPLE OF YOURS FOR A PAD", r.removeFromTop(22), juce::Justification::centredLeft, 1);
            g.setColour(ui::dim.brighter(0.2f)); g.setFont(ui::font(10.f, false, 0.05f));
            g.drawFittedText("1.  Add your one-shot folder (once - it is remembered).\n"
                             "2.  Click a file (or use the arrow keys): you hear it as loud as the pad.\n"
                             "3.  Double-click it, drag it onto a pad, or press USE ON PAD:\n"
                             "     it becomes the pad's sound C. The MIDI stays the same.\n"
                             "4.  SIMILAR: the sounds of your folders nearest to the pad.",
                             r.withTrimmedTop(6), juce::Justification::topLeft, 8, 1.f);
            return;
        }
        if (mode == 0)
        {
            g.setColour(ui::dim); g.setFont(ui::font(9.f, true, 0.1f));
            g.drawText(dir.isDirectory() ? shortPath(dir) : juce::String(), 16, 94, getWidth() - 32, 14, juce::Justification::centredLeft);
            if (items.isEmpty()) { g.setColour(ui::dim); g.setFont(ui::font(10.f, true, 0.2f)); g.drawText("NOTHING HERE", list.getBounds(), juce::Justification::centred); }
            return;
        }
        // SIMILAR: what the library knows
        auto& lib = *proc.library;
        g.setColour(ui::dim.brighter(0.2f)); g.setFont(ui::font(9.f, true, 0.1f));
        {
            const auto st = lib.status();
            g.setColour(ui::text.withAlpha(0.85f)); g.setFont(ui::font(9.5f, true, 0.1f));
            g.drawText(st.upToFirstOccurrenceOf("\n", false, false), 14, 36, getWidth() - 150, 14, juce::Justification::centredLeft);
            g.setColour(ui::dim); g.setFont(ui::font(8.f, true, 0.05f));
            g.drawFittedText(st.fromFirstOccurrenceOf("\n", false, false), 14, 50, getWidth() - 150, 12, juce::Justification::centredLeft, 1, 0.8f);
        }
        if (lib.scanning())
        {
            const auto bar = juce::Rectangle<float>(14.f, 62.f, (float) getWidth() - 28.f, 4.f);
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(bar, 2.f);
            g.setColour(ui::amber); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * lib.progress()), 2.f);
        }
        g.setColour(ui::text.withAlpha(0.8f)); g.setFont(ui::font(9.5f, true, 0.15f));
        g.drawText(queryText, 14, 72, getWidth() - 180, 20, juce::Justification::centredLeft);
        if (rank && !ranked && !matches.empty())
        {
            g.setColour(ui::amber.withAlpha(0.8f)); g.setFont(ui::font(8.f, true, 0.1f));
            g.drawText("CHECKING AFTER MATCH  " + juce::String(rank->done.load()) + " / " + juce::String((int) matches.size()), 14, 86, getWidth() - 28, 10, juce::Justification::centredLeft);
        }
        if (matches.empty())
        {
            g.setColour(ui::dim); g.setFont(ui::font(10.f, true, 0.2f));
            const auto ix = lib.index();
            const juce::String why = !havePad ? "SELECT A PAD WITH A SOUND"
                                   : (!ix || ix->oneShots == 0) ? (lib.scanning() ? "SCANNING YOUR FOLDERS ..." : "PRESS RESCAN TO READ YOUR FOLDERS")
                                   : "NOTHING FOUND";
            g.drawText(why, list.getBounds(), juce::Justification::centred);
        }
    }
    void resized() override
    {
        const int w = getWidth(), h = getHeight();
        tabFolders.setBounds(12, 10, 76, 22); tabSimilar.setBounds(92, 10, 76, 22);
        closeBtn.setBounds(w - 36, 10, 24, 22);

        roots.setBounds(12, 38, w - 24 - 64, 24);
        addBtn.setBounds(w - 72, 38, 28, 24); remBtn.setBounds(w - 40, 38, 28, 24);
        upBtn.setBounds(12, 66, 44, 24);
        filter.setBounds(62, 66, w - 74, 24);
        scanBtn.setBounds(w - 132, 38, 64, 22); limitBtn.setBounds(w - 64, 38, 52, 22);
        moreBtn.setBounds(w - 72, 70, 60, 22);
        for (int k = 0; k < 3; ++k) likeBtn[(size_t) k].setBounds(w - 162 + k * 28, 70, 26, 22);
        list.setBounds(8, mode == 0 ? 110 : 96, w - 16, h - (mode == 0 ? 110 : 96) - 46);
        firstBtn.setBounds(30, 210, w - 60, 40);
        const int bw = (w - 24 - 12) / 3;
        useBtn.setBounds(12, h - 40, bw + 16, 30);
        tryBtn.setBounds(useBtn.getRight() + 6, h - 40, bw - 8, 30);
        revealBtn.setBounds(tryBtn.getRight() + 6, h - 40, w - 12 - tryBtn.getRight() - 6, 30);
    }

private:
    DrumSlicerProcessor& proc;
    juce::ListBox list;
    juce::ComboBox roots;
    juce::TextEditor filter;
    juce::TextButton addBtn, remBtn, upBtn, useBtn, revealBtn, closeBtn, firstBtn, tabFolders, tabSimilar, scanBtn, limitBtn, moreBtn;
    std::array<juce::TextButton, 3> likeBtn;
    juce::TextButton tryBtn;
    bool tryMode = false;
    int likeSrc = 0;
    juce::uint32 lastFind = 0;
    juce::Array<juce::File> rootDirs, items;       // items: folders first, then audio files
    juce::File dir;
    std::unique_ptr<juce::FileChooser> chooser;
    bool wasPlaying = false;
    int mode = 0;                                  // 0 = FOLDERS, 1 = SIMILAR
    // SIMILAR
    std::vector<ds::Match> matches;
    std::shared_ptr<const ds::LibIndex> holder;    // the matches point into it
    std::shared_ptr<DrumSlicerProcessor::RankState> rank;   // each candidate matched in the background
    std::vector<float> after;                      // score after a match (-1 = not known yet)
    bool ranked = false;
    juce::String queryKey, queryText;
    bool havePad = false;
    int want = 5;

    static juce::File settingsFile() { return ds::root().getChildFile("browser.txt"); }
    static bool isAudio(const juce::File& f) { return f.hasFileExtension("wav;wave;aif;aiff;flac;mp3;ogg"); }
    juce::String shortPath(const juce::File& d) const
    {
        for (auto& r : rootDirs) if (d == r || d.isAChildOf(r)) return (r.getFileName() + "/" + d.getRelativePathFrom(r)).trimCharactersAtEnd("/.").toUpperCase();
        return d.getFullPathName();
    }
    bool isInsideRoot(const juce::File& d) const { for (auto& r : rootDirs) if (d == r || d.isAChildOf(r)) return true; return false; }
    void loadSettings()
    {
        juce::StringArray lines; lines.addLines(settingsFile().loadFileAsString());
        for (auto& l : lines)
        {
            const auto k = l.upToFirstOccurrenceOf("=", false, false), v = l.fromFirstOccurrenceOf("=", false, false);
            if (k == "root" && juce::File::isAbsolutePath(v) && juce::File(v).isDirectory()) rootDirs.addIfNotAlreadyThere(juce::File(v));
            else if (k == "last" && juce::File::isAbsolutePath(v)) dir = juce::File(v);
            else if (k == "limit" && v.getFloatValue() >= 1.f) proc.library->setMaxSeconds(v.getFloatValue());
            else if (k == "mode") mode = v.getIntValue() == 1 ? 1 : 0;
        }
        if (!isInsideRoot(dir)) dir = juce::File();
    }
    void saveSettings()
    {
        juce::StringArray lines;
        for (auto& r : rootDirs) lines.add("root=" + r.getFullPathName());
        if (dir.isDirectory()) lines.add("last=" + dir.getFullPathName());
        lines.add("limit=" + juce::String(proc.library->maxSeconds(), 1));
        lines.add("mode=" + juce::String(mode));
        ds::writeText(settingsFile(), lines.joinIntoString("\n"));
    }
    void updateLimit() { limitBtn.setButtonText("< " + juce::String(juce::roundToInt(proc.library->maxSeconds())) + " S"); }
    void setMode(int m)
    {
        mode = m;
        tabFolders.setToggleState(mode == 0, juce::dontSendNotification); tabSimilar.setToggleState(mode == 1, juce::dontSendNotification);
        list.setRowHeight(mode == 0 ? 24 : 40);
        updateControls();
        resized();
        if (mode == 1)
        {
            proc.library->scanOnce();                  // new / changed files: checked once per session, quietly
            want = 5; findSimilar(true);
        }
        else rebuild();
        saveSettings();
        repaint();
    }
    void refreshRoots()
    {
        roots.clear(juce::dontSendNotification);
        for (int i = 0; i < rootDirs.size(); ++i) roots.addItem(rootDirs[i].getFileName().toUpperCase(), i + 1);
        for (int i = 0; i < rootDirs.size(); ++i) if (dir == rootDirs[i] || dir.isAChildOf(rootDirs[i])) roots.setSelectedItemIndex(i, juce::dontSendNotification);
        remBtn.setEnabled(!rootDirs.isEmpty());
        updateControls();
    }
    void updateControls()
    {   // no folder yet: only the explanation and one big button
        const bool any = !rootDirs.isEmpty(), f = any && mode == 0, sm = any && mode == 1;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &roots, &remBtn, &upBtn, &filter, &addBtn }) c->setVisible(f);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &scanBtn, &limitBtn, &moreBtn, &likeBtn[0], &likeBtn[1], &likeBtn[2] }) c->setVisible(sm);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &list, &useBtn, &revealBtn, &tabFolders, &tabSimilar, &tryBtn }) c->setVisible(any);
        firstBtn.setVisible(!any);
    }
    void addFolder()
    {
        chooser = std::make_unique<juce::FileChooser>("Your sample folder", dir.isDirectory() ? dir : juce::File());
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, sp = juce::Component::SafePointer<SampleBrowser>(this)](const juce::FileChooser& fc)
            {
                if (sp == nullptr) return;
                const auto d = fc.getResult();
                if (!d.isDirectory()) return;
                rootDirs.addIfNotAlreadyThere(d);
                saveSettings(); open(d);
                proc.library->scan(false);                 // the archive learns the new folder in the background
            });
    }
    void removeFolder()
    {
        const int i = roots.getSelectedItemIndex();
        if (i < 0 || i >= rootDirs.size()) return;
        rootDirs.remove(i);
        dir = rootDirs.isEmpty() ? juce::File() : rootDirs[0];
        saveSettings(); refreshRoots(); rebuild();
        proc.library->scan(false);
    }
    void open(const juce::File& d)
    {
        dir = d; filter.clear();
        refreshRoots(); rebuild();
        list.scrollToEnsureRowIsOnscreen(0);
    }
    void rebuild()
    {
        if (mode != 0) return;
        items.clear();
        if (dir.isDirectory())
        {
            const auto q = filter.getText().trim().toLowerCase();
            juce::Array<juce::File> dirs, files;
            for (const auto& e : juce::RangedDirectoryIterator(dir, false, "*", juce::File::findFilesAndDirectories))
            {
                const auto f = e.getFile();
                if (f.getFileName().startsWithChar('.')) continue;
                if (e.isDirectory()) { if (q.isEmpty() || f.getFileName().toLowerCase().contains(q)) dirs.add(f); }
                else if (isAudio(f) && (q.isEmpty() || f.getFileName().toLowerCase().contains(q))) files.add(f);
            }
            auto byName = [](const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural(b.getFileName()) < 0; };
            std::sort(dirs.begin(), dirs.end(), byName); std::sort(files.begin(), files.end(), byName);
            items.addArray(dirs); items.addArray(files);
        }
        list.updateContent(); list.deselectAllRows(); list.repaint(); repaint();
    }
    // the pad's sound as it is chosen now (A / B / C) against the library
    void findSimilar(bool force)
    {
        if (mode != 1) return;
        auto r = proc.getResult();
        const int pad = targetPad ? targetPad() : -1;
        havePad = r && pad >= 0 && pad < ds::kPads && r->pads[(size_t) pad].valid;
        const bool hasC = havePad && r->pads[(size_t) pad].user.getNumSamples() > 0;
        if (likeSrc == 2 && !hasC) likeSrc = 0;
        const int src = likeSrc;
        for (int k = 0; k < 3; ++k) { likeBtn[(size_t) k].setToggleState(k == src, juce::dontSendNotification); likeBtn[(size_t) k].setEnabled(havePad && (k < 2 || hasC)); }
        auto ix = proc.library->index();
        const juce::String key = havePad ? juce::String(proc.getResultVersion()) + "|" + juce::String(pad) + "|" + juce::String(src) + "|"
                                           + juce::String((juce::pointer_sized_int) ix.get()) : juce::String();
        if (!force && key == queryKey) return;
        // while the archive is still being read the list would jump every few seconds: at most every 3 s then
        const auto now = juce::Time::getMillisecondCounter();
        const bool onlyIndexChanged = queryKey.upToLastOccurrenceOf("|", false, false) == key.upToLastOccurrenceOf("|", false, false);
        if (!force && onlyIndexChanged && proc.library->scanning() && now - lastFind < 3000) return;
        lastFind = now;
        queryKey = key;
        if (rank) rank->cancel = true;
        rank.reset(); ranked = false; after.clear();
        matches.clear(); holder.reset();
        if (havePad)
        {
            const auto& pd = r->pads[(size_t) pad];
            matches = proc.library->nearest(pd.get(src == 1, src == 2), r->sr, pd.name, std::max(30, want), holder, pd.userPath);
            after.assign(matches.size(), -1.f);
            std::vector<juce::String> paths; for (auto& m : matches) paths.push_back(m.e->path);
            if (!paths.empty()) rank = proc.startRank(pad, src, paths);   // then: ranked by how near each gets after a match
            queryText = "LIKE  " + pd.name;
        }
        else queryText.clear();
        moreBtn.setEnabled(!matches.empty());
        list.updateContent(); list.deselectAllRows(); list.repaint(); repaint();
    }
    juce::File selectedFile() const
    {
        const int r = list.getSelectedRow();
        if (mode == 1) return r >= 0 && r < (int) matches.size() ? juce::File(matches[(size_t) r].e->path) : juce::File();
        return r >= 0 && r < items.size() && items[r].existsAsFile() ? items[r] : juce::File();
    }
    juce::File rowFile(int row) const
    {
        if (mode == 1) return row >= 0 && row < (int) matches.size() ? juce::File(matches[(size_t) row].e->path) : juce::File();
        return row >= 0 && row < items.size() ? items[row] : juce::File();
    }
    void play(const juce::File& f)
    {
        if (!f.existsAsFile()) return;
        const int pad = targetPad ? targetPad() : -1;
        auto r = proc.getResult();
        if (tryMode && r && pad >= 0 && r->pads[(size_t) pad].valid) { proc.tryOnPad(pad, f); if (onTried) onTried(); }   // on the pad, in the pattern
        else proc.previewSample(f, pad);
        list.repaint();
    }
public:
    std::function<void()> onTried;
    bool isTrying() const { return proc.isTrying(); }
    void stopTrying() { if (tryMode) { tryMode = false; tryBtn.setToggleState(false, juce::dontSendNotification); } proc.endTry(false); }
private:

    int getNumRows() override { return mode == 1 ? std::min(want, (int) matches.size()) : items.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        const auto f = rowFile(row);
        if (f == juce::File()) return;
        const bool folder = mode == 0 && f.isDirectory();
        const bool playing = !folder && proc.isPreviewing() && proc.previewingKit() == f;
        const auto r = juce::Rectangle<float>(2.f, 1.f, (float) w - 4.f, (float) h - 2.f);
        if (sel) { g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(r, 5.f); }
        if (playing) { g.setColour(ui::amber); g.fillRoundedRectangle(r.withWidth(3.f), 1.5f); }
        if (mode == 1)
        {   // name, how it differs, how near, where it lives
            const auto& m = matches[(size_t) row];
            g.setColour(playing ? ui::amber : ui::text); g.setFont(ui::font(10.5f, true, 0.03f));
            g.drawText(f.getFileNameWithoutExtension(), 12, 3, w - 70, 16, juce::Justification::centredLeft);
            g.setColour(ui::amber.withAlpha(0.85f)); g.setFont(ui::font(8.5f, true, 0.15f));
            g.drawText(m.tags.isEmpty() ? juce::String(m.score >= 90.f ? "VERY CLOSE" : m.score >= 75.f ? "CLOSE" : "SIMILAR, NO BIG DIFFERENCE") : m.tags.joinIntoString("  "),
                       12, 20, w / 2 + 40, 14, juce::Justification::centredLeft);
            g.setColour(ui::dim); g.setFont(ui::font(8.5f, false, 0.05f));
            g.drawText(f.getParentDirectory().getFileName(), w / 2, 20, w / 2 - 100, 14, juce::Justification::centredRight);
            const float aft = row < (int) after.size() ? after[(size_t) row] : -1.f;
            const auto bar = juce::Rectangle<float>((float) w - 52.f, 9.f, 40.f, 4.f);
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(bar, 2.f);
            g.setColour(ui::amber.withAlpha(0.45f)); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, m.score / 100.f)), 2.f);
            if (aft >= 0) { g.setColour(ui::amber); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, aft / 100.f)), 2.f); }
            g.setColour(ui::dim.brighter(0.2f)); g.setFont(ui::font(8.5f, true, 0.05f));
            // as it is -> after MATCH TO A
            g.drawText(juce::String(juce::roundToInt(m.score)) + "%" + (aft >= 0 ? " > " + juce::String(juce::roundToInt(aft)) + "%" : juce::String()),
                       w - 96, 18, 86, 14, juce::Justification::centredRight);
            return;
        }
        const auto ic = juce::Rectangle<float>(10.f, (float) h / 2.f - 6.f, 14.f, 12.f);
        if (folder) { g.setColour(ui::dim.brighter(0.2f)); g.drawRoundedRectangle(ic, 2.f, 1.2f); g.fillRect(ic.getX(), ic.getY() - 2.f, 6.f, 2.5f); }
        else
        {
            g.setColour(playing ? ui::amber : ui::dim);
            for (int k = 0; k < 4; ++k) { const float bh = (k == 1 ? 10.f : k == 2 ? 7.f : 4.f); g.fillRect(ic.getX() + 1.f + k * 3.5f, ic.getCentreY() - bh / 2.f, 2.f, bh); }
        }
        g.setColour(playing ? ui::amber : folder ? ui::text.withAlpha(0.8f) : ui::text);
        g.setFont(ui::font(10.5f, folder, 0.03f));
        g.drawText(folder ? f.getFileName() : f.getFileNameWithoutExtension(), 32, 0, w - 40, h, juce::Justification::centredLeft);
    }
    juce::uint32 selectedAt = 0;
    void selectedRowsChanged(int row) override       // also the arrow keys: each file plays as you walk
    {
        selectedAt = juce::Time::getMillisecondCounter();
        const auto f = rowFile(row);
        if (f.existsAsFile()) play(f);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {   // a click on the row that was already selected plays it again (a new selection has just played)
        if (juce::Time::getMillisecondCounter() - selectedAt < 250) return;
        const auto f = rowFile(row);
        if (f.existsAsFile()) play(f);
    }
    void activate(int row)
    {
        const auto f = rowFile(row);
        if (mode == 0 && f.isDirectory()) open(f);
        else if (proc.isTrying() && proc.triedFile() == f) { proc.endTry(true); if (onKept) onKept(f); }
        else if (f.existsAsFile() && onUse) onUse(f);
    }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { activate(row); }
    void returnKeyPressed(int row) override { activate(row); }
    void backgroundClicked(const juce::MouseEvent&) override {}
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override
    {
        if (rows.size() != 1) return {};
        const auto f = rowFile(rows[0]);
        return f.existsAsFile() ? juce::var("rekit-sample:" + f.getFullPathName()) : juce::var();
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::backspaceKey && mode == 0 && !filter.hasKeyboardFocus(true)) { upBtn.triggerClick(); return true; }
        if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
        return false;
    }
    void timerCallback() override
    {
        const bool p = proc.isPreviewing(); if (p || p != wasPlaying) list.repaint(); wasPlaying = p;
        if (mode == 1)
        {
            if (rank && !ranked)
            {
                if (rank->finished.load())
                {   // all checked: the list is ranked by how near each one gets after a match
                    { std::lock_guard<std::mutex> g(rank->lock); after = rank->after; }
                    std::vector<size_t> order(matches.size());
                    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
                    std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) { return after[a] > after[b]; });
                    std::vector<ds::Match> m2; std::vector<float> a2;
                    for (auto i : order) { m2.push_back(matches[i]); a2.push_back(after[i]); }
                    matches = std::move(m2); after = std::move(a2);
                    ranked = true;
                    list.updateContent(); list.deselectAllRows(); list.repaint();
                }
                repaint();
            }
            findSimilar(false);                    // the pad, its A / B / C or the library changed
            if (proc.library->scanning() || scanWas) repaint();
            scanWas = proc.library->scanning();
        }
    }
    bool scanWas = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleBrowser)
};

class DrumSlicerPanel : public juce::Component, public juce::FileDragAndDropTarget, public juce::TextDragAndDropTarget,
                        public juce::DragAndDropContainer, public juce::DragAndDropTarget, private juce::Timer
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
            knob(mamt[(size_t) i], "mamt" + s, [](double v) { return juce::String(juce::roundToInt(v)) + " %"; });
            knob(tone[(size_t) i], "tone" + s, [](double v) { return std::abs(v) < 0.05 ? juce::String("0") : juce::String(v > 0 ? "BRIGHT +" : "DARK ") + juce::String(std::abs(v), 1); });
        }
        knob(master, "master", dB);
        knob(sens, "sens", [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
        knob(grid, "quant", [](double v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
        knob(bpm, "bpm", [](double v) { return juce::String(v, 1); });
        for (auto* s : { &master, &sens, &grid, &bpm }) s->setVisible(true);
        sens.onDragEnd = [this] { if (proc.getResult() != nullptr && proc.getSource() != nullptr) proc.reanalyse(); };
        sens.setTooltip("Hit detection sensitivity (re-analyses the loop when you let go)");
        grid.setTooltip("TIGHT MIDI: 100% = fully on the grid, lower keeps some of the loop's groove");
        bpm.setTooltip("Tempo used for the MIDI when the host does not report one");

        for (auto* b : { &loadBtn, &reBtn }) addAndMakeVisible(b);
        loadBtn.setButtonText("LOAD"); reBtn.setButtonText("ANALYSE");
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser>("Select drum loop", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) proc.loadSource(f); });
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
        reBtn.setTooltip("ANALYSE: learn the sounds of the selected part.  RE-ANALYSE: the same part again (ignores the cache)");

        addAndMakeVisible(playMidi);
        playMidi.setTooltip("Listen to the MIDI (EXACT / SYNC / TIGHT as chosen) with your kit as it is now, at the project tempo, looped");
        playMidi.onClick = [this] { proc.playMidi(!proc.isMidiPlaying()); };
        addAndMakeVisible(dragMidi);
        dragMidi.getProperties().set("primary", true);
        dragMidi.getProperties().set("compact", true);
        dragMidi.setButtonText("DRAG MIDI");
        dragMidi.setTooltip("Drag onto a MIDI / instrument track in your DAW");
        dragMidi.onDrag = [this]
        {
            auto f = proc.makeMidiFile();
            if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
        };
        for (int b = 0; b < 3; ++b)                  // buttons: EXACT | SYNC | TIGHT
        {
            const int mode = b == 0 ? ds::kExact : b == 1 ? ds::kSync : ds::kTight;
            addAndMakeVisible(modeBtn[(size_t) b]);
            modeBtn[(size_t) b].setButtonText(b == 0 ? "EXACT" : b == 1 ? "SYNC" : "TIGHT");
            modeBtn[(size_t) b].setTooltip(b == 0 ? "MIDI = the loop as played, at its own speed (under the original audio)"
                                         : b == 1 ? "MIDI = the loop as played (groove, every hit), following your project tempo"
                                                  : "MIDI = clean drum-machine pattern on the grid, follows your project tempo");
            modeBtn[(size_t) b].onClick = [this, mode] { proc.setMidiMode(mode); };
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
        addAndMakeVisible(matchBtn);
        matchBtn.setButtonText("MATCH TO A");
        matchBtn.setTooltip("Shape your sample C (pitch, length, tone, punch, sustain, fade in) to come nearest to A - shown first, you decide");
        matchBtn.onClick = [this] { matchMenu(selected); };
        addAndMakeVisible(browseBtn);
        browseBtn.setButtonText("BROWSE");
        browseBtn.setTooltip("Your sample folders: click = listen (as loud as this pad), double-click or drag onto a pad = YOUR SAMPLE (C)");
        browseBtn.onClick = [this] { if (samples.isVisible()) samples.setVisible(false); else openSamples(); };
        addChildComponent(samples);
        samples.onClose = [this] { samples.setVisible(false); };
        samples.onUse = [this](const juce::File& f) { useSample(selected, f); };
        samples.targetPad = [this] { return selected; };
        samples.onTried = [this] { updateVisibility(); repaint(); };
        samples.onKept = [this](const juce::File& f) { toast("C  " + f.getFileNameWithoutExtension()); updateVisibility(); repaint(); };
        samples.targetName = [this] { auto r = proc.getResult(); if (!r) return juce::String();
                                      return r->pads[(size_t) selected].valid ? r->pads[(size_t) selected].name
                                                                              : "NEW " + (pendingType.isNotEmpty() ? pendingType : juce::String("SOUND")); };
        addAndMakeVisible(dragWav);
        dragWav.setButtonText("DRAG WAV");
        dragWav.setTooltip("Drag this pad's sound (as you hear it) into the DAW");
        dragWav.onDrag = [this]
        {
            auto f = proc.shotFile(selected);
            if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
        };
        addAndMakeVisible(shape);
        shape.setTooltip("Click: listen.  Flags: START / END.  Squares: drag sideways = fade length, up / down = curve.  GAIN (right): this sound's level (A, B, C each their own; SHIFT = fine).  Double-click a handle = reset.  Right-click = reset menu.");
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
        for (int i = 0; i < ds::kPads; ++i) if (padRect(i).contains(x, y)) return i;    // empty pads too: a new sound
        return -1;
    }
    void dropFile(const juce::File& f, int x, int y)
    {
        const int pad = padAt(x, y);
        if (pad >= 0) useSample(pad, f);
        else proc.loadSource(f);
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
    // a file dragged from the SAMPLES list: only onto a pad
    static juce::File browserFile(const SourceDetails& d)
    {
        const auto t = d.description.toString();
        return t.startsWith("rekit-sample:") ? juce::File(t.fromFirstOccurrenceOf(":", false, false)) : juce::File();
    }
    bool isInterestedInDragSource(const SourceDetails& d) override { return browserFile(d).existsAsFile(); }
    void itemDragEnter(const SourceDetails& d) override { hoverPad = padAt(d.localPosition.x, d.localPosition.y); repaint(); }
    void itemDragMove(const SourceDetails& d) override { const int p = padAt(d.localPosition.x, d.localPosition.y); if (p != hoverPad) { hoverPad = p; repaint(); } }
    void itemDragExit(const SourceDetails&) override { hoverPad = -1; repaint(); }
    void itemDropped(const SourceDetails& d) override
    {
        const int pad = padAt(d.localPosition.x, d.localPosition.y);
        hoverPad = -1;
        if (pad >= 0) useSample(pad, browserFile(d));
        repaint();
    }
    void useSample(int pad, const juce::File& f)
    {
        auto res = proc.getResult();
        if (pad < 0 || !f.existsAsFile() || !res) return;
        selected = pad;
        if (res->pads[(size_t) pad].valid) { if (proc.setPadSample(pad, f)) toast("C  " + f.getFileNameWithoutExtension()); }
        else if (proc.addPad(pad, pendingType, f)) toast("NEW  " + proc.getResult()->pads[(size_t) pad].name + "  -  " + f.getFileNameWithoutExtension());
        pendingType.clear();
        updateVisibility(); repaint();
    }
    juce::String pendingType;                        // chosen for an empty pad before its sample
    void emptyPadMenu(int pad)
    {
        juce::PopupMenu types, m;
        const auto& list = DrumSlicerProcessor::padTypes();
        for (int t = 0; t < (int) list.size(); ++t) types.addItem(100 + t, list[(size_t) t]);
        m.addSectionHeader("EMPTY PAD");
        m.addSubMenu("ADD A SOUND:  THIS PAD IS A ...", types);
        m.addItem(1, "ADD A SOUND  (TYPE FROM THE FILE NAME) ...");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(padRect(pad))),
            [this, pad, sp = juce::Component::SafePointer<DrumSlicerPanel>(this)](int id)
            {
                if (sp == nullptr || id == 0) return;
                pendingType = id >= 100 ? DrumSlicerProcessor::padTypes()[(size_t) (id - 100)] : juce::String();
                selected = pad; openSamples();
            });
    }

    // ---------------------------------------------------------------- layout (1000 x 600)
    const juce::Rectangle<int> display { 20, 76, 960, 140 }, padsPanel { 20, 226, 624, 206 }, editPanel { 20, 440, 624, 140 },
                               rightPanel { 654, 226, 326, 262 }, soundPanel { 654, 496, 326, 84 };
    juce::Rectangle<int> padRect(int i) const { return { 30 + (i % 4) * 152, 236 + (i / 4) * 100, 146, 92 }; }
    bool padHasSound(int i) const { auto r = proc.getResult(); return r && i >= 0 && i < ds::kPads && r->pads[(size_t) i].valid; }
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
            punch[(size_t) i].setBounds(670, 503, 42, 42); sus[(size_t) i].setBounds(730, 503, 42, 42);
            pitch[(size_t) i].setBounds(790, 503, 42, 42); tone[(size_t) i].setBounds(850, 503, 42, 42); mamt[(size_t) i].setBounds(910, 503, 42, 42);
        }
        master.setBounds(669, 362, 42, 42); sens.setBounds(749, 362, 42, 42); grid.setBounds(829, 362, 42, 42); bpm.setBounds(909, 362, 42, 42);
        playMidi.setBounds(666, 440, 34, 36);
        dragMidi.setBounds(704, 440, 68, 36);
        modeBtn[0].setBounds(776, 440, 46, 36); modeBtn[1].setBounds(824, 440, 46, 36); modeBtn[2].setBounds(872, 440, 46, 36);
        exportBtn.setBounds(922, 440, 48, 36);
        srcBtn[0].setBounds(32, 450, 102, 22); srcBtn[1].setBounds(133, 450, 102, 22); srcBtn[2].setBounds(234, 450, 122, 22);
        dragWav.setBounds(552, 450, 80, 22);
        browseBtn.setBounds(464, 450, 80, 22);
        matchBtn.setBounds(362, 450, 96, 22);
        samples.setBounds(rightPanel.getX(), rightPanel.getY(), rightPanel.getWidth(), soundPanel.getBottom() - rightPanel.getY());
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
        m.addSubMenu("MOVE ITS HITS TO ...  (THIS PAD GOES)", into, into.getNumItems() > 0);
        m.addSeparator();
        m.addItem(300, "CHOOSE YOUR SAMPLE (C) ...  (LISTEN FIRST)");
        m.addItem(302, "FIND SIMILAR IN YOUR SAMPLES ...");
        m.addItem(301, "REMOVE YOUR SAMPLE (C)", res->pads[(size_t) pad].user.getNumSamples() > 0);
        m.addSeparator();
        m.addItem(310, "CLEAR SOUND  (KEEP ITS HITS: THE PATTERN STAYS)");
        m.addItem(311, "DELETE PAD AND ITS HITS ...");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(padRect(pad))),
            [this, pad, sp = juce::Component::SafePointer<DrumSlicerPanel>(this)](int id)
            {
                if (sp == nullptr || id == 0) return;
                if (id >= 100 && id < 200) { proc.setPadType(pad, DrumSlicerProcessor::padTypes()[(size_t) (id - 100)]); toast("PAD  " + proc.getResult()->pads[(size_t) pad].name); }
                else if (id == 300) { selected = pad; openSamples(); }
                else if (id == 302) { selected = pad; openSamples(); samples.showSimilar(); }   // the SAMPLES panel: listen at the pad's level, then use
                else if (id == 301) { proc.clearPadSample(pad); toast("BACK TO A"); }
                else if (id == 310) { proc.clearPadSound(pad); toast("NO SOUND - DROP OR CHOOSE ONE, THE HITS WAIT"); }
                else if (id == 311)
                {
                    const auto nm = proc.getResult()->pads[(size_t) pad].name;
                    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "DELETE PAD",
                        "Delete " + nm + " and its " + juce::String(proc.getResult()->pads[(size_t) pad].hits) + " hits from the MIDI?", "DELETE", "CANCEL", this,
                        juce::ModalCallbackFunction::create([this, pad, nm, sp](int ok) { if (sp != nullptr && ok) { proc.deletePad(pad); toast("DELETED  " + nm); updateVisibility(); repaint(); } }));
                }
                else if (id >= 200) { const int q = id - 200; proc.mergePad(pad, q); selected = q; toast("HITS MOVED TO  " + proc.getResult()->pads[(size_t) q].name); }
                updateVisibility(); repaint();
            });
    }

    // MATCH TO A: measure + apply (in the background), then each part on / off, the AMOUNT knob, again / remove
    void matchMenu(int pad)
    {
        auto r = proc.getResult();
        if (!r || !r->pads[(size_t) pad].valid || r->pads[(size_t) pad].user.getNumSamples() == 0) return;
        const auto m = proc.getMatch(pad);
        if (!m.on) { proc.startMatch(pad); toast("MATCHING C TO A ..."); return; }
        juce::PopupMenu menu;
        menu.addSectionHeader("MATCHED TO A:   " + juce::String(juce::roundToInt(m.before)) + "%   ->   " + juce::String(juce::roundToInt(m.after)) + "%");
        menu.addItem(1, "EQ  (26 BANDS)", true, m.useEq);
        menu.addItem(2, "SHAPE  (ATTACK, BODY, DECAY IN 8 BANDS)", true, m.useShape);
        menu.addItem(3, m.roomMix > 0 ? "ROOM  (A'S SPACE)" : "ROOM  (NOT NEEDED: C HAS AS MUCH)", m.roomMix > 0, m.useRoom && m.roomMix > 0);
        menu.addItem(4, "PITCH  " + juce::String(m.pitch > 0 ? "+" : "") + juce::String(m.pitch, 1) + " ST", true, m.usePitch);
        menu.addItem(5, m.en < 1999.f ? "LENGTH  END " + juce::String(juce::roundToInt(m.en)) + " MS" : juce::String("LENGTH  (AS IT IS)"), m.en < 1999.f, m.useLen && m.en < 1999.f);
        bool glide = false; for (float v : m.glide) glide = glide || std::abs(v) > 0.05f;
        menu.addItem(6, glide ? "PITCH DROP  (THE FALL OF ITS NOTE, LIKE A'S)" : juce::String("PITCH DROP  (NOT NEEDED / NOT TONAL)"), glide, m.useGlide && glide);
        menu.addItem(7, m.driveDb > 0.05f ? "SATURATION  " + juce::String(m.driveDb, 1) + " DB DRIVE" : juce::String("SATURATION  (NOT NEEDED)"), m.driveDb > 0.05f, m.useDrive && m.driveDb > 0.05f);
        menu.addItem(8, std::abs(m.widthDb) > 0.05f ? "WIDTH  " + juce::String(m.widthDb > 0 ? "WIDER " : "NARROWER ") + juce::String(std::abs(m.widthDb), 1) + " DB" : juce::String("WIDTH  (AS IT IS)"), std::abs(m.widthDb) > 0.05f, m.useWidth && std::abs(m.widthDb) > 0.05f);
        menu.addSeparator();
        menu.addItem(10, "MATCH AGAIN");
        menu.addItem(11, "REMOVE MATCH");
        menu.addSeparator();
        const bool lay = r->pads[(size_t) pad].layered();
        menu.addItem(12, lay ? "LAYER AGAIN  (ANOTHER SECOND SAMPLE)" : "LAYER MATCH  (ADD A SECOND SAMPLE OF YOURS)");
        if (lay) menu.addItem(13, "REMOVE LAYER  (" + r->pads[(size_t) pad].layerName.toUpperCase() + ")");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&matchBtn),
            [this, pad, m, sp = juce::Component::SafePointer<DrumSlicerPanel>(this)](int id)
            {
                if (sp == nullptr || id == 0) return;
                if (id >= 1 && id <= 8)
                {
                    const bool cur = id == 1 ? m.useEq : id == 2 ? m.useShape : id == 3 ? m.useRoom : id == 4 ? m.usePitch : id == 5 ? m.useLen
                                   : id == 6 ? m.useGlide : id == 7 ? m.useDrive : m.useWidth;
                    proc.setMatchPart(pad, id - 1, !cur);
                }
                else if (id == 10) { proc.startMatch(pad); toast("MATCHING C TO A ..."); }
                else if (id == 11) { proc.removeMatch(pad); toast("MATCH REMOVED"); }
                else if (id == 12) { proc.startLayer(pad); toast("LOOKING FOR A SECOND SAMPLE ..."); }
                else if (id == 13) { proc.removeLayer(pad); }
                shape.invalidate(); repaint();
            });
    }
    void openSamples() { updateVisibility(); samples.setVisible(true); samples.toFront(false); samples.showFor(); repaint(); }
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
            if (proc.isTrying() && i != selected) proc.endTry(false);     // trying on another pad: that one goes back
            selected = i;
            if (e.mods.isPopupMenu())
            {
                if (res && res->pads[(size_t) i].valid) padMenu(i);
                else if (res) emptyPadMenu(i);
                else toast("ANALYSE A LOOP FIRST");
                updateVisibility(); repaint(); return;
            }
            if (res && res->pads[(size_t) i].valid && abRect(i).contains(e.getPosition()))
            {
                const auto ab = abRect(i); const int n = padHasC(i) ? 3 : 2;
                setSource(i, juce::jlimit(0, n - 1, (e.getPosition().x - ab.getX()) * n / ab.getWidth()));
            }
            else if (!res || !res->pads[(size_t) i].valid || proc.padIsSilent(i))
            {   // nothing to hear: give it a sound
                if (samples.isTrying()) samples.stopTrying();
                if (res) { pendingType.clear(); openSamples(); }
            }
            else proc.audition(i);
            updateVisibility(); repaint();
        }
        if (display.contains(e.getPosition())) displayDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { displayDrag(e); }
    void mouseUp(const juce::MouseEvent&) override { dragWhat = 0; }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int h = display.contains(e.getPosition()) ? handleAt(e.position.x, e.position.y) : 0;
        setMouseCursor(h != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto src = proc.getSource();
        if (!src || !display.contains(e.getPosition()) || proc.isBusy()) return;
        const int h = handleAt(e.position.x, e.position.y);
        if (h == 1) proc.setSelection(0, proc.selEnd());                       // double-click a handle: back to the file's edge
        else if (h == 2) proc.setSelection(proc.selStart(), src->length());
        else if (waveZone().contains(e.position)) { v0 = 0; v1 = src->length(); }   // elsewhere: whole file
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto src = proc.getSource();
        if (!src || !display.contains(e.getPosition())) return;
        fixView(*src);
        const double span = (double) (v1 - v0), n = (double) src->length();
        if (e.mods.isShiftDown() || std::abs(w.deltaX) > std::abs(w.deltaY))     // shift + wheel: move along
        {
            const double d = -(std::abs(w.deltaX) > std::abs(w.deltaY) ? w.deltaX : w.deltaY) * span * 0.25;
            const double a = juce::jlimit(0.0, n - span, (double) v0 + d);
            v0 = (juce::int64) a; v1 = (juce::int64) (a + span);
        }
        else                                                                     // wheel: zoom where the mouse is
        {
            const double at = (double) sampleAt(e.position.x);
            const double ns = juce::jlimit(std::min(n, 0.05 * src->sr), n, span * std::pow(0.8, w.deltaY * 4.0));
            const double f = (at - (double) v0) / span;
            const double a = juce::jlimit(0.0, n - ns, at - f * ns);
            v0 = (juce::int64) a; v1 = (juce::int64) (a + ns);
        }
        repaint();
    }

    void timerCallback() override
    {
        if (proc.getResultVersion() != shownVersion) { shownVersion = proc.getResultVersion(); shape.invalidate(); repaint(); }
        const int mm = proc.midiMode();
        modeBtn[0].setToggleState(mm == ds::kExact, juce::dontSendNotification);
        modeBtn[1].setToggleState(mm == ds::kSync, juce::dontSendNotification);
        modeBtn[2].setToggleState(mm == ds::kTight, juce::dontSendNotification);
        const int src = proc.padSource(selected);
        for (int k = 0; k < 3; ++k) srcBtn[(size_t) k].setToggleState(src == k, juce::dontSendNotification);
        bool lit = false;
        for (size_t i = 0; i < 8; ++i) { levels[i] = std::max(proc.padLevel[i].exchange(0.f) > 0.f ? 1.f : 0.f, levels[i] * 0.85f); lit |= levels[i] > 0.01f; }
        auto res = proc.getResult();
        const bool hasPad = res && res->pads[(size_t) selected].valid;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &dragMidi, &exportBtn, &modeBtn[0], &modeBtn[1], &modeBtn[2] }) c->setEnabled(res != nullptr);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &srcBtn[0], &srcBtn[1], &dragWav }) c->setEnabled(hasPad);
        const bool hasC = hasPad && res->pads[(size_t) selected].user.getNumSamples() > 0;
        srcBtn[2].setEnabled(hasC);
        srcBtn[2].setTooltip(hasC ? "Your sample: " + res->pads[(size_t) selected].userName + (res->pads[(size_t) selected].layered() ? " + " + res->pads[(size_t) selected].layerName + " (layered)" : juce::String())
                                  : juce::String("A sample of yours: drop a file on the pad (or right-click the pad > LOAD YOUR SAMPLE)"));
        {
            const bool hasSrc = proc.getSource() != nullptr, done = proc.selectionAnalysed();
            reBtn.setEnabled(hasSrc && !proc.isBusy());
            reBtn.setButtonText(done ? "RE-ANALYSE" : "ANALYSE");
            reBtn.setToggleState(hasSrc && !done && !proc.isBusy(), juce::dontSendNotification);   // lit = waiting for you
            if (proc.isSourcePlaying() || wasPlaying) repaint(display);
            wasPlaying = proc.isSourcePlaying();
        }
        browseBtn.setToggleState(samples.isVisible(), juce::dontSendNotification);
        {
            const bool canMatch = hasPad && res->pads[(size_t) selected].user.getNumSamples() > 0 && proc.padSource(selected) == 2
                               && res->pads[(size_t) selected].audio.getMagnitude(0, res->pads[(size_t) selected].audio.getNumSamples()) > 1.0e-6f;   // a cleared pad has no A
            const auto md = proc.getMatch(selected);
            matchBtn.setEnabled(canMatch && !proc.isMatching());
            matchBtn.setButtonText(proc.isMatching() ? (proc.matchMessage().startsWith("LAYERING") ? proc.matchMessage().replace("LAYERING  ", "LAYER ") : juce::String("MATCHING...")) : (canMatch && md.on) ? "MATCH " + juce::String(juce::roundToInt(md.after)) + "%" : juce::String("MATCH TO A"));
            matchBtn.setToggleState(canMatch && md.on, juce::dontSendNotification);
            for (int i = 0; i < ds::kPads; ++i) mamt[(size_t) i].setEnabled(i == selected && canMatch && md.on);
            const auto msg = proc.matchMessage();
            if (msg != lastMatchMsg) { lastMatchMsg = msg; if (msg.startsWith("MATCHED") || msg.startsWith("A HAS") || msg.startsWith("ALREADY") || msg.startsWith("LAYERED") || msg.startsWith("NO LAYER") || msg.startsWith("LAYER REMOVED")) toast(msg); }
        }
        {   // a file that could not be read keeps the loop you had: said out loud, not only in the status line
            const auto st = proc.getStatus();
            if (st != lastStatus) { lastStatus = st; if (st.startsWith("COULD NOT")) toast(st + "  -  THE LOOP YOU HAD STAYS"); }
        }
        playMidi.setToggleState(proc.isMidiPlaying(), juce::dontSendNotification);
        playMidi.setEnabled(res != nullptr);
        if (!res && proc.isMidiPlaying()) proc.playMidi(false);
        if (samples.isVisible() && selected != browsedPad) { browsedPad = selected; samples.repaint(); }
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
        proc.audition(pad); shape.invalidate(); repaint();
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
            const juce::String ver = "v1.13.0";
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

    // ---------------------------------------------------------------- top display: the loaded file
    // player (cursor), selection START / END (snap to the attacks), wheel = zoom, shift + wheel = move
    float wx0() const { return (float) display.getX() + 19.f; }
    float ww() const { return (float) display.getWidth() - 38.f; }
    juce::Rectangle<int> playRect() const { return { (int) wx0(), display.getY() + 11, 26, 22 }; }
    juce::Rectangle<int> snapRect() const { return { (int) wx0() + 32, display.getY() + 11, 64, 22 }; }
    juce::Rectangle<float> waveZone() const { return { wx0() - 6.f, (float) display.getY() + 36.f, ww() + 12.f, 80.f }; }
    float waveMid() const { return (float) display.getY() + 68.f; }
    static constexpr float waveH = 30.f;
    void fixView(const ds::Source& s)
    {
        if (s.id != viewId || v1 <= v0 || v1 > s.length()) { v0 = 0; v1 = s.length(); viewId = s.id; }
    }
    float xOf(juce::int64 smp) const { return wx0() + (float) ((double) (smp - v0) / (double) std::max<juce::int64>(1, v1 - v0)) * ww(); }
    juce::int64 sampleAt(float x) const { return v0 + (juce::int64) std::llround((double) (x - wx0()) / ww() * (double) (v1 - v0)); }
    int handleAt(float x, float y) const
    {
        auto src = proc.getSource();
        if (!src || !waveZone().contains(x, y)) return 0;
        const float xs = xOf(proc.selStart()), xe = xOf(proc.selEnd());
        const float ds_ = std::abs(x - xs), de = std::abs(x - xe);
        if (std::min(ds_, de) > 7.f) return 0;
        return ds_ <= de ? 1 : 2;
    }
    // the strongest attack near the mouse (10 px), else the nearest one (40 px), else where the mouse is
    juce::int64 snapped(const ds::Source& s, juce::int64 pos) const
    {
        const double spp = (double) (v1 - v0) / ww();
        const juce::int64 near = (juce::int64) (10 * spp), far = (juce::int64) (40 * spp);
        const auto& on = s.onsets;
        auto it = std::lower_bound(on.begin(), on.end(), pos - far, [](const ds::Onset& o, juce::int64 p) { return o.pos < p; });
        float top = 0; std::vector<ds::Onset> cand;
        juce::int64 nearest = -1, nd = far + 1;
        for (; it != on.end() && it->pos <= pos + far; ++it)
        {
            const juce::int64 d = std::abs(it->pos - pos);
            if (d < nd) { nd = d; nearest = it->pos; }
            if (d <= near) { cand.push_back(*it); top = std::max(top, it->strength); }
        }
        for (juce::int64 edge : { (juce::int64) 0, s.length() })                // the file's own start and end
            if (std::abs(edge - pos) <= near) { cand.push_back({ edge, 1.f }); top = 1.f; }
        if (!cand.empty())
        {
            juce::int64 best = cand.front().pos, bd = std::numeric_limits<juce::int64>::max();
            for (auto& c : cand) if (c.strength >= 0.7f * top && std::abs(c.pos - pos) < bd) { bd = std::abs(c.pos - pos); best = c.pos; }
            return best;
        }
        return nearest >= 0 ? nearest : pos;
    }
    void displayDown(const juce::MouseEvent& e)
    {
        auto src = proc.getSource();
        if (!src) { if (!proc.isBusy()) loadBtn.triggerClick(); return; }
        if (proc.isBusy() || e.mods.isPopupMenu()) return;
        fixView(*src);
        if (playRect().contains(e.getPosition())) { proc.playSource(!proc.isSourcePlaying()); repaint(); return; }
        if (snapRect().contains(e.getPosition())) { proc.snap = !proc.snap.load(); repaint(); return; }
        if (!waveZone().contains(e.position)) return;
        dragWhat = handleAt(e.position.x, e.position.y);
        if (dragWhat == 0)
        {   // click on the wave: play from there (inside the selection)
            const auto p = juce::jlimit(proc.selStart(), std::max(proc.selStart(), proc.selEnd() - 1), sampleAt(e.position.x));
            proc.seekSource(p);
            proc.playSource(true);
            repaint();
        }
    }
    void displayDrag(const juce::MouseEvent& e)
    {
        if (dragWhat == 0) return;
        auto src = proc.getSource(); if (!src) return;
        auto p = juce::jlimit<juce::int64>(0, src->length(), sampleAt(e.position.x));
        if (proc.snap.load() != e.mods.isAltDown()) p = snapped(*src, p);              // ALT: the other way round
        if (dragWhat == 1) proc.setSelection(std::min(p, proc.selEnd() - 1), proc.selEnd());
        else proc.setSelection(proc.selStart(), std::max(p, proc.selStart() + 1));
        repaint(display);
    }
    static juce::String clock(double sec)
    {
        const int m = (int) (sec / 60.0);
        return juce::String(m) + ":" + juce::String(sec - 60.0 * m, 2).paddedLeft('0', 5);
    }
    void computePeaks(const ds::Source& s)
    {
        if (s.id == peaksId && v0 == pk0 && v1 == pk1) return;
        peaksId = s.id; pk0 = v0; pk1 = v1;
        peaks.clear();
        const int w = (int) ww(), n = (int) s.length();
        if (w <= 0 || n <= 0) return;
        if (s.id != gmaxId) { gmax = 1e-6f; for (int c = 0; c < s.audio.getNumChannels(); ++c) gmax = std::max(gmax, s.audio.getMagnitude(c, 0, n)); gmaxId = s.id; }
        const float* L = s.audio.getReadPointer(0); const float* R = s.audio.getReadPointer(s.audio.getNumChannels() > 1 ? 1 : 0);
        for (int x = 0; x < w; ++x)
        {
            const juce::int64 a = juce::jlimit<juce::int64>(0, n - 1, v0 + (v1 - v0) * x / w), b = juce::jlimit<juce::int64>(a + 1, n, v0 + (v1 - v0) * (x + 1) / w);
            const juce::int64 step = std::max<juce::int64>(1, (b - a) / 64);
            float mn = 0.f, mxv = 0.f;
            for (juce::int64 i = a; i < b; i += step) { const float v = 0.5f * (L[i] + R[i]) / gmax; mn = std::min(mn, v); mxv = std::max(mxv, v); }
            peaks.push_back({ mn, mxv });
        }
    }

    void drawDisplay(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        auto d = display.toFloat().reduced(1.f);
        g.setColour(ui::well.withAlpha(0.7f)); g.fillRoundedRectangle(d.reduced(6.f), 8.f);
        if (hover) { g.setColour(ui::amber.withAlpha(0.8f)); g.drawRoundedRectangle(d.reduced(4.f), 10.f, 1.5f); }
        if (proc.isBusy())
        {
            const bool loading = proc.getStatus().startsWith("LOADING") || proc.getStatus().startsWith("OPENING");
            const auto bar = juce::Rectangle<float>(d.getX() + 120, d.getCentreY() + 8, d.getWidth() - 240, 6);
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillRoundedRectangle(bar, 3.f);
            g.setColour(ui::amber); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * (loading ? 0.5f : juce::jlimit(0.f, 1.f, proc.getProgress()))), 3.f);
            g.setFont(ui::font(13.f, true, 0.3f));
            g.drawText(loading ? juce::String("LOADING") : "LEARNING THE SOUNDS   " + juce::String(juce::roundToInt(100.f * proc.getProgress())) + "%", d.withTrimmedBottom(40).toNearestInt(), juce::Justification::centred);
            g.setColour(ui::dim); g.setFont(ui::font(9.5f, false, 0.05f));
            g.drawText(proc.getStatus(), d.withTrimmedTop(84).reduced(24, 0).toNearestInt(), juce::Justification::centred);
            return;
        }
        auto src = proc.getSource();
        if (!src)
        {
            const auto st = proc.getStatus();
            g.setColour(ui::amber.withAlpha(0.9f)); g.setFont(ui::font(14.f, true, 0.3f));
            g.drawText(st.startsWith("COULD NOT") ? st : juce::String("DROP A DRUM LOOP HERE"), d.withTrimmedBottom(16).toNearestInt(), juce::Justification::centred);
            g.setColour(ui::dim); g.setFont(ui::font(9.5f, true, 0.3f));
            g.drawText("DRAG A FILE FROM THE POOL / MEDIABAY / EXPLORER  -  OR CLICK TO BROWSE", d.withTrimmedTop(36).toNearestInt(), juce::Justification::centred);
            return;
        }
        fixView(*src);
        computePeaks(*src);
        const juce::int64 sa = proc.selStart(), sb = proc.selEnd();
        const float x0 = wx0(), w = ww(), mid = waveMid(), hh = waveH;
        const float xs = juce::jlimit(x0, x0 + w, xOf(sa)), xe = juce::jlimit(x0, x0 + w, xOf(sb));

        // play / stop
        {
            const auto pr = playRect().toFloat();
            const bool on = proc.isSourcePlaying();
            g.setColour(on ? ui::amber : ui::text.withAlpha(0.85f));
            g.drawRoundedRectangle(pr.reduced(0.5f), 5.f, 1.f);
            if (on) g.fillRect(pr.withSizeKeepingCentre(8.f, 8.f));
            else { juce::Path t; const auto c = pr.getCentre(); t.addTriangle(c.x - 3.5f, c.y - 5.f, c.x - 3.5f, c.y + 5.f, c.x + 5.f, c.y); g.fillPath(t); }
        }
        // snap: hollow circle = off, filled = on
        {
            const auto sr = snapRect().toFloat();
            const bool on = proc.snap.load();
            const auto c = juce::Rectangle<float>(sr.getX() + 4.f, sr.getCentreY() - 5.5f, 11.f, 11.f);
            g.setColour(on ? ui::amber : ui::dim);
            g.drawEllipse(c, 1.4f);
            if (on) g.fillEllipse(c.reduced(2.6f));
            g.setFont(ui::font(9.5f, true, 0.25f));
            g.drawText("SNAP", sr.withTrimmedLeft(21).toNearestInt(), juce::Justification::centredLeft);
        }
        // selection text
        {
            const double secs = (double) (sb - sa) / src->sr;
            juce::String t = "SELECTION   " + clock((double) sa / src->sr) + "  -  " + clock((double) sb / src->sr) + "    " + juce::String(secs, 2) + " s";
            if (res && res->sourceId == src->id && res->bpm > 0) t << "   ~" << juce::String(secs * res->bpm / 240.0, 2) << " BARS @ " << juce::String(res->bpm, 1);
            if (v0 > 0 || v1 < src->length()) t << "    ZOOM";
            g.setColour(ui::dim.brighter(0.25f)); g.setFont(ui::font(9.5f, true, 0.18f));
            g.drawText(t, juce::Rectangle<float>(x0 + 106.f, (float) display.getY() + 11.f, 560.f, 22.f).toNearestInt(), juce::Justification::centredLeft);
        }
        // wave: bright inside the selection, dim outside
        {
            juce::Path wav; wav.startNewSubPath(x0, mid);
            for (int x = 0; x < (int) peaks.size(); ++x) wav.lineTo(x0 + (float) x, mid - peaks[(size_t) x].second * hh);
            for (int x = (int) peaks.size() - 1; x >= 0; --x) wav.lineTo(x0 + (float) x, mid - peaks[(size_t) x].first * hh);
            wav.closeSubPath();
            g.setColour(ui::faint.brighter(0.15f)); g.fillPath(wav);
            {
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(juce::Rectangle<float>(xs, mid - hh - 4.f, std::max(0.f, xe - xs), 2.f * hh + 8.f).toNearestInt());
                g.setColour(ui::amber.withAlpha(0.18f)); g.strokePath(wav, juce::PathStrokeType(3.f));
                g.setColour(ui::amber); g.fillPath(wav);
            }
        }
        // hits of the analysed part
        const juce::int64 n = src->length();
        if (res && res->sourceId == src->id)
            for (auto& h : res->hits)
            {
                const juce::int64 t = (juce::int64) h.time + res->regionStart;
                if (t < v0 || t > v1) continue;
                const float x = xOf(t);
                const auto c = ui::padColour(res->pads[(size_t) h.pad].name);
                g.setColour(c.withAlpha(h.pad == selected ? 1.f : 0.55f));
                g.fillRoundedRectangle(x - 1.f, mid + hh + 5.f, 2.f, h.pad == selected ? 10.f : 7.f, 1.f);
            }
        // START / END handles (flags like the sound editor)
        for (int k = 0; k < 2; ++k)
        {
            const juce::int64 at = k == 0 ? sa : sb;
            if (at < v0 || at > v1) continue;
            const float x = xOf(at);
            g.setColour(ui::text);
            g.fillRect(x - 0.75f, mid - hh - 2.f, 1.5f, 2.f * hh + 4.f);
            juce::Path f;
            const float fy = mid - hh - 2.f;
            if (k == 0) f.addTriangle(x, fy, x + 9.f, fy + 5.f, x, fy + 10.f);
            else        f.addTriangle(x, fy, x - 9.f, fy + 5.f, x, fy + 10.f);
            g.fillPath(f);
        }
        // the cursor
        {
            const double p = proc.sourcePos();
            if (p >= (double) v0 && p <= (double) v1 && (proc.isSourcePlaying() || (p > (double) sa && p < (double) sb)))
            {
                const float x = xOf((juce::int64) p);
                g.setColour(juce::Colours::white.withAlpha(proc.isSourcePlaying() ? 0.9f : 0.4f));
                g.fillRect(x - 0.5f, mid - hh - 4.f, 1.f, 2.f * hh + 8.f);
            }
        }
        juce::String info;
        if (res && res->sourceId == src->id)
        {
            const double hb = proc.getHostBpm();
            info = juce::String(res->bpm, 2) + " BPM   |   " + juce::String(res->sr / 1000.0, 1) + " kHz   |   " + juce::String((int) res->hits.size())
                 + " HITS   |   " + (proc.midiMode() != ds::kExact ? juce::String("MIDI FOLLOWS THE PROJECT TEMPO")
                                    : "MIDI @ " + (hb > 0 ? "HOST " + juce::String(hb, 2) : juce::String(proc.midiBpm(), 2) + " BPM"));
            if (!proc.selectionAnalysed()) info << "   |   SELECTION CHANGED: PRESS ANALYSE";
        }
        else info = "FILE  " + clock((double) n / src->sr) + "   |   " + juce::String(src->sr / 1000.0, 1) + " kHz   |   SELECT THE PART (DRAG THE FLAGS), THEN ANALYSE";
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
            if (proc.padIsSilent(i))
            {
                g.setColour(c.withAlpha(0.8f)); g.setFont(ui::font(8.5f, true, 0.15f));
                g.drawText("NO SOUND - CLICK TO ADD", juce::Rectangle<float>(r.getX() + 14, r.getY() + 22, r.getWidth() - 20, 12).toNearestInt(), juce::Justification::centredLeft);
            }
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
            g.setFont(ui::font(10.f, true, 0.25f)); g.drawText(padHasSound(hoverPad) ? "DROP = YOUR SAMPLE (C)" : "DROP = NEW SOUND", r.withTrimmedTop(r.getHeight() * 0.35f).withHeight(14.f).toNearestInt(), juce::Justification::centred);
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
        knobLabel(g, tone[(size_t) selected], "TONE", 1);
        knobLabel(g, mamt[(size_t) selected], "MATCH", 1);
    }

    void updateVisibility()
    {
        for (int i = 0; i < ds::kPads; ++i)
            for (auto* k : { &vol[(size_t) i], &pan[(size_t) i], &punch[(size_t) i], &sus[(size_t) i], &pitch[(size_t) i], &tone[(size_t) i], &mamt[(size_t) i] }) k->setVisible(i == selected);
        shape.pad = selected; shape.invalidate();
    }

    DrumSlicerProcessor& proc;
    ui::Skin skin;
    ui::Look look;
    int selected = 0, shownVersion = -1;
    bool hover = false, wasBusy = false;
    int hoverPad = -1;
    std::array<float, 8> levels {};
    std::vector<std::pair<float, float>> peaks;
    juce::int64 v0 = 0, v1 = 0, pk0 = -1, pk1 = -1;   // the part of the file in view (samples)
    juce::uint64 viewId = 0, peaksId = 0, gmaxId = 0;
    float gmax = 1.f;
    int dragWhat = 0;                                // 1 = START, 2 = END
    bool wasPlaying = false;

    std::array<juce::Slider, ds::kPads> vol, pan, punch, sus, pitch, tone, mamt;
    juce::Slider master, sens, grid, bpm;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> atts;
    juce::TextButton loadBtn, reBtn, exportBtn, kitsBtn, browseBtn, matchBtn;
    KitBrowser kitBrowser { proc };
    SampleBrowser samples { proc };
    juce::String lastMatchMsg, lastStatus;
    int browsedPad = -1;
    juce::String toastText; juce::uint32 toastUntil = 0; bool wasToasting = false;
    std::array<juce::TextButton, 3> modeBtn;
    std::array<juce::TextButton, 3> srcBtn;
    ui::DragButton dragMidi, dragWav;
    ui::PlayButton playMidi;
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
    // Cubase sometimes puts a just-inserted instrument's window behind its own (until the instrument button is
    // pressed): when the window first shows, its frame is brought to the front - once, and only if it is not there
    void parentHierarchyChanged() override
    {
        if (raised || getPeer() == nullptr) return;
        raised = true;
        for (int ms : { 120, 600 })
            juce::Timer::callAfterDelay(ms, [sp = juce::Component::SafePointer<DrumSlicerEditor>(this)] { if (sp != nullptr) sp->raiseFrame(); });
    }
    void raiseFrame()
    {
        if (auto* peer = getPeer()) rekitRaiseFrame(peer->getNativeHandle());
    }
    bool raised = false;
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
