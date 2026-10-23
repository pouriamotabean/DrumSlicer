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
//  REKIT 2  -  a black device: few keys, one display that shows the details of wherever you are.
//  Monochrome + one orange.  Design size 1240 x 720 (scaled).   by Pouria Motabean
//  Pages of the display (one level deep): kit, source, pattern, pad (+ its ···), samples, kits.
//  No popup menus and no dialogs: every choice is on a page.
// =====================================================================================================
namespace ui
{
static const juce::Colour white(0xfff2f2f0), dim(0xff8a8a8c), dim2(0xff77777a), faint(0xff5e5e61), line(0xff2e2e31),
                          dark(0xff3a3a3d), screen(0xff050506), chip(0xff141416), chip2(0xff1c1c1f), chipOn(0xff2a2a2d),
                          card(0xff101012), cardOn(0xff1a1a1d), orange(0xffff5a1f), label(0xffa6a6a8), soft(0xffb0b0b2);

#if HAS_ASSETS
inline juce::Typeface::Ptr face(int i)
{
    static juce::Typeface::Ptr t[6];
    if (t[i] == nullptr)
    {
        const void* d = nullptr; int n = 0;
        switch (i)
        {
            case 0: d = BinaryData::InterLight_ttf;       n = BinaryData::InterLight_ttfSize; break;
            case 1: d = BinaryData::InterRegular_ttf;     n = BinaryData::InterRegular_ttfSize; break;
            case 2: d = BinaryData::InterMedium_ttf;      n = BinaryData::InterMedium_ttfSize; break;
            case 3: d = BinaryData::InterSemiBold_ttf;    n = BinaryData::InterSemiBold_ttfSize; break;
            case 4: d = BinaryData::SpaceMonoRegular_ttf; n = BinaryData::SpaceMonoRegular_ttfSize; break;
            default: d = BinaryData::SpaceMonoBold_ttf;   n = BinaryData::SpaceMonoBold_ttfSize; break;
        }
        t[i] = juce::Typeface::createSystemTypefaceFor(d, (size_t) n);
    }
    return t[i];
}
#endif
// Inter: w 300 / 400 / 500 / 600
inline juce::Font inter(float px, int w = 400, float kern = 0.f)
{
#if HAS_ASSETS
    const int i = w <= 300 ? 0 : w <= 400 ? 1 : w <= 500 ? 2 : 3;
    if (auto f = face(i)) return juce::Font(juce::FontOptions(f).withPointHeight(px).withKerningFactor(kern));
#endif
    return juce::Font(juce::FontOptions(px * 1.2f, w >= 500 ? juce::Font::bold : juce::Font::plain).withKerningFactor(kern));
}
inline juce::Font mono(float px, bool bold = false)
{
#if HAS_ASSETS
    if (auto f = face(bold ? 5 : 4)) return juce::Font(juce::FontOptions(f).withPointHeight(px));
#endif
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), px * 1.2f, bold ? juce::Font::bold : juce::Font::plain));
}
inline float textW(const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth(f, s); }
inline juce::String lower(const juce::String& s) { return s.toLowerCase(); }
inline juce::String nice(juce::String s) { return s.replaceCharacter('_', ' ').trim(); }

// the sign of each sound (lanes, pads): kick = filled dot, snare = ring + dot, c-hat = two dots, o-hat = ring,
// rim = triangle, tom = square, clap = x, shaker = lines, crash = ring + slash, ride = ring + dot filled, perc = diamond
inline void glyph(juce::Graphics& g, const juce::String& name, juce::Rectangle<float> r, juce::Colour c)
{
    const float s = r.getWidth() / 14.f;
    auto P = [&](float x, float y) { return juce::Point<float>(r.getX() + x * s, r.getY() + y * s); };
    juce::Path p; const float lw = 1.4f * s;
    g.setColour(c);
    auto ring = [&](float rad) { const auto c0 = P(7, 7); p.addEllipse(c0.x - rad * s, c0.y - rad * s, 2 * rad * s, 2 * rad * s); };
    if (name.startsWith("KICK")) { ring(5.5f); g.fillPath(p); return; }
    if (name.startsWith("SNARE")) { ring(5.2f); g.strokePath(p, juce::PathStrokeType(lw)); const auto c0 = P(7, 7); g.fillEllipse(c0.x - 1.4f * s, c0.y - 1.4f * s, 2.8f * s, 2.8f * s); return; }
    if (name.startsWith("C-HAT") || name.startsWith("HAT")) { for (float x : { 4.f, 10.f }) { const auto c0 = P(x, 7); g.fillEllipse(c0.x - 1.8f * s, c0.y - 1.8f * s, 3.6f * s, 3.6f * s); } return; }
    if (name.startsWith("O-HAT")) { ring(5.f); g.strokePath(p, juce::PathStrokeType(lw)); return; }
    if (name.startsWith("RIM")) { p.addTriangle(P(7, 2), P(12.5f, 12), P(1.5f, 12)); g.strokePath(p, juce::PathStrokeType(lw)); return; }
    if (name.startsWith("TOM")) { g.fillRect(juce::Rectangle<float>(P(2, 2), P(12, 12))); return; }
    if (name.startsWith("CLAP")) { p.startNewSubPath(P(2.5f, 2.5f)); p.lineTo(P(11.5f, 11.5f)); p.startNewSubPath(P(11.5f, 2.5f)); p.lineTo(P(2.5f, 11.5f)); g.strokePath(p, juce::PathStrokeType(lw)); return; }
    if (name.startsWith("SHAKER")) { for (float y : { 4.f, 7.f, 10.f }) { p.startNewSubPath(P(2, y)); p.lineTo(P(12, y)); } g.strokePath(p, juce::PathStrokeType(lw)); return; }
    if (name.startsWith("CRASH")) { ring(5.f); p.startNewSubPath(P(3.5f, 10.5f)); p.lineTo(P(10.5f, 3.5f)); g.strokePath(p, juce::PathStrokeType(lw)); return; }
    if (name.startsWith("RIDE")) { ring(5.f); g.strokePath(p, juce::PathStrokeType(lw)); const auto c0 = P(7, 7); g.fillEllipse(c0.x - 2.4f * s, c0.y - 2.4f * s, 4.8f * s, 4.8f * s); return; }
    if (name == "-" || name.isEmpty()) { p.addEllipse(P(5.5f, 5.5f).x, P(5.5f, 5.5f).y, 3 * s, 3 * s); g.strokePath(p, juce::PathStrokeType(lw)); return; }
    p.startNewSubPath(P(7, 1.5f)); p.lineTo(P(12.5f, 7)); p.lineTo(P(7, 12.5f)); p.lineTo(P(1.5f, 7)); p.closeSubPath();   // PERC
    g.strokePath(p, juce::PathStrokeType(lw));
}
inline void lockIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool shut)
{
    const float s = r.getWidth() / 24.f;
    juce::Path body; body.addRoundedRectangle(r.getX() + 5 * s, r.getY() + 11 * s, 14 * s, 10 * s, 2 * s);
    juce::Path sh; sh.startNewSubPath(r.getX() + 8 * s, r.getY() + 11 * s); sh.lineTo(r.getX() + 8 * s, r.getY() + 7 * s);
    sh.addCentredArc(r.getX() + 12 * s, r.getY() + 7 * s, 4 * s, 4 * s, 0.f, -juce::MathConstants<float>::halfPi, shut ? juce::MathConstants<float>::halfPi : 0.35f, false);
    if (shut) sh.lineTo(r.getX() + 16 * s, r.getY() + 11 * s);
    g.setColour(c); g.strokePath(body, juce::PathStrokeType(2.4f * s)); g.strokePath(sh, juce::PathStrokeType(2.4f * s));
}
// a soft shadow without a blur: a few wider, fainter shapes (cheap enough to draw 30 times a second)
inline void shadow(juce::Graphics& g, juce::Rectangle<float> r, float rad, float alpha, float dy, float spread, bool round = false)
{
    for (int i = 4; i >= 1; --i)
    {
        const float e = spread * i / 4.f;
        const auto q = r.translated(0, dy * i / 4.f).expanded(e);
        g.setColour(juce::Colours::black.withAlpha(alpha * 0.22f));
        if (round) g.fillEllipse(q); else g.fillRoundedRectangle(q, rad + e);
    }
}
inline void glow(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float alpha, float dy, float spread)
{
    for (int i = 4; i >= 1; --i) { const float e = spread * i / 4.f; g.setColour(c.withAlpha(alpha * 0.2f)); g.fillEllipse(r.translated(0, dy * i / 4.f).expanded(e)); }
}
inline void dashed(juce::Graphics& g, juce::Rectangle<float> r, float rad, juce::Colour c)
{
    juce::Path p, d; p.addRoundedRectangle(r, rad);
    const float dl[] = { 3.f, 3.f };
    juce::PathStrokeType(1.f).createDashedStroke(d, p, dl, 2);
    g.setColour(c); g.fillPath(d);
}

struct Look : juce::LookAndFeel_V4
{
    Look()
    {
        setColour(juce::TextButton::buttonColourId, chip2);
        setColour(juce::TextButton::textColourOffId, white);
        setColour(juce::TooltipWindow::backgroundColourId, chip2);
        setColour(juce::TooltipWindow::textColourId, white);
        setColour(juce::TooltipWindow::outlineColourId, line);
        setColour(juce::TextEditor::backgroundColourId, chip);
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, line);
        setColour(juce::TextEditor::textColourId, white);
        setColour(juce::TextEditor::highlightColourId, white.withAlpha(0.2f));
        setColour(juce::CaretComponent::caretColourId, orange);
        setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ScrollBar::thumbColourId, line);
    }
    // knobs: a dark (or the one orange) cap with a soft shadow and a white mark
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h);
        const float d = std::min(b.getWidth(), b.getHeight()) - 24.f, r = d * 0.5f;
        const auto c = b.getCentre();
        const bool hero = s.getProperties()["hero"] && s.isEnabled();
        const float ang = a0 + pos * (a1 - a0);
        juce::Path cap; cap.addEllipse(c.x - r, c.y - r, d, d);
        if (hero) glow(g, cap.getBounds(), orange, 0.5f, r * 0.2f, r * 0.22f);
        else shadow(g, cap.getBounds(), r, 0.7f, r * 0.2f, r * 0.12f, true);
        juce::ColourGradient gr(hero ? juce::Colour(0xffff7b47) : juce::Colour(0xff3c3c3f), c.x - r * 0.28f, c.y - r * 0.44f,
                                hero ? juce::Colour(0xffd8440e) : juce::Colour(0xff19191b), c.x + r * 0.7f, c.y + r * 0.9f, true);
        gr.addColour(0.55, hero ? juce::Colour(0xfff2541a) : juce::Colour(0xff242427));
        g.setGradientFill(gr); g.fillPath(cap);
        g.setColour(juce::Colours::white.withAlpha(hero ? 0.25f : 0.07f)); g.drawEllipse(c.x - r + 1.f, c.y - r + 1.f, d - 2.f, d - 2.f, 1.f);
        juce::Path m; m.addRoundedRectangle(-2.f, -r + 9.f, 4.f, r * 0.38f, 2.f);
        g.setColour(s.isEnabled() ? white : faint); g.fillPath(m, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
    // buttons used on the pages: chips. property "kind": primary (white), accent (orange), ghost (outline)
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const auto kind = b.getProperties()["kind"].toString();
        const float rad = std::min(9.f, r.getHeight() * 0.5f);
        juce::Colour bg = b.getToggleState() ? chipOn : chip2;
        if (kind == "primary") bg = white;
        else if (kind == "accent") bg = orange;
        else if (kind == "ghost") bg = juce::Colours::transparentBlack;
        if (!b.isEnabled()) bg = bg.withMultipliedAlpha(0.4f);
        g.setColour(bg.brighter(over && b.isEnabled() ? (down ? 0.15f : 0.07f) : 0.f)); g.fillRoundedRectangle(r, rad);
        if (kind == "ghost") { g.setColour(dark); g.drawRoundedRectangle(r, rad, 1.f); }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto kind = b.getProperties()["kind"].toString();
        g.setFont(inter(12.f, b.getToggleState() || kind == "primary" || kind == "accent" ? 600 : 400));
        juce::Colour c = kind == "primary" ? juce::Colour(0xff0b0b0c) : kind == "accent" ? juce::Colours::white : b.getToggleState() ? white : soft;
        g.setColour(c.withMultipliedAlpha(b.isEnabled() ? 1.f : 0.4f));
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return inter(12.f); }
    void fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor&) override
    { g.setColour(chip); g.fillRoundedRectangle(juce::Rectangle<float>(0, 0, (float) w, (float) h), 8.f); }
    void drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& t) override
    { if (t.hasKeyboardFocus(true)) { g.setColour(line); g.drawRoundedRectangle(juce::Rectangle<float>(0.5f, 0.5f, (float) w - 1, (float) h - 1), 8.f, 1.f); } }
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool) override
    {
        const auto t = vertical ? juce::Rectangle<float>((float) x + w * 0.5f - 1.5f, (float) thumbStart, 3.f, (float) thumbSize)
                                : juce::Rectangle<float>((float) thumbStart, (float) y + h * 0.5f - 1.5f, (float) thumbSize, 3.f);
        g.setColour(over ? dark : line); g.fillRoundedRectangle(t, 1.5f);
    }
};
} // namespace ui

