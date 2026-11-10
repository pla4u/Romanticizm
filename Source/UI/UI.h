#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace rmz
{
inline juce::Font serifFont (float h, bool italic = true)
{ return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), h, italic ? juce::Font::italic : juce::Font::plain)); }
inline juce::Font sansFont (float h, bool bold = false)
{ return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain)); }

inline float animTime() { return (float) (juce::Time::getMillisecondCounterHiRes() * 0.001); }

// ---------------------------------------------------------------- hearts
inline juce::Path heartShape (float cx, float cy, float s, float rot = 0.0f)
{
    juce::Path p;
    p.startNewSubPath (0.0f, 0.9f);
    p.cubicTo (-0.5f, 0.55f, -1.0f, 0.15f, -1.0f, -0.30f);
    p.cubicTo (-1.0f, -0.75f, -0.60f, -0.95f, -0.30f, -0.95f);
    p.cubicTo (-0.10f, -0.95f, 0.0f, -0.75f, 0.0f, -0.60f);
    p.cubicTo (0.0f, -0.75f, 0.10f, -0.95f, 0.30f, -0.95f);
    p.cubicTo (0.60f, -0.95f, 1.0f, -0.75f, 1.0f, -0.30f);
    p.cubicTo (1.0f, 0.15f, 0.5f, 0.55f, 0.0f, 0.9f);
    p.closeSubPath();
    p.applyTransform (juce::AffineTransform::scale (s).rotated (rot).translated (cx, cy));
    return p;
}

