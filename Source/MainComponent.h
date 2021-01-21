#pragma once

#include <JuceHeader.h>
#include "SynthAudioSource.h"
#include "ChromaKeyboard.h"


class MainComponent :
    public  AudioAppComponent,
    private Timer
{
public:
    MainComponent() :
        synthAudioSource(keyboardState),
        keyboardComponent(keyboardState, ChromaKeyboard::horizontal)
    {
		addAndMakeVisible(keyboardComponent);
		addAndMakeVisible(octaveSizeInput);
		octaveSizeInput.setEditable(true);
		octaveSizeInput.setText(String(octaveSize), sendNotificationAsync);
		octaveSizeInput.onEditorShow = [this] {
		  octaveSizeInput.getCurrentTextEditor()->setInputRestrictions(2, "0123456789");
		};
		octaveSizeInput.onEditorHide = [this] {
		  octaveSizeInputChanged();
		};
		addAndMakeVisible(octaveSizeLabel);
		octaveSizeLabel.attachToComponent(&octaveSizeInput, true);

        setAudioChannels(0, 2);

        setSize (800, 100);
        startTimer(400);
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

	void octaveSizeInputChanged() {
		BigInteger parsed; parsed.parseString(octaveSizeInput.getText(), 10);
		if (0 < parsed.toInt64()) {
			octaveSize = parsed.toInt64();
			keyboardComponent.setOctaveSize(octaveSize);
			synthAudioSource.setOctaveSize(octaveSize);
		}
		octaveSizeInput.setText(String(octaveSize), dontSendNotification);
	}

    /*
     * AudioSource
     */

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        synthAudioSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        synthAudioSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        synthAudioSource.releaseResources();
    }

    /*
     * Component
     */

    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        keyboardComponent.setBounds(
        	0,
        	0,
        	getWidth(),
        	getHeight());

		octaveSizeInput.setBounds(256, 0, 64, keyboardComponent.optionBarHeight);
    }

    void focusGained(FocusChangeType cause) override
	{
    	keyboardComponent.grabKeyboardFocus();
	}

private:

    /* Timer */

    void timerCallback() override
    {
    	if (keyboardComponent.isShowing()) {
			keyboardComponent.grabKeyboardFocus();
			stopTimer();
    	}
    }

	MidiKeyboardState keyboardState;
	SynthAudioSource synthAudioSource;
	ChromaKeyboard keyboardComponent;

	int octaveSize = 12;
	Label octaveSizeLabel { {}, "base:"};
	Label octaveSizeInput;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
