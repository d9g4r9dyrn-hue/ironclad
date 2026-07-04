#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace IroncladLayout {
constexpr float baseW = 1774.0f;
constexpr float baseH = 887.0f;

inline juce::Rectangle<float> r(float x, float y, float w, float h) { return {x, y, w, h}; }

struct Regions {
    juce::Rectangle<float> leftPanel      = r(140, 40, 385, 780);
    juce::Rectangle<float> centerPanel    = r(500, 95, 745, 595);
    juce::Rectangle<float> rightPanel     = r(1240, 40, 380, 775);
    juce::Rectangle<float> leftArmor      = r(0, 18, 145, 830);
    juce::Rectangle<float> rightArmor     = r(1630, 18, 145, 830);
    juce::Rectangle<float> centerScreen   = r(520, 120, 704, 560);
};

struct Controls {
    juce::Rectangle<float> drive      = r(166,126,152,152);
    juce::Rectangle<float> tone       = r(332,126,152,152);
    juce::Rectangle<float> bass       = r(166,308,152,152);
    juce::Rectangle<float> mid        = r(332,308,152,152);
    juce::Rectangle<float> presence   = r(166,516,152,152);
    juce::Rectangle<float> output     = r(332,516,152,152);
    juce::Rectangle<float> lowCut     = r(1238,235,84,330);
    juce::Rectangle<float> highCut    = r(1328,235,84,330);
    juce::Rectangle<float> mix        = r(1418,235,84,330);
    juce::Rectangle<float> level      = r(1503,235,84,330);
};
}