inline void glossHeart (juce::Graphics& g, float cx, float cy, float s, juce::Colour base, juce::Colour outline,
                        float rot = 0.0f, float lineW = 1.4f)
{
    const auto p = heartShape (cx, cy, s, rot);
    juce::ColourGradient gr (base.brighter (0.45f), cx - s * 0.5f, cy - s * 0.9f, base.darker (0.5f), cx + s * 0.4f, cy + s * 0.95f, false);
    g.setGradientFill (gr);
    g.fillPath (p);
    juce::Path sp;
    sp.addEllipse (-0.70f, -0.80f, 0.40f, 0.22f);
    sp.applyTransform (juce::AffineTransform::scale (s).rotated (rot).translated (cx, cy));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillPath (sp);
    if (lineW > 0.0f)
    {
        g.setColour (outline);
        g.strokePath (p, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ============================================================================
class RmzLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RmzLookAndFeel() { setTheme (0); }

    void setTheme (int i)
    {
        index = juce::jlimit (0, (int) themes().size() - 1, i);
        const auto& t = th();
        setColour (juce::PopupMenu::backgroundColourId, C (t.style == 1 ? 0xffe0e1dd : t.card));
        setColour (juce::PopupMenu::textColourId, C (t.style == 1 ? 0xff1b1917 : t.ink));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, C (t.accent));
        setColour (juce::PopupMenu::highlightedTextColourId, C (t.style == 1 ? 0xff1b1917 : t.paper));
    }
    const Theme& th() const { return themes()[(size_t) index]; }
    int themeIndex() const { return index; }
    int style() const { return th().style; }

    juce::Font getPopupMenuFont() override { return sansFont (14.0f); }

    // ======================================================================== rotary
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        const auto& t = th();
        const float labelH = 14.0f;
        const float side = std::min ((float) w, (float) h - labelH);
        const float cx = (float) x + (float) w * 0.5f, cy = (float) y + side * 0.5f;
        const float R = side * 0.5f - (t.style == 1 ? 4.0f : 1.0f);
        const bool hot = s.isMouseOverOrDragging();
        const bool bi = (bool) s.getProperties().getWithDefault ("bi", false);
        const float amount = bi ? std::abs (pos - 0.5f) * 2.0f : pos;
        const float ang = -2.4f + pos * 4.8f;

        const bool wall = (bool) s.getProperties().getWithDefault ("wall", false);
        if (t.style == 1)      knobDeck (g, cx, cy, R, pos, bi, hot, ang, wall);
        else if (t.style == 2) knobHearts (g, cx, cy, R, pos, bi, hot, ang);
        else                   knobFlower (g, cx, cy, R, amount, hot, ang);

        const juce::Colour lab = wall ? C (t.head) : C (t.ink);
        g.setColour (hot ? C (t.style == 1 ? 0xffd9480f : t.accent) : lab.withAlpha (0.78f));
        g.setFont (hot ? sansFont (11.0f, true) : sansFont (9.5f, true).withExtraKerningFactor (0.1f));
        const juce::String txt = hot ? s.getTextFromValue (s.getValue()) : s.getName().toUpperCase();
        g.drawText (txt, juce::Rectangle<float> ((float) x - 6.0f, (float) y + side + 1.0f, (float) w + 12.0f, labelH), juce::Justification::centred, false);
    }

    // ======================================================================== ribbon / fader
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const auto& t = th();
        const bool hot = s.isMouseOverOrDragging();
        const float ty = (float) y + (float) h - 5.0f;
        const float x0 = (float) x + 8.0f, x1 = (float) x + (float) w - 8.0f;
        const float kx = juce::jlimit (x0, x1, pos);

        if (t.style == 1)
        {
            g.setColour (juce::Colour (0xff23201d));
            g.fillRoundedRectangle (x0 - 2.0f, ty - 2.5f, x1 - x0 + 4.0f, 5.0f, 2.5f);
            g.setColour (C (t.accent));
            g.fillRoundedRectangle (x0, ty - 1.2f, std::max (2.0f, kx - x0), 2.4f, 1.2f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawRoundedRectangle (x0 - 2.0f, ty - 2.5f, x1 - x0 + 4.0f, 5.0f, 2.5f, 0.8f);
            juce::Rectangle<float> cap (kx - 5.5f, ty - 6.5f, 11.0f, 13.0f);
            juce::ColourGradient cg (juce::Colour (0xff55514c), cap.getX(), 0.0f, juce::Colour (0xff1c1a18), cap.getRight(), 0.0f, false);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (cap, 2.5f);
            g.setColour (hot ? C (t.accent2) : C (t.paper));
            g.fillRect (kx - 0.7f, cap.getY() + 2.0f, 1.4f, cap.getHeight() - 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (cap, 2.5f, 1.0f);
        }
        else if (t.style == 2)
        {
            g.setColour (C (t.ink));
            g.drawLine (x0, ty, x1, ty, 1.8f);
            g.fillEllipse (x0 - 3.0f, ty - 3.0f, 6.0f, 6.0f);
            g.setColour (C (t.accent));
            g.drawLine (x0, ty, kx, ty, 3.4f);
            glossHeart (g, kx, ty - 0.5f, hot ? 6.6f : 6.0f, C (t.accent), C (t.ink), 0.0f, 1.1f);
        }
        else
        {
            g.setColour (C (t.ink).withAlpha (0.18f));
            g.drawLine (x0, ty, x1, ty, 3.0f);
            juce::Path p;
            p.startNewSubPath (x0, ty);
            for (float xx = x0; xx <= kx; xx += 3.0f) p.lineTo (xx, ty + std::sin ((xx - x0) * 0.35f) * 0.8f);
            g.setColour (C (t.accent));
            g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            for (int i = 0; i < 5; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 5.0f;
                g.setColour (C (t.accent).withAlpha (hot ? 1.0f : 0.85f));
                g.fillEllipse (kx + std::sin (a) * 4.2f - 3.2f, ty - std::cos (a) * 4.2f - 3.2f, 6.4f, 6.4f);
            }
            g.setColour (C (t.paper));
            g.fillEllipse (kx - 3.0f, ty - 3.0f, 6.0f, 6.0f);
        }
        g.setColour (C (t.ink).withAlpha (0.8f));
        g.setFont (sansFont (9.5f, true).withExtraKerningFactor (0.1f));
        g.drawText (s.getName().toUpperCase(), x, y - 2, w / 2, 12, juce::Justification::left, false);
        if (hot)
        {
            g.setColour (C (t.style == 1 ? 0xffd9480f : t.accent));
            g.setFont (sansFont (10.5f, true));
            g.drawText (s.getTextFromValue (s.getValue()), x + w / 2, y - 2, w / 2, 12, juce::Justification::right, false);
        }
    }

    // ======================================================================== lamps and chips
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool down) override
    {
        const auto& t = th();
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const float breathe = 0.86f + 0.14f * std::sin (animTime() * 2.3f);
        const juce::String kind = b.getProperties().getWithDefault ("kind", "lamp").toString();
        if (kind == "chip") { drawChip (g, r.reduced (1.0f), b.getButtonText(), on, hover, down); return; }

        const float bx = r.getX() + 13.0f, by = r.getCentreY();
        if (t.style == 1)          // LED in a metal bezel
        {
            if (on)
            {
                juce::ColourGradient gl (C (t.glow).withAlpha (0.8f * breathe), bx, by, C (t.glow).withAlpha (0.0f), bx + 12.0f, by, true);
                g.setGradientFill (gl); g.fillEllipse (bx - 12.0f, by - 12.0f, 24.0f, 24.0f);
            }
            juce::ColourGradient bz (juce::Colour (0xfff0f0ee), bx - 6.0f, by - 7.0f, juce::Colour (0xff77797c), bx + 6.0f, by + 7.0f, false);
            g.setGradientFill (bz); g.fillEllipse (bx - 8.0f, by - 8.0f, 16.0f, 16.0f);
            juce::ColourGradient led (on ? C (t.accent2).brighter (0.5f) : juce::Colour (0xff5a2a1c), bx - 2.0f, by - 3.0f,
                                      on ? C (t.accent) : juce::Colour (0xff2a120c), bx + 4.0f, by + 5.0f, false);
            g.setGradientFill (led); g.fillEllipse (bx - 5.5f, by - 5.5f, 11.0f, 11.0f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.25f));
            g.fillEllipse (bx - 3.5f, by - 4.0f, 3.5f, 2.5f);
        }
        else if (t.style == 2)     // heart lamp
        {
            if (on)
            {
                juce::ColourGradient gl (C (t.glow).withAlpha (0.85f * breathe), bx, by, C (t.glow).withAlpha (0.0f), bx + 12.0f, by, true);
                g.setGradientFill (gl); g.fillEllipse (bx - 12.0f, by - 12.0f, 24.0f, 24.0f);
            }
            if (on) glossHeart (g, bx, by - 0.5f, 8.6f + 0.5f * breathe, C (t.accent), C (t.ink), 0.0f, 1.5f);
            else
            {
                g.setColour (juce::Colours::white);
                g.fillPath (heartShape (bx, by - 0.5f, 9.0f));
                g.setColour (C (t.ink));
                g.strokePath (heartShape (bx, by - 0.5f, 9.0f), juce::PathStrokeType (1.5f));
            }
        }
        else                       // bulb
        {
            const float by2 = by - 1.0f;
            if (on)
            {
                juce::ColourGradient gl (C (t.glow).withAlpha (0.85f * breathe), bx, by2, C (t.glow).withAlpha (0.0f), bx + 12.0f, by2, true);
                g.setGradientFill (gl); g.fillEllipse (bx - 12.0f, by2 - 12.0f, 24.0f, 24.0f);
            }
            g.setColour (on ? C (t.glow).brighter (0.5f) : C (t.ink).withAlpha (0.12f));
            g.fillEllipse (bx - 7.0f, by2 - 8.0f, 14.0f, 14.0f);
            g.setColour (C (t.ink).withAlpha (0.55f));
            g.drawEllipse (bx - 7.0f, by2 - 8.0f, 14.0f, 14.0f, 1.2f);
            g.fillRect (bx - 4.0f, by2 + 6.0f, 8.0f, 4.0f);
        }
        g.setColour ((b.getProperties().getWithDefault ("wall", false) ? C (t.head) : C (t.ink)).withAlpha (on ? 1.0f : 0.75f));
        g.setFont (sansFont (10.0f, true).withExtraKerningFactor (0.1f));
        g.drawText (b.getButtonText().toUpperCase(), (int) bx + 14, (int) r.getY(), (int) r.getWidth() - 28, (int) r.getHeight(), juce::Justification::centredLeft, false);
    }

    void drawChip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, bool hover, bool down)
    {
        const auto& t = th();
        if (t.style == 1)          // pressable deck key
        {
            const float rad = 4.0f;
            if (on)
            {
                g.setColour (juce::Colour (0xff1d1a17));
                g.fillRoundedRectangle (r, rad);
                juce::ColourGradient gg (C (t.accent).brighter (0.2f), 0.0f, r.getY(), C (t.accent).darker (0.35f), 0.0f, r.getBottom(), false);
                g.setGradientFill (gg);
                g.fillRoundedRectangle (r.reduced (1.5f).translated (0.0f, 1.0f), rad - 1.0f);
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.drawRoundedRectangle (r, rad, 1.0f);
                g.setColour (C (0xff1b1917));
            }
            else
            {
                juce::ColourGradient gg (juce::Colour (hover ? 0xfff2f2ef : 0xffe6e7e3), 0.0f, r.getY(), juce::Colour (0xffa9abaa), 0.0f, r.getBottom(), false);
                g.setGradientFill (gg);
                g.fillRoundedRectangle (r, rad);
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.drawRoundedRectangle (r, rad, 1.0f);
                g.setColour (C (t.ink));
            }
            g.setFont (sansFont (10.5f, true));
            g.drawText (text, r, juce::Justification::centred, false);
            (void) down;
            return;
        }
        if (t.style == 2)          // candy pill
        {
            const float rad = r.getHeight() * 0.5f;
            if (on)
            {
                juce::ColourGradient gg (C (t.accent).brighter (0.5f), 0.0f, r.getY(), C (t.accent).darker (0.4f), 0.0f, r.getBottom(), false);
                g.setGradientFill (gg);
                g.fillRoundedRectangle (r, rad);
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.fillRoundedRectangle (r.reduced (rad * 0.55f, 0.0f).withHeight (r.getHeight() * 0.38f).translated (0.0f, 2.0f), rad * 0.3f);
            }
            else
            {
                g.setColour (hover ? C (0xffffe3ea) : juce::Colours::white);
                g.fillRoundedRectangle (r, rad);
            }
            g.setColour (C (t.ink));
            g.drawRoundedRectangle (r, rad, 1.6f);
            g.setColour (on ? juce::Colours::white : C (t.ink));
            g.setFont (sansFont (10.5f, true));
            g.drawText (text, r, juce::Justification::centred, false);
            return;
        }
        g.setColour (on ? C (t.accent) : C (t.ink).withAlpha (hover ? 0.12f : 0.06f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (C (t.ink).withAlpha (0.3f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? C (t.paper) : C (t.ink));
        g.setFont (sansFont (10.5f, true));
        g.drawText (text, r, juce::Justification::centred, false);
    }

    // ======================================================================== ghost buttons
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        const auto& t = th();
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        if (t.style == 1)
        {
            juce::ColourGradient gg (juce::Colour (down ? 0xffa9abaa : (hover ? 0xfff2f2ef : 0xffe0e1dd)), 0.0f, r.getY(), juce::Colour (0xff9fa1a0), 0.0f, r.getBottom(), false);
            g.setGradientFill (gg);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            return;
        }
        if (t.style == 2)
        {
            g.setColour (down ? C (0xffffd0dc) : (hover ? C (0xffffe6ec) : juce::Colours::white));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (C (t.ink));
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.6f);
            return;
        }
        g.setColour (C (t.paper).withAlpha (down ? 0.9f : (hover ? 0.75f : 0.5f)));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (C (t.ink).withAlpha (0.28f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto& t = th();
        g.setColour (C (t.style == 1 ? 0xff1b1917 : t.ink));
        g.setFont (b.getProperties().contains ("serif") && t.style != 1 ? serifFont (15.0f) : sansFont (11.5f, true));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, true);
    }

private:
    // ---------------------------------------------------------------- style 0: layered flower
    void knobFlower (juce::Graphics& g, float cx, float cy, float R, float amount, bool hot, float ang)
    {
        const auto& t = th();
        if (hot)
        {
            juce::ColourGradient gl (C (t.glow).withAlpha (0.55f), cx, cy, C (t.glow).withAlpha (0.0f), cx, cy - R, true);
            g.setGradientFill (gl); g.fillEllipse (cx - R, cy - R, R * 2.0f, R * 2.0f);
        }
        const int N = 8;
        auto petal = [&] (float a, float centre, float halfLen, float halfWid, juce::Colour fill, float edge)
        {
            juce::Path p; p.addEllipse (-halfWid, -halfLen, halfWid * 2.0f, halfLen * 2.0f);
            const auto tf = juce::AffineTransform::translation (0.0f, -centre).rotated (a).translated (cx, cy);
            g.setColour (fill); g.fillPath (p, tf);
            g.setColour (C (t.ink).withAlpha (edge)); g.strokePath (p, juce::PathStrokeType (0.8f), tf);
        };
        const auto off = C (t.dark ? 0xff4a4260 : 0xffd2c6c6);
        for (int i = 0; i < N; ++i)
            petal (juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / (float) N, R * 0.60f, R * 0.34f, R * 0.14f, off.withAlpha (0.55f), 0.15f);
        for (int i = 0; i < N; ++i)
        {
            const float lit = juce::jlimit (0.0f, 1.0f, amount * (float) N - (float) i);
            const float br = 1.0f + 0.018f * lit * std::sin (animTime() * 1.7f + (float) i * 0.8f);
            petal (juce::MathConstants<float>::twoPi * (float) i / (float) N, R * (0.58f + 0.02f * lit) * br, R * (0.36f + 0.05f * lit) * br, R * (0.17f + 0.02f * lit) * br,
                   off.interpolatedWith (C (t.accent), lit).withAlpha (0.65f + 0.35f * lit), 0.30f);
        }
        const float cr = R * 0.40f;
        g.setColour (C (t.paper)); g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (C (t.ink).withAlpha (0.35f)); g.drawEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f, 1.0f);
        g.setColour (C (t.accent));
        g.drawLine (cx, cy, cx + std::sin (ang) * cr * 0.85f, cy - std::cos (ang) * cr * 0.85f, 2.0f);
        g.setColour (C (t.ink)); g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }

    // ---------------------------------------------------------------- style 1: hi-fi deck knob
    void knobDeck (juce::Graphics& g, float cx, float cy, float R, float pos, bool bi, bool hot, float ang, bool wall)
    {
        const auto& t = th();
        const int ticks = 21;
        for (int i = 0; i < ticks; ++i)
        {
            const float q = (float) i / (float) (ticks - 1);
            const float a = -2.4f + q * 4.8f;
            const bool lit = bi ? (pos >= 0.5f ? (q >= 0.5f - 0.001f && q <= pos + 0.001f) : (q <= 0.5f + 0.001f && q >= pos - 0.001f))
                                : (q <= pos + 0.001f && pos > 0.001f);
            const bool major = i % 5 == 0;
            const float r0 = R * (major ? 0.80f : 0.85f), r1 = R * 0.98f;
            g.setColour (lit ? C (t.accent) : (wall ? C (t.head) : C (t.ink)).withAlpha (0.4f));
            g.drawLine (cx + std::sin (a) * r0, cy - std::cos (a) * r0, cx + std::sin (a) * r1, cy - std::cos (a) * r1, major ? 2.2f : 1.4f);
        }
        const float kr = R * 0.66f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (cx - kr - 1.0f, cy - kr + 2.5f, kr * 2.0f + 2.0f, kr * 2.0f);
        juce::ColourGradient body (juce::Colour (0xff4a4642), cx - kr * 0.6f, cy - kr * 0.7f, juce::Colour (0xff141211), cx + kr * 0.8f, cy + kr * 0.9f, false);
        g.setGradientFill (body); g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
        for (int i = 0; i < 32; ++i)                            // fluted rim
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 32.0f;
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.drawLine (cx + std::sin (a) * kr * 0.88f, cy - std::cos (a) * kr * 0.88f, cx + std::sin (a) * kr * 0.99f, cy - std::cos (a) * kr * 0.99f, 1.1f);
        }
        const float cr = kr * 0.72f;
        juce::ColourGradient cap (juce::Colour (0xffe8e8e6), cx - cr * 0.7f, cy - cr * 0.8f, juce::Colour (0xff86888a), cx + cr * 0.7f, cy + cr * 0.8f, false);
        g.setGradientFill (cap); g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f, 1.0f);
        g.setColour (hot ? C (t.accent) : C (0xff1b1917));
        g.drawLine (cx + std::sin (ang) * cr * 0.25f, cy - std::cos (ang) * cr * 0.25f, cx + std::sin (ang) * cr * 0.92f, cy - std::cos (ang) * cr * 0.92f, 2.6f);
    }

    // ---------------------------------------------------------------- style 2: ring of hearts around a cherry
    void knobHearts (juce::Graphics& g, float cx, float cy, float R, float pos, bool bi, bool hot, float ang)
    {
        const auto& t = th();
        if (hot)
        {
            juce::ColourGradient gl (C (t.glow).withAlpha (0.6f), cx, cy, C (t.glow).withAlpha (0.0f), cx, cy - R, true);
            g.setGradientFill (gl); g.fillEllipse (cx - R, cy - R, R * 2.0f, R * 2.0f);
        }
        const int N = 10;
        for (int i = 0; i < N; ++i)
        {
            const float q = (float) i / (float) (N - 1);
            const float a = -2.4f + q * 4.8f;
            const bool lit = bi ? (pos >= 0.5f ? (q >= 0.5f - 0.01f && q <= pos + 0.02f && pos > 0.52f) : (q <= 0.5f + 0.01f && q >= pos - 0.02f && pos < 0.48f))
                                : (q < pos + 0.0001f && pos > 0.0001f);
            const float hx = cx + std::sin (a) * R * 0.79f, hy = cy - std::cos (a) * R * 0.79f;
            if (lit) glossHeart (g, hx, hy, R * (0.165f + 0.008f * std::sin (animTime() * 3.0f + (float) i * 0.7f)), C (t.accent), C (t.ink), 0.0f, 1.0f);
            else
            {
                g.setColour (C (0xffffd9e2));
                g.fillPath (heartShape (hx, hy, R * 0.15f));
                g.setColour (C (t.ink).withAlpha (0.75f));
                g.strokePath (heartShape (hx, hy, R * 0.15f), juce::PathStrokeType (0.9f));
            }
        }
        const float cr = R * 0.50f;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (cx - cr, cy - cr + 2.0f, cr * 2.0f, cr * 2.0f);
        juce::ColourGradient sph (C (t.accent).brighter (0.7f), cx - cr * 0.5f, cy - cr * 0.6f, C (t.accent).darker (0.7f), cx + cr * 0.8f, cy + cr * 0.9f, true);
        g.setGradientFill (sph); g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (C (t.ink)); g.drawEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f, 1.8f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (cx - cr * 0.62f, cy - cr * 0.72f, cr * 0.5f, cr * 0.3f);
        g.setColour (juce::Colours::white);
        g.drawLine (cx + std::sin (ang) * cr * 0.2f, cy - std::cos (ang) * cr * 0.2f, cx + std::sin (ang) * cr * 0.8f, cy - std::cos (ang) * cr * 0.8f, 3.0f);
    }

    int index = 0;
};
}
