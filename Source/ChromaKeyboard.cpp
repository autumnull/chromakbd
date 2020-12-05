#include "ChromaKeyboard.h"

class ChromaKeyboard_UpDownButton :
	public Button
{
public:
	ChromaKeyboard_UpDownButton (ChromaKeyboard& c, int d) :
		Button ({}), owner (c), delta (d)
	{ }

	void clicked()
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
		Graphics& g,
		bool shouldDrawButtonAsHighlighted,
		bool shouldDrawButtonAsDown )
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
	ChromaKeyboard& owner;
	const int delta;

	JUCE_DECLARE_NON_COPYABLE (ChromaKeyboard_UpDownButton)
};

// TODO: change note state array to int counter array to allow multiple pressers

// TODO: only repaint required notes

// TODO add methods to change options like octave size, guitar mode

ChromaKeyboard::ChromaKeyboard(MidiKeyboardState& s, ChromaKeyboard::Orientation o) :
		state(s),
		orientation(o)
{
	scrollDown.reset (new ChromaKeyboard_UpDownButton (*this, -1));
	scrollUp  .reset (new ChromaKeyboard_UpDownButton (*this, 1));

	addChildComponent (scrollDown.get());
	addChildComponent (scrollUp.get());

	octaveSize = 12;

	bool guitarMode = true;
	char layout[] = ";qjkxbmwvzaoeuidhtns',.pyfgcrl123456789*";
	if (guitarMode) {
		for (int row = 0; row < 4; row++) {
			for (int fret = 0; fret < 10; fret++) {
				char c = layout[row*10 + fret];
				int note = row*5 + fret;
				keycodeState.set(c, KeyPress::isKeyCurrentlyDown(c));
				mapKeycodeToKey(c, note);
			}
		}
	}
	else {
		int note = 0;
		for (char c : layout) {
			keycodeState.set(c, KeyPress::isKeyCurrentlyDown(c));
			mapKeycodeToKey(c, note++);
		}
	}

	mouseOverNotes.insertMultiple(0, -1, 32);
	mouseDownNotes.insertMultiple(0, -1, 32);

	colourChanged();
	setWantsKeyboardFocus(true);	// enables recieving keypresses

	state.addListener(this);

	startTimerHz(30);
}

ChromaKeyboard::~ChromaKeyboard()
{
	state.removeListener (this);
}

void ChromaKeyboard::setVelocity (float v, bool useMousePosition)
{
	velocity = jlimit (0.0f, 1.0f, v);
	useMousePositionForVelocity = useMousePosition;
}

void ChromaKeyboard::setMidiChannel (int midiChannelNumber)
{
	jassert (0 < midiChannelNumber && midiChannelNumber <= 16);
	if (midiChannel != midiChannelNumber) {
		resetAnyKeysInUse();
		midiChannel = jlimit (1, 16, midiChannelNumber);
	}
}

int ChromaKeyboard::getMidiChannel() const noexcept { return midiChannel; }

void ChromaKeyboard::setMidiChannelsToDisplay (int midiChannelMask)
{
	midiInChannelMask = midiChannelMask;
	shouldCheckState = true;
}

int ChromaKeyboard::getMidiChannelsToDisplay() const noexcept { return midiInChannelMask; }

void ChromaKeyboard::setKeyWidth (float widthInPixels)
{
	jassert (widthInPixels > 0);
	// Prevent infinite recursion if the width is being computed in a 'resized()' call-back
	if (keyWidth != widthInPixels) {
		keyWidth = widthInPixels;
		resized();
	}
}

float ChromaKeyboard::getKeyWidth() const noexcept { return keyWidth; }

void ChromaKeyboard::setScrollButtonWidth (int widthInPixels)
{
	jassert (widthInPixels > 0);
	if (scrollButtonWidth != widthInPixels) {
		scrollButtonWidth = widthInPixels;
		resized();
	}
}

int ChromaKeyboard::getScrollButtonWidth() const noexcept { return scrollButtonWidth; }