// -----------------------------------------------------------------------------------------------------
// the selected pad's sound (pad page): START / END flags, FADE IN / FADE OUT squares, GAIN.
// Drag a square sideways = fade length, up / down = its curve. Double-click a handle = reset it.
// Click the sound = listen. FIT / FULL = zoom to the used part. ("reset" on the page resets the whole shape)
// -----------------------------------------------------------------------------------------------------
class ShapeView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ShapeView(DrumSlicerProcessor& p) : proc(p) {}
    int pad = 0;

    void paint(juce::Graphics& g) override
    {
        auto res = proc.getResult();
        if (!res || !res->pads[(size_t) pad].valid) return;
        const auto sp = proc.shapeParams(pad);
        refreshPeaks(*res, sp.best, sp.user);
        const auto w = wave(); const float mid = w.getCentreY(), hh = w.getHeight() * 0.47f;
        const float endMs = std::min(sp.end, totalMs);
        const float xs = msToX(sp.start), xe = msToX(endMs);
        // the sound as heard: white bars (every 3 px, like the display's own dots); the untouched take dark behind
        for (int i = 0; i < (int) peaks.size(); i += 3)
        {
            const float x = w.getX() + (float) i, ms = xToMs(x + 0.5f);
            const auto pk = peaks[(size_t) i];
            const float a = std::max(std::abs(pk.first), std::abs(pk.second));
            const bool in = ms >= sp.start && ms <= endMs;
            const float e = in ? level(sp, ms) : 1.f;
            const float bh = std::max(1.5f, a * hh * e);
            g.setColour(in ? (e * a > 0.02f ? ui::white : ui::line) : juce::Colour(0xff1c1c1f));
            g.fillRoundedRectangle(x - 1.f, mid - bh, 2.f, 2.f * bh, 1.f);
        }
        // envelope line
        const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        juce::Path env;
        for (float x = xs; x <= xe + 0.5f; x += 1.f)
        {
            const float y = bot - (bot - top) * level(sp, xToMs(std::min(x, xe)));
            if (x == xs) env.startNewSubPath(x, y); else env.lineTo(x, y);
        }
        g.setColour(ui::white.withAlpha(0.35f)); g.strokePath(env, juce::PathStrokeType(1.f));
        // START / END flags
        const bool hs = hot == 1 || drag == 1, he = hot == 5 || drag == 5;
        g.setColour(hs ? ui::orange : ui::dim); g.drawLine(xs, w.getY(), xs, w.getBottom(), hs ? 2.f : 1.2f);
        juce::Path fs; fs.addTriangle(xs, w.getY(), xs + 9.f, w.getY() + 5.f, xs, w.getY() + 10.f); g.fillPath(fs);
        g.setColour(he ? ui::orange : ui::dim); g.drawLine(xe, w.getY(), xe, w.getBottom(), he ? 2.f : 1.2f);
        juce::Path fe; fe.addTriangle(xe, w.getBottom(), xe - 9.f, w.getBottom() - 5.f, xe, w.getBottom() - 10.f); g.fillPath(fe);
        auto square = [&](juce::Point<float> c, bool on)
        { g.setColour(on ? ui::orange : ui::white); g.fillEllipse(juce::Rectangle<float>(9.f, 9.f).withCentre(c)); };
        square(fadeInHandle(sp), hot == 2 || drag == 2);
        square(fadeOutHandle(sp), hot == 6 || drag == 6);
        // GAIN fader, right edge
        {
            const auto fr = gainRect();
            const bool on = hot == 7 || drag == 7;
            g.setColour(ui::chip); g.fillRoundedRectangle(fr, 3.f);
            const float y0 = gainToY(0.f), y = gainToY(sp.gainDb);
            g.setColour(ui::line); g.drawHorizontalLine((int) y0, fr.getX(), fr.getRight());
            g.setColour(on ? ui::orange : ui::white); g.fillRoundedRectangle(juce::Rectangle<float>(fr.getWidth() + 4.f, 4.f).withCentre({ fr.getCentreX(), y }), 2.f);
            if (on || std::abs(sp.gainDb) > 0.05f)
            {
                g.setFont(ui::mono(9.f)); g.setColour(on ? ui::orange : ui::dim2);
                g.drawText((sp.gainDb > 0 ? "+" : "") + juce::String(sp.gainDb, 1) + " db", juce::Rectangle<float>(fr.getX() - 56.f, y - 6.f, 52.f, 12.f), juce::Justification::centredRight);
            }
        }
        // values (only while you touch a handle) + zoom
        g.setFont(ui::mono(9.f));
        if (drag || hot)
        {
            const juce::String t = (drag ? drag : hot) == 1 ? "start " + ms(sp.start) : (drag ? drag : hot) == 5 ? "end " + ms(endMs)
                                 : (drag ? drag : hot) == 2 ? "fade in " + ms(sp.attack) + curveText(sp.attackCurve)
                                 : (drag ? drag : hot) == 6 ? "fade out " + ms(sp.fadeOut) + curveText(sp.fadeOutCurve) : juce::String();
            g.setColour(ui::orange); g.drawText(t, juce::Rectangle<float>(w.getX(), 0, w.getWidth(), 11.f), juce::Justification::centredLeft);
        }
        else { g.setColour(ui::faint); g.drawText(ms(endMs - sp.start), juce::Rectangle<float>(w.getX(), 0, w.getWidth(), 11.f), juce::Justification::centredLeft); }
    }

    void mouseMove(const juce::MouseEvent& e) override { const int h = hit(e.position); if (h != hot) { hot = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { if (hot) { hot = 0; repaint(); } }
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto res = proc.getResult(); if (!res || !res->pads[(size_t) pad].valid) return;
        drag = hit(e.position);
        if (drag && e.getNumberOfClicks() > 1) { proc.undo.beginNewTransaction(); resetHandle(drag); drag = 0; }
        else if (drag) { proc.undo.beginNewTransaction(); beginGesture(); startSp = proc.shapeParams(pad); startPos = e.position; }
        else proc.audition(pad);
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!drag) return;
        const auto sp = proc.shapeParams(pad);
        const float m = juce::jlimit(0.f, totalMs, xToMs(e.position.x)), endMs = std::min(sp.end, totalMs), len = endMs - sp.start;
        const auto w = wave(); const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        const float m0 = juce::jlimit(0.f, totalMs, xToMs(startPos.x)), dy = e.position.y - startPos.y;
        auto curveFrom = [&](float c0) {
            const float L0 = (float) std::pow(0.5, std::pow(2.0, -3.0 * c0));
            const float L = juce::jlimit(0.03f, 0.97f, L0 - dy / (bot - top));
            const float pw = std::log(L) / std::log(0.5f);
            return juce::jlimit(-1.f, 1.f, -std::log2(pw) / 3.f); };
        switch (drag)
        {
            case 1: set("st", juce::jlimit(0.f, endMs - 5.f - sp.attack - sp.fadeOut, m)); break;
            case 5: set("en", m >= std::min(totalMs, viewMs()) - 0.5f ? 2000.f : std::max(m, sp.start + 5.f + sp.attack + sp.fadeOut)); break;
            case 2: set("atk", juce::jlimit(0.f, std::max(0.f, len - sp.fadeOut), startSp.attack + 2.f * (m - m0)));
                    if (std::abs(dy) > 1.f) set("fic", curveFrom(startSp.attackCurve));
                    break;
            case 6: set("fo", juce::jlimit(0.f, std::max(0.f, len - sp.attack), startSp.fadeOut - 2.f * (m - m0)));
                    if (std::abs(dy) > 1.f) set("foc", curveFrom(startSp.fadeOutCurve));
                    break;
            case 7: { float gdb = yToGain(gainToY(startSp.gainDb) + dy * (e.mods.isShiftDown() ? 0.25f : 1.f));
                      if (std::abs(gdb) < 0.25f) gdb = 0.f;
                      set("gain", std::round(gdb * 10.f) / 10.f); break; }
            default: break;
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { if (drag) { endGesture(); proc.audition(pad); } drag = 0; repaint(); }
    void invalidate() { peakKey = -1; repaint(); }
    void resetAll() { proc.undo.beginNewTransaction(); resetIds({ "st", "en", "atk", "fic", "fo", "foc", "hold", "dcy", "gain" }); }

private:
    float fadeOutLevel(const ShapeParams& sp, float msIn) const
    {
        const float endMs = std::min(sp.end, totalMs);
        if (sp.fadeOut <= 0 || msIn < endMs - sp.fadeOut) return 1.f;
        return (float) std::pow(std::max(0.0, (double) (endMs - msIn) / sp.fadeOut), std::pow(2.0, -3.0 * sp.fadeOutCurve));
    }
    float level(const ShapeParams& sp, float msIn) const
    {
        const float t = msIn - sp.start;
        double env = 1.0;
        if (t < sp.attack) env = std::pow(std::max(0.f, t) / std::max(0.001f, sp.attack), std::pow(2.0, -3.0 * sp.attackCurve));
        else if (t > sp.attack + sp.hold) env = std::pow(10.0, -3.0 * (t - sp.attack - sp.hold) / std::max(1.f, sp.decay));
        return (float) juce::jlimit(0.0, 1.0, env * fadeOutLevel(sp, msIn));
    }
    juce::Point<float> fadeInHandle(const ShapeParams& sp) const
    {
        const auto w = wave(); const float top = w.getY() + 2.f, bot = w.getBottom() - 1.f;
        const float L = (float) std::pow(0.5, std::pow(2.0, -3.0 * sp.attackCurve));
        return { std::max(msToX(sp.start) + 10.f, msToX(sp.start + 0.5f * sp.attack)), bot - (bot - top) * L };
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
        if (gainRect().expanded(5.f, 4.f).contains(p)) return 7;
        if (p.getDistanceFrom(fadeInHandle(sp)) < 9.f) return 2;
        if (p.getDistanceFrom(fadeOutHandle(sp)) < 9.f) return 6;
        const float xs = msToX(sp.start), xe = msToX(std::min(sp.end, totalMs));
        const float ds = std::abs(p.x - xs), de = std::abs(p.x - xe);
        if (std::min(ds, de) < 7.f && p.y >= w.getY() - 4 && p.y <= w.getBottom() + 4) return ds <= de ? 1 : 5;
        return 0;
    }
    juce::Rectangle<float> wave() const { return getLocalBounds().toFloat().withTrimmedTop(14.f).withTrimmedRight(22.f).withTrimmedLeft(4.f); }
    juce::Rectangle<float> gainRect() const { const auto w = wave(); return { (float) getWidth() - 10.f, w.getY() + 4.f, 6.f, w.getHeight() - 8.f }; }
    float gainToY(float db) const { const auto r = gainRect(); return r.getY() + (12.f - juce::jlimit(-24.f, 12.f, db)) / 36.f * r.getHeight(); }
    float yToGain(float y) const { const auto r = gainRect(); return juce::jlimit(-24.f, 12.f, 12.f - (y - r.getY()) / r.getHeight() * 36.f); }
    static float gainLin(const ShapeParams& sp) { return juce::Decibels::decibelsToGain(sp.gainDb); }
    // the view: the sound up to where it really ends (-60 dB), a little more - it does not move while you trim
    float viewMs() const { return juce::jlimit(20.f, std::max(20.f, totalMs), audibleMs * 1.1f + 5.f); }
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
    void refreshPeaks(const ds::Result& r, bool best, bool user)
    {
        const auto& a = r.pads[(size_t) pad].get(best, user);
        const int n = a.getNumSamples(); totalMs = (float) (n / r.sr * 1000.0);
        const int key = pad * 4 + (best ? 1 : 0) + (user ? 2 : 0) + 100 * proc.getResultVersion();
        const int W = (int) wave().getWidth();
        if (key == peakKey && (int) peaks.size() == W) return;
        {   // where the sound really ends
            float pk = 1e-9f; for (int c = 0; c < a.getNumChannels(); ++c) pk = std::max(pk, a.getMagnitude(c, 0, n));
            int last = 0;
            for (int i = n - 1; i >= 0; --i) { bool loud = false; for (int c = 0; c < a.getNumChannels(); ++c) loud = loud || std::abs(a.getSample(c, i)) > pk * 0.003f; if (loud) { last = i; break; } }
            audibleMs = (float) (last / r.sr * 1000.0);
        }
        const float vm = viewMs();
        peakKey = key; peakView = vm; peaks.clear();
        const int nv = std::min(n, (int) (vm * 0.001 * r.sr) + 1);
        float mx = 1e-6f; for (int c = 0; c < a.getNumChannels(); ++c) mx = std::max(mx, a.getMagnitude(c, 0, n));
        for (int x = 0; x < W; ++x)
        {
            const int s0 = (int) ((juce::int64) x * nv / W), s1 = std::max(s0 + 1, (int) ((juce::int64) (x + 3) * nv / W));
            float lo = 0, hi = 0;
            for (int s = s0; s < std::min(n, s1); ++s) { const float v = 0.5f * (a.getSample(0, s) + a.getSample(a.getNumChannels() > 1 ? 1 : 0, s)) / mx; lo = std::min(lo, v); hi = std::max(hi, v); }
            peaks.push_back({ lo, hi });
        }
    }
    DrumSlicerProcessor& proc;
    std::vector<std::pair<float, float>> peaks;
    int peakKey = -1, drag = 0, hot = 0;
    float totalMs = 800.f, peakView = -1.f, audibleMs = 800.f;
    ShapeParams startSp; juce::Point<float> startPos;
};

// -----------------------------------------------------------------------------------------------------
// a "hold" chip: press and keep pressing ~0.9 s (the fill shows it) - instead of an "are you sure?" window
// -----------------------------------------------------------------------------------------------------
struct HoldButton : juce::Component, private juce::Timer
{
    juce::String text; std::function<void()> onHold;
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(ui::orange); g.drawRoundedRectangle(r, 9.f, 1.f);
        if (frac > 0) { g.setColour(ui::orange.withAlpha(0.35f)); g.fillRoundedRectangle(r.withWidth(r.getWidth() * frac), 9.f); }
        g.setFont(ui::inter(12.f)); g.setColour(ui::orange);
        g.drawText(text, getLocalBounds(), juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent&) override { t0 = juce::Time::getMillisecondCounterHiRes(); frac = 0; startTimerHz(60); }
    void mouseUp(const juce::MouseEvent&) override { stopTimer(); frac = 0; repaint(); }
    void timerCallback() override
    {
        frac = (float) ((juce::Time::getMillisecondCounterHiRes() - t0) / 900.0);
        if (frac >= 1.f) { stopTimer(); frac = 0; repaint(); if (onHold) onHold(); return; }
        repaint();
    }
    double t0 = 0; float frac = 0;
};

// -----------------------------------------------------------------------------------------------------
// page: KITS (save, open, export all, delete)
// -----------------------------------------------------------------------------------------------------
class KitsPage : public juce::Component, private juce::Timer
{
public:
    std::function<void(const juce::File&)> onLoad;
    std::function<void(const juce::String&)> onToast;

    explicit KitsPage(DrumSlicerProcessor& p) : proc(p)
    {
        addAndMakeVisible(name);
        name.setFont(ui::inter(13.f)); name.setIndents(10, 6);
        name.setTextToShowWhenEmpty("kit name", ui::faint);
        name.onReturnKey = [this] { save(); };
        for (auto* b : { &saveBtn, &exportBtn, &folderBtn }) addAndMakeVisible(b);
        saveBtn.setButtonText("save this kit"); saveBtn.getProperties().set("kind", "primary");
        exportBtn.setButtonText("export all  wav + midi");
        folderBtn.setButtonText("open folder");
        saveBtn.onClick = [this] { save(); };
        exportBtn.onClick = [this] { auto d = proc.exportFinal(); if (d.exists()) { d.revealToUser(); if (onToast) onToast("exported  " + d.getParentDirectory().getFileName()); } };
        exportBtn.setTooltip("The one-shots as you hear them + the MIDI, into Documents/ReKit/<loop>/EXPORT");
        folderBtn.onClick = [] { ds::kitsRoot().createDirectory(); ds::kitsRoot().startAsProcess(); };
        addChildComponent(del);
        del.text = "hold to delete";
        del.onHold = [this] { deleteKit(hoverKit); };
    }
    void refresh()
    {
        kits.clear();
        for (const auto& d : ds::kitsRoot().findChildFiles(juce::File::findDirectories, false))
            if (ds::isKit(d) && !d.getFileName().startsWithChar('.')) kits.add(d);
        std::sort(kits.begin(), kits.end(), [](const juce::File& a, const juce::File& b) {
            return ds::kitData(a).getChildFile("kit.txt").getLastModificationTime() > ds::kitData(b).getChildFile("kit.txt").getLastModificationTime(); });
        if (name.getText().isEmpty() || !name.hasKeyboardFocus(true)) name.setText(proc.baseName(), false);
        saveBtn.setEnabled(proc.getResult() != nullptr && !proc.isBusy());
        exportBtn.setEnabled(proc.getResult() != nullptr);
        scroll = juce::jlimit(0, std::max(0, rows() - visRows()), scroll);
        confirmUntil = 0; saveBtn.setButtonText("save this kit");
        repaint();
    }
    void visibilityChanged() override { if (isVisible()) { refresh(); startTimerHz(15); } else { stopTimer(); proc.stopPreview(); } }
    void resized() override
    {
        const int w = getWidth();
        folderBtn.setBounds(w - 104, 4, 104, 30);
        exportBtn.setBounds(folderBtn.getX() - 168, 4, 160, 30);
        saveBtn.setBounds(exportBtn.getX() - 124, 4, 116, 30);
        name.setBounds(saveBtn.getX() - 190, 4, 182, 30);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
        g.drawText("Kits", 0, 0, 120, 38, juce::Justification::centredLeft);
        if (kits.isEmpty())
        {
            g.setColour(ui::dim); g.setFont(ui::inter(13.f));
            g.drawText("no saved kits yet: analyse a loop, then save this kit", getLocalBounds().withTrimmedTop(50), juce::Justification::centred);
            return;
        }
        for (int i = 0; i < kits.size(); ++i)
        {
            const auto r = cardRect(i);
            if (r.isEmpty()) continue;
            const auto& k = kits[i];
            const bool loaded = k == proc.getKitDir(), playing = proc.isPreviewing() && proc.previewingKit() == k, hov = i == hoverKit;
            g.setColour(loaded ? ui::cardOn : ui::card); g.fillRoundedRectangle(r, 12.f);
            if (loaded || hov) { g.setColour(loaded ? ui::white : ui::dark); g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.f); }
            g.setColour(ui::white); g.setFont(ui::inter(15.f, 500));
            g.drawText(ui::nice(k.getFileName()), r.reduced(12.f, 0).withHeight(30.f).translated(0, 6.f).toNearestInt(), juce::Justification::centredLeft, true);
            const auto t = ds::kitData(k).getChildFile("kit.txt").getLastModificationTime();
            g.setColour(ui::dim2); g.setFont(ui::mono(10.f));
            g.drawText(lowerDate(t) + (loaded ? "  .  open now" : ""), r.reduced(12.f, 0).withTrimmedTop(36.f).withHeight(14.f).toNearestInt(), juce::Justification::centredLeft);
            // listen (its pattern with its sounds), shown on hover / while playing
            if (hov || playing)
            {
                const auto pz = playRect(i);
                g.setColour(playing ? ui::orange : ui::white); g.drawEllipse(pz.reduced(1.f), 1.2f);
                juce::Path ic;
                if (playing) ic.addRectangle(pz.withSizeKeepingCentre(7.f, 7.f));
                else ic.addTriangle(pz.getCentreX() - 2.5f, pz.getCentreY() - 4.f, pz.getCentreX() - 2.5f, pz.getCentreY() + 4.f, pz.getCentreX() + 4.5f, pz.getCentreY());
                g.fillPath(ic);
                g.setColour(ui::dim); g.setFont(ui::mono(10.f));
                g.drawText("folder", folderRect(i).toNearestInt(), juce::Justification::centred);
            }
        }
        if (rows() > visRows())
        {
            g.setColour(ui::faint); g.setFont(ui::mono(10.f));
            g.drawText(juce::String(scroll + 1) + "-" + juce::String(std::min(rows(), scroll + visRows())) + " / " + juce::String(rows()) + " rows  (wheel)",
                       getLocalBounds().removeFromBottom(14), juce::Justification::centredRight);
        }
        else { g.setColour(ui::faint); g.setFont(ui::mono(10.f)); g.drawText("documents / rekit / kits   .   click = open  .  hover = listen, folder, delete", getLocalBounds().removeFromBottom(14), juce::Justification::centredLeft); }
    }
    void mouseMove(const juce::MouseEvent& e) override { setHover(kitAt(e.position)); }
    void mouseExit(const juce::MouseEvent&) override { if (!del.isMouseButtonDown()) setHover(kitAt(getMouseXYRelative().toFloat())); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    { scroll = juce::jlimit(0, std::max(0, rows() - visRows()), scroll + (w.deltaY < 0 ? 1 : -1)); setHover(-1); repaint(); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const int i = kitAt(e.position);
        if (i < 0 || e.mouseWasDraggedSinceMouseDown()) return;
        if (playRect(i).contains(e.position)) { if (proc.isPreviewing() && proc.previewingKit() == kits[i]) proc.stopPreview(); else proc.previewKit(kits[i]); repaint(); return; }
        if (folderRect(i).contains(e.position)) { kits[i].revealToUser(); return; }
        if (onLoad) onLoad(kits[i]);
    }

private:
    DrumSlicerProcessor& proc;
    juce::TextEditor name;
    juce::TextButton saveBtn, exportBtn, folderBtn;
    HoldButton del;
    juce::Array<juce::File> kits;
    int scroll = 0, hoverKit = -1;
    juce::uint32 confirmUntil = 0;
    bool wasPlaying = false;

    static juce::String lowerDate(juce::Time t) { return t.formatted("%d %b %Y").toLowerCase(); }
    int rows() const { return (kits.size() + 2) / 3; }
    int visRows() const { return std::max(1, (getHeight() - 66) / 104); }
    juce::Rectangle<float> cardRect(int i) const
    {
        const int row = i / 3 - scroll, col = i % 3;
        if (row < 0 || row >= visRows()) return {};
        const float w = ((float) getWidth() - 20.f) / 3.f;
        return { col * (w + 10.f), 50.f + row * 104.f, w, 94.f };
    }
    juce::Rectangle<float> playRect(int i) const { const auto r = cardRect(i); return { r.getX() + 12.f, r.getBottom() - 34.f, 22.f, 22.f }; }
    juce::Rectangle<float> folderRect(int i) const { const auto r = cardRect(i); return { r.getX() + 42.f, r.getBottom() - 34.f, 50.f, 22.f }; }
    juce::Rectangle<int> delRect(int i) const { const auto r = cardRect(i); return juce::Rectangle<float>(r.getRight() - 122.f, r.getBottom() - 36.f, 110.f, 26.f).toNearestInt(); }
    int kitAt(juce::Point<float> p) const { for (int i = 0; i < kits.size(); ++i) if (cardRect(i).contains(p)) return i; return -1; }
    void setHover(int i)
    {
        if (i == hoverKit) return;
        hoverKit = i;
        del.setVisible(i >= 0);
        if (i >= 0) del.setBounds(delRect(i));
        repaint();
    }
    void deleteKit(int i)
    {
        if (i < 0 || i >= kits.size()) return;
        const auto k = kits[i];
        proc.stopPreview();
        if (k.moveToTrash() || k.deleteRecursively()) { proc.kitRemoved(k); if (onToast) onToast("kit deleted  " + k.getFileName()); }
        setHover(-1); refresh();
    }
    void save()
    {
        const auto nm = juce::File::createLegalFileName(name.getText().trim());
        if (nm.isEmpty() || proc.getResult() == nullptr) return;
        const auto dir = ds::kitsRoot().getChildFile(nm);
        if (ds::isKit(dir) && dir != proc.getKitDir() && juce::Time::getMillisecondCounter() > confirmUntil)
        {   // a kit with that name exists: the button asks once more (no dialog)
            confirmUntil = juce::Time::getMillisecondCounter() + 3000;
            saveBtn.setButtonText("replace it?"); return;
        }
        confirmUntil = 0; saveBtn.setButtonText("save this kit");
        const auto k = proc.saveKit(nm);
        if (onToast) onToast(k.isDirectory() ? "kit saved  " + k.getFileName() : juce::String("could not save the kit"));
        refresh();
    }
    void timerCallback() override
    {
        const bool p = proc.isPreviewing(); if (p || p != wasPlaying) repaint(); wasPlaying = p;
        if (confirmUntil && juce::Time::getMillisecondCounter() > confirmUntil) { confirmUntil = 0; saveBtn.setButtonText("save this kit"); }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KitsPage)
};

// -----------------------------------------------------------------------------------------------------
// page: SAMPLES (yours, for the selected pad)
// FOLDERS: your sample folders (added once, remembered), click = listen as loud as the pad.
// SIMILAR: the sounds of your archive nearest to the pad's sound, then ranked by how near each gets after a match.
// Double-click / drag onto a pad / USE ON PAD = it becomes that pad's sound. Arrow keys walk the list and play.
// -----------------------------------------------------------------------------------------------------
class SampleBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void(const juce::File&)> onUse;
    std::function<void(const juce::File&)> onKept;
    std::function<int()> targetPad;
    std::function<juce::String()> targetName;
    std::function<void()> onTried;
    std::function<void()> onBack;                  // the path at the top:  ‹ Kick   Samples
    std::function<juce::String()> backName;

    std::function<void()> onOther;                 // LOOPS: a file from anywhere (the system's file window)
    explicit SampleBrowser(DrumSlicerProcessor& p, bool loopsPage = false) : proc(p), loopsMode(loopsPage)
    {
        addAndMakeVisible(list);
        list.setModel(this); list.setRowHeight(30); list.setOutlineThickness(0);
        list.setWantsKeyboardFocus(true);
        for (auto* b : { &addBtn, &remBtn, &upBtn, &useBtn, &revealBtn, &tabFolders, &tabSimilar, &scanBtn, &limitBtn, &moreBtn, &tryBtn }) addAndMakeVisible(b);
        addChildComponent(firstBtn);
        firstBtn.setButtonText("add your sample folder"); firstBtn.getProperties().set("kind", "primary");
        firstBtn.onClick = [this] { addFolder(); };
        addBtn.setButtonText("+ folder"); remBtn.setButtonText("remove"); upBtn.setButtonText("up");
        useBtn.setButtonText("keep on pad"); useBtn.getProperties().set("kind", "primary");
        useBtn.setTooltip("The selected file becomes this pad's sound (and the pad is locked)");
        revealBtn.setButtonText("open folder");
        tabFolders.setButtonText("folders"); tabSimilar.setButtonText("similar  .  ranked after match");
        tabFolders.onClick = [this] { setMode(0); };
        tabSimilar.onClick = [this] { setMode(1); };
        scanBtn.setButtonText("rescan"); scanBtn.setTooltip("Look for new / changed files in your sample folders (rescan all with SHIFT)");
        scanBtn.onClick = [this] { proc.library->scan(juce::ModifierKeys::currentModifiers.isShiftDown()); repaint(); };
        limitBtn.setTooltip("Files longer than this are loops / long files and left out (click to change)");
        limitBtn.onClick = [this] { const float l = proc.library->maxSeconds(); const float n = l < 6 ? 8.f : l < 9 ? 10.f : l < 11 ? 15.f : 5.f;
                                    proc.library->setMaxSeconds(n); saveSettings(); updateLimit(); proc.library->scan(false); };
        moreBtn.setButtonText("5 more");
        tryBtn.setButtonText("try in kit");
        tryBtn.setTooltip("On: every file you pick sits on the pad for now and plays in the pattern. use on pad keeps it, off goes back.");
        tryBtn.onClick = [this] { tryMode = !tryMode; tryBtn.setToggleState(tryMode, juce::dontSendNotification); if (!tryMode) proc.endTry(false); };
        // what to look like: what the pad's card says (the loop's sound, or its best hit) - or, switched on, your sample
        addAndMakeVisible(likeBtn[0]);
        likeBtn[0].setButtonText("like my sample");
        likeBtn[0].setTooltip("On: look for sounds like the sample on this pad (not like the loop)");
        likeBtn[0].onClick = [this] { likeMine = !likeMine; want = 5; findSimilar(true); };
        moreBtn.onClick = [this] { want += 5; if (want > (int) matches.size()) findSimilar(true); else { list.updateContent(); list.repaint(); } };
        addBtn.setTooltip("Add a sample folder (remembered)"); remBtn.setTooltip("Take the folder you are in off the list (files stay)");
        addAndMakeVisible(filter);
        filter.setFont(ui::inter(12.f)); filter.setIndents(10, 6);
        filter.setTextToShowWhenEmpty("filter  (kick, 808, snare ...)", ui::faint);
        filter.onTextChange = [this] { rebuild(); };
        addBtn.onClick = [this] { addFolder(); };
        remBtn.onClick = [this] { removeFolder(); };
        upBtn.onClick = [this] { if (dir.getParentDirectory() != dir && isInsideRoot(dir.getParentDirectory())) open(dir.getParentDirectory()); };
        useBtn.onClick = [this] {
            auto f = selectedFile();
            if (proc.isTrying() && proc.triedFile() == f) { proc.endTry(true); if (onKept) onKept(f); return; }
            if (f.existsAsFile() && onUse) onUse(f); };
        revealBtn.onClick = [this] { if (auto f = selectedFile(); f.existsAsFile()) f.revealToUser(); else if (dir.isDirectory()) dir.revealToUser(); };
        loadSettings();
        updateLimit();
        if (loopsMode)
        {   // LOOPS: your loop folders; click = hear it as it is, double-click / load = it becomes the loop
            mode = 0;
            firstBtn.setButtonText("add your loop folder");
            useBtn.setButtonText("load this loop"); useBtn.setTooltip("Load the selected loop (then choose the part and analyse)");
            revealBtn.setButtonText("other file ...");
            revealBtn.onClick = [this] { if (onOther) onOther(); };
            for (juce::Component* c : std::initializer_list<juce::Component*> { &tabFolders, &tabSimilar, &tryBtn }) c->setVisible(false);
        }
    }
    ~SampleBrowser() override { if (rank) rank->cancel = true; saveSettings(); }

    void showFor() { if (!dir.isDirectory() && !rootDirs.isEmpty()) open(rootDirs[0]); else rebuild(); setMode(mode); list.grabKeyboardFocus(); repaint(); }
    void showSimilar() { setMode(1); }
    void visibilityChanged() override { if (isVisible()) startTimerHz(10); else { stopTimer(); proc.stopPreview(); stopTrying(); saveSettings(); } }
    bool isTrying() const { return proc.isTrying(); }
    void stopTrying() { if (tryMode) { tryMode = false; tryBtn.setToggleState(false, juce::dontSendNotification); } proc.endTry(false); }

    void paint(juce::Graphics& g) override
    {
        {
            const auto bn = backName ? backName() : juce::String();
            float x = 0;
            if (bn.isNotEmpty())
            {
                const auto t = juce::String(juce::CharPointer_UTF8("\xe2\x80\xb9 ")) + bn;
                const float w = ui::textW(ui::inter(30.f, 300), t);
                backRect = { 0.f, 0.f, w + 4.f, 38.f };
                g.setColour(backHot ? ui::white : ui::dim); g.setFont(ui::inter(30.f, 300));
                g.drawText(t, backRect, juce::Justification::centredLeft);
                x = w + 18.f;
            }
            else backRect = {};
            g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
            g.drawText(loopsMode ? "Loops" : "Samples", juce::Rectangle<float>(x, 0.f, 160.f, 38.f), juce::Justification::centredLeft);
        }
        if (tryMode)
        {   // trying: say it, so it is not forgotten
            const auto tn = targetName ? targetName() : juce::String();
            g.setColour(ui::orange); g.setFont(ui::mono(10.f));
            g.drawText("trying on " + ui::lower(tn) + "  .  press play to hear it in the kit  .  keep on pad keeps it",
                       juce::Rectangle<int>(revealBtn.getRight() + 12, revealBtn.getY(), getWidth() - revealBtn.getRight() - 90, revealBtn.getHeight()), juce::Justification::centredLeft, true);
        }
        if (rootDirs.isEmpty())
        {
            auto r = juce::Rectangle<int>(0, 60, getWidth(), 120);
            g.setColour(ui::white); g.setFont(ui::inter(14.f, 500));
            g.drawText(loopsMode ? juce::String("listen to your loops before you load one") : juce::String("choose a sample of yours for a pad"), r.removeFromTop(24), juce::Justification::centredLeft);
            g.setColour(ui::dim); g.setFont(ui::inter(12.f));
            g.drawFittedText("1.  add your one-shot folder (once, it is remembered)\n2.  click a file (or the arrow keys): you hear it as loud as the pad\n"
                             "3.  double-click it, drag it onto a pad, or use on pad\n4.  similar: the sounds of your folders nearest to the pad",
                             r.withTrimmedTop(4), juce::Justification::topLeft, 6, 1.f);
            return;
        }
        // left column: your folders (folders) / what the archive knows (similar)
        const auto lc = leftCol();
        if (mode == 0)
        {
            for (int i = 0; i < rootDirs.size(); ++i)
            {
                const auto r = rootRect(i);
                if (r.getBottom() > (float) addBtn.getY() - 4.f) break;
                const bool on = dir == rootDirs[i] || dir.isAChildOf(rootDirs[i]);
                if (on) { g.setColour(ui::chip); g.fillRoundedRectangle(r, 7.f); }
                g.setColour(on ? ui::soft : ui::dim); g.setFont(ui::inter(12.f));
                g.drawText(rootDirs[i].getFileName(), r.reduced(9.f, 0).toNearestInt(), juce::Justification::centredLeft, true);
            }
            g.setColour(ui::faint); g.setFont(ui::mono(10.f));
            g.drawText(dir.isDirectory() ? shortPath(dir) : juce::String(), juce::Rectangle<int>(listX() + 60, 46, getWidth() - listX() - 60, 14), juce::Justification::centredRight, true);
            if (items.isEmpty()) { g.setColour(ui::dim); g.setFont(ui::inter(12.f)); g.drawText("nothing here", list.getBounds(), juce::Justification::centred); }
            return;
        }
        auto& lib = *proc.library;
        {   // what it looks for
            g.setColour(ui::dim); g.setFont(ui::inter(11.f));
            g.drawText(likeSrc == 2 ? "looking like: your sample" : likeSrc == 1 ? "looking like: the best hit" : "looking like: the loop's sound",
                       lc.withTrimmedTop(32).withHeight(16).toNearestInt(), juce::Justification::centredLeft);
        }
        {
            const auto st = lib.status();
            g.setColour(ui::soft); g.setFont(ui::inter(11.f));
            g.drawFittedText(ui::lower(st.upToFirstOccurrenceOf("\n", false, false)), lc.withTrimmedTop(116).withHeight(30).toNearestInt(), juce::Justification::topLeft, 2, 1.f);
            g.setColour(ui::faint); g.setFont(ui::mono(9.f));
            g.drawFittedText(ui::lower(st.fromFirstOccurrenceOf("\n", false, false)), lc.withTrimmedTop(148).withHeight(40).toNearestInt(), juce::Justification::topLeft, 3, 1.f);
        }
        if (lib.scanning())
        {
            const auto bar = lc.withTrimmedTop(192).withHeight(3.f);
            g.setColour(ui::line); g.fillRoundedRectangle(bar, 1.5f);
            g.setColour(ui::orange); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * lib.progress()), 1.5f);
        }
        if (rank && !ranked && !matches.empty())
        {
            g.setColour(ui::orange); g.setFont(ui::mono(10.f));
            g.drawText("checking after match  " + juce::String(rank->done.load()) + " / " + juce::String((int) matches.size()),
                       juce::Rectangle<int>(listX(), 46, getWidth() - listX(), 14), juce::Justification::centredRight);
        }
        if (matches.empty())
        {
            g.setColour(ui::dim); g.setFont(ui::inter(12.f));
            const auto ix = lib.index();
            const juce::String why = !havePad ? "choose a pad with a sound"
                                   : (!ix || ix->oneShots == 0) ? (lib.scanning() ? "reading your folders ..." : "press rescan to read your folders")
                                   : "nothing found";
            g.drawText(why, list.getBounds(), juce::Justification::centred);
        }
    }
    void resized() override
    {
        const int w = getWidth(), h = getHeight();
        tabSimilar.setBounds(w - 196, 4, 196, 30);
        tabFolders.setBounds(tabSimilar.getX() - 78, 4, 74, 30);
        const int lx = listX();
        // left column
        const int lw = (int) leftCol().getWidth();
        addBtn.setBounds(0, h - 30, 72, 26); remBtn.setBounds(76, h - 30, 66, 26);
        likeBtn[0].setBounds(0, 46, lw, 26);
        scanBtn.setBounds(0, h - 30, 70, 26); limitBtn.setBounds(76, h - 30, 56, 26);
        // list + bottom row
        upBtn.setBounds(lx, 42, 44, 24);
        filter.setBounds(lx + 50, 42, 200, 24);
        list.setBounds(lx, mode == 0 ? 72 : 66, w - lx, h - (mode == 0 ? 72 : 66) - 36);
        firstBtn.setBounds(0, 186, 240, 34);
        useBtn.setBounds(lx, h - 30, 104, 28);
        tryBtn.setBounds(useBtn.getRight() + 8, h - 30, 90, 28);
        revealBtn.setBounds(loopsMode ? useBtn.getRight() + 8 : tryBtn.getRight() + 8, h - 30, 100, 28);
        moreBtn.setBounds(w - 70, h - 30, 70, 28);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!backRect.isEmpty() && backRect.contains(e.position)) { if (onBack) onBack(); return; }
        if (mode != 0) return;
        for (int i = 0; i < rootDirs.size(); ++i) if (rootRect(i).contains(e.position)) { open(rootDirs[i]); return; }
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const bool h = !backRect.isEmpty() && backRect.contains(e.position);
        setMouseCursor(h ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        if (h != backHot) { backHot = h; repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { if (backHot) { backHot = false; repaint(); } }

private:
    DrumSlicerProcessor& proc;
    juce::ListBox list;
    juce::TextEditor filter;
    juce::TextButton addBtn, remBtn, upBtn, useBtn, revealBtn, firstBtn, tabFolders, tabSimilar, scanBtn, limitBtn, moreBtn;
    std::array<juce::TextButton, 3> likeBtn;
    juce::TextButton tryBtn;
    bool tryMode = false, likeMine = false;
    int likeSrc = 0;
    juce::uint32 lastFind = 0;
    juce::Array<juce::File> rootDirs, items;
    juce::File dir;
    std::unique_ptr<juce::FileChooser> chooser;
    bool wasPlaying = false;
    int mode = 0;
    std::vector<ds::Match> matches;
    std::shared_ptr<const ds::LibIndex> holder;
    std::shared_ptr<DrumSlicerProcessor::RankState> rank;
    std::vector<float> after;
    bool ranked = false;
    juce::String queryKey;
    bool havePad = false;
    int want = 5;
    juce::uint32 selectedAt = 0;
    bool scanWas = false;

    juce::Rectangle<float> backRect; bool backHot = false;
    int listX() const { return 206; }
    juce::Rectangle<float> leftCol() const { return { 0.f, 46.f, 190.f, (float) getHeight() - 46.f }; }
    juce::Rectangle<float> rootRect(int i) const { return { 0.f, 46.f + i * 30.f, 190.f, 26.f }; }
    bool loopsMode = false;
    juce::File settingsFile() const { return ds::root().getChildFile(loopsMode ? "loops.txt" : "browser.txt"); }
    static bool isAudio(const juce::File& f) { return f.hasFileExtension("wav;wave;aif;aiff;flac;mp3;ogg"); }
    juce::String shortPath(const juce::File& d) const
    {
        for (auto& r : rootDirs) if (d == r || d.isAChildOf(r)) return (r.getFileName() + "/" + d.getRelativePathFrom(r)).trimCharactersAtEnd("/.").toLowerCase();
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
    void updateLimit() { limitBtn.setButtonText("< " + juce::String(juce::roundToInt(proc.library->maxSeconds())) + " s"); }
    void setMode(int m)
    {
        mode = loopsMode ? 0 : m;
        tabFolders.setToggleState(mode == 0, juce::dontSendNotification); tabSimilar.setToggleState(mode == 1, juce::dontSendNotification);
        list.setRowHeight(30);
        updateControls();
        resized();
        if (mode == 1) { proc.library->scanOnce(); want = 5; findSimilar(true); }
        else rebuild();
        saveSettings();
        repaint();
    }
    void updateControls()
    {
        const bool any = !rootDirs.isEmpty(), f = any && mode == 0, sm = any && mode == 1;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &remBtn, &upBtn, &filter, &addBtn }) c->setVisible(f);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &scanBtn, &limitBtn, &moreBtn, &likeBtn[0] }) c->setVisible(sm);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &list, &useBtn, &revealBtn, &tabFolders, &tabSimilar, &tryBtn }) c->setVisible(any && !(loopsMode && (c == &tabFolders || c == &tabSimilar || c == &tryBtn)));
        if (loopsMode) revealBtn.setVisible(true);
        firstBtn.setVisible(!any);
        remBtn.setEnabled(any);
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
                saveSettings(); updateControls(); open(d);
                proc.library->scan(false);
            });
    }
    void removeFolder()
    {
        int i = -1; for (int k = 0; k < rootDirs.size(); ++k) if (dir == rootDirs[k] || dir.isAChildOf(rootDirs[k])) i = k;
        if (i < 0) return;
        rootDirs.remove(i);
        dir = rootDirs.isEmpty() ? juce::File() : rootDirs[0];
        saveSettings(); updateControls(); rebuild(); repaint();
        proc.library->scan(false);
    }
    void open(const juce::File& d) { dir = d; filter.clear(); rebuild(); list.scrollToEnsureRowIsOnscreen(0); repaint(); }
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
    void findSimilar(bool force)
    {
        if (mode != 1) return;
        auto r = proc.getResult();
        const int pad = targetPad ? targetPad() : -1;
        havePad = r && pad >= 0 && pad < ds::kPads && r->pads[(size_t) pad].valid;
        const bool hasC = havePad && r->pads[(size_t) pad].user.getNumSamples() > 0;
        likeSrc = likeMine && hasC ? 2 : (havePad && proc.padSource(pad) == 1 ? 1 : 0);
        const int src = likeSrc;
        likeBtn[0].setToggleState(likeMine && hasC, juce::dontSendNotification); likeBtn[0].setEnabled(hasC);
        auto ix = proc.library->index();
        const juce::String key = havePad ? juce::String(proc.getResultVersion()) + "|" + juce::String(pad) + "|" + juce::String(src) + "|"
                                           + juce::String((juce::pointer_sized_int) ix.get()) : juce::String();
        if (!force && key == queryKey) return;
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
            if (!paths.empty()) rank = proc.startRank(pad, src, paths);
        }
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
        if (loopsMode) proc.previewLoopFile(f);
        else if (tryMode && r && pad >= 0 && r->pads[(size_t) pad].valid) { proc.tryOnPad(pad, f); if (onTried) onTried(); }
        else proc.previewSample(f, pad);
        list.repaint();
    }
    int getNumRows() override { return mode == 1 ? std::min(want, (int) matches.size()) : items.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        const auto f = rowFile(row);
        if (f == juce::File()) return;
        const bool folder = mode == 0 && f.isDirectory();
        const bool playing = !folder && proc.isPreviewing() && proc.previewingKit() == f;
        const bool onPad = proc.isTrying() && proc.triedFile() == f;
        const auto r = juce::Rectangle<float>(1.f, 2.f, (float) w - 2.f, (float) h - 4.f);
        if (sel) { g.setColour(ui::cardOn); g.fillRoundedRectangle(r, 8.f); g.setColour(ui::white); g.drawRoundedRectangle(r, 8.f, 1.f); }
        // a play sign, the name, then (similar) how it differs and how near it gets
        juce::Path ic;
        if (folder) { g.setColour(ui::dim); g.drawRoundedRectangle(10.f, h * 0.5f - 5.f, 12.f, 10.f, 2.f, 1.2f); }
        else { ic.addTriangle(12.f, h * 0.5f - 5.f, 12.f, h * 0.5f + 5.f, 20.f, h * 0.5f); g.setColour(playing ? ui::orange : ui::white); g.fillPath(ic); }
        g.setColour(playing ? ui::orange : folder ? ui::soft : ui::white); g.setFont(ui::inter(12.f, folder ? 500 : 400));
        if (mode == 1)
        {
            const auto& m = matches[(size_t) row];
            const float aft = row < (int) after.size() ? after[(size_t) row] : -1.f;
            g.drawText(f.getFileNameWithoutExtension(), 32, 0, w - 330, h, juce::Justification::centredLeft, true);
            g.setColour(ui::dim2); g.setFont(ui::mono(10.f));
            g.drawText(m.tags.isEmpty() ? juce::String(m.score >= 90.f ? "very close" : m.score >= 75.f ? "close" : "similar") : m.tags.joinIntoString("  ").toLowerCase(),
                       w - 300, 0, 180, h, juce::Justification::centredRight, true);
            g.setColour(ui::white); g.setFont(ui::mono(12.f, true));
            g.drawText(juce::String(juce::roundToInt(aft >= 0 ? aft : m.score)), w - 112, 0, 34, h, juce::Justification::centredRight);
            g.setColour(onPad ? ui::orange : ui::soft); g.setFont(ui::inter(10.f));
            g.drawText(onPad ? "on the pad" : aft >= 0 ? "after match" : "as it is", w - 76, 0, 70, h, juce::Justification::centredRight);
            return;
        }
        g.drawText(folder ? f.getFileName() : f.getFileNameWithoutExtension(), 32, 0, w - 110, h, juce::Justification::centredLeft, true);
        if (onPad) { g.setColour(ui::orange); g.setFont(ui::inter(10.f)); g.drawText("on the pad", w - 76, 0, 70, h, juce::Justification::centredRight); }
    }
    void selectedRowsChanged(int row) override
    {
        selectedAt = juce::Time::getMillisecondCounter();
        const auto f = rowFile(row);
        if (f.existsAsFile()) play(f);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
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
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override
    {
        if (rows.size() != 1 || loopsMode) return {};
        const auto f = rowFile(rows[0]);
        return f.existsAsFile() ? juce::var("rekit-sample:" + f.getFullPathName()) : juce::var();
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::backspaceKey && mode == 0 && !filter.hasKeyboardFocus(true)) { upBtn.triggerClick(); return true; }
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
                {
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
            findSimilar(false);
            if (proc.library->scanning() || scanWas) repaint();
            scanWas = proc.library->scanning();
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleBrowser)
};

