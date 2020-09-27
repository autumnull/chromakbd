#pragma once

#include <JuceHeader.h>
#include "SynthAudioSource.h"
#include "ChromaKeyboard.h"


class MainComponent :
    public  juce::AudioAppComponent,
    private juce::Timer
{
public:
    MainComponent() :
        synthAudioSource(keyboardState),
        keyboardComponent(keyboardState, ChromaticKeyboardComponent::horizontalKeyboard)
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

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        synthAudioSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        synthAudioSource.releaseResources();
    }

    /* Component */

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
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

    juce::MidiKeyboardState keyboardState;
    SynthAudioSource synthAudioSource;
    ChromaticKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