void ChromaKeyboard::setOrientation (ChromaKeyboard::Orientation newOrientation)
{
	if (orientation != newOrientation) {
		orientation = newOrientation;
		resized();
	}
}

ChromaKeyboard::Orientation ChromaKeyboard::getOrientation() const noexcept { return orientation; }

void ChromaKeyboard::setAvailableRange (int lowestNote, int highestNote)
{
	jassert (lowestNote >= 0 && lowestNote <= 127);
	jassert (highestNote >= 0 && highestNote <= 127);
	jassert (lowestNote <= highestNote);

	if (rangeStart != lowestNote || rangeEnd != highestNote) {
		rangeStart = jlimit (0, 127, lowestNote);
		rangeEnd = jlimit (0, 127, highestNote);
		firstKey = jlimit ((float) rangeStart, (float) rangeEnd, firstKey);
		resized();
	}
}

int ChromaKeyboard::getRangeStart() const noexcept { return rangeStart; }

int ChromaKeyboard::getRangeEnd() const noexcept { return rangeEnd; }

void ChromaKeyboard::setLowestVisibleKey (int noteNumber)
{
	setLowestVisibleKeyFloat ((float) noteNumber);
}

int ChromaKeyboard::getLowestVisibleKey() const noexcept { return (int) firstKey; }

void ChromaKeyboard::setScrollButtonsVisible (bool newCanScroll)
{
	if (canScroll != newCanScroll) {
		canScroll = newCanScroll;
		resized();
	}
}

float ChromaKeyboard::getKeyStartPosition (int midiNoteNumber) const
{
	return getKeyPos(midiNoteNumber).getStart();
}

float ChromaKeyboard::getTotalKeyboardWidth() const noexcept
{
	return getKeyPos(rangeEnd).getEnd();
}

int ChromaKeyboard::getNoteAtPosition (Point<float> position)
{
	float v;
	return xyToNote (position, v);
}

void ChromaKeyboard::clearKeyMappings()
{
	resetAnyKeysInUse();
	keycodeToMidiKey.clear();
}

void ChromaKeyboard::mapKeycodeToKey (int keycode, int midiNoteOffsetFromC)
{
	keycodeToMidiKey.set(keycode, midiNoteOffsetFromC);
}

void ChromaKeyboard::unmapKeycode (int keycode)
{
	keycodeToMidiKey.remove(keycode);
}

void ChromaKeyboard::setKeyPressBaseNote (int newBaseNote)
{
	jassert (newBaseNote >= 0 && newBaseNote < 128);
	keyMappingBase = newBaseNote;
}

/*
 * Component
 */

void ChromaKeyboard::paint (Graphics& graphics)
{
	graphics.fillAll (findColour (whiteNoteColourId));

	auto lineColour = Colour(0x55000000);
	auto textColour = findColour (textLabelColourId);
	for (float jNote = 0; jNote < 128; jNote ++) {
		auto noteColour = getNoteColour(jNote, octaveSize);
			drawKey (
				jNote,
				graphics,
				getRectangleForKey (jNote),
				state.isNoteOnForChannels (midiInChannelMask, jNote),
				mouseOverNotes.contains (jNote),
				noteColour,
				lineColour,
				textColour);
	}

	auto width = getWidth();
	auto height = getHeight();

	auto x = getKeyPos(rangeEnd).getEnd();

	graphics.setColour(lineColour);

	// Line at the bottom of keys
	switch (orientation) {
		case horizontal:
			graphics.fillRect(0.0f, height - 1.0f, x, 1.0f);
			break;
		case verticalFacingLeft:
			graphics.fillRect(0.0f, 0.0f, 1.0f, x);
			break;
		case verticalFacingRight:
			graphics.fillRect(width - 1.0f, 0.0f, 1.0f, x);
			break;
		default:
			break;
	}
}

