#pragma once

#include <JuceHeader.h>
#include "SineWaveSound.h"
#include "SineWaveVoice.h"


class SynthAudioSource :
    public AudioSource {
public:
	SynthAudioSource(MidiKeyboardState& keyState)
		:keyboardState(keyState)
	{
		for (auto jVoice = 0; jVoice < 8; jVoice++)
			synth.addVoice(new SineWaveVoice(12));

		synth.addSound(new SineWaveSound());
	}

	void setUsingSineWaveSound()
	{
		synth.clearSounds();
	}

	void setOctaveSize(int newSize) {
		synth.clearVoices();
		for (auto jVoice = 0; jVoice < 8; jVoice++)
			synth.addVoice(new SineWaveVoice(newSize));
	}

	void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override
	{
		synth.setCurrentPlaybackSampleRate(sampleRate);
	}

	void releaseResources() override
	{ }

    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        MidiBuffer incomingMidi;
        keyboardState.processNextMidiBuffer(
            incomingMidi,
            bufferToFill.startSample,
            bufferToFill.numSamples,
            true
        );

        synth.renderNextBlock(
            *bufferToFill.buffer,
            incomingMidi,
            bufferToFill.startSample,
            bufferToFill.numSamples
        );
    }

private:
    MidiKeyboardState& keyboardState;
    Synthesiser synth;
};