#include "PluginEditor.h"
#include <BinaryData.h>

using namespace rmz;

namespace
{
constexpr float kPanelX = 30.0f;        // the controls live in a 1000 px panel inside the 1060 px frame
constexpr float kFrameW = 1060.0f;

struct Card { juce::Rectangle<int> r; juce::String title, sub; float tilt; };
struct Hint { juce::Rectangle<int> r; juce::String text; bool caps; };

const std::vector<Card>& cardList()
{
    static const std::vector<Card> c = {
        { { 14, 80, 316, 310 }, "Voice", "closeness & lens", -0.12f },
        { { 342, 80, 316, 310 }, "Mood", "distance x age", 0.10f },
        { { 670, 80, 316, 310 }, "Echoes", "twin & tape echo", -0.08f },
        { { 14, 402, 316, 224 }, "Tape", "wobble, hiss, dust", 0.09f },
        { { 342, 402, 316, 224 }, "Haze", "bloom & petals", -0.10f },
        { { 670, 402, 316, 224 }, "Gloss", "sparkle & width", 0.11f } };
    return c;
}
const std::vector<Hint>& hintList()
{
    static const std::vector<Hint> h = {
        { { 24, 230, 296, 16 }, "Lens", true },
        { { 26, 344, 292, 40 }, "Closeness leans you into the mic. Phone squeezes the voice into a pocket recorder.", false },
        { { 680, 354, 290, 30 }, "Echo time follows the tempo of your project.", false },
        { { 352, 566, 298, 56 }, "Bloom: the haze ducks while you sing and swells in the gaps. Petals: a veil an octave up.", false },
        { { 680, 556, 290, 66 }, "Gloss adds sparkle on top. Width blooms the stereo image. Drift is how far the twin voice wanders.", false } };
    return h;
}

// black swirly vines with curls, like the embroidery reference
void drawVines (juce::Graphics& g, bool right)
{
    g.saveState();
    if (right) g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (kFrameW, 0.0f));
    const juce::Colour black (0xff140609);
    juce::Path v;
    v.startNewSubPath (22.0f, 92.0f);
    for (float y = 92.0f; y <= 618.0f; y += 5.0f) v.lineTo (22.0f + 9.0f * std::sin ((y - 92.0f) / 31.0f), y);
    g.setColour (black);
    g.strokePath (v, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto curl = [&] (float vy, float sgn, float r0)
    {
        const float vx = 22.0f + 9.0f * std::sin ((vy - 92.0f) / 31.0f);
        const float cx = vx + sgn * r0, cy = vy;
        juce::Path c;
        bool first = true;
        for (float a = 0.0f; a < 11.5f; a += 0.18f)
        {
            const float r = r0 * (1.0f - a / 13.5f);
            const float px = cx - sgn * r * std::cos (a), py = cy + sgn * r * std::sin (a);
            if (first) { c.startNewSubPath (px, py); first = false; } else c.lineTo (px, py);
        }
        g.strokePath (c, juce::PathStrokeType (2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    curl (146.0f, 1.0f, 12.0f); curl (252.0f, -1.0f, 9.0f); curl (352.0f, 1.0f, 13.0f);
    curl (462.0f, -1.0f, 10.0f); curl (566.0f, 1.0f, 12.0f);
    g.restoreState();
}

void drawHeartClusters (juce::Graphics& g, bool right, const Theme& t)
{
    g.saveState();
    if (right) g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (kFrameW, 0.0f));
    glossHeart (g, 26.0f, 204.0f, 17.0f, C (t.accent), C (0xff140609), -0.38f, 1.8f);
    glossHeart (g, 12.0f, 262.0f, 11.0f, C (t.accent), C (0xff140609), 0.32f, 1.6f);
    glossHeart (g, 30.0f, 298.0f, 8.0f,  C (t.accent2), C (0xff140609), 0.12f, 1.5f);
    glossHeart (g, 24.0f, 474.0f, 16.0f, C (t.accent), C (0xff140609), 0.34f, 1.8f);
    glossHeart (g, 10.0f, 530.0f, 10.0f, C (t.accent2), C (0xff140609), -0.3f, 1.6f);
    glossHeart (g, 31.0f, 566.0f, 7.5f,  C (t.accent), C (0xff140609), 0.1f, 1.5f);
    g.restoreState();
}

// ============================================================================
//  Static backdrop: wall, paper / metal / candy cards, header, ornaments
// ============================================================================
class Backdrop : public juce::Component
{
public:
    explicit Backdrop (RmzLookAndFeel& l) : lf (l)
    {
        setInterceptsMouseClicks (false, false);
        setBufferedToImage (true);
        auto img = juce::ImageFileFormat::loadFrom (BinaryData::logo_png, (size_t) BinaryData::logo_pngSize);
        logo = img.convertedToFormat (juce::Image::SingleChannel);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = lf.th();
        const int st = t.style;
        const auto b = getLocalBounds().toFloat();
        paintWall (g, t, st, b);
        if (st == 2) { drawVines (g, false); drawVines (g, true); }

        g.saveState();
        g.addTransform (juce::AffineTransform::translation (kPanelX, 0.0f));
        paintHeader (g, t, st);
        for (const auto& c : cardList()) paintCard (g, t, st, c);
        paintFrame (g, t, st);
        for (const auto& h : hintList()) paintHint (g, t, st, h);
        g.restoreState();

        if (st == 2) { drawHeartClusters (g, false, t); drawHeartClusters (g, true, t); }
    }

private:
    void paintWall (juce::Graphics& g, const Theme& t, int st, juce::Rectangle<float> b)
    {
        if (st == 1)            // dark brushed case
        {
            juce::ColourGradient gr (C (0xff3a3531), 0.0f, 0.0f, C (0xff1d1a18), 0.0f, b.getHeight(), false);
            g.setGradientFill (gr); g.fillRect (b);
            for (float y = 0; y < b.getHeight(); y += 2.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (((int) y / 2) % 3 == 0 ? 0.035f : 0.012f));
                g.fillRect (0.0f, y, b.getWidth(), 1.0f);
            }
            return;
        }
        if (st == 2)            // juicy cherry red
        {
            juce::ColourGradient gr (C (0xffff2d52), b.getCentreX(), b.getCentreY() - 40.0f, C (0xff5a0517), 0.0f, 0.0f, true);
            gr.addColour (0.55, C (0xffc8102e));
            g.setGradientFill (gr); g.fillRect (b);
            juce::Path streak;
            streak.startNewSubPath (0.0f, 0.0f); streak.lineTo (560.0f, 0.0f); streak.lineTo (0.0f, 470.0f); streak.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillPath (streak);
            juce::Path streak2;
            streak2.startNewSubPath (620.0f, 0.0f); streak2.lineTo (700.0f, 0.0f); streak2.lineTo (0.0f, 570.0f); streak2.lineTo (0.0f, 520.0f); streak2.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillPath (streak2);
            int row = 0;
            for (float y = 18.0f; y < b.getHeight(); y += 46.0f, ++row)
                for (float x = (row % 2 ? 24.0f : 0.0f); x < b.getWidth(); x += 48.0f)
                { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillPath (heartShape (x, y, 4.6f)); }
            return;
        }
        g.fillAll (C (t.bg));
        g.setColour (C (t.tile).withAlpha (0.55f));
        for (float x = 0; x < b.getWidth(); x += 64.0f) g.fillRect (x, 0.0f, 2.0f, b.getHeight());
        for (float y = 0; y < b.getHeight(); y += 64.0f) g.fillRect (0.0f, y, b.getWidth(), 2.0f);
        auto blot = [&] (float cx, float cy, float rad, juce::Colour c)
        {
            juce::ColourGradient gr (c, cx, cy, c.withAlpha (0.0f), cx + rad, cy, true);
            g.setGradientFill (gr); g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        };
        blot (150.0f, 90.0f, 420.0f, C (t.accent).withAlpha (t.dark ? 0.20f : 0.22f));
        blot (920.0f, 560.0f, 400.0f, C (t.accent2).withAlpha (t.dark ? 0.16f : 0.22f));
        blot (540.0f, 320.0f, 560.0f, C (t.paper).withAlpha (t.dark ? 0.04f : 0.40f));
    }

    void paintHeader (juce::Graphics& g, const Theme& t, int st)
    {
        g.setColour (C (t.head));
        if (logo.isValid())
            g.drawImage (logo, juce::Rectangle<float> (20.0f, 14.0f, 46.0f, 30.0f), juce::RectanglePlacement::centred, true);
        const float bx = 43.0f, by = 36.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 5.0f;
            g.setColour (st == 2 ? C (0xffffc2d0) : C (t.accent));
            g.fillEllipse (bx + std::sin (a) * 3.6f - 2.6f, by - std::cos (a) * 3.6f - 2.6f, 5.2f, 5.2f);
        }
        g.setColour (st == 2 ? C (0xffe5102f) : C (t.paper));
        g.fillEllipse (bx - 1.8f, by - 1.8f, 3.6f, 3.6f);

        g.setFont (st == 1 ? sansFont (28.0f, true).withExtraKerningFactor (0.02f) : serifFont (32.0f));
        if (st == 2)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawText ("Romanticizm", 80, 6, 250, 38, juce::Justification::centredLeft, false);
        }
        g.setColour (C (t.head));
        g.drawText ("Romanticizm", 78, 4, 250, 38, juce::Justification::centredLeft, false);
        g.setColour (C (t.head).withAlpha (0.7f));
        g.setFont (sansFont (10.0f).withExtraKerningFactor (0.12f));
        g.drawText (st == 1 ? "STEREO VOCAL CASSETTE DECK" : "VOCAL BLOSSOM FOR LAMP-LIT NIGHTS", 80, 42, 250, 14, juce::Justification::centredLeft, false);
    }