void ChromaKeyboard::resized()
{
	auto w = getWidth();
	auto h = getHeight();

	if (w > 0 && h > 0) {
		if (orientation != horizontal)
			std::swap (w, h);

		auto kx2 = getKeyPos (rangeEnd).getEnd();
		if ((int) firstKey != rangeStart) {
			auto kx1 = getKeyPos (rangeStart).getStart();
			if (kx2 - kx1 <= w) {
				firstKey = (float) rangeStart;
				sendChangeMessage();
				repaint();
			}
		}

		scrollDown->setVisible (canScroll && firstKey > (float) rangeStart);
		xOffset = 0;
		if (canScroll) {
			auto scrollButtonW = jmin (scrollButtonWidth, w / 2);
			auto r = getLocalBounds();

			if (orientation == horizontal) {
				scrollDown->setBounds (r.removeFromLeft  (scrollButtonW));
				scrollUp  ->setBounds (r.removeFromRight (scrollButtonW));
			}
			else if (orientation == verticalFacingLeft) {
				scrollDown->setBounds (r.removeFromTop    (scrollButtonW));
				scrollUp  ->setBounds (r.removeFromBottom (scrollButtonW));
			}
			else {
				scrollDown->setBounds (r.removeFromBottom (scrollButtonW));
				scrollUp  ->setBounds (r.removeFromTop    (scrollButtonW));
			}

			auto endOfLastKey = getKeyPos (rangeEnd).getEnd();

			float mousePositionVelocity;
			auto spaceAvailable = w;
			auto lastStartKey = remappedXYToNote ({ endOfLastKey - spaceAvailable, 0 }, mousePositionVelocity) + 1;

			if (lastStartKey >= 0 && ((int) firstKey) > lastStartKey) {
				firstKey = (float) jlimit (rangeStart, rangeEnd, lastStartKey);
				sendChangeMessage();
			}
			xOffset = getKeyPos ((int) firstKey).getStart();
		}
		else {
			firstKey = (float) rangeStart;
		}

		scrollUp->setVisible (canScroll && getKeyPos (rangeEnd).getStart() > w);
		repaint();
	}
}

void ChromaKeyboard::mouseMove (const MouseEvent& e)
{
	updateNoteUnderMouse (e, false);
}

void ChromaKeyboard::mouseDrag (const MouseEvent& e)
{
	float mousePositionVelocity;
	auto newNote = xyToNote (e.position, mousePositionVelocity);

	updateNoteUnderMouse (e, true);
}

void ChromaKeyboard::mouseDown (const MouseEvent& e)
{
	float mousePositionVelocity;
	auto newNote = xyToNote (e.position, mousePositionVelocity);

	if (newNote >= 0 && mouseDownOnKey (newNote, e))
		updateNoteUnderMouse (e, true);
}


void ChromaKeyboard::mouseUp (const MouseEvent& e)
{
	updateNoteUnderMouse (e, false);

	float mousePositionVelocity;
	auto note = xyToNote (e.position, mousePositionVelocity);

	if (note >= 0)
		mouseUpOnKey (note, e);
}

void ChromaKeyboard::mouseEnter (const MouseEvent& e)
{
	updateNoteUnderMouse (e, false);
}

void ChromaKeyboard::mouseExit (const MouseEvent& e)
{
	updateNoteUnderMouse (e, false);
}

void ChromaKeyboard::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
	auto amount = (orientation == horizontal && wheel.deltaX != 0)
		? wheel.deltaX : (
			orientation == verticalFacingLeft
			? wheel.deltaY : -wheel.deltaY
		);

	setLowestVisibleKeyFloat (firstKey - amount * keyWidth);
}

void ChromaKeyboard::colourChanged()
{
	setOpaque(findColour(whiteNoteColourId).isOpaque());
	repaint();
}

// called when a physical key is pressed
bool ChromaKeyboard::keyPressed (const KeyPress& keypress)
{
	return keycodeToMidiKey.contains(keypress.getKeyCode());
}

