#include "ChromaKeyboard.h"

class ChromaKeyboard_ScrollButton :
	public Button
{
public:
	ChromaKeyboard_ScrollButton (ChromaKeyboard& c, int d) :
		Button ({}), owner (c), delta (d)
	{ }

	void clicked()
	{
		auto note = owner.getLowestVisibleKey();
		if (delta < 0)
			note = (note - 1)/owner.octaveSize;
		else
			note = note/owner.octaveSize + 1;
		owner.setLowestVisibleKey(note*owner.octaveSize);
	}

	using Button::clicked;

	void paintButton (
		Graphics& g,
		bool shouldDrawButtonAsHighlighted,
		bool shouldDrawButtonAsDown )
	{
		owner.drawScrollButton(
			g,
			getWidth(),
			getHeight(),
			shouldDrawButtonAsHighlighted,
			shouldDrawButtonAsDown,
			delta>0
		);
	}

private:
	ChromaKeyboard& owner;
	const int delta;

	JUCE_DECLARE_NON_COPYABLE(ChromaKeyboard_ScrollButton)
};

ChromaKeyboard::ChromaKeyboard(MidiKeyboardState& s, ChromaKeyboard::Orientation o) :
		state(s),
		orientation(o)
{
	addAndMakeVisible(layoutSelector);
	layoutSelector.setWantsKeyboardFocus(false);	// otherwise takes up/down buttons
	layoutSelector.addItemList(layoutNames, 1); // offset must be > 0
	layoutSelector.onChange = [this] {
	  setLayout((Layout)layoutSelector.getSelectedId());
	};
	layoutSelector.setSelectedId(1);
	addAndMakeVisible(layoutSelectorLabel);
	layoutSelectorLabel.attachToComponent(&layoutSelector, true);

	scrollDown.reset(new ChromaKeyboard_ScrollButton (*this, -1));
	scrollUp  .reset(new ChromaKeyboard_ScrollButton (*this, 1));
	addChildComponent(scrollDown.get());
	addChildComponent(scrollUp.get());

	midiKeysPressed.insertMultiple(0, 0, 256);
	keycodeToKey.insertMultiple(0, -1, 256);
	resetKeycodeStates();

	colourChanged();
	setWantsKeyboardFocus(true);	// enables recieving keypresses

	state.addListener(this);

	startTimerHz(40);
}

ChromaKeyboard::~ChromaKeyboard()
{
	state.removeListener (this);
}

void ChromaKeyboard::setVelocity(float v, bool useMousePosition)
{
	velocity = jlimit (0.0f, 1.0f, v);
	useMousePositionForVelocity = useMousePosition;
}

void ChromaKeyboard::setMidiChannel(int midiChannelNumber)
{
	jassert (0 < midiChannelNumber && midiChannelNumber <= 16);
	if (midiChannel != midiChannelNumber) {
		resetAnyKeysInUse();
		midiChannel = jlimit (1, 16, midiChannelNumber);
	}
}

int ChromaKeyboard::getMidiChannel() const noexcept { return midiChannel; }

void ChromaKeyboard::setMidiChannelsToDisplay(int midiChannelMask)
{
	midiInChannelMask = midiChannelMask;
	shouldCheckState = true;
}

int ChromaKeyboard::getMidiChannelsToDisplay() const noexcept { return midiInChannelMask; }

void ChromaKeyboard::setKeyWidth(float widthInPixels)
{
	jassert (widthInPixels > 0);
	// Prevent infinite recursion if the width is being computed in a 'resized()' call-back
	if (keyWidth != widthInPixels) {
		keyWidth = widthInPixels;
		resized();
	}
}

float ChromaKeyboard::getKeyWidth() const noexcept { return keyWidth; }

void ChromaKeyboard::setScrollButtonWidth(int widthInPixels)
{
	jassert (widthInPixels > 0);
	if (scrollButtonWidth != widthInPixels) {
		scrollButtonWidth = widthInPixels;
		resized();
	}
}

int ChromaKeyboard::getScrollButtonWidth() const noexcept { return scrollButtonWidth; }