    void paintCard (juce::Graphics& g, const Theme& t, int st, const Card& c)
    {
        const auto r = c.r.toFloat();
        if (st == 1)
        {
            juce::Path p; p.addRoundedRectangle (r, 9.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 14, { 0, 5 }).drawForPath (g, p);
            juce::ColourGradient gr (C (0xffe3e4e0), 0.0f, r.getY(), C (0xffb6b8b5), 0.0f, r.getBottom(), false);
            g.setGradientFill (gr); g.fillPath (p);
            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.drawLine (r.getX() + 10.0f, r.getY() + 1.5f, r.getRight() - 10.0f, r.getY() + 1.5f, 1.2f);
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.strokePath (p, juce::PathStrokeType (1.3f));
            const float sx[2] = { r.getX() + 11.0f, r.getRight() - 11.0f }, sy[2] = { r.getY() + 11.0f, r.getBottom() - 11.0f };
            for (int a = 0; a < 2; ++a)
                for (int bb = 0; bb < 2; ++bb)
                {
                    juce::ColourGradient sg (C (0xfff4f4f2), sx[a] - 2.0f, sy[bb] - 2.0f, C (0xff6e7073), sx[a] + 3.0f, sy[bb] + 3.0f, false);
                    g.setGradientFill (sg); g.fillEllipse (sx[a] - 3.6f, sy[bb] - 3.6f, 7.2f, 7.2f);
                    g.setColour (juce::Colours::black.withAlpha (0.6f));
                    const float ang = 0.7f + (float) (a * 2 + bb);
                    g.drawLine (sx[a] - std::cos (ang) * 2.8f, sy[bb] - std::sin (ang) * 2.8f, sx[a] + std::cos (ang) * 2.8f, sy[bb] + std::sin (ang) * 2.8f, 1.0f);
                }
            g.setColour (C (t.ink));
            g.setFont (sansFont (13.5f, true).withExtraKerningFactor (0.16f));
            g.drawText (c.title.toUpperCase(), c.r.getX() + 24, c.r.getY() + 9, 170, 18, juce::Justification::centredLeft, false);
            const juce::uint32 stripe[4] = { 0xffff6a1a, 0xffffc23a, 0xffd8402a, 0xff2d9c8f };
            for (int i = 0; i < 4; ++i) { g.setColour (C (stripe[i])); g.fillRect (c.r.getX() + 24, c.r.getY() + 28 + i, 64, 1); }
            g.setColour (C (t.ink).withAlpha (0.55f));
            g.setFont (sansFont (9.0f).withExtraKerningFactor (0.1f));
            g.drawText (c.sub.toUpperCase(), c.r.getRight() - 190, c.r.getY() + 10, 168, 16, juce::Justification::centredRight, false);
            return;
        }
        if (st == 2)
        {
            juce::Path p; p.addRoundedRectangle (r, 20.0f);
            juce::DropShadow (C (0xff3a0310).withAlpha (0.7f), 16, { 0, 7 }).drawForPath (g, p);
            juce::ColourGradient gr (C (t.card), 0.0f, r.getY(), C (0xffffd6e0), 0.0f, r.getBottom(), false);
            g.setGradientFill (gr); g.fillPath (p);
            g.setColour (C (t.accent).withAlpha (0.55f));
            g.drawRoundedRectangle (r.reduced (5.0f), 16.0f, 1.2f);
            g.setColour (C (t.ink));
            g.strokePath (p, juce::PathStrokeType (2.4f));
            glossHeart (g, r.getX() + 38.0f, r.getY() + 3.0f, 11.0f, C (t.accent), C (t.ink), -0.3f, 1.6f);
            g.setColour (C (t.ink));
            g.setFont (serifFont (22.0f).withStyle (juce::Font::bold | juce::Font::italic));
            g.drawText (c.title, c.r.getX() + 60, c.r.getY() + 5, 150, 26, juce::Justification::centredLeft, false);
            g.setColour (C (t.accent));
            g.setFont (sansFont (9.5f, true).withExtraKerningFactor (0.1f));
            g.drawText (c.sub.toUpperCase(), c.r.getRight() - 172, c.r.getY() + 10, 154, 18, juce::Justification::centredRight, false);
            return;
        }
        juce::Path p; p.addRoundedRectangle (r, 18.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (t.dark ? 0.45f : 0.20f), 16, { 0, 6 }).drawForPath (g, p);
        g.setColour (C (t.card).withAlpha (t.dark ? 0.86f : 0.84f));
        g.fillPath (p);
        g.setColour (C (t.ink).withAlpha (0.22f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (c.tilt, r.getX() + 40.0f, r.getY() + 3.0f));
        g.setColour (C (t.accent).withAlpha (0.38f));
        g.fillRect (r.getX() + 16.0f, r.getY() - 6.0f, 54.0f, 16.0f);
        g.restoreState();
        g.setColour (C (t.ink));
        g.setFont (serifFont (21.0f));
        g.drawText (c.title, c.r.getX() + 84, c.r.getY() + 5, 150, 26, juce::Justification::centredLeft, false);
        g.setColour (C (t.ink).withAlpha (0.55f));
        g.setFont (sansFont (9.5f).withExtraKerningFactor (0.1f));
        g.drawText (c.sub.toUpperCase(), c.r.getRight() - 170, c.r.getY() + 10, 154, 18, juce::Justification::centredRight, false);
    }

    void paintFrame (juce::Graphics& g, const Theme& t, int st)
    {
        auto pr = juce::Rectangle<float> (348.0f, 110.0f, 304.0f, 274.0f);
        juce::Path p;
        if (st == 1)
        {
            p.addRoundedRectangle (pr, 9.0f);
            g.setColour (C (0xff15110f)); g.fillPath (p);
            g.setColour (juce::Colours::white.withAlpha (0.14f)); g.strokePath (p, juce::PathStrokeType (1.2f));
            g.setColour (juce::Colours::black); g.drawRoundedRectangle (pr.expanded (1.0f), 10.0f, 1.2f);
            return;
        }
        if (st == 2)
        {
            p.addRoundedRectangle (pr, 12.0f);
            juce::DropShadow (C (0xff3a0310).withAlpha (0.5f), 8, { 0, 3 }).drawForPath (g, p);
            g.setColour (juce::Colours::white); g.fillPath (p);
            g.setColour (C (t.accent).withAlpha (0.6f)); g.drawRoundedRectangle (pr.reduced (4.0f), 9.0f, 1.2f);
            g.setColour (C (t.ink)); g.strokePath (p, juce::PathStrokeType (2.2f));
            return;
        }
        p.addRoundedRectangle (pr, 6.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.22f), 8, { 0, 3 }).drawForPath (g, p);
        g.setColour (C (t.paper)); g.fillPath (p);
        g.setColour (C (t.ink).withAlpha (0.2f)); g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    void paintHint (juce::Graphics& g, const Theme& t, int st, const Hint& h)
    {
        g.setColour (C (t.ink).withAlpha (0.66f));
        if (h.caps)
        {
            g.setFont (sansFont (9.5f, true).withExtraKerningFactor (0.14f));
            g.drawText (h.text.toUpperCase(), h.r, juce::Justification::centredLeft, false);
            g.setColour (C (t.ink).withAlpha (0.2f));
            g.fillRect (h.r.getX() + 44, h.r.getCentreY(), h.r.getWidth() - 44, 1);
        }
        else
        {
            g.setFont (st == 1 ? sansFont (10.5f) : serifFont (12.5f));
            g.drawFittedText (h.text, h.r, juce::Justification::topLeft, 3, 1.0f);
        }
    }

    RmzLookAndFeel& lf;
    juce::Image logo;
};

// ============================================================================
//  Film grain, vignette, light leak, VHS scan-lines (static, on top of all)
// ============================================================================
class GrainOverlay : public juce::Component
{
public:
    explicit GrainOverlay (RmzLookAndFeel& l) : lf (l)
    {
        setInterceptsMouseClicks (false, false);
        setBufferedToImage (true);
        noise = juce::Image (juce::Image::ARGB, 256, 256, true);
        juce::Random rnd (1234);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                const bool light = rnd.nextBool();
                noise.setPixelAt (x, y, juce::Colour::fromRGBA (light ? 255 : 0, light ? 255 : 0, light ? 255 : 0, (juce::uint8) rnd.nextInt (34)));
            }
    }
    void paint (juce::Graphics& g) override
    {
        const auto& t = lf.th();
        const auto b = getLocalBounds().toFloat();
        g.setOpacity (t.grain);
        for (int y = 0; y < getHeight(); y += 256)
            for (int x = 0; x < getWidth(); x += 256)
                g.drawImageAt (noise, x, y);
        if (t.style == 0)
        {
            juce::ColourGradient leak (C (t.glow).withAlpha (t.dark ? 0.16f : 0.26f), b.getRight(), 0.0f, C (t.glow).withAlpha (0.0f), b.getRight() - 420.0f, 200.0f, true);
            g.setGradientFill (leak); g.fillRect (b);
        }
        const auto vc = t.style == 2 ? C (0xff3a0310) : (t.dark || t.style == 1 ? juce::Colours::black : C (t.ink));
        juce::ColourGradient vig (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(), vc.withAlpha (t.style == 2 ? 0.38f : (t.dark || t.style == 1 ? 0.42f : 0.22f)), 0.0f, 0.0f, true);
        g.setGradientFill (vig); g.fillRect (b);
        if (t.scanlines)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            for (int y = 0; y < getHeight(); y += 3) g.fillRect (0, y, getWidth(), 1);
        }
    }