// called when a physical key state changes
bool ChromaKeyboard::keyStateChanged (bool isKeyDown)
{
	char keycode = findKeycodeChanged();
	if (!keycode)
		return false;

	int midiKey = keyMappingBase + keycodeToMidiKey.getReference(keycode);
	if (isKeyDown) {
		if (midiKeysPressed[midiKey] == 0)
			state.noteOn(midiChannel, midiKey, velocity);
		midiKeysPressed.getReference(midiKey)++;
	} else {
		midiKeysPressed.getReference(midiKey)--;
		if (midiKeysPressed[midiKey] == 0)
			state.noteOff(midiChannel, midiKey, velocity);
	}
	return true;
}

void ChromaKeyboard::focusLost (FocusChangeType)
{
	resetAnyKeysInUse();
}

/*
 * Timer
 */

void ChromaKeyboard::timerCallback()
{
	if (shouldCheckState) {
		shouldCheckState = false;
		for (int jKey = rangeStart; jKey <= rangeEnd; jKey++) {
			bool isOn = state.isNoteOnForChannels (midiInChannelMask, jKey);

			if (keysCurrentlyShownPressed[jKey] != isOn) {
				keysCurrentlyShownPressed.setBit(jKey, isOn);
					repaintKey(jKey);
			}
		}
	}
}

/*
 * MidiKeyboardState::Listener
 */

void ChromaKeyboard::handleNoteOn (
	MidiKeyboardState*,
	int midiChan,
	int midiNoteNumber,
	float v
)
{
	shouldCheckState = true;
}

void ChromaKeyboard::handleNoteOff (
	MidiKeyboardState*,
	int midiChan,
	int midiNoteNumber,
	float v )
{
	shouldCheckState = true;
}

void ChromaKeyboard::drawKey (
	int midiKeyNumber,
	Graphics& g,
	Rectangle<float> area,
	bool isDown,
	bool isOver,
	Colour keyColour,
	Colour lineColour,
	Colour textColour )
{
	auto c = keyColour;

	if (isOver)
		c = c.overlaidWith(Colour(0x55FFFFFF));
	if (isDown)
		c = c.overlaidWith(Colour(0x77FFFFFF));

	g.setColour (c);
	g.fillRect (area);

	auto text = getNoteText (midiKeyNumber);

	if (text.isNotEmpty()) {
		auto fontHeight = jmin (12.0f, keyWidth * 0.9f);

		g.setColour (textColour);
		g.setFont (Font (fontHeight).withHorizontalScale (0.8f));

		switch (orientation) {
			case horizontal:
				g.drawText(
					text,
					area.withTrimmedLeft(1.0f).withTrimmedBottom(2.0f),
					Justification::centredBottom,
					false );
				break;
			case verticalFacingLeft:
				g.drawText(
					text,
					area.reduced(2.0f),
					Justification::centredLeft,
					false );
				break;
			case verticalFacingRight:
				g.drawText(
					text,
					area.reduced(2.0f),
					Justification::centredRight,
					false );
				break;
			default:
				break;
		}
	}

	if (! lineColour.isTransparent()) {
		g.setColour (lineColour);

		switch (orientation) {
			case horizontal:
				g.fillRect(area.withWidth(1.0f));
				break;
			case verticalFacingLeft:
				g.fillRect(area.withHeight(1.0f));
				break;
			case verticalFacingRight:
				g.fillRect(area.removeFromBottom(1.0f));
				break;
			default:
				break;
		}

		if (midiKeyNumber == rangeEnd)
		{
			switch (orientation) {
				case horizontal:
					g.fillRect(area.expanded(1.0f, 0).removeFromRight(1.0f));
					break;
				case verticalFacingLeft:
					g.fillRect(area.expanded(0, 1.0f).removeFromBottom(1.0f));
					break;
				case verticalFacingRight:
					g.fillRect(area.expanded(0, 1.0f).removeFromTop(1.0f));
					break;
				default:
					break;
			}
		}
	}
}

String ChromaKeyboard::getNoteText (int midiNoteNumber)
{
	auto octave = midiNoteNumber / octaveSize;
	if (midiNoteNumber % octaveSize == 0)
		return String(octave);

	return {};
}

