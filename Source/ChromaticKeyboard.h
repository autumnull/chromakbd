#pragma once

#include <JuceHeader.h>


class ChromaticKeyboardComponent :
    public  juce::Component,
    public  juce::MidiKeyboardState::Listener,
    public  juce::ChangeBroadcaster,
    private juce::Timer
{
public:
    enum Orientation
    {
        horizontalKeyboard,
        verticalKeyboardFacingLeft,
        verticalKeyboardFacingRight,
    };



    ChromaticKeyboardComponent(juce::MidiKeyboardState& s, Orientation o) :
        state(s),
        orientation(o)
    {
        scrollDown.reset (new UpDownButton (*this, -1));
        scrollUp  .reset (new UpDownButton (*this, 1));

        addChildComponent (scrollDown.get());
        addChildComponent (scrollUp.get());

        octaveSize = 12;

        bool guitarMode = true;

        char layout[] = "zxcvbnm,./asdfghjkl;qwertyuiop1234567890";
        if (!guitarMode) {
            int note = 0;
            for (char c : layout)
                setNoteForKeyPress(juce::KeyPress(c, 0, 0), note++);
        }
        else
        {
            for (int row = 0; row < 4; row++){
                for (int fret = 0; fret < 10; fret++){
                    char c = layout[row*10 + fret];
                    int note = row*5 + fret;
                    setNoteForKeyPress(juce::KeyPress(c, 0, 0), note);
                }
            }
        }

        mouseOverNotes.insertMultiple (0, -1, 32);
        mouseDownNotes.insertMultiple (0, -1, 32);

        colourChanged();
        setWantsKeyboardFocus (true);

        state.addListener (this);

        startTimerHz (60);
    }

    ~ChromaticKeyboardComponent() override
    {
        state.removeListener (this);
    }

    void setVelocity (float v, bool useMousePosition)
    {
        velocity = juce::jlimit (0.0f, 1.0f, v);
        useMousePositionForVelocity = useMousePosition;
    }

    void setMidiChannel (int midiChannelNumber)
    {
        jassert (midiChannelNumber > 0 && midiChannelNumber <= 16);

        if (midiChannel != midiChannelNumber)
        {
            resetAnyKeysInUse();
            midiChannel = juce::jlimit (1, 16, midiChannelNumber);
        }
    }

    int getMidiChannel() const noexcept { return midiChannel; }

    void setMidiChannelsToDisplay (int midiChannelMask)
    {
        midiInChannelMask = midiChannelMask;
        shouldCheckState = true;
    }

    int getMidiChannelsToDisplay() const noexcept { return midiInChannelMask; }

    void setKeyWidth (float widthInPixels)
    {
        jassert (widthInPixels > 0);

        if (keyWidth != widthInPixels) // Prevent infinite recursion if the width is being computed in a 'resized()' call-back
        {
            keyWidth = widthInPixels;
            resized();
        }
    }

    float getKeyWidth() const noexcept { return keyWidth; }

    void setScrollButtonWidth (int widthInPixels)
    {
        jassert (widthInPixels > 0);

        if (scrollButtonWidth != widthInPixels)
        {
            scrollButtonWidth = widthInPixels;
            resized();
        }
    }

    int getScrollButtonWidth() const noexcept { return scrollButtonWidth; }

    void setOrientation (Orientation newOrientation)
    {
        if (orientation != newOrientation)
        {
            orientation = newOrientation;
            resized();
        }
    }

    Orientation getOrientation() const noexcept { return orientation; }

    void setAvailableRange (int lowestNote, int highestNote)
    {
        jassert (lowestNote >= 0 && lowestNote <= 127);
        jassert (highestNote >= 0 && highestNote <= 127);
        jassert (lowestNote <= highestNote);

        if (rangeStart != lowestNote || rangeEnd != highestNote)
        {
            rangeStart = juce::jlimit (0, 127, lowestNote);
            rangeEnd = juce::jlimit (0, 127, highestNote);
            firstKey = juce::jlimit ((float) rangeStart, (float) rangeEnd, firstKey);
            resized();
        }
    }

    int getRangeStart() const noexcept { return rangeStart; }

    int getRangeEnd() const noexcept { return rangeEnd; }

    void setLowestVisibleKey (int noteNumber)
    {
        setLowestVisibleKeyFloat ((float) noteNumber);
    }

    int getLowestVisibleKey() const noexcept { return (int) firstKey; }

    void setScrollButtonsVisible (bool newCanScroll)
    {
        if (canScroll != newCanScroll)
        {
            canScroll = newCanScroll;
            resized();
        }
    }

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

    float getKeyStartPosition (int midiNoteNumber) const
    {
        return getKeyPos (midiNoteNumber).getStart();
    }

    float getTotalKeyboardWidth() const noexcept
    {
        return getKeyPos (rangeEnd).getEnd();
    }

    int getNoteAtPosition (juce::Point<float> position)
    {
        float v;
        return xyToNote (position, v);
    }

    void clearKeyMappings()
    {
        resetAnyKeysInUse();
        keyPressNotes.clear();
        keyPresses.clear();
    }

    void setNoteForKeyPress (const juce::KeyPress& key, int midiNoteOffsetFromC)
    {
        keyPressNotes.add(midiNoteOffsetFromC);
        keyPresses.add(key);
    }

    void removeNoteForKeyPress (juce::KeyPress keyPress)
    {
        for (int i = keyPresses.size(); --i >= 0;)
        {
            if (keyPresses.getUnchecked(i) == keyPress)
            {
                keyPressNotes.remove(i);
                keyPresses.remove(i);
            }
        }
    }

    void setKeyPressBaseNote (int newBaseNote)
    {
        jassert (newBaseNote >= 0 && newBaseNote < 128);

        keyMappingBase = newBaseNote;
    }

    void setMiddleC (int noteNum)
    {
        middleC = noteNum;
        repaint();
    }

    int getMiddleC() const noexcept { return middleC; }

    /*
     * Component
     */

    void paint (juce::Graphics& graphics) override
    {
        graphics.fillAll (findColour (whiteNoteColourId));

        auto lineColour = juce::Colour(0x55000000);
        auto textColour = findColour (textLabelColourId);

        for (float jNote = 0; jNote < 128; jNote ++)
        {
            auto noteColour = getNoteColour(jNote, octaveSize);
            drawNote(
                jNote,
                graphics,
                getRectangleForKey(jNote),
                state.isNoteOnForChannels(midiInChannelMask, jNote),
                mouseOverNotes.contains(jNote),
                noteColour,
                lineColour,
                textColour
            );
        }

        auto width = getWidth();
        auto height = getHeight();

        auto x = getKeyPos(rangeEnd).getEnd();

        graphics.setColour(lineColour);

        // Line at the bottom of keys
        switch (orientation) {
            case horizontalKeyboard:
                graphics.fillRect(0.0f, height - 1.0f, x, 1.0f);
                break;
            case verticalKeyboardFacingLeft:
                graphics.fillRect(0.0f, 0.0f, 1.0f, x);
                break;
            case verticalKeyboardFacingRight:
                graphics.fillRect(width - 1.0f, 0.0f, 1.0f, x);
                break;
            default:
                break;
        }
    }

    void resized() override
    {
        auto w = getWidth();
        auto h = getHeight();

        if (w > 0 && h > 0)
        {
            if (orientation != horizontalKeyboard)
                std::swap (w, h);

            auto kx2 = getKeyPos (rangeEnd).getEnd();

            if ((int) firstKey != rangeStart)
            {
                auto kx1 = getKeyPos (rangeStart).getStart();

                if (kx2 - kx1 <= w)
                {
                    firstKey = (float) rangeStart;
                    sendChangeMessage();
                    repaint();
                }
            }

            scrollDown->setVisible (canScroll && firstKey > (float) rangeStart);

            xOffset = 0;

            if (canScroll)
            {
                auto scrollButtonW = juce::jmin (scrollButtonWidth, w / 2);
                auto r = getLocalBounds();

                if (orientation == horizontalKeyboard)
                {
                    scrollDown->setBounds (r.removeFromLeft  (scrollButtonW));
                    scrollUp  ->setBounds (r.removeFromRight (scrollButtonW));
                }
                else if (orientation == verticalKeyboardFacingLeft)
                {
                    scrollDown->setBounds (r.removeFromTop    (scrollButtonW));
                    scrollUp  ->setBounds (r.removeFromBottom (scrollButtonW));
                }
                else
                {
                    scrollDown->setBounds (r.removeFromBottom (scrollButtonW));
                    scrollUp  ->setBounds (r.removeFromTop    (scrollButtonW));
                }

                auto endOfLastKey = getKeyPos (rangeEnd).getEnd();

                float mousePositionVelocity;
                auto spaceAvailable = w;
                auto lastStartKey = remappedXYToNote ({ endOfLastKey - spaceAvailable, 0 }, mousePositionVelocity) + 1;

                if (lastStartKey >= 0 && ((int) firstKey) > lastStartKey)
                {
                    firstKey = (float) juce::jlimit (rangeStart, rangeEnd, lastStartKey);
                    sendChangeMessage();
                }

                xOffset = getKeyPos ((int) firstKey).getStart();
            }
            else
            {
                firstKey = (float) rangeStart;
            }

            scrollUp->setVisible (canScroll && getKeyPos (rangeEnd).getStart() > w);
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        updateNoteUnderMouse (e, false);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        float mousePositionVelocity;
        auto newNote = xyToNote (e.position, mousePositionVelocity);

        updateNoteUnderMouse (e, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        float mousePositionVelocity;
        auto newNote = xyToNote (e.position, mousePositionVelocity);

        if (newNote >= 0 && mouseDownOnKey (newNote, e))
            updateNoteUnderMouse (e, true);
    }


    void mouseUp (const juce::MouseEvent& e) override
    {
        updateNoteUnderMouse (e, false);

        float mousePositionVelocity;
        auto note = xyToNote (e.position, mousePositionVelocity);

        if (note >= 0)
            mouseUpOnKey (note, e);
    }

    void mouseEnter (const juce::MouseEvent& e) override
    {
        updateNoteUnderMouse (e, false);
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        updateNoteUnderMouse (e, false);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        auto amount = (orientation == horizontalKeyboard && wheel.deltaX != 0)
                      ? wheel.deltaX : (orientation == verticalKeyboardFacingLeft ? wheel.deltaY
                                                                                  : -wheel.deltaY);

        setLowestVisibleKeyFloat (firstKey - amount * keyWidth);
    }

    void colourChanged() override
    {
        setOpaque (findColour (whiteNoteColourId).isOpaque());
        repaint();
    }

    bool keyStateChanged (bool isKeyDown) override
    {
        bool keyPressUsed = false;

        for (int i = 0; i < keyPresses.size(); i++)
        {
            auto note = keyMappingBase + keyPressNotes.getUnchecked(i);

            if (keyPresses.getReference(i).isCurrentlyDown())
            {
                if (! keysPressed[i])
                {
                    keysPressed.setBit(i);
                    state.noteOn(midiChannel, note, velocity);
                    keyPressUsed = true;
                }
            }
            else
            {
                if (keysPressed[i])
                {
                    keysPressed.clearBit(i);
                    state.noteOff(midiChannel, note, 0.0f);
                    keyPressUsed = true;
                }
            }
        }

        return keyPressUsed;
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        // Return true if we handle this keypress.
        return keyPresses.contains(key);
    }

    void focusLost (FocusChangeType) override
    {
        resetAnyKeysInUse();
    }

    /*
     * Timer
     */

    void timerCallback() override
    {
        if (shouldCheckState)
        {
            shouldCheckState = false;

            for (int i = rangeStart; i <= rangeEnd; ++i)
            {
                bool isOn = state.isNoteOnForChannels (midiInChannelMask, i);

                if (keysCurrentlyDrawnDown[i] != isOn)
                {
                    keysCurrentlyDrawnDown.setBit (i, isOn);
                    repaintNote (i);
                }
            }
        }
    }

    /*
     * MidiKeyboardState::Listener
     */

    void handleNoteOn (
        juce::MidiKeyboardState*,
        int midiChan,
        int midiNoteNumber,
        float v
    ) override
    {
        shouldCheckState = true;
    }

    void handleNoteOff (
        juce::MidiKeyboardState*,
        int midiChan,
        int midiNoteNumber,
        float v
    ) override
    {
        shouldCheckState = true;
    }

protected:
    virtual void drawNote (
        int midiNoteNumber,
        juce::Graphics& g,
        juce::Rectangle<float> area,
        bool isDown,
        bool isOver,
        juce::Colour noteColour,
        juce::Colour lineColour,
        juce::Colour textColour
    ) {
        auto c = noteColour;

        if (isOver)  c = c.overlaidWith(juce::Colour(0x55FFFFFF));
        if (isDown)  c = c.overlaidWith(juce::Colour(0x77FFFFFF));

        g.setColour (c);
        g.fillRect (area);

        auto text = getNoteText (midiNoteNumber);

        if (text.isNotEmpty())
        {
            auto fontHeight = juce::jmin (12.0f, keyWidth * 0.9f);

            g.setColour (textColour);
            g.setFont (juce::Font (fontHeight).withHorizontalScale (0.8f));

            switch (orientation) {
                case horizontalKeyboard:
                    g.drawText(
                        text,
                        area.withTrimmedLeft(1.0f).withTrimmedBottom(2.0f),
                        juce::Justification::centredBottom,
                        false
                    );
                    break;
                case verticalKeyboardFacingLeft:
                    g.drawText(text, area.reduced(2.0f), juce::Justification::centredLeft, false);
                    break;
                case verticalKeyboardFacingRight:
                    g.drawText(text, area.reduced(2.0f), juce::Justification::centredRight, false);
                    break;
                default:
                    break;
            }
        }

        if (! lineColour.isTransparent())
        {
            g.setColour (lineColour);

            switch (orientation) {
                case horizontalKeyboard:
                    g.fillRect(area.withWidth(1.0f));
                    break;
                case verticalKeyboardFacingLeft:
                    g.fillRect(area.withHeight(1.0f));
                    break;
                case verticalKeyboardFacingRight:
                    g.fillRect(area.removeFromBottom(1.0f));
                    break;
                default:
                    break;
            }

            if (midiNoteNumber == rangeEnd)
            {
                switch (orientation) {
                    case horizontalKeyboard:
                        g.fillRect(area.expanded(1.0f, 0).removeFromRight(1.0f));
                        break;
                    case verticalKeyboardFacingLeft:
                        g.fillRect(area.expanded(0, 1.0f).removeFromBottom(1.0f));
                        break;
                    case verticalKeyboardFacingRight:
                        g.fillRect(area.expanded(0, 1.0f).removeFromTop(1.0f));
                        break;
                    default:
                        break;
                }
            }
        }
    }

    virtual juce::String getNoteText (int midiNoteNumber)
    {
        auto octave = midiNoteNumber / octaveSize;
        if (midiNoteNumber % octaveSize == 0)
            return juce::String(octave);

        return {};
    }

    virtual void drawUpDownButton (
        juce::Graphics& g,
        int w,
        int h,
        bool isMouseOver,
        bool isButtonPressed,
        bool movesOctavesUp
    ) {
        g.fillAll (findColour (upDownButtonBackgroundColourId));

        float angle = 0;

        switch (orientation) {
            case horizontalKeyboard:
                angle = movesOctavesUp ? 0.0f : 0.5f;
                break;
            case verticalKeyboardFacingLeft:
                angle = movesOctavesUp ? 0.25f : 0.75f;
                break;
            case verticalKeyboardFacingRight:
                angle = movesOctavesUp ? 0.75f : 0.25f;
                break;
            default:
                jassertfalse;
                break;
        }

        juce::Path path;
        path.addTriangle (0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f);
        path.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::twoPi * angle, 0.5f, 0.5f));

        g.setColour (findColour (upDownButtonArrowColourId)
                     .withAlpha (isButtonPressed ? 1.0f : (isMouseOver ? 0.6f : 0.4f)));

        g.fillPath (path, path.getTransformToScaleToFit (1.0f, 1.0f, w - 2.0f, h - 2.0f, true));
    }

    virtual bool mouseDownOnKey (int midiNoteNumber, const juce::MouseEvent& e) { return true; }

    virtual bool mouseDraggedToKey (int midiNoteNumber, const juce::MouseEvent& e) { return true; }

    virtual void mouseUpOnKey (int midiNoteNumber, const juce::MouseEvent& e) {}

    virtual juce::Range<float> getKeyPosition (int midiNoteNumber, float targetKeyWidth) const
    {
        jassert (midiNoteNumber >= 0 && midiNoteNumber < 128);

        auto octave = midiNoteNumber / octaveSize;
        auto note   = midiNoteNumber % octaveSize;

        auto start = octave * octaveSize * targetKeyWidth + (float)note * targetKeyWidth;

        return { start, start + targetKeyWidth };
    }

    juce::Rectangle<float> getRectangleForKey (int midiNoteNumber) const
    {
        jassert (midiNoteNumber >= rangeStart && midiNoteNumber <= rangeEnd);

        auto pos = getKeyPos (midiNoteNumber);
        auto x = pos.getStart();
        auto w = pos.getLength();


        switch (orientation)
        {
            case horizontalKeyboard:
                return {x, 0, w, (float) getHeight()};
            case verticalKeyboardFacingLeft:
                return {0, x, (float) getWidth(), w};
            case verticalKeyboardFacingRight:
                return {0, getHeight() - x - w, (float) getWidth(), w};
            default:
                jassertfalse;
                break;
        }

        return {};
    }

private:
    class UpDownButton :
        public juce::Button
    {
    public:
        UpDownButton (ChromaticKeyboardComponent& c, int d) :
            Button ({}), owner (c), delta (d)
        {

        }

        void clicked() override
        {
            auto note = owner.getLowestVisibleKey();

            if (delta < 0)
                note = (note - 1) / owner.octaveSize;
            else
                note = note / owner.octaveSize + 1;

            owner.setLowestVisibleKey (note * owner.octaveSize);
        }

        using Button::clicked;

        void paintButton (
            juce::Graphics& g,
            bool shouldDrawButtonAsHighlighted,
            bool shouldDrawButtonAsDown
        ) override
        {
            owner.drawUpDownButton (
                g,
                getWidth(),
                getHeight(),
                shouldDrawButtonAsHighlighted,
                shouldDrawButtonAsDown,
                delta > 0
            );
        }

    private:
        ChromaticKeyboardComponent& owner;
        const int delta;

        JUCE_DECLARE_NON_COPYABLE (UpDownButton)
    };

    juce::MidiKeyboardState& state;
    float xOffset = 0;
    float keyWidth = 16.0f;
    int scrollButtonWidth = 12;
    Orientation orientation;

    int midiChannel = 1, midiInChannelMask = 0xffff;
    float velocity = 1.0f;

    juce::Array<int> mouseOverNotes, mouseDownNotes;
    juce::BigInteger keysPressed, keysCurrentlyDrawnDown;
    bool shouldCheckState = false;

    int rangeStart = 0, rangeEnd = 127;
    float firstKey = 48.0f;
    bool canScroll = true, useMousePositionForVelocity = true;
    std::unique_ptr<juce::Button> scrollDown, scrollUp;

    juce::Array<juce::KeyPress> keyPresses;
    juce::Array<int> keyPressNotes;
    int keyMappingBase = 64, middleC = 64;
    int octaveSize = 13;

    juce::uint32 noteColours[13] = {
        0xFFF00000,
        0xFFFFAE00,
        0xFFFFE700,
        0xFFBAFF00,
        0xFF00F000,
        0xFF00DDB8,
        0xFF00DEFF,
        0xFF009FFF,
        0xFF5600E9,
        0xFF9B00FF,
        0xFFE100EB,
        0xFFFF00AD,
        0xFFF00000
    };

    juce::Colour getNoteColour(int note, int base)
    {
        juce::Colour mixedColour;
        float index = (float)(note % base) / (float)base * 12.0f;
        for (int jSector = 0; jSector <= index; jSector++)
        {
            if (jSector <= index && index < jSector + 1)
            {
                auto startColour = juce::Colour(noteColours[jSector]);
                auto endColour = juce::Colour(noteColours[jSector+1]);
                mixedColour = startColour.interpolatedWith(endColour, index - jSector);
            }
        }
        return mixedColour;
    }

    juce::Range<float> getKeyPos (int midiNoteNumber) const
    {
        return getKeyPosition (midiNoteNumber, keyWidth)
               - xOffset
               - getKeyPosition (rangeStart, keyWidth).getStart();
    }

    int xyToNote (juce::Point<float> pos, float& mousePositionVelocity)
    {
        if (! reallyContains (pos.toInt(), false))
            return -1;

        auto p = pos;

        if (orientation != horizontalKeyboard)
        {
            p = { p.y, p.x };

            if (orientation == verticalKeyboardFacingLeft)
                p = { p.x, getWidth() - p.y };
            else
                p = { getHeight() - p.x, p.y };
        }

        return remappedXYToNote (p + juce::Point<float> (xOffset, 0), mousePositionVelocity);
    }

    int remappedXYToNote (juce::Point<float> pos, float& mousePositionVelocity) const
    {
        for (int note = rangeStart; note <= rangeEnd; note++)
        {
            if (getKeyPos (note).contains (pos.x - xOffset)) {
                auto noteLength = (orientation == horizontalKeyboard) ? getHeight() : getWidth();
                mousePositionVelocity = juce::jmax(0.0f, pos.y/noteLength);
                return note;
            }
        }

        mousePositionVelocity = 0;
        return -1;
    }

    void resetAnyKeysInUse()
    {
        if (! keysPressed.isZero())
        {
            for (int i = 128; --i >= 0;)
                if (keysPressed[i])
                    state.noteOff (midiChannel, i, 0.0f);

            keysPressed.clear();
        }

        for (int i = mouseDownNotes.size(); --i >= 0;)
        {
            auto noteDown = mouseDownNotes.getUnchecked(i);

            if (noteDown >= 0)
            {
                state.noteOff (midiChannel, noteDown, 0.0f);
                mouseDownNotes.set (i, -1);
            }

            mouseOverNotes.set (i, -1);
        }
    }

    void updateNoteUnderMouse (juce::Point<float> pos, bool isDown, int fingerNum)
    {
        float mousePositionVelocity = 0.0f;
        auto newNote = xyToNote (pos, mousePositionVelocity);
        auto oldNote = mouseOverNotes.getUnchecked (fingerNum);
        auto oldNoteDown = mouseDownNotes.getUnchecked (fingerNum);
        auto eventVelocity = useMousePositionForVelocity ? mousePositionVelocity * velocity : velocity;

        if (oldNote != newNote)
        {
            repaintNote (oldNote);
            repaintNote (newNote);
            mouseOverNotes.set (fingerNum, newNote);
        }

        if (isDown)
        {
            if (newNote != oldNoteDown)
            {
                if (oldNoteDown >= 0)
                {
                    mouseDownNotes.set (fingerNum, -1);

                    if (! mouseDownNotes.contains (oldNoteDown))
                        state.noteOff (midiChannel, oldNoteDown, eventVelocity);
                }

                if (newNote >= 0 && ! mouseDownNotes.contains (newNote))
                {
                    state.noteOn (midiChannel, newNote, eventVelocity);
                    mouseDownNotes.set (fingerNum, newNote);
                }
            }
        }
        else if (oldNoteDown >= 0)
        {
            mouseDownNotes.set (fingerNum, -1);

            if (! mouseDownNotes.contains (oldNoteDown))
                state.noteOff (midiChannel, oldNoteDown, eventVelocity);
        }
    }

    void updateNoteUnderMouse (const juce::MouseEvent& e, bool isDown)
    {
        updateNoteUnderMouse (e.getEventRelativeTo (this).position, isDown, e.source.getIndex());
    }

    void repaintNote (int midiNoteNumber)
    {
        if (midiNoteNumber >= rangeStart && midiNoteNumber <= rangeEnd)
            repaint (getRectangleForKey (midiNoteNumber).getSmallestIntegerContainer());
    }

    void setLowestVisibleKeyFloat (float noteNumber)
    {
        noteNumber = juce::jlimit ((float) rangeStart, (float) rangeEnd, noteNumber);

        if (noteNumber != firstKey)
        {
            bool hasMoved = (((int) firstKey) != (int) noteNumber);
            firstKey = noteNumber;

            if (hasMoved)
                sendChangeMessage();

            resized();
        }
    }
};