// =====================================================================================================
//  THE DEVICE
// =====================================================================================================
class DrumSlicerPanel : public juce::Component, public juce::FileDragAndDropTarget, public juce::TextDragAndDropTarget,
                        public juce::DragAndDropContainer, public juce::DragAndDropTarget, private juce::Timer
{
public:
    enum Page { pKit = 0, pSource, pPattern, pPad, pMore, pSamples, pKits, pLoops };
    static constexpr int W = 1240, H = 720;

    explicit DrumSlicerPanel(DrumSlicerProcessor& p) : proc(p), shape(p), samples(p), loops(p, true), kitsPage(p)
    {
        addChildComponent(loops);
        loops.onUse = [this](const juce::File& f) { proc.stopPreview(); proc.loadSource(f); setPage(pSource); };
        loops.onBack = [this] { setPage(pSource); };
        loops.backName = [] { return juce::String("Source"); };
        loops.onOther = [this] { chooseLoop(); };
        setSize(W, H);
        setLookAndFeel(&look);
        setWantsKeyboardFocus(true);
        for (int k = 0; k < 8; ++k)
        {
            auto& s = knobs[(size_t) k];
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            s.setMouseDragSensitivity(200);
            s.onDragStart = [this] { proc.undo.beginNewTransaction(); };
            s.onValueChange = [this] { repaint(knobArea()); shape.repaint(); };
            addAndMakeVisible(s);
        }
        knobs[0].getProperties().set("hero", true);
        addChildComponent(shape);
        shape.setTooltip("Click: listen.  Flags: start / end.  Dots: drag sideways = fade length, up / down = curve.  Right edge: gain (SHIFT = fine).  Double-click a handle = reset it.");
        addChildComponent(samples);
        samples.onUse = [this](const juce::File& f) { useSample(selected, f); };
        samples.targetPad = [this] { return selected; };
        samples.onBack = [this] { setPage(samplesFrom == pSamples ? pPad : samplesFrom); };
        samples.backName = [this] { auto r = proc.getResult(); if (!r) return juce::String();
                                    const auto& pd = r->pads[(size_t) selected];
                                    if (samplesFrom == pKit) return juce::String("Kit");
                                    return pd.valid ? juce::String(pd.name.substring(0, 1)) + pd.name.substring(1).toLowerCase() : juce::String("Empty pad"); };
        samples.onTried = [this] { repaint(); };
        samples.onKept = [this](const juce::File& f) { proc.setPadLocked(selected, true); toast("yours  " + f.getFileNameWithoutExtension()); repaint(); };
        samples.targetName = [this] { auto r = proc.getResult(); if (!r) return juce::String();
                                      return r->pads[(size_t) selected].valid ? r->pads[(size_t) selected].name : "new " + (pendingType.isNotEmpty() ? pendingType : juce::String("sound")); };
        addChildComponent(kitsPage);
        kitsPage.onLoad = [this](const juce::File& k) { proc.stopPreview(); proc.loadKit(k); toast("kit  " + k.getFileName()); setPage(pKit); };
        kitsPage.onToast = [this](const juce::String& t) { toast(t); };
        addChildComponent(delPad);
        delPad.text = "hold to delete pad + hits";
        delPad.onHold = [this] {
            auto r = proc.getResult(); if (!r || !r->pads[(size_t) selected].valid) return;
            const auto nm = r->pads[(size_t) selected].name;
            proc.deletePad(selected); toast("deleted  " + nm); setPage(pKit); };
        page = proc.getSource() ? (proc.getResult() ? pKit : pSource) : pSource;
        rebindKnobs();
        updatePage();
        startTimerHz(30);
    }
    ~DrumSlicerPanel() override { setLookAndFeel(nullptr); }
    // tests / screenshots: show a page as if clicked there
    void debugShow(int pg, int pad, int bank, int hover)
    { juce::ignoreUnused(bank); selected = juce::jlimit(0, ds::kPads - 1, pad); hoverPad = hover; page = (Page) pg; updatePage(); timerCallback(); }

    // ------------------------------------------------------------------ layout (design size 1240 x 720)
    // the display is the work place: big. Knobs and pads only as big as a hand needs.
    // the display: the whole width. Under it, a row of hardware: keys | 8 square pads | 4 knobs + loop / your kit
    const juce::Rectangle<float> display { 34.f, 64.f, 1172.f, 430.f };
    const juce::Rectangle<float> content { 60.f, 84.f, 1120.f, 368.f };         // inside the display, above the page dots
    juce::Rectangle<int> knobArea() const { return { 800, 500, 420, 210 }; }
    juce::Rectangle<float> knobRect(int k) const { return { 818.f + (k % 4) * 98.f, 528.f + (k / 4) * 80.f, 74.f, 74.f }; }   // 8 knobs, 2 rows
    juce::Rectangle<float> padRect(int i) const { return { 240.f + (i % 4) * 138.f, 512.f + (i / 4) * 92.f, 128.f, 84.f }; }
    juce::Rectangle<float> buildRect() const { return { 84.f, 514.f, 92.f, 92.f }; }
    juce::Rectangle<float> keyRect(int k) const { return { 58.f + k * 52.f, 628.f, 40.f, 40.f }; }   // play, undo, midi
    juce::Rectangle<float> switchRect() const { return { 1096.f, 507.f, 44.f, 22.f }; }

    void resized() override
    {
        for (int k = 0; k < 8; ++k) knobs[(size_t) k].setBounds(knobRect(k).toNearestInt());
        shape.setBounds(juce::Rectangle<float>(content.getX(), content.getY() + 128.f, content.getWidth() - 290.f, content.getHeight() - 180.f).toNearestInt());
        samples.setBounds(content.withTrimmedBottom(0).toNearestInt());
        kitsPage.setBounds(content.toNearestInt());
        loops.setBounds(content.toNearestInt());
        delPad.setBounds(juce::Rectangle<float>(content.getX() + 230.f, content.getBottom() - 34.f, 200.f, 30.f).toNearestInt());
    }

    // ------------------------------------------------------------------ drops: a file over a pad = its sound, else a new loop
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
    int padAt(juce::Point<float> p) const { for (int i = 0; i < ds::kPads; ++i) if (padRect(i).contains(p)) return i; return -1; }
    void dropFile(const juce::File& f, juce::Point<float> p)
    {
        const int pad = padAt(p);
        if (pad >= 0 && proc.getResult()) useSample(pad, f);
        else { proc.loadSource(f); setPage(pSource); }
    }
    bool isInterestedInFileDrag(const juce::StringArray& f) override { for (auto& s : f) if (isAudio(juce::File(s))) return true; return false; }
    void fileDragEnter(const juce::StringArray&, int x, int y) override { dropping = true; dropPad = padAt({ (float) x, (float) y }); repaint(); }
    void fileDragMove(const juce::StringArray&, int x, int y) override { const int p = padAt({ (float) x, (float) y }); if (p != dropPad) { dropPad = p; repaint(); } }
    void fileDragExit(const juce::StringArray&) override { dropping = false; dropPad = -1; repaint(); }
    void filesDropped(const juce::StringArray& f, int x, int y) override
    {
        dropping = false; dropPad = -1;
        for (auto& s : f) if (isAudio(juce::File(s))) { dropFile(juce::File(s), { (float) x, (float) y }); break; }
        repaint();
    }
    bool isInterestedInTextDrag(const juce::String& t) override { return fileFromText(t).existsAsFile(); }
    void textDragEnter(const juce::String&, int x, int y) override { dropping = true; dropPad = padAt({ (float) x, (float) y }); repaint(); }
    void textDragMove(const juce::String&, int x, int y) override { const int p = padAt({ (float) x, (float) y }); if (p != dropPad) { dropPad = p; repaint(); } }
    void textDragExit(const juce::String&) override { dropping = false; dropPad = -1; repaint(); }
    void textDropped(const juce::String& t, int x, int y) override { dropping = false; dropPad = -1; auto f = fileFromText(t); if (f.existsAsFile()) dropFile(f, { (float) x, (float) y }); repaint(); }
    static juce::File browserFile(const SourceDetails& d)
    {
        const auto t = d.description.toString();
        return t.startsWith("rekit-sample:") ? juce::File(t.fromFirstOccurrenceOf(":", false, false)) : juce::File();
    }
    bool isInterestedInDragSource(const SourceDetails& d) override { return browserFile(d).existsAsFile(); }
    void itemDragEnter(const SourceDetails& d) override { dropPad = padAt(d.localPosition.toFloat()); repaint(); }
    void itemDragMove(const SourceDetails& d) override { const int p = padAt(d.localPosition.toFloat()); if (p != dropPad) { dropPad = p; repaint(); } }
    void itemDragExit(const SourceDetails&) override { dropPad = -1; repaint(); }
    void itemDropped(const SourceDetails& d) override
    {
        const int pad = padAt(d.localPosition.toFloat());
        dropPad = -1;
        if (pad >= 0) useSample(pad, browserFile(d));
        repaint();
    }
    void useSample(int pad, const juce::File& f)
    {
        auto res = proc.getResult();
        if (pad < 0 || !f.existsAsFile() || !res) return;
        selected = pad;
        if (res->pads[(size_t) pad].valid) { if (proc.setPadSample(pad, f)) toast("yours  " + f.getFileNameWithoutExtension()); }
        else if (proc.addPad(pad, pendingType, f)) toast("new  " + ui::lower(proc.getResult()->pads[(size_t) pad].name) + "  .  " + f.getFileNameWithoutExtension());
        pendingType.clear();
        rebindKnobs(); shape.invalidate(); repaint();
    }

    // ------------------------------------------------------------------ mouse: zones drawn by the pages, pads, keys
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int hp = padAt(e.position);
        const int hz = zoneAt(e.position);
        const auto hr = hz >= 0 ? zones[(size_t) hz].r : juce::Rectangle<float>();
        if (hp != hoverPad || hr != hotRect) { hoverPad = hp; hotRect = hr; repaint(); }
        const int h = page == pSource ? handleAt(e.position.x, e.position.y) : 0;
        setMouseCursor(h != 0 ? juce::MouseCursor::LeftRightResizeCursor : hz >= 0 && zones[(size_t) hz].click ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { if (hoverPad >= 0 || !hotRect.isEmpty()) { hoverPad = -1; hotRect = {}; repaint(); } }
    // a scrollbar under a zoomed view: drag the handle (or click beside it) to move along
    int scrolling = 0; double scrollGrab = 0;     // 1 = pattern, 2 = source
    juce::Rectangle<float> patScrollRect() const { const auto L = patLanes(); return { L.getX(), L.getBottom() + 4.f, L.getWidth(), 6.f }; }
    juce::Rectangle<float> srcScrollRect() const { return { content.getX(), content.getY() + 308.f, content.getWidth(), 6.f }; }
    static juce::Rectangle<float> thumbOf(juce::Rectangle<float> r, double a, double b, double total)
    { return { r.getX() + (float) (a / total) * r.getWidth(), r.getY(), std::max(18.f, (float) ((b - a) / total) * r.getWidth()), r.getHeight() }; }
    void drawScroll(juce::Graphics& g, juce::Rectangle<float> r, double a, double b, double total)
    {
        if (total <= 0 || b - a >= total - 0.5) return;            // not zoomed: no bar
        g.setColour(juce::Colour(0xff141416)); g.fillRoundedRectangle(r, 3.f);
        g.setColour(scrolling ? ui::white : ui::dim); g.fillRoundedRectangle(thumbOf(r, a, b, total), 3.f);
    }
    void scrollTo(float x)
    {
        auto r = proc.getResult(); auto src = proc.getSource();
        if (scrolling == 1 && r)
        {
            const double total = std::max(1, r->loop.getNumSamples()), span = pv1 - pv0, sr = patScrollRect().getWidth();
            const double a = juce::jlimit(0.0, total - span, (double) (x - patScrollRect().getX()) / sr * total - scrollGrab);
            pv0 = a; pv1 = a + span;
        }
        if (scrolling == 2 && src)
        {
            const double total = (double) src->length(), span = (double) (v1 - v0), sr = srcScrollRect().getWidth();
            const double a = juce::jlimit(0.0, total - span, (double) (x - srcScrollRect().getX()) / sr * total - scrollGrab);
            v0 = (juce::int64) a; v1 = (juce::int64) (a + span);
        }
        repaint();
    }
    bool scrollDown(juce::Point<float> p)
    {
        auto r = proc.getResult(); auto src = proc.getSource();
        if (page == pPattern && r && patScrollRect().expanded(0, 5.f).contains(p) && pv1 - pv0 < r->loop.getNumSamples() - 0.5)
        {
            const double total = std::max(1, r->loop.getNumSamples());
            const auto th = thumbOf(patScrollRect(), pv0, pv1, total);
            scrolling = 1; scrollGrab = th.contains(p.x, th.getCentreY()) ? (double) (p.x - th.getX()) / patScrollRect().getWidth() * total : (pv1 - pv0) * 0.5;
            scrollTo(p.x); return true;
        }
        if (page == pSource && src && srcScrollRect().expanded(0, 5.f).contains(p) && v1 - v0 < src->length())
        {
            const double total = (double) src->length();
            const auto th = thumbOf(srcScrollRect(), (double) v0, (double) v1, total);
            scrolling = 2; scrollGrab = th.contains(p.x, th.getCentreY()) ? (double) (p.x - th.getX()) / srcScrollRect().getWidth() * total : (double) (v1 - v0) * 0.5;
            scrollTo(p.x); return true;
        }
        return false;
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (scrollDown(e.position)) return;
        const int dz = zoneAt(e.position); dragStarted = false; downPad = -1; hasDown = dz >= 0;
        if (hasDown) { downZone = zones[(size_t) dz]; return; }   // a zone acts on mouse up (or on a drag)
        const int pad = padAt(e.position);
        if (pad >= 0)
        {
            if (proc.isTrying() && pad != selected) proc.endTry(false);
            if (e.mods.isPopupMenu()) { selectPad(pad); setPage(pMore); return; }
            downPad = pad; return;
        }
        if (page == pSource && display.contains(e.position)) sourceDown(e);
        if (page == pPattern && tightRect().contains(e.position)) { proc.undo.beginNewTransaction(); dragTight = true; setTight(e.position.x); return; }
        if (page == pPattern && rulerRect().contains(e.position)) { dragCursor = true; setCursor(e.position.x); return; }
        if (page == pPattern && patLanes().expanded(6.f, 8.f).contains(e.position)) patternDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (scrolling) { scrollTo(e.position.x); return; }
        if (dragTight) { setTight(e.position.x); return; }
        if (!dragStarted && e.getDistanceFromDragStart() > 6)
        {
            if (hasDown && downZone.drag) { dragStarted = true; auto fn = downZone.drag; fn(); return; }
            if (downPad >= 0 && padHasSound(downPad))
            {   // a pad dragged out: its sound as you hear it, as a WAV
                dragStarted = true;
                auto f = proc.shotFile(downPad);
                if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
                return;
            }
        }
        if (page == pSource) sourceDrag(e);
        if (page == pPattern) { if (dragCursor) setCursor(e.position.x); else patternDrag(e); }
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const bool wasDragTight = dragTight; dragTight = false; dragWhat = 0;
        if (scrolling) { scrolling = 0; repaint(); return; }
        if (dragCursor) { dragCursor = false; return; }
        if (patEditing) { patternUp(); repaint(); return; }
        if (wasDragTight || dragStarted) return;
        if (hasDown) { hasDown = false; if (downZone.r.contains(e.position) && downZone.click) { auto fn = downZone.click; fn(); repaint(); } return; }
        if (downPad >= 0 && downPad == padAt(e.position)) padClick(downPad, e.position);
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto src = proc.getSource();
        if (page != pSource || !src || !display.contains(e.position) || proc.isBusy()) return;
        const int h = handleAt(e.position.x, e.position.y);
        if (h == 1) proc.setSelection(0, proc.selEnd());
        else if (h == 2) proc.setSelection(proc.selStart(), src->length());
        else if (waveZone().contains(e.position)) { v0 = 0; v1 = src->length(); }
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (page == pPattern && display.contains(e.position)) { patternWheel(e, w); return; }
        auto src = proc.getSource();
        if (page != pSource || !src || !display.contains(e.position)) return;
        fixView(*src);
        const double span = (double) (v1 - v0), n = (double) src->length();
        if (e.mods.isShiftDown() || std::abs(w.deltaX) > std::abs(w.deltaY))
        {
            const double d = -(std::abs(w.deltaX) > std::abs(w.deltaY) ? w.deltaX : w.deltaY) * span * 0.25;
            const double a = juce::jlimit(0.0, n - span, (double) v0 + d);
            v0 = (juce::int64) a; v1 = (juce::int64) (a + span);
        }
        else
        {
            const double at = (double) sampleAt(e.position.x);
            const double ns = juce::jlimit(std::min(n, 0.05 * src->sr), n, span * std::pow(0.8, w.deltaY * 4.0));
            const double f = (at - (double) v0) / span;
            const double a = juce::jlimit(0.0, n - ns, at - f * ns);
            v0 = (juce::int64) a; v1 = (juce::int64) (a + ns);
        }
        repaint();
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { setPage(proc.getResult() ? pKit : pSource); return true; }
        if (page == pPattern && (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)) { deleteSelectedHit(); repaint(); return true; }
        return false;
    }

    // ------------------------------------------------------------------ timer: follow the processor
    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        auto res = proc.getResult();
        // a new analysis (or kit) arrived
        if (proc.getResultVersion() != shownVersion)
        {
            const bool fresh = res && res.get() != lastRes;
            shownVersion = proc.getResultVersion(); lastRes = res.get();
            if (fresh && analysing) { settleStart = now; setPage(pKit); }
            if (res && !res->pads[(size_t) selected].valid) for (int i = 0; i < ds::kPads; ++i) if (res->pads[(size_t) i].valid) { selected = i; break; }
            shape.invalidate(); rebindKnobs(); repaint();
        }
        const bool busy = proc.isBusy();
        if (busy && !analysing) { analysing = true; analyseStart = now; }
        if (!busy && analysing) { analysing = false; if (!res) setPage(pSource); }
        // build my kit: the display follows it on the kit page
        const bool building = proc.isBuilding();
        if (building && !wasBuilding) { buildStart = now; scoreFrom = kitScore(); setPage(pKit); }
        if (!building && wasBuilding) { buildEnd = now; scoreFrom = shownScore; }
        wasBuilding = building;
        // the sounds changed (a sample, a match, a suggestion): undo steps of the knobs before it would not fit any more
        {
            juce::String sig;
            if (res) for (int i = 0; i < ds::kPads; ++i) { const auto& pd = res->pads[(size_t) i]; const auto md = proc.getMatch(i);
                sig << pd.userPath << pd.layerPath << (int) md.on << juce::roundToInt(md.after) << proc.suggestionIndex(i) << "|"; }
            if (res) { sig << (juce::int64) res->sourceId; for (auto& pd : res->pads) sig << pd.name << (int) pd.valid; }
            sig << proc.getKitDir().getFullPathName() << proc.getLoopFile().getFullPathName();
            if (sig != soundSig) { soundSig = sig; proc.undo.clearUndoHistory(); }
        }
        // messages of the processor -> toast
        {
            const auto msg = proc.matchMessage();
            if (msg != lastMatchMsg)
            {
                lastMatchMsg = msg;
                if (msg.startsWith("MATCHED") || msg.startsWith("A HAS") || msg.startsWith("ALREADY") || msg.startsWith("LAYERED") || msg.startsWith("NO LAYER")
                    || msg.startsWith("LAYER REMOVED") || msg.startsWith("UNDO") || msg.startsWith("KIT BUILT") || msg.startsWith("BUILD MY KIT") || msg.startsWith("SUGGESTION") || msg.startsWith("COULD NOT"))
                    toast(ui::lower(msg));
                rebindKnobs();
            }
            const auto st = proc.getStatus();
            if (st != lastStatus) { lastStatus = st; if (st.startsWith("COULD NOT")) toast(ui::lower(st) + "  .  the loop you had stays"); }
        }
        // pads light up as they sound
        for (size_t i = 0; i < 8; ++i) levels[i] = std::max(proc.padLevel[i].exchange(0.f) > 0.f ? 1.f : 0.f, levels[i] * 0.86f);
        if (page == pSamples && selected != browsedPad) { browsedPad = selected; samples.repaint(); }
        rebindKnobs();
        bool lit = false; for (float l : levels) lit = lit || l > 0.01f;
        const bool moving = lit || patEditing || loopListening || scrolling || busy || building || proc.isMatching() || proc.isMidiPlaying() || proc.isSourcePlaying() || proc.isPreviewing()
                         || now - settleStart < 1000 || now - buildEnd < 1000 || juce::Time::getMillisecondCounter() < toastUntil + 100
                         || std::abs(tightShown - (proc.midiMode() == ds::kTight ? proc.apvts.getRawParameterValue("quant")->load() : 0.f)) > 0.001f;
        if (moving || ++idle >= 6) { idle = 0; repaint(); }      // otherwise 5 times a second is enough to follow changes
    }

    // ------------------------------------------------------------------ drawing
    void paint(juce::Graphics& g) override
    {
        zones.clear();
        const auto now = juce::Time::getMillisecondCounterHiRes();
        auto res = proc.getResult();
        // the body
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff232326), 0, 0, juce::Colour(0xff17171a), 0, (float) H, false));
        g.fillAll();
        g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawHorizontalLine(0, 0, (float) W);
        drawHeader(g, res);
        // the display
        {
            juce::Path p; p.addRoundedRectangle(display, 20.f);
            g.setColour(ui::screen); g.fillPath(p);
            g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.8f), 0, display.getY(), juce::Colours::transparentBlack, 0, display.getY() + 14.f, false));
            g.fillPath(p);
            g.setColour(juce::Colour(0xff2a2a2c)); g.strokePath(p, juce::PathStrokeType(1.f));
            if (dropping && dropPad < 0) { g.setColour(ui::orange); g.strokePath(p, juce::PathStrokeType(1.5f)); }
        }
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(display.reduced(2.f).toNearestInt());
            if (proc.isBusy()) drawAnalysing(g, now);
            else
                switch (page)
                {
                    case pKit:     drawKit(g, res, now); break;
                    case pSource:  drawSource(g, res); break;
                    case pPattern: drawPattern(g, res, now); break;
                    case pPad:     drawPad(g, res); break;
                    case pMore:    drawMore(g, res); break;
                    default: break;
                }
            drawTabs(g, res);
        }
        hint.clear();
        drawKnobs(g, res);
        drawKeys(g, res);
        drawPads(g, res, now);
        drawToast(g);
    }