private:
    RmzLookAndFeel& lf;
    juce::Image noise;
};

// ============================================================================
//  Chips bound to a choice parameter
// ============================================================================
class ChoiceChips : public juce::Component
{
public:
    ChoiceChips (juce::AudioProcessorValueTreeState& s, const char* id, RmzLookAndFeel& l, const juce::StringArray& n) : lf (l), names (n)
    {
        param = dynamic_cast<juce::RangedAudioParameter*> (s.getParameter (id));
        attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { idx = (int) (v + 0.5f); repaint(); });
        attach->sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& t = lf.th();
        const float w = (float) getWidth() / (float) names.size();
        for (int i = 0; i < names.size(); ++i)
        {
            auto r = juce::Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (2.0f, 1.0f);
            const bool on = i == idx;
            g.setColour (on ? C (t.accent) : C (t.ink).withAlpha (0.07f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (C (t.ink).withAlpha (0.28f));
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            g.setColour (on ? C (t.paper) : C (t.ink));
            g.setFont (sansFont (10.5f, true));
            g.drawText (names[i], r, juce::Justification::centred, false);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, names.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) names.size())));
        attach->beginGesture(); attach->setValueAsPartOfGesture ((float) i); attach->endGesture();
    }
private:
    RmzLookAndFeel& lf;
    juce::StringArray names;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
    int idx = 0;
};