void ChromaKeyboard::drawUpDownButton (
	Graphics& g,
	int w,
	int h,
	bool isMouseOver,
	bool isButtonPressed,
	bool movesOctavesUp )
{
	g.fillAll (findColour (upDownButtonBackgroundColourId));

	float angle = 0;
	switch (orientation) {
		case horizontal:
			angle = movesOctavesUp ? 0.0f : 0.5f;
			break;
		case verticalFacingLeft:
			angle = movesOctavesUp ? 0.25f : 0.75f;
			break;
		case verticalFacingRight:
			angle = movesOctavesUp ? 0.75f : 0.25f;
			break;
		default:
			jassertfalse;
			break;
	}

	auto tau = MathConstants<float>::twoPi;

	Path path;
	path.addTriangle(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f);
	path.applyTransform(AffineTransform::rotation (tau * angle, 0.5f, 0.5f));

	g.setColour(findColour(upDownButtonArrowColourId).withAlpha(
		isButtonPressed
		? 1.0f : (
			isMouseOver
			? 0.6f : 0.4f
			)
		));
	g.fillPath(path, path.getTransformToScaleToFit (1.0f, 1.0f, w - 2.0f, h - 2.0f, true));
}

bool ChromaKeyboard::mouseDownOnKey (int midiNoteNumber, const MouseEvent& e)
{
	return true;
}

bool ChromaKeyboard::mouseDraggedToKey (int midiNoteNumber, const MouseEvent& e)
{
	return true;
}

void ChromaKeyboard::mouseUpOnKey (int midiNoteNumber, const MouseEvent& e)
{ }

Range<float> ChromaKeyboard::getKeyPosition (int midiNoteNumber, float targetKeyWidth) const
{
	jassert (midiNoteNumber >= 0 && midiNoteNumber < 128);

	auto octave = midiNoteNumber / octaveSize;
	auto note   = midiNoteNumber % octaveSize;
	auto start = octave * octaveSize * targetKeyWidth + (float)note * targetKeyWidth;

	return { start, start + targetKeyWidth };
}

Rectangle<float> ChromaKeyboard::getRectangleForKey (int midiNoteNumber) const
{
	jassert (midiNoteNumber >= rangeStart && midiNoteNumber <= rangeEnd);

	auto pos = getKeyPos (midiNoteNumber);
	auto x = pos.getStart();
	auto w = pos.getLength();
	switch (orientation) {
		case horizontal:
			return {x, 0, w, (float) getHeight()};
		case verticalFacingLeft:
			return {0, x, (float) getWidth(), w};
		case verticalFacingRight:
			return {0, getHeight() - x - w, (float) getWidth(), w};
		default:
			jassertfalse;
			break;
	}

	return {};
}

Colour ChromaKeyboard::getNoteColour(int note, int base)
{
	float index = (note % base)/(float)base;
	for (int j = 0; j <= 12; j++) {
		if (j/13. <= index && index < (j+1)/12.) {
			float r = (12*index - j);
			Colour c = Colour(keyColours[j]).interpolatedWith(
				Colour(keyColours[j+1]),
				r );
			return c;
		}
	}
	return Colours::black;
}

Range<float> ChromaKeyboard::getKeyPos (int midiNoteNumber) const
{
	return getKeyPosition(midiNoteNumber, keyWidth)
		- xOffset
		- getKeyPosition(rangeStart, keyWidth).getStart();
}

int ChromaKeyboard::xyToNote (Point<float> pos, float& mousePositionVelocity)
{
	if (! reallyContains (pos.toInt(), false))
		return -1;

	auto p = pos;
	if (orientation != horizontal) {
		p = { p.y, p.x };
		if (orientation == verticalFacingLeft)
			p = { p.x, getWidth() - p.y };
		else
			p = { getHeight() - p.x, p.y };
	}
	return remappedXYToNote (p + Point<float> (xOffset, 0), mousePositionVelocity);
}