void ChromaKeyboard::setOrientation(ChromaKeyboard::Orientation newOrientation)
{
	if (orientation != newOrientation) {
		orientation = newOrientation;
		resized();
	}
}

ChromaKeyboard::Orientation ChromaKeyboard::getOrientation() const noexcept { return orientation; }

void ChromaKeyboard::setAvailableRange(int lowestNote, int highestNote)
{
	jassert (0 <= lowestNote  && lowestNote  < 128);
	jassert (0 <= highestNote && highestNote < 128);
	jassert (lowestNote <= highestNote);

	if (rangeStart != lowestNote || rangeEnd != highestNote) {
		rangeStart = jlimit (0, 127, lowestNote);
		rangeEnd = jlimit (0, 127, highestNote);
		lowestVisibleKey = jlimit ((float) rangeStart, (float) rangeEnd, (float) lowestVisibleKey);
		resized();
	}
}

int ChromaKeyboard::getRangeStart() const noexcept { return rangeStart; }

int ChromaKeyboard::getRangeEnd() const noexcept { return rangeEnd; }

void ChromaKeyboard::setLowestVisibleKey(int noteNumber)
{
	setLowestVisibleKeyFloat ((float) noteNumber);
}

int ChromaKeyboard::getLowestVisibleKey() const noexcept { return (int) lowestVisibleKey; }

void ChromaKeyboard::setScrollButtonsVisible(bool newCanScroll)
{
	if (canScroll != newCanScroll) {
		canScroll = newCanScroll;
		resized();
	}
}

float ChromaKeyboard::getKeyStartPosition(int midiNoteNumber) const
{
	return getKeyPos(midiNoteNumber).getStart();
}

float ChromaKeyboard::getTotalKeyboardWidth() const noexcept
{
	return getKeyPos(rangeEnd).getEnd();
}

int ChromaKeyboard::getNoteAtPosition(Point<float> position)
{
	float v;
	return xyToNote (position, v);
}

void ChromaKeyboard::clearKeyMappings()
{
	resetAnyKeysInUse();
	keycodeToKey.fill(-1);
}

void ChromaKeyboard::mapKeycodeToMidiKey(int keycode, int midiKey)
{
	if (rangeStart <= midiKey && midiKey <= rangeEnd)
		keycodeToKey.set(keycode, midiKey);
	else
		unmapKeycode(keycode);
}

void ChromaKeyboard::unmapKeycode(int keycode)
{
	keycodeToKey.set(keycode, -1);
}

void ChromaKeyboard::setKeyMapBase(int newBaseNote)
{
	jassert (newBaseNote >= 0 && newBaseNote < 128);
	keyMapBase = newBaseNote;
	setLayout(currentLayout);
}

void ChromaKeyboard::shiftKeyMapBase(int offset)
{
	setKeyMapBase(keyMapBase+offset);
}

void ChromaKeyboard::setLayout(Layout newLayout)
{
	if (newLayout != (Layout)layoutSelector.getSelectedId())
		layoutSelector.setSelectedId((int)newLayout);
	int xStep, yStep;
	switch (newLayout)
	{
	case linear:
		xStep = 1, yStep = 10;
		break;
	case guitar:
		xStep = 1, yStep = 5;
		break;
	case organ:
		setOrganLayout();
		return;
	case harpejji:
		xStep = 2, yStep = 1;
		break;
	case hexagonal:
		xStep = 4, yStep = 3;
		break;
	default:
		return;
	}
	clearKeyMappings();
	for (int jRow = 0; jRow < 4; jRow++) {
		for (int jCol = 0; jCol < 10; jCol++) {
			char c = kbdString[jRow*10 + jCol];
			int midiKey = keyMapBase + jCol*xStep + jRow*yStep;
			mapKeycodeToMidiKey(c, midiKey);
		}
	}
	currentLayout = newLayout;
}

ChromaKeyboard::Layout ChromaKeyboard::getLayout()
{
	return currentLayout;
}