// ============================================================================
//  MOOD stage: glowing waveform + falling bits + the lamp you drag
//  (Cherry Hearts: an animated pair of legs dances behind the scope)
// ============================================================================
class MoodStage : public juce::Component, private juce::Timer
{
public:
    MoodStage (RomanticizmProcessor& p, RmzLookAndFeel& l) : proc (p), lf (l)
    {
        px = p.apvts.getParameter (pid::moodX); py = p.apvts.getParameter (pid::moodY);
        juce::Random r (77);
        for (auto& pt : petals)
        {
            pt.x = r.nextFloat() * 296.0f; pt.y = r.nextFloat() * 214.0f;
            pt.rot = r.nextFloat() * 6.28f; pt.vr = (r.nextFloat() - 0.5f) * 2.0f;
            pt.vy = 14.0f + r.nextFloat() * 26.0f; pt.sw = 8.0f + r.nextFloat() * 18.0f;
            pt.ph = r.nextFloat() * 6.28f; pt.sz = 3.2f + r.nextFloat() * 3.6f;
        }
        bx = px->getValue(); by = py->getValue();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = lf.th();
        const int st = t.style;
        const auto b = getLocalBounds().toFloat();
        juce::Path clip; clip.addRoundedRectangle (b, st == 2 ? 8.0f : 4.0f);
        g.saveState();
        g.reduceClipRegion (clip);

        // --- background
        if (st == 1)
        {
            juce::ColourGradient bg (C (0xff2c2118), 0.0f, 0.0f, C (0xff0c0907), 0.0f, b.getHeight(), false);
            g.setGradientFill (bg); g.fillRect (b);
            const float sy = std::fmod (phase * 24.0f, b.getHeight() + 40.0f) - 30.0f;
            juce::ColourGradient band (C (t.accent2).withAlpha (0.0f), 0.0f, sy, C (t.accent2).withAlpha (0.09f), 0.0f, sy + 14.0f, false);
            band.addColour (1.0, C (t.accent2).withAlpha (0.0f));
            g.setGradientFill (band); g.fillRect (0.0f, sy, b.getWidth(), 28.0f);
            g.setColour (C (t.accent2).withAlpha (0.10f));
        }
        else if (st == 2)
        {
            juce::ColourGradient bg (C (0xffff5c93), 0.0f, 0.0f, C (0xffd10a2c), 0.0f, b.getHeight(), false);
            g.setGradientFill (bg); g.fillRect (b);
            juce::ColourGradient hl (juce::Colours::white.withAlpha (0.22f), b.getCentreX(), b.getCentreY(), juce::Colours::transparentWhite, b.getCentreX(), 0.0f, true);
            g.setGradientFill (hl); g.fillRect (b);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
        }
        else
        {
            juce::ColourGradient bg (C (t.paper).interpolatedWith (C (t.accent), 0.10f), 0.0f, 0.0f,
                                     C (t.accent2).withAlpha (0.50f).interpolatedWith (C (t.paper), 0.45f), 0.0f, b.getHeight(), false);
            g.setGradientFill (bg); g.fillRect (b);
            g.setColour (C (t.ink).withAlpha (0.07f));
        }
        for (float x = 0; x < b.getWidth(); x += 37.0f) g.fillRect (x, 0.0f, 1.0f, b.getHeight());
        for (float y = 0; y < b.getHeight(); y += 37.0f) g.fillRect (0.0f, y, b.getWidth(), 1.0f);

        // --- dancing legs
        if (st == 2)
        {
            const auto& sheet = legsSheet();
            if (sheet.isValid())
            {
                const float dh = b.getHeight() * 1.02f, dw = dh;
                g.setOpacity (1.0f);
                g.drawImage (sheet, juce::Rectangle<float> (b.getCentreX() - dw * 0.5f, b.getBottom() - dh + 4.0f, dw, dh),
                             juce::RectanglePlacement::stretchToFit, false);
            }
        }

        // --- waveform
        const float mid = b.getHeight() * 0.5f;
        const int cols = (int) (b.getWidth() / 2.0f);
        const int span = 2048, bucket = span / cols;
        const int pos = proc.scopePos.load();
        juce::Path wave;
        for (int c = 0; c < cols; ++c)
        {
            float mn = 0, mx = 0;
            for (int k = 0; k < bucket; ++k)
            {
                const float v = proc.scope[(size_t) (((pos - span + c * bucket + k) % RomanticizmProcessor::scopeSize + RomanticizmProcessor::scopeSize) % RomanticizmProcessor::scopeSize)];
                mn = std::min (mn, v); mx = std::max (mx, v);
            }
            const float x = (float) c * 2.0f + 1.0f;
            wave.startNewSubPath (x, mid - std::tanh (mx * 1.5f) * b.getHeight() * 0.38f - 0.5f);
            wave.lineTo (x, mid - std::tanh (mn * 1.5f) * b.getHeight() * 0.38f + 0.5f);
        }
        if (st == 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.strokePath (wave, juce::PathStrokeType (4.5f));
            g.setColour (C (t.ink)); g.strokePath (wave, juce::PathStrokeType (1.5f));
        }
        else
        {
            g.setColour ((st == 1 ? C (t.accent) : C (t.glow)).withAlpha (0.35f)); g.strokePath (wave, juce::PathStrokeType (5.0f));
            g.setColour (st == 1 ? C (t.accent2) : C (t.accent)); g.strokePath (wave, juce::PathStrokeType (1.6f));
        }

        // --- falling things
        int idx = 0;
        for (const auto& pt : petals)
        {
            ++idx;
            if (st == 2)
            {
                g.setColour ((idx % 2 ? juce::Colours::white : C (0xffffb0c6)).withAlpha (0.9f));
                g.fillPath (heartShape (pt.x, pt.y, pt.sz * 0.95f, pt.rot * 0.3f));
            }
            else if (st == 1)
            {
                g.setColour (C (t.accent2).withAlpha (0.55f));
                g.fillRect (pt.x, pt.y, pt.sz * 0.7f, pt.sz * 0.7f);
            }
            else
            {
                juce::Path p; p.addEllipse (-pt.sz * 0.55f, -pt.sz, pt.sz * 1.1f, pt.sz * 2.0f);
                g.setColour ((t.dark ? C (t.glow) : C (t.accent)).withAlpha (0.78f));
                g.fillPath (p, juce::AffineTransform::rotation (pt.rot).translated (pt.x, pt.y));
            }
        }

        // --- labels
        g.setColour (st == 1 ? C (t.accent2).withAlpha (0.8f) : (st == 2 ? C (t.ink).withAlpha (0.85f) : C (t.ink).withAlpha (0.5f)));
        g.setFont (sansFont (9.5f, true).withExtraKerningFactor (0.14f));
        g.drawText ("NEAR", 8, (int) mid - 7, 50, 14, juce::Justification::left, false);
        g.drawText ("FAR", (int) b.getWidth() - 58, (int) mid - 7, 50, 14, juce::Justification::right, false);
        g.drawText ("WORN", 0, 6, (int) b.getWidth(), 14, juce::Justification::centred, false);
        g.drawText ("FRESH", 0, (int) b.getHeight() - 20, (int) b.getWidth(), 14, juce::Justification::centred, false);

        // --- the lamp
        const float lx = bx * b.getWidth(), ly = (1.0f - by) * b.getHeight();
        const float flick = 0.92f + 0.08f * std::sin (phase * 7.3f) * std::sin (phase * 2.9f);
        juce::ColourGradient gl (C (t.glow).withAlpha (0.85f * flick), lx, ly, C (t.glow).withAlpha (0.0f), lx + 84.0f, ly, true);
        g.setGradientFill (gl); g.fillEllipse (lx - 84.0f, ly - 84.0f, 168.0f, 168.0f);
        if (st == 2)
        {
            glossHeart (g, lx, ly, 12.0f, C (0xffffe4ea), C (t.ink), 0.0f, 1.8f);
        }
        else
        {
            g.setColour (st == 1 ? C (0xfffff0c8) : C (t.paper));
            g.fillEllipse (lx - 9.0f, ly - 9.0f, 18.0f, 18.0f);
            g.setColour (C (t.accent));
            g.drawEllipse (lx - 9.0f, ly - 9.0f, 18.0f, 18.0f, 2.0f);
            if (st == 0)
            {
                g.setColour (C (t.ink).withAlpha (0.8f));
                g.drawLine (lx - 3.0f, ly + 2.0f, lx, ly - 3.0f, 1.0f); g.drawLine (lx, ly - 3.0f, lx + 3.0f, ly + 2.0f, 1.0f);
            }
        }
        g.restoreState();

        g.setColour (st == 2 ? C (t.ink).withAlpha (0.8f) : (st == 1 ? juce::Colours::black : C (t.ink).withAlpha (0.25f)));
        g.drawRoundedRectangle (b.reduced (0.5f), st == 2 ? 8.0f : 4.0f, st == 2 ? 1.6f : 1.0f);
    }

