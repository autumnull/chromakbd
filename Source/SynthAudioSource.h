#pragma once

#include <JuceHeader.h>
#include "SineWaveSound.h"
#include "SineWaveVoice.h"


class SynthAudioSource :
    public juce::AudioSource
{
public:
    SynthAudioSource (juce::MidiKeyboardState& keyState)
        : keyboardState (keyState)
    {
        for (auto i = 0; i < 4; i++) {
            synth.addVoice(new SineWaveVoice());
        }

        synth.addSound(new SineWaveSound());
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void releaseResources() override
    {

    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer incomingMidi;
        keyboardState.processNextMidiBuffer(
            incomingMidi,
            bufferToFill.startSample,
            bufferToFill.numSamples,
            true
        );

        synth.renderNextBlock (
            *bufferToFill.buffer,
            incomingMidi,
            bufferToFill.startSample,
            bufferToFill.numSamples
        );
    }

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;
};