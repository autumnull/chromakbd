#pragma once

#include <JuceHeader.h>

class ChromaKeyboard_UpDownButton;

class ChromaKeyboard :
    public  Component,
    public  MidiKeyboardState::Listener,
    public  ChangeBroadcaster,
    private Timer
{
public:
	friend ChromaKeyboard_UpDownButton;

    enum Orientation
    {
        horizontal,
        verticalFacingLeft,
        verticalFacingRight,
    };

    enum ColourIds
    {
        whiteNoteColourId               = 0x1005000,
        keySeparatorLineColourId        = 0x1005002,
        mouseOverKeyOverlayColourId     = 0x1005003,
        keyDownOverlayColourId          = 0x1005004,
        textLabelColourId               = 0x1005005,
        upDownButtonBackgroundColourId  = 0x1005006,
        upDownButtonArrowColourId       = 0x1005007,
        shadowColourId                  = 0x1005008
    };

    ChromaKeyboard(MidiKeyboardState& s, Orientation o);
    ~ChromaKeyboard() override;

    void setVelocity (float v, bool useMousePosition);
    void setMidiChannel (int midiChannelNumber);
    int getMidiChannel() const noexcept;
    void setMidiChannelsToDisplay (int midiChannelMask);
    int getMidiChannelsToDisplay() const noexcept;
    void setKeyWidth (float widthInPixels);
    float getKeyWidth() const noexcept;
    void setScrollButtonWidth (int widthInPixels);
    int getScrollButtonWidth() const noexcept;
    void setOrientation (Orientation newOrientation);
    Orientation getOrientation() const noexcept;
    void setAvailableRange (int lowestNote, int highestNote);
    int getRangeStart() const noexcept;
    int getRangeEnd() const noexcept;
    void setLowestVisibleKey (int noteNumber);
    int getLowestVisibleKey() const noexcept;
    void setScrollButtonsVisible (bool newCanScroll);
    float getKeyStartPosition (int midiNoteNumber) const;
    float getTotalKeyboardWidth() const noexcept;
    int getNoteAtPosition (Point<float> position);
    void clearKeyMappings();
    void mapKeycodeToKey (int keycode, int midiNoteOffsetFromC);
    void unmapKeycode (int keycode);
    void setKeyPressBaseNote (int newBaseNote);

    /*
     * Component
     */
    void paint (Graphics& graphics) override;
    void resized() override;
    void mouseMove (const MouseEvent& e) override;
    void mouseDrag (const MouseEvent& e) override;
    void mouseDown (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel) override;
    void colourChanged() override;
    bool keyStateChanged(bool isKeyDown) override;
    bool keyPressed (const KeyPress& keypress) override;
    void focusLost (FocusChangeType) override;

    /*
     * Timer
     */
    void timerCallback() override;

    /*
     * MidiKeyboardState::Listener
     */
    void handleNoteOn (
        MidiKeyboardState*,
        int midiChan,
        int midiNoteNumber,
        float v
    ) override;

    void handleNoteOff (
        MidiKeyboardState*,
        int midiChan,
        int midiNoteNumber,
        float v
    ) override;

protected:
    virtual void drawKey (
        int midiKeyNumber,
        Graphics& g,
        Rectangle<float> area,
        bool isDown,
        bool isOver,
        Colour keyColour,
        Colour lineColour,
        Colour textColour
    );
    virtual String getNoteText (int midiNoteNumber);
    virtual void drawUpDownButton (
        Graphics& g,
        int w,
        int h,
        bool isMouseOver,
        bool isButtonPressed,
        bool movesOctavesUp
    );
    virtual bool mouseDownOnKey (int midiNoteNumber, const MouseEvent& e);
    virtual bool mouseDraggedToKey (int midiNoteNumber, const MouseEvent& e);
    virtual void mouseUpOnKey (int midiNoteNumber, const MouseEvent& e);
    virtual Range<float> getKeyPosition (int midiNoteNumber, float targetKeyWidth) const;
    Rectangle<float> getRectangleForKey (int midiNoteNumber) const;

private:
	// these have mixed lower and upper case on purpose;
	// the lowercase "ff" is the alpha channel.
	const u_int32_t keyColours[13] = {
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

    Colour getNoteColour(int note, int base);
    Range<float> getKeyPos (int midiNoteNumber) const;
    int xyToNote (Point<float> pos, float& mousePositionVelocity);
    int remappedXYToNote (Point<float> pos, float& mousePositionVelocity) const;
    void resetAnyKeysInUse();
    void updateNoteUnderMouse (Point<float> pos, bool isDown, int fingerNum);
    void updateNoteUnderMouse (const MouseEvent& e, bool isDown);
    void repaintKey (int midiNoteNumber);
    void setLowestVisibleKeyFloat (float noteNumber);
    int findKeycodeChanged();

    MidiKeyboardState& state;
    Orientation orientation;

    Array<int> mouseOverNotes, mouseDownNotes;
    HashMap<char, int> keycodeToMidiKey;	// maps keycodes to midi keys
    HashMap<int, int> midiKeysPressed; 		// midi keys to number of pressers
    HashMap<char, bool> keycodeState {256}; // keeps track of physical keyboard state
    BigInteger keysCurrentlyShownPressed;
    std::unique_ptr<Button> scrollDown, scrollUp;

    float velocity = 1.0f;
    int midiChannel = 1, midiInChannelMask = 0xffff;
    bool shouldCheckState = false;
    bool canScroll = true, useMousePositionForVelocity = true;

    int rangeStart = 0, rangeEnd = 127;
    float firstKey = 48.0f;
    int keyMappingBase = 64;
    int octaveSize = 12;

    float xOffset = 0;
    float keyWidth = 16.0f;
    int scrollButtonWidth = 12;
};