    /** a soft flurry of petals / hearts when a preset is loaded */
    void burst()
    {
        boost = 1.0f;
        for (size_t i = 0; i < petals.size(); i += 2) petals[i].y = -juce::Random::getSystemRandom().nextFloat() * (float) getHeight();
    }

    void mouseDown (const juce::MouseEvent& e) override { px->beginChangeGesture(); py->beginChangeGesture(); set (e); }
    void mouseDrag (const juce::MouseEvent& e) override { set (e); }
    void mouseUp (const juce::MouseEvent&) override { px->endChangeGesture(); py->endChangeGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        px->beginChangeGesture(); py->beginChangeGesture();
        px->setValueNotifyingHost (px->getDefaultValue()); py->setValueNotifyingHost (py->getDefaultValue());
        px->endChangeGesture(); py->endChangeGesture();
    }

private:
    struct Petal { float x, y, rot, vr, vy, sw, ph, sz; };

    // 30 frames, 6 x 5 cells of 240 px; frames are cut out once into separate images
    const juce::Image& legsSheet()
    {
        if (! legsReady)
        {
            legsReady = true;
            auto sheet = juce::ImageFileFormat::loadFrom (BinaryData::legs_png, (size_t) BinaryData::legs_pngSize);
            for (int i = 0; i < 30; ++i)
                legsFrames[(size_t) i] = sheet.getClippedImage ({ (i % 6) * 240, (i / 6) * 240, 240, 240 });
        }
        return legsFrames[(size_t) frame];
    }

    void set (const juce::MouseEvent& e)
    {
        px->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth()));
        py->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.y / (float) getHeight()));
    }
    void timerCallback() override
    {
        const float dt = 1.0f / 30.0f;
        phase += dt;
        if (lf.style() == 2 && (++tick & 1) == 0) frame = (frame + 1) % 30;
        const float w = (float) getWidth(), h = (float) getHeight();
        for (auto& p : petals)
        {
            p.y += p.vy * (1.0f + boost * 2.4f) * dt; p.x += std::sin (phase * 0.9f + p.ph) * p.sw * dt; p.rot += p.vr * dt;
            if (p.y > h + 10.0f) { p.y = -10.0f; p.x = juce::Random::getSystemRandom().nextFloat() * w; }
        }
        boost *= 0.95f;
        bx += (px->getValue() - bx) * 0.35f; by += (py->getValue() - by) * 0.35f;
        repaint();
    }

    RomanticizmProcessor& proc;
    RmzLookAndFeel& lf;
    juce::RangedAudioParameter *px = nullptr, *py = nullptr;
    std::array<Petal, 16> petals {};
    std::array<juce::Image, 30> legsFrames;
    bool legsReady = false;
    int frame = 0, tick = 0;
    float bx = 0.5f, by = 0.5f, phase = 0.0f, boost = 0.0f;
};