int ChromaKeyboard::getOctaveSize() const
{
	return octaveSize;
}
void ChromaKeyboard::setOctaveSize(int newSize)
{
	octaveSize = newSize;
	repaint();
}

/*
 * Component
 */

void ChromaKeyboard::paint(Graphics& g)
{
	auto lineColour = Colour(0x55000000);
	auto textColour = Colour(0xffFFFFFF);
	for (float jNote = 0; jNote < 128; jNote ++) {
		auto noteColour = getNoteColour(jNote, octaveSize);
			drawKey (
				jNote,
				g,
				getRectangleForKey(jNote),
				state.isNoteOnForChannels(midiInChannelMask, jNote),
				keyHovered == jNote,
				noteColour,
				lineColour,
				textColour);
	}

	auto width = getWidth();
	auto height = getHeight();

	auto x = getKeyPos(rangeEnd).getEnd();

	g.setColour(lineColour);

	// Line at the bottom of keys
	switch (orientation) {
		case horizontal:
			g.fillRect(0.0f, height - 1.0f, x, 1.0f);
			break;
		case verticalFacingLeft:
			g.fillRect(0.0f, 0.0f, 1.0f, x);
			break;
		case verticalFacingRight:
			g.fillRect(width - 1.0f, 0.0f, 1.0f, x);
			break;
		default:
			break;
	}
}

void ChromaKeyboard::resized()
{
	layoutSelector.setBounds(64, 0, 128, optionBarHeight);

	float w = getWidth();
	float h = getHeight() - optionBarHeight;

	if (w > 0 && h > 0) {
		if (orientation != horizontal)
			std::swap (w, h);

		auto kx2 = getKeyPos(rangeEnd).getEnd();
		if ((int) lowestVisibleKey != rangeStart) {
			auto kx1 = getKeyPos(rangeStart).getStart();
			if (kx2 - kx1 <= w) {
				lowestVisibleKey = (float) rangeStart;
				sendChangeMessage();
				repaint();
			}
		}

		scrollDown->setVisible(canScroll && lowestVisibleKey > (float) rangeStart);
		xOffset = 0;
		if (canScroll) {
			auto scrollButtonW = jmin(scrollButtonWidth, w / 2);
			auto r = getLocalBounds();

			if (orientation == horizontal) {
				r.removeFromTop(optionBarHeight);
				scrollDown->setBounds(r.removeFromLeft  (scrollButtonW));
				scrollUp  ->setBounds(r.removeFromRight (scrollButtonW));
			}
			else if (orientation == verticalFacingLeft) {
				r.removeFromRight(optionBarHeight);
				scrollDown->setBounds(r.removeFromTop    (scrollButtonW));
				scrollUp  ->setBounds(r.removeFromBottom (scrollButtonW));
			}
			else {
				r.removeFromLeft(optionBarHeight);
				scrollDown->setBounds(r.removeFromBottom (scrollButtonW));
				scrollUp  ->setBounds(r.removeFromTop    (scrollButtonW));
			}

			auto endOfLastKey = getKeyPos(rangeEnd).getEnd();

			float mousePositionVelocity;
			auto spaceAvailable = w;
			auto lastStartKey = remappedXYToNote({ endOfLastKey - spaceAvailable, optionBarHeight+1 }, mousePositionVelocity) + 1;

			if (lastStartKey >= 0 && ((int) lowestVisibleKey) > lastStartKey) {
				lowestVisibleKey = (float) jlimit(rangeStart, rangeEnd, lastStartKey);
				sendChangeMessage();
			}
			xOffset = getKeyPos((int) lowestVisibleKey).getStart();
		}
		else {
			lowestVisibleKey = (float) rangeStart;
		}

		scrollUp->setVisible(canScroll && getKeyPos(rangeEnd).getStart() > w);
		repaint();
	}
}

void ChromaKeyboard::mouseMove(const MouseEvent& e)
{
	updateNoteUnderMouse(e.position, false);
}

void ChromaKeyboard::mouseDrag(const MouseEvent& e)
{
	float mousePositionVelocity;
	auto newNote = xyToNote(e.position, mousePositionVelocity);

	updateNoteUnderMouse(e.position, true);
}

