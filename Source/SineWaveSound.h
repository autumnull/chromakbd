#pragma once

#include <JuceHeader.h>

class SineWaveSound :
    public SynthesiserSound
{
public:
    SineWaveSound() {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};