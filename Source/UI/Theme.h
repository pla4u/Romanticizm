#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace rmz
{
struct Theme
{
    const char* name;
    juce::uint32 bg, tile, card, ink, accent, accent2, glow, paper;
    bool dark;
    float grain;        // film grain strength 0..1
    bool scanlines;
    juce::uint32 head;  // header text colour
    int style;          // 0 flowers, 1 cassette deck, 2 cherry hearts
};

inline const std::vector<Theme>& themes()
{
    static const std::vector<Theme> t =
    {
        { "Sakura",          0xffd3c9c8, 0xffb9aeae, 0xfffaf1f0, 0xff4a3b41, 0xffe2819f, 0xff93b07e, 0xffffb7c9, 0xfffff9f7, false, 0.9f, false, 0xff4a3b41, 0 },
        { "Moss Tiles",      0xffbfc4b4, 0xffa2a893, 0xfff0f2e6, 0xff2f3b2c, 0xff78a05a, 0xffe49aa8, 0xffd9eaa0, 0xfffbfcf2, false, 0.9f, false, 0xff2f3b2c, 0 },
        { "Red Eye",         0xffdcd8d4, 0xffbfbbb8, 0xfff7f4f1, 0xff3a3a3d, 0xffd42a3c, 0xff7aa974, 0xffff7a7a, 0xfffffdfb, false, 1.0f, false, 0xff3a3a3d, 0 },
        { "Polaroid Sepia",  0xffcdbb9f, 0xffb4a285, 0xfff3e8d2, 0xff4b3a28, 0xffc2663a, 0xff7e925a, 0xffffc98d, 0xfffff7e6, false, 1.0f, false, 0xff4b3a28, 0 },
        { "VHS Night",       0xff17131f, 0xff262033, 0xff241f33, 0xffefe6f6, 0xffff5fa2, 0xff5fd6ff, 0xffff7ac0, 0xff1b1626, true,  0.8f, true,  0xffefe6f6, 0 },
        { "Cyanotype",       0xff1d3a63, 0xff2c4e7e, 0xff27497a, 0xffe6f0ff, 0xff9fd0ff, 0xffffefb0, 0xffbfe3ff, 0xff203f6c, true,  0.7f, false, 0xffe6f0ff, 0 },
        { "Cassette Deck",   0xff2b2724, 0xff3a3531, 0xffd2d3cf, 0xff1b1917, 0xffff6a1a, 0xffffc23a, 0xffffa646, 0xffefe8d6, false, 0.45f, false, 0xffefe8d6, 1 },
        { "Cherry Hearts",   0xffc8102e, 0xffa80c26, 0xfffff2f5, 0xff1a0a0e, 0xffe5102f, 0xffff5c93, 0xffff4d79, 0xffffffff, false, 0.30f, false, 0xffffffff, 2 },
    };
    return t;
}

inline juce::Colour C (juce::uint32 c) { return juce::Colour (c); }
}
