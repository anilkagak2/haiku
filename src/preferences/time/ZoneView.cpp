/*
 * Copyright 2004-2010, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Mike Berg <mike@berg-net.us>
 *		Julun <host.haiku@gmx.de>
 *		Philippe Saint-Pierre <stpere@gmail.com>
 *		Adrien Destugues <pulkomandy@pulkomandy.ath.cx>
 *		Oliver Tappe <zooey@hirschkaefer.de>
 */


#include "ZoneView.h"

#include <stdlib.h>

#include <map>
#include <new>

#include <AutoDeleter.h>
#include <Button.h>
#include <Collator.h>
#include <Country.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <ListItem.h>
#include <Locale.h>
#include <MutableLocaleRoster.h>
#include <OutlineListView.h>
#include <Path.h>
#include <ScrollView.h>
#include <StorageDefs.h>
#include <String.h>
#include <TimeZone.h>
#include <View.h>
#include <Window.h>

#include <syscalls.h>
#include <ToolTip.h>

#include <unicode/datefmt.h>
#include <unicode/utmscale.h>
#include <ICUWrapper.h>

#include "TimeMessages.h"
#include "TimeZoneListItem.h"
#include "TZDisplay.h"
#include "TimeWindow.h"


using BPrivate::gMutableLocaleRoster;
using BPrivate::ObjectDeleter;


struct TimeZoneItemLess {
	bool operator()(const BString& first, const BString& second)
	{
		// sort anything starting with '<' behind anything else
		if (first.ByteAt(0) == '<') {
			if (second.ByteAt(0) != '<')
				return false;
		} else if (second.ByteAt(0) == '<')
			return true;
		return fCollator.Compare(first.String(), second.String()) < 0;
	}
private:
	BCollator fCollator;
};



TimeZoneView::TimeZoneView(BRect frame)
	:
	BView(frame, "timeZoneView", B_FOLLOW_NONE, B_WILL_DRAW | B_NAVIGABLE_JUMP),
	fToolTip(NULL),
	fCurrentZoneItem(NULL),
	fOldZoneItem(NULL),
	fInitialized(false)
{
	_InitView();
}


bool
TimeZoneView::CheckCanRevert()
{
	return fCurrentZoneItem != fOldZoneItem;
}


TimeZoneView::~TimeZoneView()
{
	if (fToolTip != NULL)
		fToolTip->ReleaseReference();
}


void
TimeZoneView::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (Parent())
		SetViewColor(Parent()->ViewColor());
}


void
TimeZoneView::AllAttached()
{
	BView::AllAttached();
	if (!fInitialized) {
		fInitialized = true;

		fSetZone->SetTarget(this);
		fZoneList->SetTarget(this);

		// update displays
		if (fCurrentZoneItem != NULL) {
			fZoneList->Select(fZoneList->IndexOf(fCurrentZoneItem));
			fCurrent->SetText(fCurrentZoneItem->Text());
		}
	}
	fZoneList->ScrollToSelection();
}


void
TimeZoneView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 change;
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &change);
			switch(change) {
				case H_TM_CHANGED:
					_UpdateDateTime(message);
					break;

				default:
					BView::MessageReceived(message);
					break;
			}
			break;
		}

		case H_CITY_CHANGED:
			_UpdatePreview();
			break;

		case H_SET_TIME_ZONE:
		{
			_SetSystemTimeZone();
			((TTimeWindow*)Window())->SetRevertStatus();
			break;
		}

		case kMsgRevert:
			_Revert();
			break;

		case kRTCUpdate:
			_UpdateCurrent();
			_UpdatePreview();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


