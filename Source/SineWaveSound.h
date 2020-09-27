#pragma once

#include <JuceHeader.h>

class SineWaveSound :
    public juce::SynthesiserSound
{
public:
    SineWaveSound() {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};