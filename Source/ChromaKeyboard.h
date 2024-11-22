#pragma once

#include <JuceHeader.h>

class ChromaKeyboard_ScrollButton;

class ChromaKeyboard :
	public  juce::Component,
	public  juce::MidiKeyboardState::Listener,
	public  juce::ChangeBroadcaster,
	private juce::Timer
{
public:
	friend ChromaKeyboard_ScrollButton;

	const float optionBarHeight = 24.0f;
	juce::MidiKeyboardState& state;

	enum Orientation
	{
		horizontal,
		verticalFacingLeft,
		verticalFacingRight,
	};

	enum Layout
	{
		linear = 1,
		guitar,
		organ,
		harpejji,
		hexagonal,
	};
	ChromaKeyboard(juce::MidiKeyboardState& s, Orientation o);

	~ChromaKeyboard() override;
	void setVelocity(float v, bool useMousePosition);
	void setMidiChannel(int midiChannelNumber);
	int getMidiChannel() const noexcept;
	void setMidiChannelsToDisplay(int midiChannelMask);
	int getMidiChannelsToDisplay() const noexcept;
	void setKeyWidth(float widthInPixels);
	float getKeyWidth() const noexcept;
	void setScrollButtonWidth(int widthInPixels);
	int getScrollButtonWidth() const noexcept;
	void setOrientation(Orientation newOrientation);
	Orientation getOrientation() const noexcept;
	void setAvailableRange(int lowestNote, int highestNote);
	int getRangeStart() const noexcept;
	int getRangeEnd() const noexcept;
	void setLowestVisibleKey(int noteNumber);
	int getLowestVisibleKey() const noexcept;
	void setScrollButtonsVisible(bool newCanScroll);
	float getKeyStartPosition(int midiNoteNumber) const;
	float getTotalKeyboardWidth() const noexcept;
	int getNoteAtPosition(juce::Point<float> position);
	void clearKeyMappings();
	void mapKeycodeToMidiKey(int keycode, int midiKey);
	void unmapKeycode(int keycode);
	void setKeyMapBase(int newBaseNote);
	void shiftKeyMapBase(int offset);
	void setLayout(Layout newLayout);
	Layout getLayout();
	int getBase() const;
	void setBase(int octave_size);

	/*
	 * Component
	 */
	void paint(juce::Graphics& g) override;
	void resized() override;
	void mouseMove(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
	void mouseEnter(const juce::MouseEvent& e) override;
	void mouseExit(const juce::MouseEvent& e) override;
	void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
	void colourChanged() override;
	bool keyStateChanged(bool) override;
	bool keyPressed(const juce::KeyPress& keypress) override;
	void focusLost(FocusChangeType cause) override;
	void focusGained(FocusChangeType cause) override;

	/*
	 * Timer
	 */
	void timerCallback() override;

	/*
	 * MidiKeyboardState::Listener
	 */
	void handleNoteOn(
		juce::MidiKeyboardState*,
		int midiChan,
		int midiNoteNumber,
		float v
	) override;

	void handleNoteOff(
		juce::MidiKeyboardState*,
		int midiChan,
		int midiNoteNumber,
		float v
	) override;
protected:
	virtual void drawKey(
		int midiKeyNumber,
		juce::Graphics& g,
		juce::Rectangle<float> area,
		bool isDown,
		bool isOver,
		juce::Colour keyColour,
		juce::Colour lineColour,
		juce::Colour textColour
	);
	virtual juce::String getNoteText(int midiNoteNumber);
	virtual void drawScrollButton(
		juce::Graphics& g,
		int w,
		int h,
		bool isMouseOver,
		bool isButtonPressed,
		bool movesOctavesUp
	);
	virtual bool mouseDownOnKey(int midiNoteNumber, const juce::MouseEvent& e);
	virtual bool mouseDraggedToKey(int midiNoteNumber, const juce::MouseEvent& e);
	virtual void mouseUpOnKey(int midiNoteNumber, const juce::MouseEvent& e);
	virtual juce::Range<float> getKeyPosition(int midiNoteNumber, float targetKeyWidth) const;
	juce::Rectangle<float> getRectangleForKey(int midiNoteNumber) const;

private:
	// these have mixed lower and upper case on purpose;
	// the lowercase "ff" is the alpha channel.
	const uint32_t keyColours[13] = {
		0xffF63A45,
		0xffF58438,
		0xffF5BC38,
		0xffC9F538,
		0xff5EF538,
		0xff3BF5B7,
		0xff38B9F5,
		0xff3880F5,
		0xff4838F5,
		0xff9A38F5,
		0xffD238F5,
		0xffF538AD,
		0xffF63A45,
	};

	juce::Colour getNoteColour(int note, int base);
	juce::Range<float> getKeyPos(int midiNoteNumber) const;
	int xyToNote(juce::Point<float> pos, float& mousePositionVelocity);
	int remappedXYToNote(juce::Point<float> pos, float& mousePositionVelocity) const;
	void resetAnyKeysInUse();
	void updateNoteUnderMouse(juce::Point<float> pos, bool isDown);
	void repaintKey(int midiNoteNumber);
	void setLowestVisibleKeyFloat(float keyNumber);
	void resetKeycodeStates();
	void setOrganLayout();

	Orientation orientation;

	char kbdString[41] = ";qjkxbmwvzaoeuidhtns',.pyfgcrl123456789*";
	juce::ComboBox layoutSelector;
	juce::StringArray layoutNames {
		"linear",
		"guitar",
		"organ",
		"harpejji",
		"hexagonal",
	};
	juce::Label layoutSelectorLabel { {}, "layout:" };
	std::unique_ptr<juce::Button> scrollDown, scrollUp;

	int keyHovered = -1, keyClicked = -1; // -1 indicates no key
	juce::Array<int> keycodeToKey;	// maps keycodes to midi keys
	juce::Array<int> midiKeysPressed; 		// midi keys to number of pressers
	juce::BigInteger keycodeStates; // keeps track of physical keyboard state
	juce::BigInteger keysCurrentlyShownPressed;

	float velocity = 1.0f;
	int midiChannel = 1, midiInChannelMask = 0xffff;
	bool shouldCheckState = false;
	bool canScroll = true, useMousePositionForVelocity = true;

	int rangeStart = 0, rangeEnd = 127;	// key range
	int lowestVisibleKey = 48;
	int keyMapBase = 52;
	int base = 12;
	Layout currentLayout = linear;

	float xOffset = 0;
	float keyWidth = 16.0f;
	float scrollButtonWidth = 12.0f;
};