// ============================================================================
//  Level indicator: tube "magic eye" / VU meter / heart that fills up
// ============================================================================
class MagicEye : public juce::Component, private juce::Timer
{
public:
    MagicEye (RomanticizmProcessor& p, RmzLookAndFeel& l) : proc (p), lf (l) { startTimerHz (24); }
    void paint (juce::Graphics& g) override
    {
        const auto& t = lf.th();
        const auto b = getLocalBounds().toFloat().reduced (3.0f);
        if (t.style == 1) { paintVU (g, b, t); return; }
        if (t.style == 2) { paintHeart (g, b, t); return; }
        const auto c = b.getCentre();
        const float R = b.getWidth() * 0.5f;
        g.setColour (juce::Colour (0xff1b1b1e));
        g.fillEllipse (b.expanded (2.0f));
        juce::ColourGradient gr (juce::Colour (0xffb8ffb0), c.x, c.y, juce::Colour (0xff1e8f3e), c.x + R, c.y, true);
        g.setGradientFill (gr);
        g.fillEllipse (b.reduced (2.0f));
        const float open = juce::jlimit (0.0f, 1.0f, 1.0f - std::sqrt (level) * 0.95f);
        const float a = 0.12f + open * 1.25f;
        juce::Path wedge;
        wedge.addPieSegment (b.reduced (2.0f), juce::MathConstants<float>::pi - a, juce::MathConstants<float>::pi + a, 0.0f);
        g.setColour (juce::Colour (0xff0b2a12).withAlpha (0.92f));
        g.fillPath (wedge);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (b.getX() + R * 0.35f, b.getY() + R * 0.2f, R * 0.7f, R * 0.35f);
    }
private:
    void paintVU (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t)
    {
        g.setColour (juce::Colour (0xff14110f)); g.fillRoundedRectangle (b.expanded (2.0f), 5.0f);
        auto face = b.reduced (2.0f);
        juce::ColourGradient fg (C (0xfffff3d2), face.getCentreX(), face.getBottom(), C (t.accent2).withAlpha (0.8f), face.getCentreX(), face.getY(), false);
        g.setGradientFill (fg); g.fillRoundedRectangle (face, 3.0f);
        const float px = face.getCentreX(), py = face.getBottom() - 4.0f, L = face.getHeight() * 0.80f;
        juce::Path arc; arc.addCentredArc (px, py, L * 0.97f, L * 0.97f, 0.0f, -0.9f, 0.9f, true);
        g.setColour (juce::Colour (0xff1b1917)); g.strokePath (arc, juce::PathStrokeType (1.0f));
        juce::Path red; red.addCentredArc (px, py, L * 0.97f, L * 0.97f, 0.0f, 0.45f, 0.9f, true);
        g.setColour (juce::Colour (0xffd8402a)); g.strokePath (red, juce::PathStrokeType (2.4f));
        for (int i = 0; i <= 6; ++i)
        {
            const float a = -0.9f + 1.8f * (float) i / 6.0f;
            g.setColour (juce::Colour (0xff1b1917));
            g.drawLine (px + std::sin (a) * L * 0.97f, py - std::cos (a) * L * 0.97f, px + std::sin (a) * L * 0.84f, py - std::cos (a) * L * 0.84f, 1.0f);
        }
        const float db = 20.0f * std::log10 (std::max (level, 1.0e-4f));
        const float v = juce::jlimit (0.0f, 1.0f, (db + 30.0f) / 30.0f);
        const float a = -0.9f + 1.8f * v;
        g.setColour (juce::Colour (0xff1b1917));
        g.drawLine (px, py, px + std::sin (a) * L, py - std::cos (a) * L, 1.5f);
        g.fillEllipse (px - 2.5f, py - 2.5f, 5.0f, 5.0f);
    }
    void paintHeart (juce::Graphics& g, juce::Rectangle<float> b, const Theme& t)
    {
        const float cx = b.getCentreX(), cy = b.getCentreY() - 1.0f, s = 22.0f * (1.0f + 0.03f * std::sin (animTime() * 3.2f));
        const auto hp = heartShape (cx, cy, s);
        g.setColour (juce::Colours::white); g.fillPath (hp);
        const float v = juce::jlimit (0.0f, 1.0f, std::sqrt (level));
        const auto hb = hp.getBounds();
        const float top = hb.getBottom() - hb.getHeight() * v;
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (hp);
            juce::ColourGradient gg (C (t.accent).brighter (0.4f), 0.0f, top, C (t.accent).darker (0.4f), 0.0f, hb.getBottom(), false);
            g.setGradientFill (gg);
            g.fillRect (hb.getX(), top, hb.getWidth(), hb.getBottom() - top + 1.0f);
        }
        g.setColour (C (t.ink)); g.strokePath (hp, juce::PathStrokeType (2.0f));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (cx - 0.70f * s, cy - 0.80f * s, 0.4f * s, 0.22f * s);
    }
    void timerCallback() override
    {
        const float pk = proc.outLevel.exchange (0.0f);
        level = std::max (pk, level * 0.88f);
        repaint();
    }
    RomanticizmProcessor& proc;
    RmzLookAndFeel& lf;
    float level = 0.0f;
};

// ============================================================================
//  Little cassette whose reels turn faster / more unevenly with WOBBLE
// ============================================================================
class Reels : public juce::Component, private juce::Timer
{
public:
    Reels (RomanticizmProcessor& p, RmzLookAndFeel& l) : proc (p), lf (l) { startTimerHz (24); }
    void paint (juce::Graphics& g) override
    {
        const auto& t = lf.th();
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (C (t.paper).withAlpha (0.8f));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (C (t.ink).withAlpha (0.35f));
        g.drawRoundedRectangle (b, 10.0f, 1.2f);
        const float cy = b.getCentreY() - 4.0f;
        const float cxs[2] = { b.getX() + b.getWidth() * 0.29f, b.getX() + b.getWidth() * 0.71f };
        const float r = 24.0f;
        // tape between reels
        g.setColour (C (t.ink).withAlpha (0.55f));
        g.drawLine (cxs[0], cy - r - 4.0f, cxs[1], cy - r - 4.0f, 1.6f);
        for (int i = 0; i < 2; ++i)
        {
            g.setColour (C (t.ink).withAlpha (0.14f));
            g.fillEllipse (cxs[i] - r - 4.0f, cy - r - 4.0f, 2.0f * r + 8.0f, 2.0f * r + 8.0f);
            g.setColour (C (t.ink).withAlpha (0.6f));
            g.drawEllipse (cxs[i] - r, cy - r, 2.0f * r, 2.0f * r, 1.4f);
            g.setColour (C (t.paper));
            g.fillEllipse (cxs[i] - 10.0f, cy - 10.0f, 20.0f, 20.0f);
            g.setColour (C (t.accent));
            for (int k = 0; k < 6; ++k)
            {
                const float a = ang * (i == 0 ? 1.0f : 0.8f) + (float) k * 1.0472f;
                g.drawLine (cxs[i] + std::cos (a) * 3.0f, cy + std::sin (a) * 3.0f, cxs[i] + std::cos (a) * 10.0f, cy + std::sin (a) * 10.0f, 1.6f);
            }
        }
        g.setColour (C (t.ink).withAlpha (0.7f));
        g.setFont (serifFont (11.5f));
        g.drawText ("side A  -  romanticizm  -  60", b.withTrimmedTop (b.getHeight() - 16.0f), juce::Justification::centred, false);
    }
private:
    void timerCallback() override
    {
        const float wob = *proc.apvts.getRawParameterValue (pid::wobble) * 0.01f;
        phase += 0.1f;
        ang += 0.12f + wob * 0.10f * std::sin (phase * (1.0f + wob * 3.0f)) + 0.05f * wob;
        repaint();
    }
    RomanticizmProcessor& proc;
    RmzLookAndFeel& lf;
    float ang = 0.0f, phase = 0.0f;
};
} // namespace

