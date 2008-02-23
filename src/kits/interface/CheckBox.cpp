/*
 * Copyright 2001-2007, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 */

/*!	BCheckBox displays an on/off control. */


#include <CheckBox.h>

#include <LayoutUtils.h>
#include <Window.h>


BCheckBox::BCheckBox(BRect frame, const char *name, const char *label,
		BMessage *message, uint32 resizingMode, uint32 flags)
	: BControl(frame, name, label, message, resizingMode, flags),
	  fPreferredSize(),
	  fOutlined(false)
{
	// Resize to minimum height if needed
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float minHeight = (float)ceil(6.0f + fontHeight.ascent
		+ fontHeight.descent);
	if (Bounds().Height() < minHeight)
		ResizeTo(Bounds().Width(), minHeight);
}


BCheckBox::BCheckBox(const char *name, const char *label, BMessage *message,
	uint32 flags)
	: BControl(name, label, message, flags | B_WILL_DRAW | B_NAVIGABLE),
	  fPreferredSize(),
	  fOutlined(false)
{
}


BCheckBox::BCheckBox(const char *label, BMessage *message)
	: BControl(NULL, label, message, B_WILL_DRAW | B_NAVIGABLE),
	  fPreferredSize(),
	  fOutlined(false)
{
}


BCheckBox::~BCheckBox()
{
}


BCheckBox::BCheckBox(BMessage *archive)
	: BControl(archive),
	fOutlined(false)
{
}


BArchivable *
BCheckBox::Instantiate(BMessage *archive)
{
	if (validate_instantiation(archive, "BCheckBox"))
		return new BCheckBox(archive);

	return NULL;
}


status_t
BCheckBox::Archive(BMessage *archive, bool deep) const
{
	return BControl::Archive(archive, deep);
}


