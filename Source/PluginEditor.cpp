/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChromakbdAudioProcessorEditor::ChromakbdAudioProcessorEditor (ChromakbdAudioProcessor& p) :
	AudioProcessorEditor (&p),
	audioProcessor (p),
	keyboardComponent(p.keyboardState, ChromaKeyboard::horizontal)
{
	addAndMakeVisible(keyboardComponent);
	keyboardComponent.setLayout(ChromaKeyboard::guitar);

	addAndMakeVisible(baseInput);
	baseInput.setEditable(true);
	baseInput.setText(juce::String(base), juce::sendNotificationAsync);
	baseInput.onEditorShow = [this] {
		baseInput.getCurrentTextEditor()->setInputRestrictions(2, "0123456789");
	};
	baseInput.onEditorHide = [this] {
		baseInputChanged();
	};

	addAndMakeVisible(baseLabel);
	baseLabel.attachToComponent(&baseInput, true);

	setSize (800, 100);
	startTimer(400);
}

ChromakbdAudioProcessorEditor::~ChromakbdAudioProcessorEditor()
{
}

void ChromakbdAudioProcessorEditor::baseInputChanged() {
	juce::BigInteger parsed; parsed.parseString(baseInput.getText(), 10);
	if (0 < parsed.toInt64()) {
		base = parsed.toInt64();
		keyboardComponent.setBase(base);
	}
	baseInput.setText(juce::String(base), juce::dontSendNotification);
}

//==============================================================================
void ChromakbdAudioProcessorEditor::paint (juce::Graphics& g)
{
	g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void ChromakbdAudioProcessorEditor::resized()
{
	keyboardComponent.setBounds(
		0,
		0,
		getWidth(),
		getHeight());

	baseInput.setBounds(256, 0, 64, keyboardComponent.optionBarHeight);
}


void ChromakbdAudioProcessorEditor::focusGained(FocusChangeType cause)
{
	keyboardComponent.grabKeyboardFocus();
}

void ChromakbdAudioProcessorEditor::timerCallback()
{
	if (keyboardComponent.isShowing()) {
		keyboardComponent.grabKeyboardFocus();
		stopTimer();
	}
}