// ============================================================================
class RomanticizmEditor::Content : public juce::Component, private juce::Timer
{
public:
    Content (RomanticizmEditor& e, RomanticizmProcessor& p) : editor (e), proc (p), lfRef (e.lf)
    {
        // header
        dial (pid::inGain,  "In",  { 666, 4, 48, 62 }, false, true);
        dial (pid::outGain, "Out", { 716, 4, 48, 62 }, false, true);
        dial (pid::mix,     "Mix", { 766, 4, 48, 62 }, false, false);
        eye = std::make_unique<MagicEye> (proc, lfRef); eye->setBounds (824, 8, 54, 54); addAndMakeVisible (*eye);
        lamp (pid::limiter, "Limit",  { 886, 10, 100, 24 });
        lamp (pid::bypass,  "Bypass", { 886, 38, 100, 24 });

        // voice
        const int vx = 20;
        dial (pid::body,   "Body",     { vx,       114, 98, 112 }, true);
        dial (pid::close,  "Closeness",{ vx + 99,  114, 98, 112 });
        dial (pid::velvet, "Velvet",   { vx + 198, 114, 98, 112 });
        dial (pid::warmth, "Warmth", { vx,       248, 74, 90 }, true);
        dial (pid::bite,   "Bite",   { vx + 74,  248, 74, 90 }, true);
        dial (pid::air,    "Air",    { vx + 148, 248, 74, 90 }, true);
        dial (pid::phone,  "Phone",  { vx + 222, 248, 74, 90 });

        // mood
        stage = std::make_unique<MoodStage> (proc, lfRef);
        stage->setBounds (352, 116, 296, 210); addAndMakeVisible (*stage);
        for (auto* l : { &moodWord, &moodHint }) { l->setJustificationType (juce::Justification::centred); l->setInterceptsMouseClicks (false, false); addAndMakeVisible (*l); }
        moodWord.setBounds (352, 331, 296, 30); moodHint.setBounds (352, 359, 296, 18);
        moodHint.setText ("drag the lamp  -  double-click to centre", juce::dontSendNotification);

        // echoes
        const int ex = 676;
        dial (pid::twin,   "Twin",   { ex,       114, 98, 112 });
        dial (pid::spread, "Spread", { ex + 99,  114, 98, 112 });
        dial (pid::echo,   "Echo",   { ex + 198, 114, 98, 112 });
        ribbon (pid::echoFb,   "Feedback", { 680, 236, 288, 24 });
        ribbon (pid::echoTone, "Tone",     { 680, 264, 288, 24 });
        chipsTime = std::make_unique<ChoiceChips> (proc.apvts, pid::echoTime, lfRef, echoTimeNames());
        chipsMode = std::make_unique<ChoiceChips> (proc.apvts, pid::echoMode, lfRef, echoModeNames());
        chipsTime->setBounds (678, 296, 292, 24); chipsMode->setBounds (678, 326, 292, 24);
        addAndMakeVisible (*chipsTime); addAndMakeVisible (*chipsMode);

        // tape
        dial (pid::drive,  "Drive",  { 20,       436, 74, 92 });
        dial (pid::wobble, "Wobble", { 94,       436, 74, 92 });
        dial (pid::hiss,   "Hiss",   { 168,      436, 74, 92 });
        dial (pid::crunch, "Crunch", { 242,      436, 74, 92 });
        reels = std::make_unique<Reels> (proc, lfRef); reels->setBounds (24, 536, 288, 80); addAndMakeVisible (*reels);

        // haze
        dial (pid::haze, "Haze", { 352, 436, 98, 112 });
        ribbon (pid::size,     "Size",   { 462, 438, 186, 24 });
        ribbon (pid::hazeTone, "Tone",   { 462, 468, 186, 24 });
        ribbon (pid::bloom,    "Bloom",  { 462, 498, 186, 24 });
        ribbon (pid::petals,   "Petals", { 462, 528, 186, 24 });

        // gloss
        dial (pid::gloss, "Gloss", { 676,       436, 98, 112 });
        dial (pid::width, "Width", { 775,       436, 98, 112 });
        dial (pid::drift, "Drift", { 874,       436, 98, 112 });

        // preset / theme
        for (auto* b : { &prev, &next }) { addAndMakeVisible (*b); }
        addAndMakeVisible (presetBtn); addAndMakeVisible (themeBtn);
        prev.setButtonText ("<"); next.setButtonText (">");
        presetBtn.getProperties().set ("serif", true);
        prev.setBounds (342, 10, 28, 26); presetBtn.setBounds (374, 10, 252, 26); next.setBounds (630, 10, 28, 26);
        themeBtn.setBounds (400, 40, 160, 22);
        prev.onClick = [this] { stepPreset (-1); };
        next.onClick = [this] { stepPreset (+1); };
        presetBtn.onClick = [this] { presetMenu(); };
        themeBtn.onClick = [this] { themeMenu(); };

        refreshTexts();
        startTimerHz (12);
    }

