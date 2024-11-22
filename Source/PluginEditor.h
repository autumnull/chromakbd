/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChromaKeyboard.h"

//==============================================================================
/**
*/
class ChromakbdAudioProcessorEditor  :
	public juce::AudioProcessorEditor,
	private juce::Timer
{
public:
    ChromakbdAudioProcessorEditor (ChromakbdAudioProcessor&);
    ~ChromakbdAudioProcessorEditor() override;

	void baseInputChanged();
    //==============================================================================
	void paint (juce::Graphics&) override;
	void resized() override;
	void focusGained(FocusChangeType) override;

private:
	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	ChromakbdAudioProcessor& audioProcessor;

	ChromaKeyboard keyboardComponent;

	int base = 12;
	juce::Label baseLabel { {}, "base:"};
	juce::Label baseInput;

	void timerCallback() override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromakbdAudioProcessorEditor)
};