void ChromaKeyboard::mouseDown(const MouseEvent& e)
{
	float mousePositionVelocity;
	auto newNote = xyToNote(e.position, mousePositionVelocity);

	if (newNote >= 0 && mouseDownOnKey(newNote, e)) {
		updateNoteUnderMouse(e.position, true);
	}
}


void ChromaKeyboard::mouseUp(const MouseEvent& e)
{
	updateNoteUnderMouse(e.position, false);

	float mousePositionVelocity;
	auto note = xyToNote(e.position, mousePositionVelocity);

	if (note >= 0)
		mouseUpOnKey(note, e);
}

void ChromaKeyboard::mouseEnter(const MouseEvent& e)
{
	updateNoteUnderMouse(e.position, false);
}

void ChromaKeyboard::mouseExit(const MouseEvent& e)
{
	updateNoteUnderMouse(e.position, false);
}

void ChromaKeyboard::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel)
{
	auto amount = (orientation == horizontal && wheel.deltaX != 0)
		? wheel.deltaX : (
			orientation == verticalFacingLeft
			? wheel.deltaY : -wheel.deltaY
		);
	setLowestVisibleKeyFloat(lowestVisibleKey - amount * keyWidth);
}

void ChromaKeyboard::colourChanged()
{
	setOpaque(true);
	repaint();
}

// called when a physical key is pressed or held
bool ChromaKeyboard::keyPressed(const KeyPress& keypress)
{
	int keycode = keypress.getKeyCode();
	if (keycode == KeyPress::escapeKey) {
		resetAnyKeysInUse();
		return true;
	} else if (keycode == KeyPress::upKey) {
		shiftKeyMapBase(1);
	} else if (keycode == KeyPress::downKey) {
		shiftKeyMapBase(-1);
	} else if (keycode == KeyPress::pageUpKey) {
		shiftKeyMapBase(octaveSize);
	} else if (keycode == KeyPress::pageDownKey) {
		shiftKeyMapBase(-octaveSize);
	}
	return keycodeToKey[keycode] == -1;
}

// called when a keycode is pressed, held or released
bool ChromaKeyboard::keyStateChanged(bool)
{
	for (char keycode: kbdString) {
		bool isPressed = KeyPress::isKeyCurrentlyDown(keycode);
		if (keycodeStates[keycode] != isPressed)
		{
			int midiKey = keycodeToKey[keycode];
			if (isPressed) {
				// always add note on when key re-pressed
				state.noteOn(midiChannel, midiKey, velocity);
				midiKeysPressed.getReference(midiKey)++;
			} else {
				midiKeysPressed.getReference(midiKey) = jmax(0,midiKeysPressed[midiKey]-1);
				if (midiKeysPressed[midiKey] == 0)
					state.noteOff(midiChannel, midiKey, velocity);
			}
			keycodeStates.setBit(keycode, isPressed);
		}
	}
	return true;
}

void ChromaKeyboard::focusLost(FocusChangeType cause)
{
	resetAnyKeysInUse();
	repaint();
}

void ChromaKeyboard::focusGained(FocusChangeType cause) {
	repaint();
}

/*
 * Timer
 */