bool
TimeZoneView::GetToolTipAt(BPoint point, BToolTip** _tip)
{
	TimeZoneListItem* item = static_cast<TimeZoneListItem*>(
		fZoneList->ItemAt(fZoneList->IndexOf(point)));
	if (item == NULL || !item->HasTimeZone())
		return false;

	BString nowInTimeZone;
	time_t now = time(NULL);
	be_locale->FormatTime(&nowInTimeZone, now, false, &item->TimeZone());

	BString dateInTimeZone;
	be_locale->FormatDate(&dateInTimeZone, now, false, &item->TimeZone());

	BString toolTip = item->Text();
	toolTip << '\n' << item->TimeZone().ShortName() << " / "
			<< item->TimeZone().ShortDaylightSavingName()
			<< "\nNow: " << nowInTimeZone << " (" << dateInTimeZone << ')';

	if (fToolTip != NULL)
		fToolTip->ReleaseReference();
	fToolTip = new (std::nothrow) BTextToolTip(toolTip.String());
	if (fToolTip == NULL)
		return false;

	*_tip = fToolTip;

	return true;
}


void
TimeZoneView::_UpdateDateTime(BMessage* message)
{
	// only need to update once every minute
	int32 minute;
	if (message->FindInt32("minute", &minute) == B_OK) {
		if (fLastUpdateMinute != minute) {
			_UpdateCurrent();
			_UpdatePreview();

			fLastUpdateMinute = minute;
		}
	}
}


void
TimeZoneView::_InitView()
{
	// left side
	BRect frameLeft(Bounds());
	frameLeft.right = frameLeft.Width() / 2.0;
	frameLeft.InsetBy(10.0f, 10.0f);

	// City Listing
	fZoneList = new BOutlineListView(frameLeft, "cityList",
		B_SINGLE_SELECTION_LIST);
	fZoneList->SetSelectionMessage(new BMessage(H_CITY_CHANGED));
	fZoneList->SetInvocationMessage(new BMessage(H_SET_TIME_ZONE));

	_BuildZoneMenu();

	BScrollView* scrollList = new BScrollView("scrollList", fZoneList,
		B_FOLLOW_ALL, 0, false, true);
	AddChild(scrollList);

	// right side
	BRect frameRight(Bounds());
	frameRight.left = frameRight.Width() / 2.0;
	frameRight.InsetBy(10.0f, 10.0f);
	frameRight.top = frameLeft.top;

	// Time Displays
	fCurrent = new TTZDisplay(frameRight, "currentTime", "Current time:");
	AddChild(fCurrent);
	fCurrent->ResizeToPreferred();

	frameRight.top = fCurrent->Frame().bottom + 10.0;
	fPreview = new TTZDisplay(frameRight, "previewTime", "Preview time:");
	AddChild(fPreview);
	fPreview->ResizeToPreferred();

	// set button
	fSetZone = new BButton(frameRight, "setTimeZone", "Set time zone",
		new BMessage(H_SET_TIME_ZONE));
	AddChild(fSetZone);
	fSetZone->SetEnabled(false);
	fSetZone->ResizeToPreferred();

	fSetZone->MoveTo(frameRight.right - fSetZone->Bounds().Width(),
		scrollList->Frame().bottom - fSetZone->Bounds().Height());
}