private:
    // ---------------------------------------------------------------- zones (clickable parts of the drawing)
    struct Zone { juce::Rectangle<float> r; std::function<void()> click, drag; };
    std::vector<Zone> zones;
    Zone downZone; bool hasDown = false;
    juce::Rectangle<float> hotRect;
    int downPad = -1;
    bool dragStarted = false, dragTight = false;
    int zone(juce::Rectangle<float> r, std::function<void()> click, std::function<void()> drag = {})
    { zones.push_back({ r, std::move(click), std::move(drag) }); return (int) zones.size() - 1; }
    int zoneAt(juce::Point<float> p) const { for (int i = (int) zones.size() - 1; i >= 0; --i) if (zones[(size_t) i].r.contains(p)) return i; return -1; }
    bool hot(juce::Rectangle<float> r) const { return !hotRect.isEmpty() && hotRect == r; }

    // a chip (a small rounded button drawn on the display); returns its rectangle
    enum ChipKind { cNormal, cOn, cOutline, cDashed, cPrimary, cAccent, cGhostAccent };
    juce::Rectangle<float> chip(juce::Graphics& g, float x, float y, const juce::String& t, ChipKind k, std::function<void()> click, float h = 24.f, float fs = 11.f, bool right = false)
    {
        const auto f = ui::inter(fs, k == cPrimary || k == cAccent || k == cOn ? 600 : 400);
        const float w = ui::textW(f, t) + 20.f;
        const auto r = juce::Rectangle<float>(right ? x - w : x, y, w, h);
        const bool over = click && hot(r);
        juce::Colour bg = juce::Colours::transparentBlack, fg = ui::soft;
        switch (k)
        {
            case cNormal: bg = ui::chip2; fg = ui::soft; break;
            case cOn: bg = ui::white; fg = juce::Colour(0xff0b0b0c); break;
            case cOutline: fg = ui::soft; break;
            case cDashed: fg = ui::faint; break;
            case cPrimary: bg = ui::white; fg = juce::Colour(0xff0b0b0c); break;
            case cAccent: bg = ui::orange; fg = juce::Colours::white; break;
            case cGhostAccent: fg = ui::orange; break;
        }
        if (!bg.isTransparent()) { g.setColour(bg.brighter(over ? 0.08f : 0.f)); g.fillRoundedRectangle(r, h * 0.5f); }
        if (k == cOutline) { g.setColour(over ? ui::soft : ui::dark); g.drawRoundedRectangle(r.reduced(0.5f), h * 0.5f, 1.f); }
        if (k == cGhostAccent) { g.setColour(ui::orange); g.drawRoundedRectangle(r.reduced(0.5f), h * 0.5f, 1.f); }
        if (k == cDashed) ui::dashed(g, r.reduced(0.5f), h * 0.5f, ui::line);
        if (over && bg.isTransparent()) { g.setColour(juce::Colours::white.withAlpha(0.05f)); g.fillRoundedRectangle(r, h * 0.5f); }
        g.setColour(fg); g.setFont(f); g.drawText(t, r, juce::Justification::centred);
        if (click) zone(r, std::move(click));
        return r;
    }
    // a segmented switch: returns nothing, each part is a zone
    void segmented(juce::Graphics& g, juce::Rectangle<float> r, const juce::StringArray& parts, int on, std::function<void(int)> pick, float fs = 11.f)
    {
        g.setColour(ui::chip); g.fillRoundedRectangle(r, 9.f);
        float total = 0; for (auto& s : parts) total += ui::textW(ui::inter(fs, 600), s) + 24.f;
        float x = r.getX() + 3.f;
        const float scale = (r.getWidth() - 6.f) / std::max(1.f, total);
        for (int i = 0; i < parts.size(); ++i)
        {
            const float w = (ui::textW(ui::inter(fs, 600), parts[i]) + 24.f) * scale;
            const auto pr = juce::Rectangle<float>(x, r.getY() + 3.f, w, r.getHeight() - 6.f);
            if (i == on) { g.setColour(ui::chipOn); g.fillRoundedRectangle(pr, 7.f); }
            else if (hot(pr)) { g.setColour(juce::Colours::white.withAlpha(0.04f)); g.fillRoundedRectangle(pr, 7.f); }
            g.setColour(i == on ? ui::white : ui::dim); g.setFont(ui::inter(fs, i == on ? 600 : 400));
            g.drawText(parts[i], pr, juce::Justification::centred);
            zone(pr, [pick, i] { pick(i); });
            x += w;
        }
    }

    // ---------------------------------------------------------------- pages
    Page samplesFrom = pPad;
    void setPage(Page p)
    {
        if (p == page) { updatePage(); return; }
        if (p == pSamples) samplesFrom = page == pKit ? pKit : page == pMore ? pMore : pPad;
        page = p; updatePage(); repaint();
    }
    void updatePage()
    {
        auto res = proc.getResult();
        shape.setVisible(page == pPad && res && res->pads[(size_t) selected].valid && !proc.isBusy());
        shape.pad = selected; shape.invalidate();
        const bool showSamples = page == pSamples && !proc.isBusy();
        if (showSamples != samples.isVisible()) { samples.setVisible(showSamples); if (showSamples) samples.showFor(); }
        kitsPage.setVisible(page == pKits && !proc.isBusy());
        const bool showLoops = page == pLoops && !proc.isBusy();
        if (showLoops != loops.isVisible()) { loops.setVisible(showLoops); if (showLoops) loops.showFor(); }
        delPad.setVisible(page == pMore && res && res->pads[(size_t) selected].valid && !proc.isBusy());
        rebindKnobs();
    }
    void selectPad(int i) { selected = i; shape.pad = i; shape.invalidate(); rebindKnobs(); repaint(); }
    void padClick(int i, juce::Point<float>)
    {
        auto res = proc.getResult();
        selectPad(i);
        if (!res) { toast("analyse a loop first"); return; }
        if (!res->pads[(size_t) i].valid || proc.padIsSilent(i))
        {   // nothing to hear: give it a sound
            if (samples.isTrying()) samples.stopTrying();
            pendingType.clear(); setPage(pSamples); return;
        }
        proc.audition(i);
        if (page != pSamples && page != pKits) setPage(pPad);
        updatePage();
    }

    // ---------------------------------------------------------------- header
    void drawHeader(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        g.setColour(ui::white); g.setFont(ui::mono(16.f, true));
        g.drawText("rekit", 34, 24, 80, 22, juce::Justification::centredLeft);
       #ifdef JucePlugin_VersionString
        const juce::String ver = JucePlugin_VersionString;
       #else
        const juce::String ver = "2.3.1";
       #endif
        g.setColour(ui::faint); g.setFont(ui::mono(10.f));
        g.drawText(ver, 96, 28, 60, 16, juce::Justification::centredLeft);
        juce::String st;
        const bool busy = proc.isBusy() || proc.isMatching();
        if (proc.isBusy()) st = ui::lower(proc.getStatus());
        else if (proc.isMatching()) st = ui::lower(proc.matchMessage());
        else if (res) st = "ready  .  " + juce::String(res->bpm, 1).trimCharactersAtEnd("0").trimCharactersAtEnd(".") + " bpm  .  " + juce::String(bars(*res)) + (bars(*res) == 1 ? " bar" : " bars");
        else if (proc.getSource()) st = "loaded  .  choose the part";
        else st = "drop a drum loop";
        g.setFont(ui::mono(12.f));
        const float tw = ui::textW(ui::mono(12.f), st);
        g.setColour(juce::Colour(0xff9a9a98));
        g.drawText(st, juce::Rectangle<float>(1206.f - tw, 24.f, tw, 22.f), juce::Justification::centredRight);
        const float pulse = busy ? 0.55f + 0.45f * std::sin((float) juce::Time::getMillisecondCounterHiRes() * 0.008f) : 1.f;
        const auto dot = juce::Rectangle<float>(8.f, 8.f).withCentre({ 1206.f - tw - 16.f, 35.f });
        g.setColour(ui::orange.withAlpha(0.35f * pulse)); g.fillEllipse(dot.expanded(4.f));
        g.setColour(res || busy ? ui::orange.withAlpha(pulse) : ui::faint); g.fillEllipse(dot);
    }
    static int bars(const ds::Result& r) { return std::max(1, (int) std::lround(r.loop.getNumSamples() / r.sr * r.bpm / 240.0)); }

    // ---------------------------------------------------------------- page dots (bottom of the display)
    void drawTabs(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        static const char* names[] = { "kit", "source", "pattern", "pad", "samples", "kits" };
        static const Page pages[] = { pKit, pSource, pPattern, pPad, pSamples, pKits };
        const Page cur = page == pMore ? pPad : page == pLoops ? pSource : page;
        float x = content.getX();
        const float y = display.getBottom() - 30.f;
        g.setFont(ui::mono(11.f));
        for (int i = 0; i < 6; ++i)
        {
            const bool on = pages[i] == cur, can = res != nullptr || pages[i] == pSource || pages[i] == pKits;
            const juce::String t = juce::String(on ? juce::CharPointer_UTF8("\xe2\x97\x8f ") : juce::CharPointer_UTF8("\xe2\x97\x8b ")) + names[i];
            const float w = ui::textW(ui::mono(11.f), t);
            const auto r = juce::Rectangle<float>(x - 4.f, y - 2.f, w + 8.f, 18.f);
            g.setColour(on ? ui::white : (can ? (hot(r) ? ui::soft : ui::faint) : ui::line));
            g.drawText(t, juce::Rectangle<float>(x, y, w + 2.f, 14.f), juce::Justification::centredLeft);
            if (can) { const Page pg = pages[i]; zone(r, [this, pg] { setPage(pg); }); }
            x += w + 20.f;
        }
        if (res)
        {
            g.setColour(juce::Colour(0xff5e5e61)); g.setFont(ui::mono(11.f));
            g.drawText("extraction [" + juce::String(juce::roundToInt(100.0 * res->minHearing())) + "]", juce::Rectangle<float>(content.getRight() - 200.f, y, 200.f, 14.f), juce::Justification::centredRight);
        }
    }
    void toast(const juce::String& t) { toastText = t; toastUntil = juce::Time::getMillisecondCounter() + 2600; repaint(); }
    juce::String hint;      // what a key would do, while the mouse is over it (in the strip above the display)
    void drawToast(juce::Graphics& g)
    {
        if ((juce::Time::getMillisecondCounter() >= toastUntil || toastText.isEmpty()) && hint.isNotEmpty())
        {
            const auto f = ui::mono(11.f);
            const auto t = juce::Rectangle<float>(std::min(640.f, ui::textW(f, hint) + 36.f), 26.f).withCentre({ 600.f, 35.f });
            g.setColour(ui::dim); g.setFont(f); g.drawText(hint, t, juce::Justification::centred, true);
            return;
        }
        if (juce::Time::getMillisecondCounter() >= toastUntil || toastText.isEmpty()) return;
        const auto f = ui::mono(11.f);
        const float w = std::min(display.getWidth() - 60.f, ui::textW(f, toastText) + 36.f);
        const auto t = juce::Rectangle<float>(std::min(w, 640.f), 26.f).withCentre({ 600.f, 35.f });
        g.setColour(ui::screen); g.fillRoundedRectangle(t, 13.f);
        g.setColour(ui::orange); g.drawRoundedRectangle(t, 13.f, 1.f);
        g.setFont(f); g.drawText(toastText, t, juce::Justification::centred, true);
    }

    // ---------------------------------------------------------------- the lanes: every hit a dot at its real time
    // ---------------------------------------------------------------- page: KIT = the sounds (one row per pad)
    juce::Rectangle<float> rowsRect() const { return { content.getX(), content.getY() + 84.f, content.getWidth(), content.getHeight() - 96.f }; }
    float kitScore() const
    {
        auto r = proc.getResult(); if (!r) return -1.f;
        float s = 0; int n = 0;
        for (int i = 0; i < ds::kPads; ++i)
        {
            if (!r->pads[(size_t) i].valid || r->pads[(size_t) i].hits == 0) continue;
            const auto md = proc.getMatch(i);
            if (proc.padSource(i) == 2 && md.on) { s += md.after; ++n; }
        }
        return n ? s / n : -1.f;
    }
    // small waves of what each pad plays (from the shaped sounds)
    std::array<std::vector<float>, ds::kPads> miniWave; const void* miniKey = nullptr;
    void refreshMini()
    {
        auto sh = proc.getShaped(); auto r = proc.getResult();
        if (!sh || !r) { for (auto& m : miniWave) m.clear(); miniKey = nullptr; return; }
        if (sh.get() == miniKey) return;
        miniKey = sh.get();
        for (int p = 0; p < ds::kPads; ++p)
        {
            auto& w = miniWave[(size_t) p]; w.assign(64, 0.f);
            const auto& b = sh->buf[(size_t) p];
            const int n = b.getNumSamples(); if (n == 0) continue;
            float pk = 1e-9f; for (int k = 0; k < n; k += 2) pk = std::max(pk, std::abs(b.getSample(0, k)));
            int last = 0; for (int k = n - 1; k >= 0; k -= 2) if (std::abs(b.getSample(0, k)) > pk * 0.01f) { last = k; break; }
            const int span = juce::jlimit(std::min(n, (int) (0.05 * r->sr)), n, (int) (last * 1.1) + 16);
            float mx = 1e-6f;
            for (int i = 0; i < 64; ++i)
            {
                const int s0 = (int) ((juce::int64) span * i / 64), s1 = std::max(s0 + 1, (int) ((juce::int64) span * (i + 1) / 64));
                float m = 0; for (int k = s0; k < s1; k += 4) m = std::max(m, std::abs(b.getSample(0, k)));
                w[(size_t) i] = m; mx = std::max(mx, m);
            }
            for (auto& v : w) v /= mx;
        }
    }
    void drawKit(juce::Graphics& g, const std::shared_ptr<ds::Result>& res, double now)
    {
        if (!res) { drawDropHere(g); return; }
        const auto C = content;
        g.setColour(ui::dim2); g.setFont(ui::mono(11.f));
        const auto kd = proc.getKitDir();
        g.drawText(kd != juce::File() ? "kit  .  " + kd.getFileName() : proc.getLoopFile().getFileName().toLowerCase(), juce::Rectangle<float>(C.getX(), C.getY(), 500.f, 14.f), juce::Justification::centredLeft, true);
        const auto titleR = juce::Rectangle<float>(C.getX(), C.getY() + 14.f, 560.f, 52.f);
        g.setColour(ui::white); g.setFont(ui::inter(42.f, 300, -0.02f));
        g.drawText(ui::nice(proc.baseName()), titleR, juce::Justification::centredLeft, true);
        zone(titleR, [this] { setPage(pKits); });
        {   // your kit against the loop
            float target = kitScore();
            const double since = now - buildEnd;
            if (buildEnd > 0 && since < 700) target = scoreFrom + (std::max(0.f, target) - scoreFrom) * (float) juce::jlimit(0.0, 1.0, since / 700.0);
            shownScore = target;
            const auto rc = juce::Rectangle<float>(C.getRight() - 236.f, C.getY() + 2.f, 62.f, 62.f);
            g.setColour(juce::Colour(0xff232325)); g.drawEllipse(rc.reduced(2.f), 4.f);
            if (target > 0)
            {
                juce::Path a; a.addCentredArc(rc.getCentreX(), rc.getCentreY(), rc.getWidth() * 0.5f - 2.f, rc.getHeight() * 0.5f - 2.f, 0.f, 0.f,
                                              juce::MathConstants<float>::twoPi * juce::jlimit(0.f, 1.f, target / 100.f), true);
                g.setColour(ui::white); g.strokePath(a, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour(ui::white); g.setFont(ui::mono(24.f, true));
            g.drawText(target > 0 ? "[" + juce::String(juce::roundToInt(target)) + "]" : juce::String("[--]"), juce::Rectangle<float>(rc.getRight() + 12.f, rc.getY() + 6.f, 160.f, 30.f), juce::Justification::centredLeft);
            g.setColour(ui::dim); g.setFont(ui::inter(11.f));
            g.drawText(target > 0 ? "your kit vs the loop" : "build my kit, or choose samples", juce::Rectangle<float>(rc.getRight() + 12.f, rc.getY() + 36.f, 200.f, 16.f), juce::Justification::centredLeft);
        }
        refreshMini();
        const auto R = rowsRect();
        const float rh = R.getHeight() / 8.f;
        const bool building = proc.isBuilding();
        const double sinceEnd = now - buildEnd, sinceSettle = now - settleStart;
        if (building)
        {
            g.setColour(ui::orange); g.setFont(ui::mono(11.f));
            g.drawText("building your kit  " + proc.matchMessage().toLowerCase().replace("building kit", "").trim(), juce::Rectangle<float>(R.getX(), R.getY() - 18.f, R.getWidth(), 14.f), juce::Justification::centredRight);
        }
        int shownRow = 0, firstEmpty = -1;
        for (int k = 0; k < ds::kPads; ++k) if (!res->pads[(size_t) k].valid && firstEmpty < 0) firstEmpty = k;
        for (int k = 0; k < ds::kPads; ++k)
        {
            if (!res->pads[(size_t) k].valid)
            {
                if (k != firstEmpty) continue;
                const auto row = juce::Rectangle<float>(R.getX(), R.getY() + shownRow * rh, R.getWidth(), rh - 2.f);
                g.setColour(hot(row) ? ui::white : ui::dim); g.setFont(ui::inter(12.f));
                g.drawText("+  add a sound  (an empty pad)", row.withTrimmedLeft(34.f), juce::Justification::centredLeft);
                zone(row, [this, k] { selectPad(k); pendingType.clear(); setPage(pSamples); });
                continue;
            }
            const auto row = juce::Rectangle<float>(R.getX(), R.getY() + (shownRow++) * rh, R.getWidth(), rh - 2.f);
            const auto& pd = res->pads[(size_t) k];
            const bool valid = pd.valid, sel = k == selected;
            float alpha = 1.f;
            if (settleStart > 0 && sinceSettle < 900) alpha = (float) juce::jlimit(0.0, 1.0, (sinceSettle - 70.0 * k) / 250.0);   // rows come in one by one
            if (valid && !proc.padAudible(k)) alpha *= 0.35f;
            if (sel) { g.setColour(ui::card.withMultipliedAlpha(alpha)); g.fillRoundedRectangle(row, 8.f); }
            else if (hot(row)) { g.setColour(juce::Colour(0xff0c0c0e)); g.fillRoundedRectangle(row, 8.f); }
            const float cy = row.getCentreY(), lv = levels[(size_t) k];
            const auto ink = (lv > 0.05f ? ui::orange : ui::white).withMultipliedAlpha(alpha);
            ui::glyph(g, valid ? pd.name : "-", juce::Rectangle<float>(12.f, 12.f).withCentre({ row.getX() + 16.f, cy }), valid ? ink : ui::line);
            const juce::String nm = valid ? juce::String(pd.name.substring(0, 1)) + pd.name.substring(1).toLowerCase() : juce::String("-");
            g.setColour(valid ? ui::white.withMultipliedAlpha(alpha) : ui::line); g.setFont(ui::inter(13.f, 500));
            g.drawText(nm, juce::Rectangle<float>(row.getX() + 34.f, row.getY(), 96.f, row.getHeight()), juce::Justification::centredLeft, true);
            const int src = valid ? proc.padSource(k) : 0;
            const bool hasC = valid && pd.user.getNumSamples() > 0;
            juce::String sub = !valid ? juce::String("empty  .  click to add") : proc.padIsSilent(k) ? juce::String("no sound  .  click to add")
                             : (src == 2 && hasC) ? pd.userName + (pd.layered() ? " + " + pd.layerName : juce::String()) : src == 1 ? juce::String("best hit (the loop's)") : juce::String("the loop's sound");
            g.setColour((src == 2 && hasC ? ui::soft : ui::dim2).withMultipliedAlpha(valid ? alpha : 0.6f)); g.setFont(ui::inter(11.f));
            g.drawText(sub, juce::Rectangle<float>(row.getX() + 134.f, row.getY(), 250.f, row.getHeight()), juce::Justification::centredLeft, true);
            // the wave of what it plays (building: a light looks for its sound)
            const float sc = row.getRight() - 300.f;          // where the score column starts
            const auto wr = juce::Rectangle<float>(row.getX() + 396.f, row.getY() + 4.f, sc - 20.f - (row.getX() + 396.f), row.getHeight() - 8.f);
            const bool toBuild = building && valid && pd.hits > 0 && !proc.padLocked(k) && !(src == 2 && hasC);
            const bool locking = !building && buildEnd > 0 && sinceEnd < 700;
            const auto& w = miniWave[(size_t) k];
            for (size_t i = 0; i < w.size() && valid; ++i)
            {
                const float x = wr.getX() + (float) i * wr.getWidth() / (float) w.size();
                if (w[i] < 0.015f) continue;                        // silence: nothing drawn
                const float h = std::max(1.f, w[i] * wr.getHeight() * 0.5f);
                juce::Colour c = toBuild ? ui::line : (locking && sinceEnd < 80.0 * k ? ui::line : ink.withMultipliedAlpha(sel ? 1.f : 0.75f));
                g.setColour(c); g.fillRoundedRectangle(x, wr.getCentreY() - h, 2.f, 2.f * h, 1.f);
            }
            if (toBuild)
            {
                const float ph = (float) std::fmod((now - buildStart) / 1300.0 + k * 0.137, 1.0);
                const float x = wr.getX() + ph * wr.getWidth();
                g.setColour(ui::orange.withAlpha(0.3f)); g.fillRoundedRectangle(x - 12.f, wr.getCentreY() - 3.f, 24.f, 6.f, 3.f);
                g.setColour(ui::orange); g.fillRoundedRectangle(x - 8.f, wr.getCentreY() - 1.5f, 16.f, 3.f, 1.5f);
            }
            // how near it is (your sample, matched)
            const auto md = valid ? proc.getMatch(k) : MatchData();
            g.setFont(ui::mono(12.f, true));
            g.setColour(src == 2 && hasC && md.on ? ui::white.withMultipliedAlpha(alpha) : ui::line);
            g.drawText(src == 2 && hasC && md.on ? "[" + juce::String(juce::roundToInt(md.after)) + "]" : juce::String("[--]"), juce::Rectangle<float>(sc, row.getY(), 56.f, row.getHeight()), juce::Justification::centredLeft);
            if (valid && proc.padLocked(k)) ui::lockIcon(g, juce::Rectangle<float>(11.f, 11.f).withCentre({ sc + 66.f, cy }), ui::orange, true);
            // ‹ n / N ›
            const int ns = valid ? proc.suggestionCount(k) : 0;
            if (ns > 0)
            {
                const int si = proc.suggestionIndex(k);
                const auto lb = juce::Rectangle<float>(sc + 90.f, cy - 9.f, 18.f, 18.f), rb = juce::Rectangle<float>(sc + 162.f, cy - 9.f, 18.f, 18.f);
                g.setFont(ui::mono(13.f));
                g.setColour(si > 0 ? (hot(lb) ? ui::orange : ui::white) : ui::line); g.drawText(juce::CharPointer_UTF8("\xe2\x80\xb9"), lb, juce::Justification::centred);
                g.setColour(si + 1 < ns ? (hot(rb) ? ui::orange : ui::white) : ui::line); g.drawText(juce::CharPointer_UTF8("\xe2\x80\xba"), rb, juce::Justification::centred);
                g.setColour(ui::dim2); g.setFont(ui::mono(10.f));
                g.drawText((si >= 0 ? juce::String(si + 1) : juce::String("-")) + "/" + juce::String(ns), juce::Rectangle<float>(lb.getRight(), cy - 7.f, rb.getX() - lb.getRight(), 14.f), juce::Justification::centred);
                zone(lb, [this, k] { proc.stepSuggestion(k, -1); });
                zone(rb, [this, k] { proc.stepSuggestion(k, 1); });
            }
            // the row: click = listen (and choose it); › = its page
            zone(row.withRight(sc - 6.f), [this, k, valid] {
                selectPad(k);
                auto r = proc.getResult();
                if (!valid || proc.padIsSilent(k)) { pendingType.clear(); setPage(pSamples); return; }
                proc.audition(k); });
            const auto go = juce::Rectangle<float>(row.getRight() - 52.f, cy - 11.f, 48.f, 22.f);
            g.setColour(hot(go) ? ui::white : ui::dim); g.setFont(ui::mono(10.f));
            g.drawText("open", go, juce::Justification::centred);
            zone(go, [this, k] { selectPad(k); setPage(pPad); });
        }
    }

    // ---------------------------------------------------------------- analysing (the wait, shown with the real stages)
    void drawAnalysing(juce::Graphics& g, double now)
    {
        const auto C = content;
        auto src = proc.getSource();
        const auto st = proc.getStatus();
        const bool loading = st.startsWith("LOADING") || st.startsWith("OPENING");
        const float p = loading ? 0.f : juce::jlimit(0.f, 1.f, proc.getProgress());
        g.setColour(ui::white); g.setFont(ui::inter(p < 0.55f ? 22.f : 22.f, 300));
        g.drawText(loading ? juce::String("opening") : p < 0.55f ? (src ? proc.getLoopFile().getFileName() : juce::String("reading")) : juce::String("learning the sounds"),
                   juce::Rectangle<float>(C.getX(), C.getY(), C.getWidth() - 220.f, 30.f), juce::Justification::centredLeft, true);
        g.setColour(ui::orange); g.setFont(ui::mono(11.f));
        g.drawText(ui::lower(st), juce::Rectangle<float>(C.getRight() - 320.f, C.getY() + 8.f, 320.f, 14.f), juce::Justification::centredRight, true);
        const auto wr = juce::Rectangle<float>(C.getX(), C.getY() + 60.f, C.getWidth(), 150.f);
        if (src && !loading)
        {
            const juce::int64 a = proc.selStart(), b = std::max(a + 1, proc.selEnd());
            const auto& pk = rangePeaks(*src, a, b, (int) (wr.getWidth() / 4.f));
            const float sweep = juce::jlimit(0.f, 1.f, p / 0.55f);
            for (size_t i = 0; i < pk.size(); ++i)
            {
                const float x = wr.getX() + i * 4.f, f = (float) i / (float) pk.size();
                const float h = std::max(2.f, pk[i] * wr.getHeight() * 0.48f);
                if (p < 0.55f) { g.setColour(f < sweep ? ui::white : ui::dark); g.fillRoundedRectangle(x - 1.5f, wr.getCentreY() - h, 3.f, 2.f * h, 1.5f); }
                else
                {   // the wave breaks into dots that drift to rows (the rows are known when it is done)
                    const float q = juce::jlimit(0.f, 1.f, (p - 0.55f) / 0.45f);
                    const float lane = (float) ((i * 7919) % 8);
                    const float ty = wr.getY() + 8.f + lane * (wr.getHeight() - 16.f) / 7.f;
                    const float y = wr.getCentreY() + (ty - wr.getCentreY()) * q * (0.6f + 0.4f * std::sin((float) now * 0.004f + (float) i));
                    if (pk[i] > 0.25f) { g.setColour(ui::white.withAlpha(0.4f + 0.6f * q)); g.fillEllipse(juce::Rectangle<float>(5.f, 5.f).withCentre({ x, y })); }
                }
            }
            if (p < 0.55f)
            {   // each hit found gets an orange tick
                int n = 0;
                for (auto& o : src->onsets)
                {
                    if (o.pos < a || o.pos >= b) continue;
                    const float f = (float) ((double) (o.pos - a) / (double) (b - a));
                    if (f > sweep) break;
                    ++n;
                    const float x = wr.getX() + f * wr.getWidth();
                    g.setColour(ui::orange); g.fillRoundedRectangle(x - 1.f, wr.getY() - 14.f, 2.f, 8.f, 1.f);
                }
                g.setColour(ui::orange); g.setFont(ui::mono(11.f));
                g.drawText("finding hits  .  " + juce::String(n), juce::Rectangle<float>(C.getX(), C.getY() + 30.f, 300.f, 14.f), juce::Justification::centredLeft);
            }
        }
        else
        {   // opening: a light that runs along
            const float ph = (float) std::fmod(now / 900.0, 1.0);
            for (int i = 0; i < 60; ++i) { const float x = wr.getX() + i * wr.getWidth() / 60.f; g.setColour(std::abs(i / 60.f - ph) < 0.06f ? ui::white : ui::line); g.fillEllipse(juce::Rectangle<float>(4.f, 4.f).withCentre({ x, wr.getCentreY() })); }
        }
        const auto bar = juce::Rectangle<float>(C.getX(), C.getBottom() - 34.f, C.getWidth(), 3.f);
        g.setColour(juce::Colour(0xff1c1c1f)); g.fillRoundedRectangle(bar, 1.5f);
        g.setColour(ui::orange); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * (loading ? (float) std::fmod(now / 1500.0, 1.0) : p)), 1.5f);
    }
    std::vector<float> rp; juce::int64 rpA = -1, rpB = -1; juce::uint64 rpId = 0; int rpN = 0;
    const std::vector<float>& rangePeaks(const ds::Source& s, juce::int64 a, juce::int64 b, int n)
    {
        if (s.id == rpId && a == rpA && b == rpB && n == rpN) return rp;
        rpId = s.id; rpA = a; rpB = b; rpN = n; rp.assign((size_t) std::max(0, n), 0.f);
        const int ch = s.audio.getNumChannels(); if (ch == 0 || n <= 0) return rp;
        float mx = 1e-6f;
        for (int i = 0; i < n; ++i)
        {
            const juce::int64 s0 = a + (b - a) * i / n, s1 = std::max(s0 + 1, a + (b - a) * (i + 1) / n);
            const juce::int64 step = std::max<juce::int64>(1, (s1 - s0) / 64);
            float m = 0; for (juce::int64 k = s0; k < std::min<juce::int64>(s1, s.length()); k += step) m = std::max(m, std::abs(s.audio.getSample(0, (int) k)));
            rp[(size_t) i] = m; mx = std::max(mx, m);
        }
        for (auto& v : rp) v /= mx;
        return rp;
    }

    // ---------------------------------------------------------------- no loop yet
    void drawDropHere(juce::Graphics& g)
    {
        const auto C = content;
        const auto st = proc.getStatus();
        g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
        g.drawText(st.startsWith("COULD NOT") ? ui::lower(st) : juce::String("drop a drum loop"), C.withTrimmedBottom(60.f), juce::Justification::centred, true);
        g.setColour(ui::dim); g.setFont(ui::inter(12.f));
        g.drawText("from the pool, mediabay or explorer  .  or click: your loops, listen first", C.withTrimmedTop(70.f).withTrimmedBottom(20.f), juce::Justification::centred);
        // a quiet row of dots
        for (int i = 0; i < 32; ++i) { const float x = C.getCentreX() - 155.f + i * 10.f; g.setColour(i % 4 == 0 ? ui::dark : ui::line); g.fillEllipse(juce::Rectangle<float>(4.f, 4.f).withCentre({ x, C.getCentreY() + 60.f })); }
        zone(C, [this] { setPage(pLoops); });
    }
    void chooseLoop()
    {
        chooser = std::make_unique<juce::FileChooser>("Select drum loop", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) { proc.loadSource(f); setPage(pSource); } });
    }

    // ---------------------------------------------------------------- page: SOURCE (the loaded file, the part to learn)
    float wx0() const { return content.getX(); }
    float ww() const { return content.getWidth(); }
    juce::Rectangle<float> waveZone() const { return { content.getX() - 6.f, content.getY() + 40.f, content.getWidth() + 12.f, content.getHeight() - 86.f }; }
    float waveMid() const { return content.getY() + 171.f; }
    const float waveH = 96.f;
    void fixView(const ds::Source& s) { if (s.id != viewId || v1 <= v0 || v1 > s.length()) { v0 = 0; v1 = s.length(); viewId = s.id; } }
    float xOf(juce::int64 smp) const { return wx0() + (float) ((double) (smp - v0) / (double) std::max<juce::int64>(1, v1 - v0)) * ww(); }
    juce::int64 sampleAt(float x) const { return v0 + (juce::int64) std::llround((double) (x - wx0()) / ww() * (double) (v1 - v0)); }
    int handleAt(float x, float y) const
    {
        auto src = proc.getSource();
        if (!src || !waveZone().contains(x, y)) return 0;
        const float xs = xOf(proc.selStart()), xe = xOf(proc.selEnd());
        const float a = std::abs(x - xs), b = std::abs(x - xe);
        if (std::min(a, b) > 8.f) return 0;
        return a <= b ? 1 : 2;
    }
    juce::int64 snapped(const ds::Source& s, juce::int64 pos) const
    {
        const double spp = (double) (v1 - v0) / ww();
        const juce::int64 nr = (juce::int64) (10 * spp), fr = (juce::int64) (40 * spp);
        const auto& on = s.onsets;
        auto it = std::lower_bound(on.begin(), on.end(), pos - fr, [](const ds::Onset& o, juce::int64 p) { return o.pos < p; });
        float top = 0; std::vector<ds::Onset> cand;
        juce::int64 nearest = -1, nd = fr + 1;
        for (; it != on.end() && it->pos <= pos + fr; ++it)
        {
            const juce::int64 d = std::abs(it->pos - pos);
            if (d < nd) { nd = d; nearest = it->pos; }
            if (d <= nr) { cand.push_back(*it); top = std::max(top, it->strength); }
        }
        for (juce::int64 edge : { (juce::int64) 0, s.length() })
            if (std::abs(edge - pos) <= nr) { cand.push_back({ edge, 1.f }); top = 1.f; }
        if (!cand.empty())
        {
            juce::int64 best = cand.front().pos, bd = std::numeric_limits<juce::int64>::max();
            for (auto& c : cand) if (c.strength >= 0.7f * top && std::abs(c.pos - pos) < bd) { bd = std::abs(c.pos - pos); best = c.pos; }
            return best;
        }
        return nearest >= 0 ? nearest : pos;
    }
    void sourceDown(const juce::MouseEvent& e)
    {
        auto src = proc.getSource();
        if (!src || proc.isBusy() || e.mods.isPopupMenu()) return;
        fixView(*src);
        if (!waveZone().contains(e.position)) return;
        dragWhat = handleAt(e.position.x, e.position.y);
        if (dragWhat == 0)
        {
            const auto p = juce::jlimit(proc.selStart(), std::max(proc.selStart(), proc.selEnd() - 1), sampleAt(e.position.x));
            proc.seekSource(p); proc.playSource(true); repaint();
        }
    }
    void sourceDrag(const juce::MouseEvent& e)
    {
        if (dragWhat == 0) return;
        auto src = proc.getSource(); if (!src) return;
        auto p = juce::jlimit<juce::int64>(0, src->length(), sampleAt(e.position.x));
        if (proc.snap.load() != e.mods.isAltDown()) p = snapped(*src, p);
        if (dragWhat == 1) proc.setSelection(std::min(p, proc.selEnd() - 1), proc.selEnd());
        else proc.setSelection(proc.selStart(), std::max(p, proc.selStart() + 1));
        repaint();
    }
    static juce::String clock(double sec) { const int m = (int) (sec / 60.0); return juce::String(m) + ":" + juce::String(sec - 60.0 * m, 1).paddedLeft('0', 4); }
    void computePeaks(const ds::Source& s)
    {
        if (s.id == peaksId && v0 == pk0 && v1 == pk1) return;
        peaksId = s.id; pk0 = v0; pk1 = v1;
        peaks.clear();
        const int w = (int) (ww() / 4.f), n = (int) s.length();
        if (w <= 0 || n <= 0) return;
        if (s.id != gmaxId) { gmax = 1e-6f; for (int c = 0; c < s.audio.getNumChannels(); ++c) gmax = std::max(gmax, s.audio.getMagnitude(c, 0, n)); gmaxId = s.id; }
        const float* L = s.audio.getReadPointer(0); const float* R = s.audio.getReadPointer(s.audio.getNumChannels() > 1 ? 1 : 0);
        for (int x = 0; x < w; ++x)
        {
            const juce::int64 a = juce::jlimit<juce::int64>(0, n - 1, v0 + (v1 - v0) * x / w), b = juce::jlimit<juce::int64>(a + 1, n, v0 + (v1 - v0) * (x + 1) / w);
            const juce::int64 step = std::max<juce::int64>(1, (b - a) / 64);
            float m = 0.f;
            for (juce::int64 i = a; i < b; i += step) m = std::max(m, std::abs(0.5f * (L[i] + R[i]) / gmax));
            peaks.push_back(m);
        }
    }
    void drawSource(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        auto src = proc.getSource();
        if (!src) { drawDropHere(g); return; }
        const auto C = content;
        fixView(*src); computePeaks(*src);
        const juce::int64 sa = proc.selStart(), sb = proc.selEnd(), n = src->length();
        const bool mine = res && res->sourceId == src->id;
        // header: the file, its length, the tempo
        g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
        const auto fname = proc.getLoopFile().getFileName();
        const float fw = std::min(430.f, ui::textW(ui::inter(30.f, 300), fname));
        g.drawText(fname, juce::Rectangle<float>(C.getX(), C.getY(), fw, 38.f), juce::Justification::centredLeft, true);
        g.setColour(ui::dim2); g.setFont(ui::mono(12.f));
        g.drawText(clock((double) n / src->sr) + "  .  " + juce::String(src->sr / 1000.0, 1).trimCharactersAtEnd("0").trimCharactersAtEnd(".") + " khz",
                   juce::Rectangle<float>(C.getX() + fw + 14.f, C.getY() + 12.f, 200.f, 18.f), juce::Justification::centredLeft);
        if (mine)
        {
            g.setColour(ui::white); g.setFont(ui::mono(20.f, true));
            g.drawText(juce::String(res->bpm, 1), juce::Rectangle<float>(C.getRight() - 90.f, C.getY() + 6.f, 90.f, 26.f), juce::Justification::centredRight);
            g.setColour(ui::dim2); g.setFont(ui::mono(12.f));
            g.drawText("tempo", juce::Rectangle<float>(C.getRight() - 150.f, C.getY() + 10.f, 54.f, 20.f), juce::Justification::centredRight);
        }
        // the wave: white inside the part, dark outside
        const float mid = waveMid(), hh = waveH;
        const float xs = juce::jlimit(wx0(), wx0() + ww(), xOf(sa)), xe = juce::jlimit(wx0(), wx0() + ww(), xOf(sb));
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float x = wx0() + i * 4.f + 2.f;
            const float h = std::max(1.5f, peaks[i] * hh);
            g.setColour(x >= xs && x <= xe ? ui::white : ui::dark);
            g.fillRoundedRectangle(x - 1.5f, mid - h, 3.f, 2.f * h, 1.5f);
        }
        // the part: orange edges
        for (int k = 0; k < 2; ++k)
        {
            const juce::int64 at = k == 0 ? sa : sb;
            if (at < v0 || at > v1) continue;
            const float x = xOf(at);
            g.setColour(ui::orange); g.fillRect(x - 1.f, mid - hh - 8.f, 2.f, 2.f * hh + 16.f);
            g.fillEllipse(juce::Rectangle<float>(9.f, 9.f).withCentre({ x, mid - hh - 8.f }));
            g.setFont(ui::mono(10.f));
            g.drawText(clock((double) at / src->sr), juce::Rectangle<float>(k == 0 ? x + 4.f : x - 84.f, mid + hh + 10.f, 80.f, 12.f), k == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }
        if (xe > xs) { g.setColour(ui::orange.withAlpha(0.035f)); g.fillRect(juce::Rectangle<float>(xs, mid - hh - 8.f, xe - xs, 2.f * hh + 16.f)); }
        // the hits that were found (the selected pad's in orange)
        if (mine)
            for (auto& h : res->hits)
            {
                const juce::int64 t = (juce::int64) h.time + res->regionStart;
                if (t < v0 || t > v1) continue;
                const float x = xOf(t);
                g.setColour(h.pad == selected ? ui::orange : ui::faint);
                g.fillRoundedRectangle(x - 1.f, mid + hh + 26.f, 2.f, h.pad == selected ? 9.f : 5.f, 1.f);
            }
        // the cursor
        {
            const double p = proc.sourcePos();
            if (p >= (double) v0 && p <= (double) v1 && proc.isSourcePlaying()) { g.setColour(ui::white); g.fillRect(xOf((juce::int64) p) - 0.5f, mid - hh - 4.f, 1.f, 2.f * hh + 8.f); }
        }
        drawScroll(g, srcScrollRect(), (double) v0, (double) v1, (double) n);
        // bottom row: listen, snap, the part's length, analyse
        const float y = C.getBottom() - 46.f;
        const auto pr = juce::Rectangle<float>(C.getX(), y, 28.f, 28.f);
        g.setColour(hot(pr) ? ui::chipOn : ui::chip2); g.fillEllipse(pr);
        g.setColour(proc.isSourcePlaying() ? ui::orange : ui::white);
        if (proc.isSourcePlaying()) g.fillRect(pr.withSizeKeepingCentre(8.f, 8.f));
        else { juce::Path t; const auto c = pr.getCentre(); t.addTriangle(c.x - 3.f, c.y - 5.f, c.x - 3.f, c.y + 5.f, c.x + 5.f, c.y); g.fillPath(t); }
        zone(pr, [this] { proc.playSource(!proc.isSourcePlaying()); });
        {
            const bool on = proc.snap.load();
            const auto sr = juce::Rectangle<float>(pr.getRight() + 14.f, y + 2.f, 110.f, 24.f);
            g.setColour(on ? ui::white : ui::dim); g.drawEllipse(juce::Rectangle<float>(10.f, 10.f).withCentre({ sr.getX() + 8.f, sr.getCentreY() }), 1.2f);
            if (on) g.fillEllipse(juce::Rectangle<float>(5.f, 5.f).withCentre({ sr.getX() + 8.f, sr.getCentreY() }));
            g.setFont(ui::inter(11.f)); g.setColour(ui::dim);
            g.drawText("snap to hits", sr.withTrimmedLeft(20.f), juce::Justification::centredLeft);
            zone(sr, [this] { proc.snap = !proc.snap.load(); });
        }
        {
            const double secs = (double) (sb - sa) / src->sr;
            juce::String t = juce::String(secs, 2) + " s";
            if (mine && res->bpm > 0) t << "  .  " << juce::String(secs * res->bpm / 240.0, 1) << " bars";
            if (v0 > 0 || v1 < n) t << "  .  zoom (wheel)";
            g.setColour(ui::dim2); g.setFont(ui::mono(11.f));
            g.drawText(t, juce::Rectangle<float>(pr.getRight() + 140.f, y + 7.f, 260.f, 14.f), juce::Justification::centredLeft);
        }
        const bool done = proc.selectionAnalysed();
        const auto ab = chip(g, C.getRight(), y, done ? "re-analyse" : "analyse this part", done ? cNormal : cAccent, [this] { proc.reanalyse(); }, 30.f, 12.f, true);
        chip(g, ab.getX() - 10.f, y, "load another loop", cNormal, [this] { setPage(pLoops); }, 30.f, 12.f, true);
        if (mine && !done) { g.setColour(ui::orange); g.setFont(ui::mono(10.f)); g.drawText("the part changed: analyse it", juce::Rectangle<float>(ab.getX() - 400.f, y - 18.f, 400.f + ab.getWidth(), 12.f), juce::Justification::centredRight); }
    }

    // ---------------------------------------------------------------- page: PATTERN = the rhythm: every bar, zoom, edit the hits
    juce::Rectangle<float> patLanes() const { return { content.getX() + 26.f, content.getY() + 66.f, content.getWidth() - 26.f, content.getHeight() - 112.f }; }
    static float laneY(juce::Rectangle<float> L, int k) { return L.getY() + 8.f + k * (L.getHeight() - 16.f) / 7.f; }
    double pv0 = 0, pv1 = 0; juce::uint64 pvId = 0; bool patSnap = true;
    std::vector<ds::Hit> patEdit; int patIdx = -1, patMode = 0; bool patEditing = false, patChanged = false;
    juce::Point<float> patDown; double patT0 = 0; int patV0 = 100, patSel = -1;
    double step16(const ds::Result& r) const { return 60.0 / std::max(1.0, r.bpm) * r.sr / 4.0; }
    void fixPatView(const ds::Result& r)
    {
        const double len = std::max(1, r.loop.getNumSamples());
        if (pvId != r.sourceId || pv1 <= pv0 || pv1 > len + 1) { pv0 = 0; pv1 = len; pvId = r.sourceId; }
    }
    float pxOf(double t) const { const auto L = patLanes(); return L.getX() + (float) ((t - pv0) / std::max(1.0, pv1 - pv0)) * L.getWidth(); }
    double tOf(float x) const { const auto L = patLanes(); return pv0 + (double) (x - L.getX()) / L.getWidth() * (pv1 - pv0); }
    const std::vector<ds::Hit>& patHits(const ds::Result& r) const { return patEditing ? patEdit : r.hits; }
    double shownT(const ds::Result& r, const ds::Hit& h) const
    {
        const double s = step16(r), gridT = std::round(h.time / s) * s;
        return h.time + (gridT - h.time) * tightShown;
    }
    int patHitAt(const ds::Result& r, juce::Point<float> p) const
    {
        const auto L = patLanes(); const auto& hs = patHits(r);
        int best = -1; float bd = 10.f;
        for (size_t i = 0; i < hs.size(); ++i)
        {
            const auto& h = hs[i];
            if (h.pad < 0 || h.pad >= ds::kPads) continue;
            const float d = juce::Point<float>(pxOf(shownT(r, h)), laneY(L, h.pad)).getDistanceFrom(p);
            if (d < bd) { bd = d; best = (int) i; }
        }
        return best;
    }
    int laneAt(juce::Point<float> p) const
    {
        const auto L = patLanes();
        for (int k = 0; k < ds::kPads; ++k) if (std::abs(p.y - laneY(L, k)) < (L.getHeight() - 16.f) / 14.f) return k;
        return -1;
    }
    double snapT(const ds::Result& r, double t, bool alt) const { const double s = step16(r); return (patSnap != alt) ? std::round(t / s) * s : t; }
    double patCursor = 0; bool dragCursor = false;
    juce::Rectangle<float> rulerRect() const { const auto L = patLanes(); return { L.getX() - 4.f, L.getY() - 26.f, L.getWidth() + 8.f, 18.f }; }
    void setCursor(float x)
    {
        auto r = proc.getResult(); if (!r) return;
        const double len = std::max(1, r->loop.getNumSamples());
        patCursor = juce::jlimit(0.0, len - 1.0, tOf(x));
        proc.midiStartFrac = (float) (patCursor / len);
        if (proc.isMidiPlaying()) { proc.playMidi(false); proc.playMidi(true); }
        repaint();
    }
    void zoomAtCursor(double f)
    {
        auto r = proc.getResult(); if (!r) return;
        fixPatView(*r);
        const double len = std::max(1, r->loop.getNumSamples());
        const double ns = juce::jlimit(std::min(len, 2.0 * step16(*r)), len, (pv1 - pv0) * f);
        const double a = juce::jlimit(0.0, len - ns, patCursor - ns * 0.5); pv0 = a; pv1 = a + ns;
        repaint();
    }
    void patternDown(const juce::MouseEvent& e)
    {
        auto r = proc.getResult(); if (!r) return;
        grabKeyboardFocus();
        const int h = patHitAt(*r, e.position);
        patEdit = r->hits; patEditing = true; patChanged = false; patDown = e.position; patMode = 0;
        if (h >= 0 && e.mods.isPopupMenu())
        {   // right-click = remove it
            patEdit.erase(patEdit.begin() + h); patChanged = true; patIdx = -1; patSel = -1; return;
        }
        if (h >= 0) { patIdx = h; patSel = h; patT0 = patEdit[(size_t) h].time; patV0 = patEdit[(size_t) h].velocity; proc.audition(patEdit[(size_t) h].pad); return; }
        const int lane = laneAt(e.position);
        if (lane >= 0 && r->pads[(size_t) lane].valid && !e.mods.isPopupMenu())
        {   // an empty place on a row: a new hit (on the grid unless snap is off / ALT)
            const double len = std::max(1, r->loop.getNumSamples());
            ds::Hit nh; nh.pad = lane; nh.velocity = 100; nh.time = juce::jlimit(0.0, len - 1.0, snapT(*r, tOf(e.position.x), e.mods.isAltDown()));
            patEdit.push_back(nh); patIdx = (int) patEdit.size() - 1; patSel = patIdx; patT0 = nh.time; patV0 = 100; patChanged = true;
            proc.audition(lane);
            return;
        }
        patEditing = false; patIdx = -1;
    }
    void patternDrag(const juce::MouseEvent& e)
    {
        auto r = proc.getResult(); if (!r || !patEditing || patIdx < 0 || patIdx >= (int) patEdit.size()) return;
        const auto d = e.position - patDown;
        if (patMode == 0 && std::max(std::abs(d.x), std::abs(d.y)) > 4.f) patMode = std::abs(d.x) >= std::abs(d.y) ? 1 : 2;
        auto& h = patEdit[(size_t) patIdx];
        const double len = std::max(1, r->loop.getNumSamples());
        if (patMode == 1) { h.time = juce::jlimit(0.0, len - 1.0, snapT(*r, patT0 + (tOf(e.position.x) - tOf(patDown.x)), e.mods.isAltDown())); patChanged = true; }
        if (patMode == 2) { h.velocity = juce::jlimit(1, 127, patV0 - juce::roundToInt(d.y * 0.8f)); patChanged = true; }
        repaint();
    }
    void patternUp()
    {
        if (patEditing && patChanged) { proc.editHits(patEdit); patSel = -1; }
        patEditing = false; patIdx = -1;
    }
    void deleteSelectedHit()
    {
        auto r = proc.getResult(); if (!r || patSel < 0 || patSel >= (int) r->hits.size()) return;
        auto h = r->hits; h.erase(h.begin() + patSel); patSel = -1; proc.editHits(h);
    }
    void patternWheel(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        auto r = proc.getResult(); if (!r) return;
        fixPatView(*r);
        const double len = std::max(1, r->loop.getNumSamples()), span = pv1 - pv0;
        if (e.mods.isShiftDown() || std::abs(w.deltaX) > std::abs(w.deltaY))
        {
            const double dd = -(std::abs(w.deltaX) > std::abs(w.deltaY) ? w.deltaX : w.deltaY) * span * 0.25;
            const double a = juce::jlimit(0.0, len - span, pv0 + dd); pv0 = a; pv1 = a + span;
        }
        else
        {
            const double at = tOf(e.position.x);
            const double ns = juce::jlimit(std::min(len, 2.0 * step16(*r)), len, span * std::pow(0.8, w.deltaY * 4.0));
            const double f = (at - pv0) / span;
            const double a = juce::jlimit(0.0, len - ns, at - f * ns); pv0 = a; pv1 = a + ns;
        }
        repaint();
    }
    juce::Rectangle<float> tightRect() const { return { content.getRight() - 160.f, content.getBottom() - 30.f, 120.f, 16.f }; }
    void setTight(float x)
    {
        const auto r = tightRect();
        if (auto* p = proc.apvts.getParameter("quant")) p->setValueNotifyingHost(juce::jlimit(0.f, 1.f, (x - r.getX()) / r.getWidth()));
    }
    void drawPattern(juce::Graphics& g, const std::shared_ptr<ds::Result>& res, double now)
    {
        juce::ignoreUnused(now);
        if (!res) { drawDropHere(g); return; }
        const auto C = content;
        const auto& r = *res;
        fixPatView(r);
        g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
        g.drawText("Pattern", juce::Rectangle<float>(C.getX(), C.getY(), 130.f, 38.f), juce::Justification::centredLeft);
        const int nb = bars(r);
        const double len = std::max(1, r.loop.getNumSamples());
        g.setColour(ui::dim2); g.setFont(ui::mono(12.f));
        g.drawText(juce::String(nb) + (nb == 1 ? " bar" : " bars"),
                   juce::Rectangle<float>(C.getX() + 140.f, C.getY() + 12.f, 210.f, 16.f), juce::Justification::centredLeft);
        const int mm = proc.midiMode();
        segmented(g, juce::Rectangle<float>(C.getRight() - 430.f, C.getY() + 4.f, 430.f, 32.f),
                  { "exact . as played", "sync . follows the song tempo", "tight . on the grid" }, mm == ds::kExact ? 0 : mm == ds::kSync ? 1 : 2,
                  [this](int i) { proc.setMidiMode(i == 0 ? ds::kExact : i == 1 ? ds::kSync : ds::kTight); });
        const float q = proc.apvts.getRawParameterValue("quant")->load();
        const float target = mm == ds::kTight ? q : 0.f;
        tightShown += (target - tightShown) * 0.25f;
        if (std::abs(target - tightShown) < 0.002f) tightShown = target;
        const auto L = patLanes();
        {   // grid: bars (numbered), beats, sixteenths - as many as there is room for
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(L.expanded(0.f, 22.f).toNearestInt());
            const double s16 = step16(r), pxPer16 = L.getWidth() * s16 / (pv1 - pv0);
            const int i0 = (int) std::floor(pv0 / s16), i1 = (int) std::ceil(pv1 / s16);
            for (int i = std::max(0, i0); i <= i1; ++i)
            {
                const bool bar = i % 16 == 0, beat = i % 4 == 0;
                if (!bar && !(beat && pxPer16 * 4 > 14) && !(pxPer16 > 9)) continue;
                const float x = pxOf(i * s16);
                g.setColour(bar ? juce::Colour(0xff2a2a2d) : beat ? juce::Colour(0xff1a1a1c) : juce::Colour(0xff111113));
                g.drawVerticalLine((int) x, L.getY() - 4.f, L.getBottom() + 4.f);
                if (bar && pxPer16 * 16 > 26) { g.setColour(ui::faint); g.setFont(ui::mono(10.f)); g.drawText(juce::String(i / 16 + 1), juce::Rectangle<float>(x + 3.f, L.getY() - 20.f, 30.f, 12.f), juce::Justification::centredLeft); }
            }
        }
        for (int k = 0; k < ds::kPads; ++k)
        {
            const auto& pd = r.pads[(size_t) k];
            const auto gr = juce::Rectangle<float>(12.f, 12.f).withCentre({ L.getX() - 20.f, laneY(L, k) });
            ui::glyph(g, pd.valid ? pd.name : "-", gr, pd.valid ? (k == selected ? ui::white : ui::soft) : ui::line);
            if (pd.valid) zone(gr.expanded(6.f), [this, k] { selectPad(k); proc.audition(k); });
        }
        {   // the cursor's handle on the strip, play / stop, zoom - +
            const float cx = proc.isMidiPlaying() ? pxOf(proc.midiPlayFrac() * len) : pxOf(patCursor);
            juce::Path tri; tri.addTriangle(cx - 6.f, L.getY() - 24.f, cx + 6.f, L.getY() - 24.f, cx, L.getY() - 14.f);
            g.setColour(ui::orange); g.fillPath(tri);
            const float bx = C.getX() + 360.f, by = C.getY() + 6.f;
            const auto pb = juce::Rectangle<float>(bx, by, 28.f, 28.f);
            g.setColour(hot(pb) ? ui::chipOn : ui::chip2); g.fillEllipse(pb);
            g.setColour(proc.isMidiPlaying() ? ui::orange : ui::white);
            if (proc.isMidiPlaying()) g.fillRect(pb.withSizeKeepingCentre(8.f, 8.f));
            else { juce::Path t; const auto c = pb.getCentre(); t.addTriangle(c.x - 3.f, c.y - 5.f, c.x - 3.f, c.y + 5.f, c.x + 5.f, c.y); g.fillPath(t); }
            zone(pb, [this] { proc.playMidi(!proc.isMidiPlaying()); });
            const auto zm = chip(g, pb.getRight() + 10.f, by + 2.f, juce::CharPointer_UTF8("\xe2\x88\x92"), cNormal, [this] { zoomAtCursor(1.6); });
            chip(g, zm.getRight() + 6.f, by + 2.f, "+", cNormal, [this] { zoomAtCursor(0.6); });
        }
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(L.expanded(8.f, 10.f).toNearestInt());
            const auto& hs = patHits(r);
            const float playX = proc.isMidiPlaying() ? pxOf(proc.midiPlayFrac() * len) : -100.f;
            for (size_t i = 0; i < hs.size(); ++i)
            {
                const auto& h = hs[i];
                if (h.pad < 0 || h.pad >= ds::kPads || !r.pads[(size_t) h.pad].valid) continue;
                const double s = step16(r), gridT = std::round(h.time / s) * s, t = shownT(r, h);
                if (t < pv0 - s || t > pv1 + s) continue;
                const float v = juce::jlimit(0.f, 1.f, h.velocity / 127.f);
                const float x = pxOf(t), y = laneY(L, h.pad), w = 5.f + 8.f * v;
                const bool edited = patEditing && (int) i == patIdx, selHit = (int) i == patSel;
                if (std::abs(pxOf(gridT) - x) > 2.5f) { g.setColour(ui::dark); g.drawEllipse(juce::Rectangle<float>(w + 3.f, w + 3.f).withCentre({ pxOf(gridT), y }), 1.f); }
                juce::Colour c = ui::white.withAlpha(0.5f + 0.5f * v);
                if (!proc.padAudible(h.pad)) c = c.withMultipliedAlpha(0.3f);
                if (playX > 0 && x <= playX && playX - x < 12.f) { g.setColour(ui::orange.withAlpha(0.35f)); g.fillEllipse(juce::Rectangle<float>(w + 8.f, w + 8.f).withCentre({ x, y })); c = ui::orange; }
                g.setColour(c); g.fillEllipse(juce::Rectangle<float>(w, w).withCentre({ x, y }));
                if (edited || selHit) { g.setColour(ui::orange); g.drawEllipse(juce::Rectangle<float>(w + 6.f, w + 6.f).withCentre({ x, y }), 1.5f); }
                if (edited)
                {
                    const double secs = h.time / r.sr, beats = h.time / (s * 4.0);
                    const int bar = (int) (beats / 4.0) + 1, beat = (int) std::fmod(beats, 4.0) + 1, six = (int) std::fmod(beats * 4.0, 4.0) + 1;
                    g.setFont(ui::mono(10.f)); g.setColour(ui::orange);
                    g.drawText(ui::lower(r.pads[(size_t) h.pad].name) + "  " + juce::String(bar) + "." + juce::String(beat) + "." + juce::String(six) + "  v" + juce::String(h.velocity) + "  " + juce::String(secs, 3) + " s",
                               juce::Rectangle<float>(x - 120.f, y - w - 22.f, 240.f, 12.f), juce::Justification::centred);
                }
            }
            if (playX > 0) { g.setColour(ui::orange); g.fillRect(playX - 0.5f, L.getY() - 6.f, 1.f, L.getHeight() + 12.f); }
            else if (false) {}
            else
            {   // the cursor: play starts here (drag it on the strip above the rows)
                const float cx = pxOf(patCursor);
                g.setColour(ui::orange.withAlpha(0.6f)); g.fillRect(cx - 0.5f, L.getY() - 6.f, 1.f, L.getHeight() + 12.f);
            }
        }
        drawScroll(g, patScrollRect(), pv0, pv1, len);
        // bottom: how to edit, snap, how tight
        const float y = C.getBottom() - 30.f;
        g.setColour(ui::faint); g.setFont(ui::mono(10.f));
        g.drawText("click = add  .  drag = move  .  up / down = how hard  .  right-click = remove", juce::Rectangle<float>(C.getX(), y, 520.f, 16.f), juce::Justification::centredLeft);
        {
            const auto sr = juce::Rectangle<float>(C.getX() + 528.f, y - 4.f, 96.f, 24.f);
            g.setColour(patSnap ? ui::white : ui::dim); g.drawEllipse(juce::Rectangle<float>(10.f, 10.f).withCentre({ sr.getX() + 8.f, sr.getCentreY() }), 1.2f);
            if (patSnap) g.fillEllipse(juce::Rectangle<float>(5.f, 5.f).withCentre({ sr.getX() + 8.f, sr.getCentreY() }));
            g.setFont(ui::inter(11.f)); g.setColour(ui::dim); g.drawText("snap to 1/16", sr.withTrimmedLeft(20.f), juce::Justification::centredLeft);
            zone(sr, [this] { patSnap = !patSnap; });
        }
        const auto tr = tightRect();
        g.setFont(ui::inter(11.f));
        g.setColour(mm == ds::kTight ? ui::dim : ui::line); g.drawText("how tight", juce::Rectangle<float>(tr.getX() - 70.f, y, 64.f, 16.f), juce::Justification::centredRight);
        g.setColour(juce::Colour(0xff232325)); g.fillRoundedRectangle(tr.withSizeKeepingCentre(tr.getWidth(), 4.f), 2.f);
        g.setColour(mm == ds::kTight ? ui::white : ui::dark); g.fillRoundedRectangle(tr.withSizeKeepingCentre(tr.getWidth(), 4.f).withWidth(tr.getWidth() * q), 2.f);
        g.fillEllipse(juce::Rectangle<float>(12.f, 12.f).withCentre({ tr.getX() + tr.getWidth() * q, tr.getCentreY() }));
        g.setFont(ui::mono(11.f)); g.setColour(ui::white);
        g.drawText(juce::String(juce::roundToInt(q * 100)), juce::Rectangle<float>(tr.getRight() + 8.f, y, 32.f, 16.f), juce::Justification::centredLeft);
    }

    // ---------------------------------------------------------------- page: PAD (where its sound comes from, the match)
    void drawPad(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        if (!res) { drawDropHere(g); return; }
        const auto C = content;
        const auto& pd = res->pads[(size_t) selected];
        if (!pd.valid) { drawEmptyPad(g); return; }
        const int src = proc.padSource(selected);
        const bool hasC = pd.user.getNumSamples() > 0;
        const auto md = proc.getMatch(selected);
        const bool matched = hasC && md.on;
        // header
        ui::glyph(g, pd.name, juce::Rectangle<float>(16.f, 16.f).withCentre({ C.getX() + 8.f, C.getY() + 20.f }), ui::white);
        g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
        const auto nm = juce::String(pd.name.substring(0, 1)) + pd.name.substring(1).toLowerCase();
        const float nw = ui::textW(ui::inter(30.f, 300), nm);
        g.drawText(nm, juce::Rectangle<float>(C.getX() + 28.f, C.getY(), nw + 4.f, 38.f), juce::Justification::centredLeft);
        g.setColour(ui::dim2); g.setFont(ui::mono(12.f));
        g.drawText(juce::MidiMessage::getMidiNoteName(pd.note, true, true, 3).toLowerCase() + "  .  " + juce::String(pd.hits) + " hits",
                   juce::Rectangle<float>(C.getX() + 40.f + nw, C.getY() + 12.f, 160.f, 18.f), juce::Justification::centredLeft);
        // right of the header: ···, lock, the match number
        float hx = C.getRight();
        const auto more = chip(g, hx, C.getY() + 6.f, juce::CharPointer_UTF8("\xc2\xb7\xc2\xb7\xc2\xb7"), cNormal, [this] { setPage(pMore); }, 26.f, 13.f, true);
        hx = more.getX() - 10.f;
        {
            const bool lk = proc.padLocked(selected);
            const auto lr = juce::Rectangle<float>(hx - 26.f, C.getY() + 6.f, 26.f, 26.f);
            g.setColour(hot(lr) ? ui::chipOn : ui::chip2); g.fillEllipse(lr);
            ui::lockIcon(g, lr.reduced(6.f), lk ? ui::orange : ui::soft, lk);
            zone(lr, [this] { proc.setPadLocked(selected, !proc.padLocked(selected)); toast(proc.padLocked(selected) ? "locked: build my kit keeps this pad" : "unlocked: build my kit may change this pad"); });
            hx = lr.getX() - 14.f;
        }
        if (matched)
        {
            const bool asIs = md.bestAmount < 0.01f;      // nearer as it is: it plays unmatched
            const juce::String big = asIs ? "[" + juce::String(juce::roundToInt(md.after)) + "]"
                                          : juce::String(juce::roundToInt(md.before)) + juce::String(juce::CharPointer_UTF8(" \xe2\x86\x92 [")) + juce::String(juce::roundToInt(md.after)) + "]";
            const float bw = ui::textW(ui::mono(22.f, true), big);
            g.setColour(ui::white); g.setFont(ui::mono(22.f, true));
            g.drawText(big, juce::Rectangle<float>(hx - bw, C.getY() + 4.f, bw, 30.f), juce::Justification::centredRight);
            g.setColour(ui::dim2); g.setFont(ui::mono(12.f));
            g.drawText(asIs ? "like the loop  .  as it is" : "like the loop", juce::Rectangle<float>(hx - bw - 210.f, C.getY() + 10.f, 202.f, 20.f), juce::Justification::centredRight);
        }
        // the three sources: the loop's (learned), the best real hit, yours
        const float cw = (C.getWidth() - 20.f) / 3.f, cy = C.getY() + 48.f, ch = 62.f;
        for (int k = 0; k < 3; ++k)
        {
            const auto r = juce::Rectangle<float>(C.getX() + k * (cw + 10.f), cy, cw, ch);
            const bool on = src == k && (k < 2 || hasC);
            g.setColour(on ? ui::cardOn : ui::card); g.fillRoundedRectangle(r, 10.f);
            if (on) { g.setColour(ui::white); g.drawRoundedRectangle(r.reduced(0.5f), 10.f, 1.f); }
            else if (hot(r)) { g.setColour(ui::dark); g.drawRoundedRectangle(r.reduced(0.5f), 10.f, 1.f); }
            juce::String kind, title, act;
            if (k == 0) { kind = "the loop's  .  from " + juce::String(pd.hits) + " hits"; title = "learned sound"; }
            if (k == 1) { kind = "best real hit"; title = "cleanest hit, others removed"; }
            if (k == 2) { kind = hasC ? (on ? "yours  .  playing" : "yours") : "yours"; title = hasC ? pd.userName + (pd.layered() ? " + " + pd.layerName : juce::String()) : juce::String("choose from your samples"); }
            if (on && k < 2) kind << "  .  playing";
            const int kk = k;
            zone(r, [this, kk, hasC] { if (kk == 2 && !hasC) { setPage(pSamples); return; } setSource(selected, kk); });
            g.setColour(ui::dim); g.setFont(ui::inter(11.f));
            g.drawText(kind, juce::Rectangle<float>(r.getX() + 12.f, r.getY() + 8.f, r.getWidth() - 24.f, 14.f), juce::Justification::centredLeft, true);
            g.setColour(ui::white); g.setFont(ui::inter(13.f, 500));
            const auto tr = juce::Rectangle<float>(r.getX() + 12.f, r.getY() + 23.f, r.getWidth() - 24.f, 18.f);
            g.drawText(title, tr, juce::Justification::centredLeft, true);
            if (k == 2 && hasC) zone(tr.withWidth(std::min(tr.getWidth(), ui::textW(ui::inter(13.f, 500), title) + 4.f)), [this] { setPage(pSamples); });
            // small actions under the title
            float ax = r.getX() + 12.f; const float ay = r.getY() + 42.f;
            auto act2 = [&](const juce::String& t, std::function<void()> click, std::function<void()> drag = {})
            {
                const auto f = ui::mono(10.f);
                const auto ar = juce::Rectangle<float>(ax, ay, ui::textW(f, t) + 2.f, 14.f);
                g.setColour(hot(ar) ? ui::white : ui::dim); g.setFont(f); g.drawText(t, ar, juce::Justification::centredLeft);
                zone(ar.expanded(2.f, 3.f), std::move(click), std::move(drag));
                ax = ar.getRight() + 12.f;
            };
            if (k == 0) act2("show its hits on the loop", [this] { setPage(pSource); });
            if (k == 1) act2(juce::CharPointer_UTF8("\xe2\x96\xb6 hear it"), [this] { setSource(selected, 1); });
            if (k == 2 && hasC)
            {
                const auto path = pd.userPath;
                act2("open folder", [path] { juce::File f(path); if (f.existsAsFile()) f.revealToUser(); });
                act2("drag original", [] {}, [this, path] { juce::File f(path); if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this); });
                act2("change", [this] { setPage(pSamples); });
            }
            if (k == 2 && !hasC) act2("or drop a file on the pad", [this] { setPage(pSamples); });
        }
        // right of the sound editor: raw / matched, the shape, the layer
        const float rx = C.getRight() - 262.f, ry = C.getY() + 128.f;
        // listen and compare: the loop's own sound (only heard, the pad keeps what it plays) | yours raw | matched
        float ly = ry + 40.f;
        if (matched)
        {
            const float amt = proc.apvts.getRawParameterValue("mamt" + juce::String(selected))->load();
            const bool onC = src == 2;
            if (loopListening && !proc.isPreviewing()) loopListening = false;
            segmented(g, juce::Rectangle<float>(rx, ry, 262.f, 30.f), { "loop", "raw", "matched" }, loopListening ? 0 : !onC ? -1 : amt > 0.5f ? 2 : 1,
                      [this, md](int i) { loopListening = i == 0; if (i == 0) proc.previewLoopSound(selected); else { if (proc.padSource(selected) != 2) proc.setPadSource(selected, 2); setMatched(i == 2, md); } });
        }
        else if (hasC)
        {
            if (loopListening && !proc.isPreviewing()) loopListening = false;
            segmented(g, juce::Rectangle<float>(rx, ry, 262.f, 30.f), { "loop", "yours" }, loopListening ? 0 : src == 2 ? 1 : -1,
                      [this](int i) { loopListening = i == 0; if (i == 0) proc.previewLoopSound(selected); else setSource(selected, 2); });
            chip(g, rx, ry + 38.f, proc.isMatching() ? "matching ..." : "match to the loop", cAccent, [this] { if (!proc.isMatching()) { proc.startMatch(selected); toast("matching to the loop ..."); } }, 28.f, 12.f);
            ly = ry + 76.f;
        }
        else { g.setColour(ui::dim); g.setFont(ui::inter(11.f)); g.drawFittedText("the loop's own sound. choose a sample of yours and it is matched to this.", juce::Rectangle<float>(rx, ry, 262.f, 30.f).toNearestInt(), juce::Justification::topLeft, 2); }
        {
            float x = rx; const float y = ly;
            const auto f = ui::mono(10.f);
            const juce::String t1 = "drag edges, fades, gain  .  ";
            g.setColour(ui::faint); g.setFont(f); g.drawText(t1, juce::Rectangle<float>(x, y, ui::textW(f, t1) + 2.f, 14.f), juce::Justification::centredLeft);
            x += ui::textW(f, t1);
            const auto rr = juce::Rectangle<float>(x, y, ui::textW(f, "reset") + 2.f, 14.f);
            g.setColour(hot(rr) ? ui::white : ui::dim); g.drawText("reset", rr, juce::Justification::centredLeft);
            zone(rr.expanded(2.f), [this] { shape.resetAll(); toast("the shape is back as it was"); });
        }
        if (hasC)
        {
            const float y = ly + 22.f;
            g.setFont(ui::inter(11.f));
            if (pd.layered())
            {
                const juce::String a = "+ layer ";
                g.setColour(ui::dim); g.drawText(a, juce::Rectangle<float>(rx, y, 60.f, 16.f), juce::Justification::centredLeft);
                const float aw = ui::textW(ui::inter(11.f), a);
                g.setColour(ui::white); g.drawText(pd.layerName, juce::Rectangle<float>(rx + aw, y, 262.f - aw, 16.f), juce::Justification::centredLeft, true);
                float x = rx;
                for (auto [t, id] : { std::pair<const char*, int> { "change", 1 }, { "remove", 2 } })
                {
                    const auto r = juce::Rectangle<float>(x, y + 18.f, ui::textW(ui::inter(11.f), t) + 2.f, 16.f);
                    g.setColour(hot(r) ? ui::orange : ui::white); g.drawText(t, r, juce::Justification::centredLeft);
                    zone(r.expanded(2.f), [this, id] { if (id == 1) { proc.startLayer(selected); toast("looking for another layer ..."); } else proc.removeLayer(selected); });
                    x = r.getRight() + 12.f;
                }
            }
            else
            {
                const auto r = juce::Rectangle<float>(rx, y, ui::textW(ui::inter(11.f), "+ add a layer (a second sample of yours)") + 2.f, 16.f);
                g.setColour(hot(r) ? ui::white : ui::dim); g.drawText("+ add a layer (a second sample of yours)", r, juce::Justification::centredLeft);
                zone(r.expanded(2.f), [this] { if (!proc.isMatching()) { proc.startLayer(selected); toast("looking for a second sample ..."); } });
            }
        }
        // bottom row: the parts of the match, then the suggestions
        const float by = C.getBottom() - 30.f;
        float x = C.getX();
        if (matched)
        {
            bool glide = false; for (float v : md.glide) glide = glide || std::abs(v) > 0.05f;
            struct P { const char* n; int part; bool can, on; };
            const P parts[] = { { "eq", 0, true, md.useEq }, { "shape", 1, true, md.useShape }, { "pitch", 3, true, md.usePitch }, { "length", 4, md.en < 1999.f, md.useLen },
                                { "room", 2, md.roomMix > 0, md.useRoom }, { "drop", 5, glide, md.useGlide }, { "drive", 6, md.driveDb > 0.05f, md.useDrive },
                                { "width", 7, std::abs(md.widthDb) > 0.05f, md.useWidth } };
            for (auto& p : parts)
            {
                const int part = p.part; const bool on = p.on;
                const auto r = chip(g, x, by, p.n, !p.can ? cDashed : on ? cOn : cOutline, p.can ? std::function<void()>([this, part, on] { proc.setMatchPart(selected, part, !on); }) : std::function<void()>());
                x = r.getRight() + 6.f;
            }
            const auto r = chip(g, x + 4.f, by, juce::CharPointer_UTF8("\xe2\x86\xbb match again"), cNormal, [this] { if (!proc.isMatching()) { proc.startMatch(selected); toast("matching to the loop ..."); } });
            x = r.getRight();
        }
        const int ns = proc.suggestionCount(selected);
        if (ns > 0)
        {
            const int k = proc.suggestionIndex(selected);
            float rx2 = C.getRight();
            if (k + 1 < ns)
            {
                const auto nx = juce::File(proc.suggestionPath(selected, k + 1)).getFileNameWithoutExtension();
                const float af = proc.suggestionAfter(selected, k + 1);
                const auto r = chip(g, rx2, by, nx.substring(0, 22) + (af >= 0 ? "  " + juce::String(juce::roundToInt(af)) : juce::String()), cNormal, [this] { proc.stepSuggestion(selected, 1); }, 24.f, 11.f, true);
                rx2 = r.getX() - 8.f;
            }
            const auto nr = juce::Rectangle<float>(rx2 - 18.f, by + 2.f, 18.f, 20.f);
            g.setColour(k + 1 < ns ? ui::white : ui::line); g.setFont(ui::mono(13.f)); g.drawText(juce::CharPointer_UTF8("\xe2\x80\xba"), nr, juce::Justification::centred);
            zone(nr, [this] { proc.stepSuggestion(selected, 1); });
            const juce::String cnt = (k >= 0 ? juce::String(k + 1) : juce::String("-")) + " / " + juce::String(ns);
            const float cwid = ui::textW(ui::mono(11.f), cnt) + 8.f;
            g.setColour(ui::dim2); g.setFont(ui::mono(11.f)); g.drawText(cnt, juce::Rectangle<float>(nr.getX() - cwid, by + 4.f, cwid, 16.f), juce::Justification::centred);
            const auto pr = juce::Rectangle<float>(nr.getX() - cwid - 18.f, by + 2.f, 18.f, 20.f);
            g.setColour(k > 0 ? ui::white : ui::line); g.setFont(ui::mono(13.f)); g.drawText(juce::CharPointer_UTF8("\xe2\x80\xb9"), pr, juce::Justification::centred);
            zone(pr, [this] { proc.stepSuggestion(selected, -1); });
        }
    }
    bool loopListening = false;
    void setMatched(bool on, const MatchData& md)
    {
        auto* p = proc.apvts.getParameter("mamt" + juce::String(selected));
        if (p == nullptr) return;
        proc.undo.beginNewTransaction();
        const float cur = p->convertFrom0to1(p->getValue());
        if (!on) { if (cur > 0.5f) rawKeep[(size_t) selected] = cur; p->setValueNotifyingHost(p->convertTo0to1(0.f)); }
        else { const float back = rawKeep[(size_t) selected] > 0.5f ? rawKeep[(size_t) selected] : 100.f * md.bestAmount; p->setValueNotifyingHost(p->convertTo0to1(back)); }
        proc.audition(selected);
    }
    void drawEmptyPad(juce::Graphics& g)
    {
        const auto C = content;
        g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
        g.drawText("Empty pad", juce::Rectangle<float>(C.getX(), C.getY(), 300.f, 38.f), juce::Justification::centredLeft);
        g.setColour(ui::dim); g.setFont(ui::inter(12.f));
        g.drawText("a sound of yours for it: what is it?", juce::Rectangle<float>(C.getX(), C.getY() + 60.f, 400.f, 16.f), juce::Justification::centredLeft);
        float x = C.getX(), y = C.getY() + 86.f;
        for (auto& t : DrumSlicerProcessor::padTypes())
        {
            const auto tt = t;
            const auto w = ui::textW(ui::inter(11.f), ui::lower(t)) + 20.f;
            if (x + w > C.getRight()) { x = C.getX(); y += 32.f; }
            const auto r = chip(g, x, y, ui::lower(t), pendingType == t ? cOn : cOutline, [this, tt] { pendingType = tt; setPage(pSamples); });
            x = r.getRight() + 8.f;
        }
        chip(g, C.getX(), y + 44.f, "from the file name  .  choose a sample", cNormal, [this] { pendingType.clear(); setPage(pSamples); }, 30.f, 12.f);
    }

    // ---------------------------------------------------------------- page: the pad's ···  (what it is, its hits) - no menus
    void drawMore(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        if (!res) { drawDropHere(g); return; }
        const auto& pd = res->pads[(size_t) selected];
        if (!pd.valid) { drawEmptyPad(g); return; }
        const auto C = content;
        {   // the path:  ‹ Snare   ···    (click the name = back to the pad)
            const auto nm = juce::String(juce::CharPointer_UTF8("\xe2\x80\xb9 ")) + juce::String(pd.name.substring(0, 1)) + pd.name.substring(1).toLowerCase();
            const float nw = ui::textW(ui::inter(30.f, 300), nm);
            const auto br = juce::Rectangle<float>(C.getX(), C.getY(), nw + 4.f, 38.f);
            g.setColour(hot(br) ? ui::white : ui::dim); g.setFont(ui::inter(30.f, 300));
            g.drawText(nm, br, juce::Justification::centredLeft);
            zone(br, [this] { setPage(pPad); });
            g.setColour(ui::white); g.setFont(ui::inter(30.f, 300));
            g.drawText(juce::String(juce::CharPointer_UTF8("\xc2\xb7\xc2\xb7\xc2\xb7")), juce::Rectangle<float>(br.getRight() + 14.f, C.getY(), 60.f, 38.f), juce::Justification::centredLeft);
            g.setColour(ui::dim2); g.setFont(ui::mono(12.f));
            g.drawText(juce::MidiMessage::getMidiNoteName(pd.note, true, true, 3).toLowerCase() + "  .  " + juce::String(pd.hits) + " hits",
                       juce::Rectangle<float>(br.getRight() + 80.f, C.getY() + 12.f, 200.f, 18.f), juce::Justification::centredLeft);
        }
        auto row = [&](float y, const juce::String& label) { g.setColour(ui::dim); g.setFont(ui::inter(12.f)); g.drawText(label, juce::Rectangle<float>(C.getX(), y, 150.f, 24.f), juce::Justification::centredLeft); };
        float y = C.getY() + 56.f;
        row(y, "it is a");
        float x = C.getX() + 160.f;
        const auto cur = pd.name.upToFirstOccurrenceOf(" ", false, false);
        for (auto& t : DrumSlicerProcessor::padTypes())
        {
            const auto tt = t;
            const auto r = chip(g, x, y, ui::lower(t), cur == t ? cOn : cOutline, cur == t ? std::function<void()>() : std::function<void()>([this, tt] { proc.setPadType(selected, tt); toast("pad  " + ui::lower(proc.getResult()->pads[(size_t) selected].name)); }));
            x = r.getRight() + 8.f;
            if (x > C.getRight() - 60.f) { x = C.getX() + 160.f; y += 30.f; }
        }
        y += 40.f;
        row(y, "move its " + juce::String(pd.hits) + " hits into");
        x = C.getX() + 160.f;
        bool anyOther = false;
        for (int q = 0; q < ds::kPads; ++q)
        {
            if (q == selected || !res->pads[(size_t) q].valid) continue;
            anyOther = true;
            const auto& on = res->pads[(size_t) q].name;
            const auto r = chip(g, x, y, juce::String(on.substring(0, 1)) + on.substring(1).toLowerCase(), cOutline,
                                [this, q] { const auto into = proc.getResult()->pads[(size_t) q].name; proc.mergePad(selected, q); selectPad(q); toast("hits moved to  " + ui::lower(into)); setPage(pPad); });
            x = r.getRight() + 8.f;
        }
        if (!anyOther) { g.setColour(ui::faint); g.setFont(ui::inter(11.f)); g.drawText("no other pad", juce::Rectangle<float>(x, y, 200.f, 24.f), juce::Justification::centredLeft); }
        y += 40.f;
        if (pd.user.getNumSamples() > 0)
        {
            row(y, "your sample");
            chip(g, C.getX() + 160.f, y, "remove it  .  back to the loop's sound", cOutline, [this] { proc.clearPadSample(selected); toast("back to the loop's sound"); setPage(pPad); });
        }
        chip(g, C.getX(), C.getBottom() - 34.f, "clear the sound  .  hits stay", cNormal, [this] { proc.clearPadSound(selected); toast("no sound: choose one, the hits wait"); setPage(pPad); }, 30.f, 12.f);
        g.setColour(ui::faint); g.setFont(ui::mono(10.f));
        g.drawText("this page is the pad's menu  .  right-click a pad = here", juce::Rectangle<float>(C.getRight() - 360.f, C.getBottom() - 26.f, 360.f, 14.f), juce::Justification::centredRight);
    }

    // ---------------------------------------------------------------- knobs (right): they follow the page and the pad
    juce::String knobId[8];
    void rebindKnobs()
    {
        auto res = proc.getResult();
        const bool src = page == pSource || !res;
        const juce::String s(selected);
        juce::String ids[8];
        if (src) { ids[0] = "bpm"; ids[1] = "sens"; ids[2] = "quant"; ids[3] = "master"; }      // the source page: 4 (the 2nd row rests)
        else { ids[0] = "mamt" + s; ids[1] = "vol" + s; ids[2] = "pitch" + s; ids[3] = "tone" + s;
               ids[4] = "pan" + s;  ids[5] = "punch" + s; ids[6] = "sus" + s; ids[7] = "room" + s; }
        for (int k = 0; k < 8; ++k)
        {
            auto& sl = knobs[(size_t) k];
            sl.setVisible(ids[k].isNotEmpty());
            if (ids[k].isEmpty()) { knobId[k].clear(); continue; }
            if (ids[k] != knobId[k] && !sl.isMouseButtonDown())
            {
                knobAtt[(size_t) k].reset();
                knobId[k] = ids[k];
                if (auto* p = proc.apvts.getParameter(ids[k])) knobAtt[(size_t) k] = std::make_unique<juce::SliderParameterAttachment>(*p, sl, &proc.undo);
                sl.textFromValueFunction = fmtFor(ids[k]);
                sl.onDragEnd = ids[k] == "sens" ? std::function<void()>([this] { if (proc.getResult() != nullptr && proc.getSource() != nullptr) proc.reanalyse(); }) : std::function<void()>();
                sl.getProperties().set("hero", ids[k].startsWith("mamt"));
                sl.repaint();
            }
            const bool padKnob = !src;
            const bool padOk = padKnob && res && res->pads[(size_t) selected].valid;
            bool en = !padKnob || padOk;
            if (ids[k].startsWith("mamt"))
            {
                const bool hasC = padOk && res->pads[(size_t) selected].user.getNumSamples() > 0;
                en = hasC && proc.getMatch(selected).on && proc.padSource(selected) == 2;
            }
            if (sl.isEnabled() != en) sl.setEnabled(en);
        }
    }
    static std::function<juce::String(double)> fmtFor(const juce::String& id)
    {
        if (id.startsWith("mamt")) return [](double v) { return "[" + juce::String(juce::roundToInt(v)) + "]"; };
        if (id.startsWith("vol") || id == "master") return [](double v) { return v <= 0.0001 ? juce::String("-inf") : juce::String(juce::Decibels::gainToDecibels(v), 1); };
        if (id.startsWith("pan")) return [](double v) { const int p = juce::roundToInt(v * 100); return p == 0 ? juce::String("c") : juce::String(p < 0 ? "l " : "r ") + juce::String(std::abs(p)); };
        if (id.startsWith("pitch")) return [](double v) { return juce::String(v > 0 ? "+" : "") + juce::String(v, std::abs(v - std::round(v)) < 0.005 ? 0 : 1); };
        if (id.startsWith("tone")) return [](double v) { return std::abs(v) < 0.05 ? juce::String("0") : juce::String(v > 0 ? "bright " : "dark ") + juce::String(std::abs(v), 1); };
        if (id.startsWith("punch") || id.startsWith("sus")) return [](double v) { return juce::String(v > 0 ? "+" : "") + juce::String(v, 1); };
        if (id.startsWith("atk")) return [](double v) { return juce::String(v, v < 10 ? 1 : 0) + " ms"; };
        if (id.startsWith("room")) return [](double v) { return std::abs(v - 100.0) < 0.6 ? juce::String("loop") : v < 0.5 ? juce::String("dry") : juce::String(juce::roundToInt(v)) + "%"; };
        if (id == "sens" || id == "quant") return [](double v) { return juce::String(juce::roundToInt(v * 100)); };
        if (id == "bpm") return [](double v) { return juce::String(v, 1); };
        return [](double v) { return juce::String(v, 1); };
    }
    static juce::String knobName(const juce::String& id)
    {
        if (id.startsWith("mamt")) return "like the loop";
        if (id.startsWith("vol")) return "volume";
        if (id.startsWith("pitch")) return "pitch";
        if (id.startsWith("tone")) return "tone";
        if (id.startsWith("pan")) return "pan";
        if (id.startsWith("punch")) return "punch";
        if (id.startsWith("sus")) return "sustain";
        if (id.startsWith("atk")) return "attack";
        if (id.startsWith("room")) return "room";
        if (id == "bpm") return "tempo";
        if (id == "sens") return "sensitivity";
        if (id == "quant") return "how tight";
        if (id == "master") return "master";
        return id;
    }
    void drawKnobs(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        const bool src = page == pSource || !res;
        g.setColour(ui::dim2); g.setFont(ui::mono(11.f));
        g.drawText(src ? juce::String("analysis") : res->pads[(size_t) selected].valid ? ui::lower(res->pads[(size_t) selected].name) : juce::String("empty pad"),
                   juce::Rectangle<float>(822.f, 510.f, 140.f, 16.f), juce::Justification::centredLeft);
        for (int k = 0; k < 8; ++k)
        {
            const auto r = knobRect(k);
            auto& sl = knobs[(size_t) k];
            if (!sl.isVisible()) continue;
            const juce::String nm = knobName(knobId[k]);
            const juce::String val = sl.isEnabled() ? sl.getTextFromValue(sl.getValue()) : juce::String("--");
            const auto f1 = ui::inter(10.f), f2 = ui::mono(10.f);
            const float w1 = ui::textW(f1, nm + " "), w2 = ui::textW(f2, val);
            const float x0 = r.getCentreX() - (w1 + w2) * 0.5f, y = r.getBottom() - 9.f;
            g.setColour(sl.isEnabled() ? ui::label : ui::faint); g.setFont(f1); g.drawText(nm, juce::Rectangle<float>(x0, y, w1, 16.f), juce::Justification::centredLeft);
            g.setColour(sl.isEnabled() ? ui::white : ui::faint); g.setFont(f2); g.drawText(val, juce::Rectangle<float>(x0 + w1, y, w2 + 2.f, 16.f), juce::Justification::centredLeft);
        }
        // loop | your kit: every pad on the loop's own sound, or back to yours
        const auto sw = switchRect();
        const bool kit = kitMode();
        g.setColour(kit ? ui::dim : ui::white); g.setFont(ui::inter(11.f, kit ? 400 : 600));
        g.drawText("loop", juce::Rectangle<float>(sw.getX() - 44.f, sw.getY(), 38.f, sw.getHeight()), juce::Justification::centredRight);
        g.setColour(kit ? ui::white : ui::dim); g.setFont(ui::inter(11.f, kit ? 600 : 400));
        g.drawText("your kit", juce::Rectangle<float>(sw.getRight() + 8.f, sw.getY(), 60.f, sw.getHeight()), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff0e0e10)); g.fillRoundedRectangle(sw, sw.getHeight() * 0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(sw.reduced(0.5f), sw.getHeight() * 0.5f, 1.f);
        const auto knob = juce::Rectangle<float>(sw.getHeight() - 4.f, sw.getHeight() - 4.f).withCentre({ kit ? sw.getRight() - sw.getHeight() * 0.5f : sw.getX() + sw.getHeight() * 0.5f, sw.getCentreY() });
        juce::Path kp; kp.addEllipse(knob);
        ui::shadow(g, knob, 12.f, 0.5f, 2.f, 1.f, true);
        g.setColour(res ? ui::white : ui::faint); g.fillPath(kp);
        if (res) zone(sw.withLeft(sw.getX() - 44.f).withRight(sw.getRight() + 66.f), [this] { toggleKitMode(); });
    }
    std::array<int, ds::kPads> keptSrc { -1, -1, -1, -1, -1, -1, -1, -1 };
    bool kitMode() const
    {
        auto r = proc.getResult(); if (!r) return false;
        for (int i = 0; i < ds::kPads; ++i) if (r->pads[(size_t) i].valid && proc.padSource(i) == 2) return true;
        return false;
    }
    void toggleKitMode()
    {
        auto r = proc.getResult(); if (!r) return;
        if (kitMode())
        {
            for (int i = 0; i < ds::kPads; ++i) if (r->pads[(size_t) i].valid) { keptSrc[(size_t) i] = proc.padSource(i); proc.setPadSource(i, 0); }
            toast("the loop's own sounds");
        }
        else
        {
            bool any = false;
            for (int i = 0; i < ds::kPads; ++i)
                if (r->pads[(size_t) i].valid)
                {
                    const bool hasC = r->pads[(size_t) i].user.getNumSamples() > 0;
                    const int want = keptSrc[(size_t) i] >= 0 ? keptSrc[(size_t) i] : (hasC ? 2 : 0);
                    if (want != proc.padSource(i) && (want < 2 || hasC)) proc.setPadSource(i, want);
                    any = any || (want == 2 && hasC);
                }
            toast(any ? juce::String("your kit") : juce::String("no samples of yours yet: build my kit"));
        }
        shape.invalidate();
    }

    // ---------------------------------------------------------------- the keys (left): build my kit, play, undo, midi
    void drawKeys(juce::Graphics& g, const std::shared_ptr<ds::Result>& res)
    {
        // BUILD MY KIT
        {
            const auto r = buildRect();
            const bool can = res != nullptr && !proc.isMatching() && !proc.isBusy();
            juce::Path p; p.addEllipse(r);
            ui::glow(g, r, ui::orange, can ? 0.6f : 0.15f, 10.f, 14.f);
            juce::ColourGradient gr(juce::Colour(0xffff7b47), r.getX() + r.getWidth() * 0.38f, r.getY() + r.getHeight() * 0.3f, juce::Colour(0xffd8440e), r.getRight(), r.getBottom(), true);
            gr.addColour(0.55, juce::Colour(0xfff2541a));
            g.setGradientFill(gr); g.fillPath(p);
            if (!can) { g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillPath(p); }
            if (hot(r) && can) { g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillPath(p); }
            g.setColour(juce::Colours::white.withAlpha(0.25f)); g.drawEllipse(r.reduced(1.f), 1.f);
            g.setColour(juce::Colours::white); g.setFont(ui::inter(13.f, 500));
            juce::String t = "build\nmy kit";
            if (proc.isBuilding()) t = "building\n" + proc.matchMessage().fromLastOccurrenceOf(" ", false, false).toLowerCase();
            g.drawFittedText(t, r.toNearestInt(), juce::Justification::centred, 2, 1.f);
            if (hot(r) && res && !proc.isBuilding())
            {   // before you press it: what it will do
                int keep = 0; for (int i = 0; i < ds::kPads; ++i) if (res->pads[(size_t) i].valid && res->pads[(size_t) i].hits > 0) ++keep;
                const int n = proc.padsToBuild(); keep -= n;
                hint = n == 0 ? juce::String("every pad is locked  .  unlock one to build it again")
                              : "builds " + juce::String(n) + (n == 1 ? " pad" : " pads") + " from your folders: nearest, matched, layered  .  keeps " + juce::String(keep) + " locked";
            }
            zone(r, [this] {
                if (proc.getResult() == nullptr) { toast("analyse a loop first"); return; }
                if (proc.isMatching() || proc.isBusy()) return;
                proc.buildKit(); });
        }
        auto key = [&](int k, std::function<void(juce::Rectangle<float>)> drawIcon, std::function<void()> click, std::function<void()> drag = {})
        {
            const auto r = keyRect(k);
            juce::Path p; p.addEllipse(r);
            ui::shadow(g, r, 24.f, 0.8f, 6.f, 4.f, true);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff343437), r.getX() + r.getWidth() * 0.38f, r.getY() + r.getHeight() * 0.3f, juce::Colour(0xff1f1f22), r.getRight(), r.getBottom(), true));
            g.fillPath(p);
            if (hot(r)) { g.setColour(juce::Colours::white.withAlpha(0.06f)); g.fillPath(p); }
            g.setColour(juce::Colours::white.withAlpha(0.08f)); g.drawEllipse(r.reduced(0.5f), 1.f);
            drawIcon(r);
            zone(r, std::move(click), std::move(drag));
        };
        const bool srcPage = page == pSource || !res;
        const bool playing = srcPage ? proc.isSourcePlaying() : proc.isMidiPlaying();
        key(0, [&](juce::Rectangle<float> r) {
            const auto c = r.getCentre();
            g.setColour(playing ? ui::orange : ui::white);
            if (playing) g.fillRoundedRectangle(juce::Rectangle<float>(11.f, 11.f).withCentre(c), 1.5f);
            else { juce::Path t; t.addTriangle(c.x - 4.f, c.y - 6.5f, c.x - 4.f, c.y + 6.5f, c.x + 7.f, c.y); g.fillPath(t); } },
            [this, srcPage] {
                if (srcPage) { if (proc.getSource()) proc.playSource(!proc.isSourcePlaying()); }
                else if (proc.getResult()) proc.playMidi(!proc.isMidiPlaying()); });
        key(1, [&](juce::Rectangle<float> r) {
            const bool can = proc.undo.canUndo() || proc.canUndoBuild();
            juce::Path a; const auto c = r.getCentre();
            a.startNewSubPath(c.x - 1.f, c.y - 6.f); a.lineTo(c.x - 6.f, c.y - 1.f); a.lineTo(c.x - 1.f, c.y + 4.f);
            a.startNewSubPath(c.x - 6.f, c.y - 1.f); a.lineTo(c.x + 3.f, c.y - 1.f); a.quadraticTo(c.x + 7.f, c.y - 1.f, c.x + 7.f, c.y + 3.f); a.quadraticTo(c.x + 7.f, c.y + 7.f, c.x + 3.f, c.y + 7.f);
            g.setColour(can ? ui::white : ui::faint); g.strokePath(a, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); },
            [this] { if (proc.undo.canUndo()) { proc.undo.undo(); toast("undo"); shape.invalidate(); }
                     else if (proc.canUndoBuild()) { proc.undoBuild(); shape.invalidate(); }
                     else toast("nothing to undo"); });
        if (hot(keyRect(1)))
            hint = proc.undo.canUndo() ? "undo: " + ui::lower(proc.undo.getUndoDescription().isNotEmpty() ? proc.undo.getUndoDescription() : juce::String("the last knob / sound shape move"))
                 : proc.canUndoBuild() ? juce::String("undo: build my kit  (the kit as it was before)") : juce::String("nothing to undo");
        key(2, [&](juce::Rectangle<float> r) {
            g.setColour(res ? ui::white : ui::faint); g.setFont(ui::mono(9.f));
            g.drawText("midi", r.withTrimmedBottom(14.f).withTrimmedTop(8.f), juce::Justification::centred);
            juce::Path a; const auto c = r.getCentre().translated(0.f, 6.f);
            a.startNewSubPath(c.x, c.y - 4.f); a.lineTo(c.x, c.y + 3.f); a.startNewSubPath(c.x - 3.f, c.y); a.lineTo(c.x, c.y + 3.f); a.lineTo(c.x + 3.f, c.y);
            g.strokePath(a, juce::PathStrokeType(1.2f)); },
            [this] { toast(proc.getResult() ? "drag this key onto a track in cubase" : "analyse a loop first"); },
            [this] { auto f = proc.makeMidiFile(); if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this); });
        g.setColour(ui::faint); g.setFont(ui::mono(9.f));
        g.drawText(srcPage ? "play the loop" : "play the pattern", juce::Rectangle<float>(keyRect(0).getCentreX() - 50.f, keyRect(0).getBottom() + 6.f, 100.f, 12.f), juce::Justification::centred);
        g.drawText("drag", juce::Rectangle<float>(keyRect(2).getCentreX() - 24.f, keyRect(2).getBottom() + 6.f, 48.f, 12.f), juce::Justification::centred);
    }

    // ---------------------------------------------------------------- the 8 pads
    bool padHasSound(int i) const { auto r = proc.getResult(); return r && i >= 0 && i < ds::kPads && r->pads[(size_t) i].valid && !proc.padIsSilent(i); }
    void drawPads(juce::Graphics& g, const std::shared_ptr<ds::Result>& res, double now)
    {
        const double sinceEnd = now - buildEnd;
        for (int i = 0; i < ds::kPads; ++i)
        {
            const auto r = padRect(i);
            const bool has = res && res->pads[(size_t) i].valid;
            const bool sel = i == selected && res != nullptr, hov = i == hoverPad && has;
            const auto& pd = has ? res->pads[(size_t) i] : emptyPad;
            float lv = levels[(size_t) i];
            if (buildEnd > 0 && sinceEnd < 900 && has) lv = std::max(lv, (float) std::max(0.0, 1.0 - std::abs(sinceEnd - 300.0 - 50.0 * i) / 200.0));
            juce::Path p; p.addRoundedRectangle(r, 12.f);
            ui::shadow(g, r.reduced(3.f, 0.f), 12.f, 0.7f, 7.f, 4.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2b2e).brighter(hov ? 0.04f : 0.f), 0, r.getY(), juce::Colour(0xff212124), 0, r.getBottom(), false));
            g.fillPath(p);
            g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawHorizontalLine((int) r.getY() + 1, r.getX() + 10.f, r.getRight() - 10.f);
            if (lv > 0.03f) { g.setColour(ui::orange.withAlpha(0.10f * lv)); g.fillPath(p); }
            if (sel) { g.setColour(ui::white); g.strokePath(p, juce::PathStrokeType(1.5f)); }
            else if (hov) { g.setColour(juce::Colour(0xff4a4a4d)); g.strokePath(p, juce::PathStrokeType(1.f)); }
            if (dropPad == i) { g.setColour(ui::orange); g.strokePath(p, juce::PathStrokeType(2.f)); }
            const juce::String nm = has ? juce::String(pd.name.substring(0, 1)) + pd.name.substring(1).toLowerCase() : juce::String("-");
            const int src = has ? proc.padSource(i) : 0;
            juce::String sub;
            if (!has) sub = res ? "empty" : "";
            else if (proc.padIsSilent(i)) sub = "no sound";
            else if (src == 2 && pd.user.getNumSamples() > 0) sub = pd.userName;
            else sub = src == 1 ? "best hit" : "loop's";
            const bool lk = has && proc.padLocked(i), mu = proc.mute[(size_t) i].load(), so = proc.solo[(size_t) i].load();
            if (!hov)
            {   // quiet: sign, name, what it plays; lock / m / s only when on
                ui::glyph(g, has ? pd.name : "-", juce::Rectangle<float>(r.getX() + 10.f, r.getY() + 10.f, 11.f, 11.f), has ? (lv > 0.05f ? ui::orange : ui::white) : ui::line);
                float cx = r.getRight() - 8.f;
                if (lk) { ui::lockIcon(g, juce::Rectangle<float>(11.f, 11.f).withPosition(cx - 11.f, r.getY() + 9.f), ui::orange, true); cx -= 15.f; }
                if (so) { g.setColour(ui::orange); g.setFont(ui::inter(10.f, 600)); g.drawText("s", juce::Rectangle<float>(cx - 10.f, r.getY() + 7.f, 10.f, 14.f), juce::Justification::centred); cx -= 13.f; }
                if (mu) { g.setColour(ui::white); g.setFont(ui::inter(10.f, 600)); g.drawText("m", juce::Rectangle<float>(cx - 10.f, r.getY() + 7.f, 10.f, 14.f), juce::Justification::centred); }
            }
            else
            {   // hover: lock | m | s across the top
                const float bw = (r.getWidth() - 16.f - 8.f) / 3.f;
                auto small = [&](int k, std::function<void(juce::Rectangle<float>)> draw, std::function<void()> click)
                {
                    const auto br = juce::Rectangle<float>(r.getX() + 8.f + k * (bw + 4.f), r.getY() + 7.f, bw, 18.f);
                    g.setColour(juce::Colour(0xff343437).brighter(hot(br) ? 0.12f : 0.f)); g.fillRoundedRectangle(br, 5.f);
                    draw(br); zone(br, std::move(click));
                };
                small(0, [&](juce::Rectangle<float> b) { ui::lockIcon(g, b.withSizeKeepingCentre(11.f, 11.f), lk ? ui::orange : ui::white, lk); },
                      [this, i] { proc.setPadLocked(i, !proc.padLocked(i)); toast(proc.padLocked(i) ? "locked: build my kit keeps this pad" : "unlocked: build my kit may change this pad"); });
                small(1, [&](juce::Rectangle<float> b) { if (mu) { g.setColour(ui::white); g.fillRoundedRectangle(b, 5.f); } g.setColour(mu ? juce::Colour(0xff0b0b0c) : ui::white); g.setFont(ui::inter(10.f, 600)); g.drawText("m", b, juce::Justification::centred); },
                      [this, i] { proc.mute[(size_t) i] = !proc.mute[(size_t) i].load(); });
                small(2, [&](juce::Rectangle<float> b) { if (so) { g.setColour(ui::orange); g.fillRoundedRectangle(b, 5.f); } g.setColour(so ? juce::Colour(0xff0b0b0c) : ui::white); g.setFont(ui::inter(10.f, 600)); g.drawText("s", b, juce::Justification::centred); },
                      [this, i] { proc.solo[(size_t) i] = !proc.solo[(size_t) i].load(); });
            }
            g.setColour(has ? ui::white : ui::faint); g.setFont(ui::inter(13.f, 500));
            g.drawText(nm, juce::Rectangle<float>(r.getX() + 10.f, r.getBottom() - 40.f, r.getWidth() - 16.f, 16.f), juce::Justification::centredLeft, true);
            const int ns = has ? proc.suggestionCount(i) : 0;
            if (hov && ns > 0)
            {   // ‹ n/N ›
                const int k = proc.suggestionIndex(i);
                const auto row = juce::Rectangle<float>(r.getX() + 6.f, r.getBottom() - 24.f, r.getWidth() - 12.f, 16.f);
                const auto lb = row.withWidth(16.f), rb = row.withTrimmedLeft(row.getWidth() - 16.f);
                g.setFont(ui::mono(12.f));
                g.setColour(k > 0 ? (hot(lb) ? ui::orange : ui::white) : ui::faint); g.drawText(juce::CharPointer_UTF8("\xe2\x80\xb9"), lb, juce::Justification::centred);
                g.setColour(k + 1 < ns ? (hot(rb) ? ui::orange : ui::white) : ui::faint); g.drawText(juce::CharPointer_UTF8("\xe2\x80\xba"), rb, juce::Justification::centred);
                g.setColour(ui::dim); g.setFont(ui::mono(9.f));
                g.drawText((k >= 0 ? juce::String(k + 1) : juce::String("-")) + "/" + juce::String(ns), row.reduced(16.f, 0.f), juce::Justification::centred);
                zone(lb, [this, i] { proc.stepSuggestion(i, -1); });
                zone(rb, [this, i] { proc.stepSuggestion(i, 1); });
            }
            else { g.setColour(ui::dim2); g.setFont(ui::inter(9.f)); g.drawText(sub, juce::Rectangle<float>(r.getX() + 10.f, r.getBottom() - 24.f, r.getWidth() - 16.f, 12.f), juce::Justification::centredLeft, true); }
            const auto bar = juce::Rectangle<float>(r.getX() + 10.f, r.getBottom() - 8.f, r.getWidth() - 20.f, 2.f);
            if (lv > 0.03f) g.setColour(ui::orange.withAlpha(0.4f + 0.6f * lv)); else g.setColour(sel ? ui::white : juce::Colour(0xff38383b));
            g.fillRoundedRectangle(bar, 1.f);
        }
    }

    void setSource(int pad, int src) { proc.setPadSource(pad, src); proc.audition(pad); shape.invalidate(); rebindKnobs(); repaint(); }

    DrumSlicerProcessor& proc;
    ui::Look look;
    Page page = pSource;
    int idle = 0;
    int selected = 0, shownVersion = -1, hoverPad = -1, dropPad = -1, browsedPad = -1;
    bool dropping = false, analysing = false, wasBuilding = false;
    const ds::Result* lastRes = nullptr;
    double settleStart = 0, analyseStart = 0, buildStart = 0, buildEnd = 0;
    float scoreFrom = 0, shownScore = 0, tightShown = 0;
    std::array<float, 8> levels {};
    std::array<float, 8> rawKeep {};
    juce::String pendingType, lastMatchMsg, lastStatus, soundSig, toastText;
    juce::uint32 toastUntil = 0;
    const ds::Pad emptyPad;
    // source page view
    std::vector<float> peaks;
    juce::int64 v0 = 0, v1 = 0, pk0 = -1, pk1 = -1;
    juce::uint64 viewId = 0, peaksId = 0, gmaxId = 0;
    float gmax = 1.f;
    int dragWhat = 0;
    std::array<juce::Slider, 8> knobs;
    std::array<std::unique_ptr<juce::SliderParameterAttachment>, 8> knobAtt;
    ShapeView shape;
    SampleBrowser samples;
    SampleBrowser loops;
    KitsPage kitsPage;
    HoldButton delPad;
    juce::TooltipWindow tips { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerPanel)
};