void
BCheckBox::Draw(BRect updateRect)
{
	font_height fontHeight;
	GetFontHeight(&fontHeight);

	// If the focus is changing, just redraw the focus indicator
	if (IsFocusChanging()) {
		float x = (float)ceil(10.0f + fontHeight.ascent);
		float y = 5.0f + (float)ceil(fontHeight.ascent);

		if (IsFocus())
			SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
		else
			SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));

		StrokeLine(BPoint(x, y), BPoint(x + StringWidth(Label()), y));
		return;
	}

	rgb_color noTint = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color lighten1 = tint_color(noTint, B_LIGHTEN_1_TINT);
	rgb_color lightenMax = tint_color(noTint, B_LIGHTEN_MAX_TINT);
	rgb_color darken1 = tint_color(noTint, B_DARKEN_1_TINT);
	rgb_color darken2 = tint_color(noTint, B_DARKEN_2_TINT);
	rgb_color darken3 = tint_color(noTint, B_DARKEN_3_TINT);
	rgb_color darken4 = tint_color(noTint, B_DARKEN_4_TINT);
	rgb_color darkenmax = tint_color(noTint, B_DARKEN_MAX_TINT);

	BRect rect = _CheckBoxFrame();

	if (IsEnabled()) {
		// Filling
		SetHighColor(lightenMax);
		FillRect(rect);

		// Box
		if (fOutlined) {
			SetHighColor(darken3);
			StrokeRect(rect);

			rect.InsetBy(1, 1);

			BeginLineArray(6);

			AddLine(BPoint(rect.left, rect.bottom),
					BPoint(rect.left, rect.top), darken2);
			AddLine(BPoint(rect.left, rect.top),
					BPoint(rect.right, rect.top), darken2);
			AddLine(BPoint(rect.left, rect.bottom),
					BPoint(rect.right, rect.bottom), darken4);
			AddLine(BPoint(rect.right, rect.bottom),
					BPoint(rect.right, rect.top), darken4);

			EndLineArray();
		} else {
			BeginLineArray(6);

			AddLine(BPoint(rect.left, rect.bottom),
					BPoint(rect.left, rect.top), darken1);
			AddLine(BPoint(rect.left, rect.top),
					BPoint(rect.right, rect.top), darken1);
			rect.InsetBy(1, 1);
			AddLine(BPoint(rect.left, rect.bottom),
					BPoint(rect.left, rect.top), darken4);
			AddLine(BPoint(rect.left, rect.top),
					BPoint(rect.right, rect.top), darken4);
			AddLine(BPoint(rect.left + 1.0f, rect.bottom),
					BPoint(rect.right, rect.bottom), noTint);
			AddLine(BPoint(rect.right, rect.bottom),
					BPoint(rect.right, rect.top + 1.0f), noTint);

			EndLineArray();
		}

		// Checkmark
		if (Value() == B_CONTROL_ON) {
			rect.InsetBy(3, 3);

			SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
			SetPenSize(2);
			StrokeLine(BPoint(rect.left, rect.top),
					   BPoint(rect.right, rect.bottom));
			StrokeLine(BPoint(rect.left, rect.bottom),
					   BPoint(rect.right, rect.top));
			SetPenSize(1);
		}

		// Label
		SetHighColor(darkenmax);
		DrawString(Label(), BPoint((float)ceil(10.0f + fontHeight.ascent),
			3.0f + (float)ceil(fontHeight.ascent)));

		// Focus
		if (IsFocus()) {
			float x = (float)ceil(10.0f + fontHeight.ascent);
			float y = 5.0f + (float)ceil(fontHeight.ascent);

			SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
			StrokeLine(BPoint(x, y), BPoint(x + StringWidth(Label()), y));
		}
	} else {
		// Filling
		SetHighColor(lighten1);
		FillRect(rect);

		// Box
		BeginLineArray(6);

		AddLine(BPoint(rect.left, rect.bottom),
				BPoint(rect.left, rect.top), noTint);
		AddLine(BPoint(rect.left, rect.top),
				BPoint(rect.right, rect.top), noTint);
		rect.InsetBy(1, 1);
		AddLine(BPoint(rect.left, rect.bottom),
				BPoint(rect.left, rect.top), darken2);
		AddLine(BPoint(rect.left, rect.top),
				BPoint(rect.right, rect.top), darken2);
		AddLine(BPoint(rect.left + 1.0f, rect.bottom),
				BPoint(rect.right, rect.bottom), darken1);
		AddLine(BPoint(rect.right, rect.bottom),
				BPoint(rect.right, rect.top + 1.0f), darken1);

		EndLineArray();

		// Checkmark
		if (Value() == B_CONTROL_ON) {
			rect.InsetBy(3, 3);

			SetHighColor(tint_color(ui_color(B_KEYBOARD_NAVIGATION_COLOR),
				B_DISABLED_MARK_TINT));
			SetPenSize(2);
			StrokeLine(BPoint(rect.left, rect.top),
					   BPoint(rect.right, rect.bottom));
			StrokeLine(BPoint(rect.left, rect.bottom),
					   BPoint(rect.right, rect.top));
			SetPenSize(1);
		}

		// Label
		SetHighColor(tint_color(noTint, B_DISABLED_LABEL_TINT));
		DrawString(Label(), BPoint((float)ceil(10.0f + fontHeight.ascent),
			3.0f + (float)ceil(fontHeight.ascent)));
	}
}


void
BCheckBox::AttachedToWindow()
{
	BControl::AttachedToWindow();
}


void
BCheckBox::MouseDown(BPoint point)
{
	if (!IsEnabled())
		return;

	fOutlined = true;

	if (Window()->Flags() & B_ASYNCHRONOUS_CONTROLS) {
		Invalidate();
		SetTracking(true);
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	} else {
		BRect bounds = Bounds();
		uint32 buttons;

		Invalidate();
		Window()->UpdateIfNeeded();

		do {
			snooze(40000);

			GetMouse(&point, &buttons, true);

			bool inside = bounds.Contains(point);
			if (fOutlined != inside) {
				fOutlined = inside;
				Invalidate();
				Window()->UpdateIfNeeded();
			}
		} while (buttons != 0);

		if (fOutlined) {
			fOutlined = false;
			SetValue(!Value());
			Invoke();
		} else {
			Invalidate();
			Window()->UpdateIfNeeded();
		}
	}	
}


void
BCheckBox::MessageReceived(BMessage *message)
{
	BControl::MessageReceived(message);
}


void
BCheckBox::WindowActivated(bool active)
{
	BControl::WindowActivated(active);
}


void
BCheckBox::KeyDown(const char *bytes, int32 numBytes)
{
	BControl::KeyDown(bytes, numBytes);
}


