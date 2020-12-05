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
        setAudioChannels(0, 2);

        setSize (800, 100);
        startTimer(400);
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    /* AudioSource */

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

    /* Component */

    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        keyboardComponent.setBounds (0, 0, getWidth(), getHeight());
    }

private:

    /* Timer */

    void timerCallback() override
    {
        keyboardComponent.grabKeyboardFocus();
        stopTimer();
    }

    MidiKeyboardState keyboardState;
    SynthAudioSource synthAudioSource;
    ChromaKeyboard keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