    void themeChanged()
    {
        const auto& t = lfRef.th();
        moodWord.setColour (juce::Label::textColourId, t.style == 1 ? C (t.accent2) : C (t.ink));
        moodHint.setColour (juce::Label::textColourId, (t.style == 1 ? C (t.paper) : C (t.ink)).withAlpha (0.6f));
        refreshTexts();
        repaint();
    }

private:
    void dial (const char* id, const char* name, juce::Rectangle<int> r, bool bipolar = false, bool small = false)
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
        s->setName (name);
        s->setRotaryParameters (-2.4f, 2.4f, true);
        s->setMouseDragSensitivity (220);
        if (bipolar) s->getProperties().set ("bi", true);
        if (small) s->getProperties().set ("wall", true);
        finishSlider (std::move (s), id, r);
    }
    void ribbon (const char* id, const char* name, juce::Rectangle<int> r)
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        s->setName (name);
        finishSlider (std::move (s), id, r);
    }
    void finishSlider (std::unique_ptr<juce::Slider> s, const char* id, juce::Rectangle<int> r)
    {
        auto att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s);
        if (auto* p = proc.apvts.getParameter (id))
            s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s->setBounds (r);
        addAndMakeVisible (*s);
        sliders.push_back (std::move (s)); attachments.push_back (std::move (att));
    }
    void lamp (const char* id, const char* name, juce::Rectangle<int> r)
    {
        auto b = std::make_unique<juce::ToggleButton> (name);
        b->getProperties().set ("kind", "lamp");
        b->getProperties().set ("wall", true);
        b->setBounds (r);
        addAndMakeVisible (*b);
        buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *b));
        toggles.push_back (std::move (b));
    }

    void stepPreset (int d)
    {
        const int n = (int) factoryPresets().size();
        proc.setCurrentProgram (((proc.getCurrentProgram() + d) % n + n) % n);
        refreshTexts();
    }

    void presetMenu()
    {
        juce::PopupMenu m;
        const auto& list = factoryPresets();
        for (int i = 0; i < (int) list.size(); ++i)
            m.addItem (i + 1, list[(size_t) i].name, true, i == proc.getCurrentProgram());
        m.addSeparator();
        auto files = RomanticizmProcessor::getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.rmz");
        files.sort();
        for (int i = 0; i < files.size(); ++i) m.addItem (1000 + i, "User: " + files[i].getFileNameWithoutExtension());
        m.addSeparator();
        m.addItem (900, "Save preset...");
        m.addItem (901, "Load preset from file...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn).withStandardItemHeight (24),
            [this, files] (int r)
            {
                if (r <= 0) return;
                if (r < 900) { proc.setCurrentProgram (r - 1); }
                else if (r >= 1000) { proc.loadUserPreset (files[r - 1000]); }
                else if (r == 900)
                {
                    chooser = std::make_unique<juce::FileChooser> ("Save preset", RomanticizmProcessor::getUserPresetDirectory().getChildFile ("My preset.rmz"), "*.rmz");
                    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                        [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) { proc.saveUserPreset (f.withFileExtension ("rmz")); refreshTexts(); } });
                }
                else
                {
                    chooser = std::make_unique<juce::FileChooser> ("Load preset", RomanticizmProcessor::getUserPresetDirectory(), "*.rmz");
                    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) { proc.loadUserPreset (f); refreshTexts(); } });
                }
                refreshTexts();
            });
    }

    void themeMenu()
    {
        juce::PopupMenu m;
        for (int i = 0; i < (int) themes().size(); ++i)
            m.addItem (i + 1, themes()[(size_t) i].name, true, i == lfRef.themeIndex());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&themeBtn).withStandardItemHeight (24),
                         [this] (int r) { if (r > 0) editor.setTheme (r - 1); });
    }

    void refreshTexts()
    {
        const auto pn = proc.getPresetName();
        if (pn != lastPreset) { if (lastPreset.isNotEmpty() && stage) stage->burst(); lastPreset = pn; }
        presetBtn.setButtonText (pn);
        themeBtn.setButtonText ("Theme: " + juce::String (lfRef.th().name));
        const float x = *proc.apvts.getRawParameterValue (pid::moodX), y = *proc.apvts.getRawParameterValue (pid::moodY);
        const char* dn = x < 34.0f ? "close" : (x < 67.0f ? "in the room" : "far away");
        const char* an = y < 34.0f ? "fresh" : (y < 67.0f ? "warm" : "worn out");
        const auto txt = juce::String (dn) + "  /  " + an;
        if (moodWord.getText() != txt) moodWord.setText (txt, juce::dontSendNotification);
        moodWord.setFont (serifFont (21.0f));
        moodHint.setFont (sansFont (10.0f));
    }

    void timerCallback() override
    {
        if (lfRef.style() != 1)
            for (auto& sl : sliders) sl->repaint();          // gentle breathing of the lit petals / hearts
        for (auto& tg : toggles) tg->repaint();               // lamp glow
        if ((++tick & 3) == 0) refreshTexts();
    }

    RomanticizmEditor& editor;
    RomanticizmProcessor& proc;
    RmzLookAndFeel& lfRef;
    std::unique_ptr<MoodStage> stage;
    std::unique_ptr<MagicEye> eye;
    std::unique_ptr<Reels> reels;
    std::unique_ptr<ChoiceChips> chipsTime, chipsMode;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    juce::TextButton prev, next, presetBtn, themeBtn;
    juce::Label moodWord, moodHint;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastPreset;
    int tick = 0;
};

// ============================================================================
RomanticizmEditor::RomanticizmEditor (RomanticizmProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    lf.setTheme (proc.getThemeIndex());
    setLookAndFeel (&lf);
    addAndMakeVisible (root);
    backdrop = std::make_unique<Backdrop> (lf);
    content = std::make_unique<Content> (*this, proc);
    overlay = std::make_unique<GrainOverlay> (lf);
    root.addAndMakeVisible (*backdrop);
    root.addAndMakeVisible (*content);
    root.addAndMakeVisible (*overlay);
    root.setBounds (0, 0, baseW, baseH);
    backdrop->setBounds (0, 0, baseW, baseH);
    overlay->setBounds (0, 0, baseW, baseH);
    content->setBounds (30, 0, 1000, baseH);
    content->themeChanged();

    constrainer.setFixedAspectRatio ((double) baseW / (double) baseH);
    constrainer.setSizeLimits (636, 384, 2544, 1536);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (baseW, baseH);
}

RomanticizmEditor::~RomanticizmEditor() { setLookAndFeel (nullptr); }

void RomanticizmEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    root.setTransform (juce::AffineTransform::scale (s));
    root.setBounds (0, 0, baseW, baseH);
}

void RomanticizmEditor::setTheme (int index)
{
    lf.setTheme (index);
    proc.setThemeIndex (index);
    sendLookAndFeelChange();
    content->themeChanged();
    backdrop->repaint();
    overlay->repaint();
    repaint();
}