void
TimeZoneView::_BuildZoneMenu()
{
	BTimeZone defaultTimeZone;
	be_locale_roster->GetDefaultTimeZone(&defaultTimeZone);

	BLanguage language;
	be_locale->GetLanguage(&language);

	BMessage countryList;
	be_locale_roster->GetAvailableCountries(&countryList);
	countryList.AddString("country", "");

	/*
	 * Group timezones by regions, but filter out unwanted (duplicate) regions
	 * and add an additional region with generic GMT-offset timezones at the end
	 */
	typedef	std::map<BString, TimeZoneListItem*, TimeZoneItemLess> ZoneItemMap;
	ZoneItemMap zoneMap;
	const char* kOtherRegion = "<Other>";
	const char* kSupportedRegions[] = {
		"Africa", "America", "Antarctica", "Arctic", "Asia", "Atlantic",
		"Australia", "Europe", "Indian", "Pacific", kOtherRegion, NULL
	};
	for (const char** region = kSupportedRegions; *region != NULL; ++region)
		zoneMap[*region] = NULL;

	BString countryCode;
	for (int c = 0; countryList.FindString("country", c, &countryCode)
			== B_OK; c++) {
		BCountry country("", countryCode);
		BString countryName;
		country.GetName(countryName);

		// Now list the timezones for this country
		BMessage zoneList;
		be_locale_roster->GetAvailableTimeZonesForCountry(&zoneList,
			countryCode.Length() == 0 ? NULL : countryCode.String());

		int32 count = 0;
		type_code dummy;
		zoneList.GetInfo("timeZone", &dummy, &count);

		BString zoneID;
		for (int tz = 0; zoneList.FindString("timeZone", tz, &zoneID) == B_OK;
			tz++) {
			int32 slashPos = zoneID.FindFirst('/');

			// ignore any "global" timezones, as those are just aliases of
			// regional ones
			if (slashPos <= 0)
				continue;

			BString region(zoneID, slashPos);

			if (region == "Etc")
				region = kOtherRegion;
			else if (countryName.Length() == 0) {
				// skip global timezones from other regions, we are just
				// interested in the generic GMT-based ones under "Etc/"
				continue;
			}


			// just accept timezones from "proper" regions, others are aliases
			ZoneItemMap::iterator regionIter = zoneMap.find(region);
			if (regionIter == zoneMap.end())
				continue;

			BString fullCountryID = region;
			if (countryName != region)
				fullCountryID << "/" << countryName;

			TimeZoneListItem* regionItem = regionIter->second;
			if (regionItem == NULL) {
				regionItem = new TimeZoneListItem(region, NULL, NULL);
				regionItem->SetOutlineLevel(0);
				zoneMap[region] = regionItem;
			}

			BTimeZone* timeZone = new BTimeZone(zoneID, &language);
			BString tzName = timeZone->Name();
			if (tzName == "GMT+00:00")
				tzName = "GMT";

			int32 openParenthesisPos = tzName.FindFirst('(');
			if (openParenthesisPos >= 0) {
				tzName.Remove(0, openParenthesisPos + 1);
				int32 closeParenthesisPos = tzName.FindLast(')');
				if (closeParenthesisPos >= 0)
					tzName.Truncate(closeParenthesisPos);
			}
			BString fullZoneID = fullCountryID;
			fullZoneID << "/" << tzName;

			// skip duplicates
			ZoneItemMap::iterator zoneIter = zoneMap.find(fullZoneID);
			if (zoneIter != zoneMap.end()) {
				delete timeZone;
				continue;
			}

			TimeZoneListItem* countryItem = NULL;
			TimeZoneListItem* zoneItem = NULL;
			if (count > 1 && countryName.Length() > 0) {
				ZoneItemMap::iterator countryIter = zoneMap.find(fullCountryID);
				if (countryIter == zoneMap.end()) {
					countryItem = new TimeZoneListItem(countryName, NULL, NULL);
					countryItem->SetOutlineLevel(1);
					zoneMap[fullCountryID] = countryItem;
				} else
					countryItem = countryIter->second;

				zoneItem = new TimeZoneListItem(tzName, NULL, timeZone);
				zoneItem->SetOutlineLevel(2);
			} else {
				BString& name = countryName.Length() > 0 ? countryName : tzName;
				zoneItem = new TimeZoneListItem(name, NULL, timeZone);
				zoneItem->SetOutlineLevel(1);
			}
			zoneMap[fullZoneID] = zoneItem;

			if (timeZone->ID() == defaultTimeZone.ID()) {
				fCurrentZoneItem = zoneItem;
				if (countryItem != NULL)
					countryItem->SetExpanded(true);
				regionItem->SetExpanded(true);
			}
		}
	}

	fOldZoneItem = fCurrentZoneItem;

	ZoneItemMap::iterator zoneIter;
	bool lastWasCountryItem = false;
	TimeZoneListItem* lastCountryItem = NULL;
	for (zoneIter = zoneMap.begin(); zoneIter != zoneMap.end(); ++zoneIter) {
		if (zoneIter->second->OutlineLevel() == 2 && lastWasCountryItem) {
			/* Some countries (e.g. Spain and Chile) have their timezones
			 * spread across different regions. As a result, there might still
			 * be country items with only one timezone below them. We manually
			 * filter those country items here.
			 */
			ZoneItemMap::iterator next = zoneIter;
			++next;
			if (next != zoneMap.end() && next->second->OutlineLevel() != 2) {
				fZoneList->RemoveItem(lastCountryItem);
				zoneIter->second->SetText(lastCountryItem->Text());
				zoneIter->second->SetOutlineLevel(1);
				delete lastCountryItem;
			}
		}

		fZoneList->AddItem(zoneIter->second);
		if (zoneIter->second->OutlineLevel() == 1) {
			lastWasCountryItem = true;
			lastCountryItem = zoneIter->second;
		} else
			lastWasCountryItem = false;
	}
}