void ChromaKeyboard::timerCallback()
{
	if (shouldCheckState) {
		shouldCheckState = false;
		for (int jKey = rangeStart; jKey <= rangeEnd; jKey++) {
			bool isOn = state.isNoteOnForChannels(midiInChannelMask, jKey);

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

void ChromaKeyboard::handleNoteOn(
	MidiKeyboardState*,
	int midiChan,
	int midiNoteNumber,
	float v )
{
	shouldCheckState = true;
}

void ChromaKeyboard::handleNoteOff(
	MidiKeyboardState*,
	int midiChan,
	int midiNoteNumber,
	float v )
{
	shouldCheckState = true;
}

void ChromaKeyboard::drawKey(
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
		c = c.overlaidWith(Colour(0x55000000));
	if (isDown)
		c = c.overlaidWith(Colour(0x77000000));

	g.setColour(c);
	g.fillRect(area);

	auto text = getNoteText(midiKeyNumber);

	if (text.isNotEmpty()) {
		auto fontHeight = jmin(12.0f, keyWidth * 0.9f);

		g.setColour(textColour);
		g.setFont(Font(fontHeight).withHorizontalScale (0.8f));

		switch (orientation) {
			case horizontal:
				g.drawText(
					text,
					area.withTrimmedLeft(1.0f).withTrimmedBottom(2.0f),
					Justification::topLeft,
					false );
				break;
			case verticalFacingLeft:
				g.drawText(
					text,
					area.reduced(2.0f),
					Justification::topRight,
					false );
				break;
			case verticalFacingRight:
				g.drawText(
					text,
					area.reduced(2.0f),
					Justification::bottomLeft,
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

String ChromaKeyboard::getNoteText(int midiNoteNumber)
{
	auto octave = midiNoteNumber / octaveSize;
	if (midiNoteNumber % octaveSize == 0)
		return String(octave);

	return {};
}

void ChromaKeyboard::drawScrollButton(
	Graphics& g,
	int w,
	int h,
	bool isMouseOver,
	bool isButtonPressed,
	bool movesOctavesUp )
{
	g.fillAll(Colour(0xffD3D3D3));

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
	path.applyTransform(AffineTransform::rotation(tau * angle, 0.5f, 0.5f));

	g.setColour(Colour(0xff272727).withAlpha(
		isButtonPressed
		? 1.0f : (
			isMouseOver
			? 0.6f : 0.4f
			)
		));
	g.fillPath(path, path.getTransformToScaleToFit(1.0f, 1.0f, w - 2.0f, h - 2.0f, true));
}

/*
 * 3 callback functions
 */
bool ChromaKeyboard::mouseDownOnKey(int midiNoteNumber, const MouseEvent& e)
{
	return true;
}

bool ChromaKeyboard::mouseDraggedToKey(int midiNoteNumber, const MouseEvent& e)
{
	return true;
}

void ChromaKeyboard::mouseUpOnKey(int midiNoteNumber, const MouseEvent& e)
{ }

Range<float> ChromaKeyboard::getKeyPosition(int midiNoteNumber, float targetKeyWidth) const
{
	jassert (midiNoteNumber >= 0 && midiNoteNumber < 128);

	auto octave = midiNoteNumber / octaveSize;
	auto note   = midiNoteNumber % octaveSize;
	auto start = octave * octaveSize * targetKeyWidth + (float)note * targetKeyWidth;

	return { start, start + targetKeyWidth };
}

Rectangle<float> ChromaKeyboard::getRectangleForKey(int midiNoteNumber) const
{
	jassert (midiNoteNumber >= rangeStart && midiNoteNumber <= rangeEnd);

	auto pos = getKeyPos (midiNoteNumber);
	auto x = pos.getStart();
	auto w = pos.getLength();
	switch (orientation) {
		case horizontal:
			return {x, optionBarHeight, w, (float) getHeight()};
		case verticalFacingLeft:
			return {0, x, (float) getWidth()-optionBarHeight, w};
		case verticalFacingRight:
			return {optionBarHeight, getHeight() - x - w, (float) getWidth(), w};
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

Range<float> ChromaKeyboard::getKeyPos(int midiNoteNumber) const
{
	return getKeyPosition(midiNoteNumber, keyWidth)
		- xOffset
		- getKeyPosition(rangeStart, keyWidth).getStart();
}

int ChromaKeyboard::xyToNote(Point<float> pos, float& mousePositionVelocity)
{
	if (! reallyContains(pos.toInt(), false))
		return -1;

	auto p = pos;
	if (orientation != horizontal) {
		p = { p.y, p.x };
		if (orientation == verticalFacingLeft)
			p = { p.x, getWidth() - p.y };
		else
			p = { getHeight() - p.x, p.y };
	}
	return remappedXYToNote(p + Point<float> (xOffset, 0), mousePositionVelocity);
}

int ChromaKeyboard::remappedXYToNote(Point<float> pos, float& mousePositionVelocity) const
{
	if (pos.y <= optionBarHeight)
		return -1;

	for (int note = rangeStart; note <= rangeEnd; note++) {
		if (getKeyPos(note).contains(pos.x - xOffset)) {
			auto noteLength = ((orientation == horizontal) ? getHeight() : getWidth()) - optionBarHeight;
			mousePositionVelocity = jmax(0.0f, (pos.y - optionBarHeight)/noteLength);
			return note;
		}
	}
	mousePositionVelocity = 0;
	return -1;
}

void ChromaKeyboard::resetAnyKeysInUse()
{
	midiKeysPressed.getLock().enter();
	for (int i = 128; --i >= 0;)
		if (midiKeysPressed[i]) {
			state.noteOff(midiChannel, i, 0.0f);
			midiKeysPressed.set(i, 0);
		}
	midiKeysPressed.getLock().exit();

	if (keyClicked >= 0) {
		state.noteOff(midiChannel, keyClicked, 0.0f);
		keyClicked = -1;
	}
	keyHovered = -1;
}

void ChromaKeyboard::updateNoteUnderMouse(Point<float> pos, bool isDown)
{
	float mousePositionVelocity = 0.0f;
	auto newKey = xyToNote(pos, mousePositionVelocity);
	auto eventVelocity = useMousePositionForVelocity ? mousePositionVelocity*velocity : velocity;

	if (keyHovered != newKey) {
		repaintKey(keyHovered);
		repaintKey(newKey);
		keyHovered = newKey;
	}

	if (isDown) {
		if (newKey != keyClicked) {
			if (keyClicked >= 0)
				state.noteOff(midiChannel, keyClicked, eventVelocity);
			if (newKey >= 0)
				state.noteOn(midiChannel, newKey, eventVelocity);
			keyClicked = jmax(newKey, -1);
		}
	}
	else if (keyClicked >= 0) {
		state.noteOff(midiChannel, keyClicked, eventVelocity);
		keyClicked = -1;
	}
}

void ChromaKeyboard::repaintKey(int midiNoteNumber)
{
	if (midiNoteNumber >= rangeStart && midiNoteNumber <= rangeEnd)
		repaint(getRectangleForKey(midiNoteNumber).getSmallestIntegerContainer());
}

void ChromaKeyboard::setLowestVisibleKeyFloat(float keyNumber)
{
	keyNumber = jlimit ((float) rangeStart, (float) rangeEnd, keyNumber);
	if (keyNumber != lowestVisibleKey) {
		bool hasMoved = (int)lowestVisibleKey != (int)keyNumber;
		lowestVisibleKey = keyNumber;
		if (hasMoved)
			sendChangeMessage();
		resized();
	}
}

void ChromaKeyboard::resetKeycodeStates() {
	for (int jKey = 0; jKey < 256; jKey++) {
		keycodeStates.setBit(jKey, KeyPress::isKeyCurrentlyDown(jKey));
	}
}

void ChromaKeyboard::setOrganLayout() {
	// this only really makes sense if the octave size is 12,
	// but I've tried to make it work roughly logically
	// even if that isn't the case
	clearKeyMappings();
	int x = 0xdead;	// dead keys don't get mapped
	int blackKeys[7] = { x, 1, 3, x, 6, 8, 10 };
	int whiteKeys[7] = { 0, 2, 4, 5, 7, 9, 11 };	// an octave is of course *7* white keys
	for (int jRow = 0; jRow < 4; jRow++) {
		for (int jCol = 0; jCol < 10; jCol++) {
			char keycode = kbdString[jRow*10 + jCol];
			int index = (jRow/2 * 10 + jCol);	// index if the arrays weren't cyclic
			int offset = (jRow % 2 == 0 ? whiteKeys : blackKeys)[index % 7];
			if (offset == 0xdead)
				continue;
			int midiKey = keyMapBase + offset + (index/7)*12;
			mapKeycodeToMidiKey(keycode, midiKey);
		}
	}
	currentLayout = organ;
}