void
BCheckBox::MouseUp(BPoint point)
{
	if (!IsTracking())
		return;

	bool inside = Bounds().Contains(point);

	if (fOutlined != inside) {
		fOutlined = inside;
		Invalidate();
	}

	if (fOutlined) {
		fOutlined = false;
		SetValue(!Value());
		Invoke();
	} else {
		Invalidate();
	}

	SetTracking(false);
}


void
BCheckBox::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	if (!IsTracking())
		return;

	bool inside = Bounds().Contains(point);

	if (fOutlined != inside) {
		fOutlined = inside;
		Invalidate();
	}
}


void
BCheckBox::DetachedFromWindow()
{
	BControl::DetachedFromWindow();
}


void
BCheckBox::SetValue(int32 value)
{
	value = value ? B_CONTROL_ON : B_CONTROL_OFF;
		// we only accept boolean values

	if (value != Value()) {
		BControl::SetValueNoUpdate(value);
		Invalidate(_CheckBoxFrame());
	}
}


void
BCheckBox::GetPreferredSize(float* _width, float* _height)
{
	font_height fontHeight;
	GetFontHeight(&fontHeight);

	if (_width) {
		float width = 12.0f + fontHeight.ascent;

		if (Label())
			width += StringWidth(Label());

		*_width = (float)ceil(width);
	}

	if (_height)
		*_height = (float)ceil(6.0f + fontHeight.ascent + fontHeight.descent);
}


void
BCheckBox::ResizeToPreferred()
{
	BControl::ResizeToPreferred();
}


status_t
BCheckBox::Invoke(BMessage *message)
{
	return BControl::Invoke(message);
}


void
BCheckBox::FrameMoved(BPoint newLocation)
{
	BControl::FrameMoved(newLocation);
}


void
BCheckBox::FrameResized(float width, float height)
{
	BControl::FrameResized(width, height);
}


BHandler *
BCheckBox::ResolveSpecifier(BMessage *message, int32 index,
	BMessage *specifier, int32 what, const char *property)
{
	return BControl::ResolveSpecifier(message, index, specifier, what,
		property);
}


status_t
BCheckBox::GetSupportedSuites(BMessage *message)
{
	return BControl::GetSupportedSuites(message);
}


void
BCheckBox::MakeFocus(bool focused)
{
	BControl::MakeFocus(focused);
}


void
BCheckBox::AllAttached()
{
	BControl::AllAttached();
}


void
BCheckBox::AllDetached()
{
	BControl::AllDetached();
}


status_t
BCheckBox::Perform(perform_code d, void *arg)
{
	return BControl::Perform(d, arg);
}


void
BCheckBox::InvalidateLayout(bool descendants)
{
	// invalidate cached preferred size
	fPreferredSize.Set(B_SIZE_UNSET, B_SIZE_UNSET);

	BControl::InvalidateLayout(descendants);
}


BSize
BCheckBox::MinSize()
{
	return BLayoutUtils::ComposeSize(ExplicitMinSize(),
		_ValidatePreferredSize());
}


BSize
BCheckBox::MaxSize()
{
	return BLayoutUtils::ComposeSize(ExplicitMaxSize(),
		BSize(B_SIZE_UNLIMITED, _ValidatePreferredSize().height));
}


BSize
BCheckBox::PreferredSize()
{
	return BLayoutUtils::ComposeSize(ExplicitPreferredSize(),
		_ValidatePreferredSize());
}


void BCheckBox::_ReservedCheckBox1() {}
void BCheckBox::_ReservedCheckBox2() {}
void BCheckBox::_ReservedCheckBox3() {}


BCheckBox &
BCheckBox::operator=(const BCheckBox &)
{
	return *this;
}


BRect
BCheckBox::_CheckBoxFrame() const
{
	font_height fontHeight;
	GetFontHeight(&fontHeight);

	return BRect(1.0f, 3.0f, ceilf(3.0f + fontHeight.ascent),
		ceilf(5.0f + fontHeight.ascent));
}


BSize
BCheckBox::_ValidatePreferredSize()
{
	if (!fPreferredSize.IsWidthSet()) {
		font_height fontHeight;
		GetFontHeight(&fontHeight);

		float width = 12.0f + fontHeight.ascent;

		if (Label())
			width += StringWidth(Label());

		fPreferredSize.width = (float)ceil(width);

		fPreferredSize.height = (float)ceil(6.0f + fontHeight.ascent
			+ fontHeight.descent);
	}

	return fPreferredSize;
}