int ChromaKeyboard::remappedXYToNote (Point<float> pos, float& mousePositionVelocity) const
{
	for (int note = rangeStart; note <= rangeEnd; note++) {
		if (getKeyPos (note).contains (pos.x - xOffset)) {
			auto noteLength = (orientation == horizontal) ? getHeight() : getWidth();
			mousePositionVelocity = jmax(0.0f, pos.y/noteLength);
			return note;
		}
	}
	mousePositionVelocity = 0;
	return -1;
}

void ChromaKeyboard::resetAnyKeysInUse()
{
	for (int i = 128; --i >= 0;)
		if (midiKeysPressed[i])
			state.noteOff (midiChannel, i, 0.0f);
	midiKeysPressed.clear();

	for (int i = mouseDownNotes.size(); --i >= 0;) {
		auto noteDown = mouseDownNotes.getUnchecked(i);
		if (noteDown >= 0) {
			state.noteOff (midiChannel, noteDown, 0.0f);
			mouseDownNotes.set (i, -1);
		}
		mouseOverNotes.set (i, -1);
	}
}

void ChromaKeyboard::updateNoteUnderMouse (Point<float> pos, bool isDown, int fingerNum)
{
	float mousePositionVelocity = 0.0f;
	auto newNote = xyToNote (pos, mousePositionVelocity);
	auto oldNote = mouseOverNotes.getUnchecked (fingerNum);
	auto oldNoteDown = mouseDownNotes.getUnchecked (fingerNum);
	auto eventVelocity = useMousePositionForVelocity ? mousePositionVelocity * velocity : velocity;

	if (oldNote != newNote) {
			repaintKey (oldNote);
			repaintKey (newNote);
		mouseOverNotes.set (fingerNum, newNote);
	}

	if (isDown) {
		if (newNote != oldNoteDown) {
			if (oldNoteDown >= 0) {
				mouseDownNotes.set (fingerNum, -1);
				if (! mouseDownNotes.contains (oldNoteDown))
					state.noteOff (midiChannel, oldNoteDown, eventVelocity);
			}

			if (newNote >= 0 && ! mouseDownNotes.contains (newNote)) {
				state.noteOn (midiChannel, newNote, eventVelocity);
				mouseDownNotes.set (fingerNum, newNote);
			}
		}
	}
	else if (oldNoteDown >= 0) {
		mouseDownNotes.set (fingerNum, -1);
		if (! mouseDownNotes.contains (oldNoteDown))
			state.noteOff (midiChannel, oldNoteDown, eventVelocity);
	}
}

void ChromaKeyboard::updateNoteUnderMouse (const MouseEvent& e, bool isDown)
{
	updateNoteUnderMouse (e.getEventRelativeTo (this).position, isDown, e.source.getIndex());
}

void ChromaKeyboard::repaintKey (int midiNoteNumber)
{
	if (midiNoteNumber >= rangeStart && midiNoteNumber <= rangeEnd)
		repaint (getRectangleForKey (midiNoteNumber).getSmallestIntegerContainer());
}

void ChromaKeyboard::setLowestVisibleKeyFloat (float noteNumber)
{
	noteNumber = jlimit ((float) rangeStart, (float) rangeEnd, noteNumber);
	if (noteNumber != firstKey) {
		bool hasMoved = (int)firstKey != (int)noteNumber;
		firstKey = noteNumber;
		if (hasMoved)
			sendChangeMessage();

		resized();
	}
}

int ChromaKeyboard::findKeycodeChanged() {
	char keycode = '\0';
	HashMap<char, bool>::Iterator iter = keycodeState.begin();
	do {
		if (iter.getValue() != KeyPress::isKeyCurrentlyDown(iter.getKey())) {
			keycode = iter.getKey ();
			break;
		}
	} while (iter.next());
	if (keycode) {
		bool& entry = keycodeState.getReference(keycode);
		entry = !entry;
	}
	return keycode;
}