// resizable window (fixed aspect), the device is scaled - everything stays sharp because it is drawn in code
class DrumSlicerEditor : public juce::AudioProcessorEditor
{
public:
    explicit DrumSlicerEditor(DrumSlicerProcessor& p) : juce::AudioProcessorEditor(p), proc(p), panel(p)
    {
        addAndMakeVisible(panel);
        setResizable(true, true);
        setResizeLimits(868, 504, 2480, 1440);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) DrumSlicerPanel::W / DrumSlicerPanel::H);
        const int w = juce::jlimit(868, 2480, (int) proc.apvts.state.getProperty("uiWidth2", 1240));
        setSize(w, juce::roundToInt(w * (double) DrumSlicerPanel::H / DrumSlicerPanel::W));
    }
    void parentHierarchyChanged() override
    {
        if (raised || getPeer() == nullptr) return;
        raised = true;
        for (int ms : { 120, 600 })
            juce::Timer::callAfterDelay(ms, [sp = juce::Component::SafePointer<DrumSlicerEditor>(this)] { if (sp != nullptr) sp->raiseFrame(); });
    }
    void raiseFrame() { if (auto* peer = getPeer()) rekitRaiseFrame(peer->getNativeHandle()); }
    bool raised = false;
    void resized() override
    {
        panel.setBounds(0, 0, DrumSlicerPanel::W, DrumSlicerPanel::H);
        panel.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) DrumSlicerPanel::W));
        proc.apvts.state.setProperty("uiWidth2", getWidth(), nullptr);
    }
    DrumSlicerPanel& getPanel() { return panel; }
private:
    DrumSlicerProcessor& proc;
    DrumSlicerPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSlicerEditor)
};