void
TimeZoneView::_Revert()
{
	fCurrentZoneItem = fOldZoneItem;

	if (fCurrentZoneItem != NULL) {
		int32 currentZoneIndex = fZoneList->IndexOf(fCurrentZoneItem);
		fZoneList->Select(currentZoneIndex);
	} else
		fZoneList->DeselectAll();
	fZoneList->ScrollToSelection();

	_SetSystemTimeZone();
	_UpdatePreview();
	_UpdateCurrent();
}


void
TimeZoneView::_UpdatePreview()
{
	int32 selection = fZoneList->CurrentSelection();
	TimeZoneListItem* item
		= selection < 0
			? NULL
			: (TimeZoneListItem*)fZoneList->ItemAt(selection);

	if (item == NULL || !item->HasTimeZone()) {
		fPreview->SetText("");
		fPreview->SetTime("");
		return;
	}

	BString timeString = _FormatTime(item->TimeZone());
	fPreview->SetText(item->Text());
	fPreview->SetTime(timeString.String());

	fSetZone->SetEnabled((strcmp(fCurrent->Text(), item->Text()) != 0));
}


void
TimeZoneView::_UpdateCurrent()
{
	if (fCurrentZoneItem == NULL)
		return;

	BString timeString = _FormatTime(fCurrentZoneItem->TimeZone());
	fCurrent->SetText(fCurrentZoneItem->Text());
	fCurrent->SetTime(timeString.String());
}


void
TimeZoneView::_SetSystemTimeZone()
{
	/*	Set sytem timezone for all different API levels. How to do this?
	 *	1) tell locale-roster about new default timezone
	 *	2) tell kernel about new timezone offset
	 */

	int32 selection = fZoneList->CurrentSelection();
	if (selection < 0)
		return;

	TimeZoneListItem* item
		= static_cast<TimeZoneListItem*>(fZoneList->ItemAt(selection));
	if (item == NULL || !item->HasTimeZone())
		return;

	fCurrentZoneItem = item;
	const BTimeZone& timeZone = item->TimeZone();

	gMutableLocaleRoster->SetDefaultTimeZone(timeZone);

	_kern_set_timezone(timeZone.OffsetFromGMT(), timeZone.ID().String(),
		timeZone.ID().Length());

	fSetZone->SetEnabled(false);
	fLastUpdateMinute = -1;
		// just to trigger updating immediately
}


BString
TimeZoneView::_FormatTime(const BTimeZone& timeZone)
{
	BString result;

	time_t now = time(NULL);
	bool rtcIsGMT;
	_kern_get_real_time_clock_is_gmt(&rtcIsGMT);
	if (!rtcIsGMT) {
		int32 currentOffset
			= fCurrentZoneItem != NULL && fCurrentZoneItem->HasTimeZone()
				? fCurrentZoneItem->OffsetFromGMT()
				: 0;
		now -= timeZone.OffsetFromGMT() - currentOffset;
	}
	be_locale->FormatTime(&result, now, false, &timeZone);

	return result;
}
