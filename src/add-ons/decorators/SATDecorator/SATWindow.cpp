/*
 * Copyright 2010, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Clemens Zeidler <haiku@clemens-zeidler.de>
 */

#include "SATWindow.h"

#include <Debug.h>

#include "SATGroup.h"
#include "ServerApp.h"
#include "Window.h"


using namespace BPrivate;


GroupCookie::GroupCookie(SATWindow* satWindow)
	:
	fSATWindow(satWindow),

	fWindowArea(NULL),

	fLeftBorder(NULL),
	fTopBorder(NULL),
	fRightBorder(NULL),
	fBottomBorder(NULL),

	fLeftBorderConstraint(NULL),
	fTopBorderConstraint(NULL),
	fRightBorderConstraint(NULL),
	fBottomBorderConstraint(NULL),

	fLeftConstraint(NULL),
	fTopConstraint(NULL),
	fMinWidthConstraint(NULL),
	fMinHeightConstraint(NULL),
	fWidthConstraint(NULL),
	fHeightConstraint(NULL)
{
	
}


GroupCookie::~GroupCookie()
{
	Uninit();
}


void
GroupCookie::DoGroupLayout(SATWindow* triggerWindow)
{
	if (!fSATGroup.Get())
		return;

	BRect frame = triggerWindow->CompleteWindowFrame();

	// adjust window size soft constraints
	fWidthConstraint->SetRightSide(frame.Width());
	fHeightConstraint->SetRightSide(frame.Height());
	
	// adjust window position soft constraints
	// (a bit more penalty for them so they take precedence)
	fLeftConstraint->SetRightSide(frame.left);
	fTopConstraint->SetRightSide(frame.top);

	fWidthConstraint->SetPenaltyNeg(110);
	fWidthConstraint->SetPenaltyPos(110);
	fHeightConstraint->SetPenaltyNeg(110);
	fHeightConstraint->SetPenaltyPos(110);

	fLeftConstraint->SetPenaltyNeg(100);
	fLeftConstraint->SetPenaltyPos(100);
	fTopConstraint->SetPenaltyNeg(100);
	fTopConstraint->SetPenaltyPos(100);

	// After we set the new parameter solve and apply the new layout.
	ResultType result;
	for (int32 tries = 0; tries < 15; tries++) {
		result = fSATGroup->GetLinearSpec()->Solve();
		if (result == INFEASIBLE)
			break;
		if (result == OPTIMAL) {
			fSATGroup->AdjustWindows(triggerWindow);
			break;
		}
	}

	// set penalties back to normal
	fWidthConstraint->SetPenaltyNeg(10);
	fWidthConstraint->SetPenaltyPos(10);
	fHeightConstraint->SetPenaltyNeg(10);
	fHeightConstraint->SetPenaltyPos(10);

	fLeftConstraint->SetPenaltyNeg(1);
	fLeftConstraint->SetPenaltyPos(1);
	fTopConstraint->SetPenaltyNeg(1);
	fTopConstraint->SetPenaltyPos(1);
}


void
GroupCookie::MoveWindow(int32 workspace)
{
	Window* window = fSATWindow->GetWindow();
	Desktop* desktop = window->Desktop();

	BRect frame = fSATWindow->CompleteWindowFrame();
	desktop->MoveWindowBy(window, fLeftBorder->Value() - frame.left,
		fTopBorder->Value() - frame.top, workspace);

	// Update frame to the new position
	frame.OffsetBy(fLeftBorder->Value() - frame.left,
		fTopBorder->Value() - frame.top);
	desktop->ResizeWindowBy(window, fRightBorder->Value() - frame.right,
		fBottomBorder->Value() - frame.bottom);
}


void
GroupCookie::UpdateWindowSize()
{
	BRect frame = fSATWindow->CompleteWindowFrame();

	// adjust window size soft constraints
	fWidthConstraint->SetRightSide(frame.Width());
	fHeightConstraint->SetRightSide(frame.Height());
}


bool
GroupCookie::Init(SATGroup* group, WindowArea* area)
{
	ASSERT(fSATGroup.Get() == NULL);

	Window* window = fSATWindow->GetWindow();
	fSATGroup.SetTo(group);
	fWindowArea = area;

	LinearSpec* linearSpec = group->GetLinearSpec();
	// create variables
	fLeftBorder = linearSpec->AddVariable();
	fTopBorder = linearSpec->AddVariable();
	fRightBorder = linearSpec->AddVariable();
	fBottomBorder = linearSpec->AddVariable();

	if (!fLeftBorder || !fTopBorder || !fRightBorder || !fBottomBorder) {
		// clean up
		Uninit();
		return false;
	}

	// create constraints
	BRect frame = fSATWindow->CompleteWindowFrame();
	fLeftConstraint = linearSpec->AddConstraint(1.0, fLeftBorder,
		OperatorType(EQ), frame.left, 1, 1);
	fTopConstraint  = linearSpec->AddConstraint(1.0, fTopBorder,
		OperatorType(EQ), frame.top, 1, 1);

	int32 minWidth, maxWidth, minHeight, maxHeight;
	window->GetSizeLimits(&minWidth, &maxWidth, &minHeight, &maxHeight);
	fMinWidthConstraint = linearSpec->AddConstraint(1.0, fLeftBorder, -1.0,
		fRightBorder, OperatorType(LE), -minWidth);
	fMinHeightConstraint = linearSpec->AddConstraint(1.0, fTopBorder, -1.0,
		fBottomBorder, OperatorType(LE), -minHeight);

	// The width and height constraints have higher penalties than the
	// position constraints (left, top), so a window will keep its size
	// unless explicitly resized.
	fWidthConstraint = linearSpec->AddConstraint(-1.0, fLeftBorder, 1.0,
		fRightBorder, OperatorType(EQ), frame.Width(), 10, 10);
	fHeightConstraint = linearSpec->AddConstraint(-1.0, fTopBorder, 1.0,
		fBottomBorder, OperatorType(EQ), frame.Height(), 10, 10);
	
	if (!fLeftConstraint || !fTopConstraint || !fMinWidthConstraint
		|| !fMinHeightConstraint || !fWidthConstraint || !fHeightConstraint) {
		// clean up
		Uninit();
		return false;
	}
	
	fLeftBorderConstraint = area->LeftTab()->Connect(fLeftBorder);
	fTopBorderConstraint = area->TopTab()->Connect(fTopBorder);
	fRightBorderConstraint = area->RightTab()->Connect(fRightBorder);
	fBottomBorderConstraint = area->BottomTab()->Connect(fBottomBorder);

	if (!fLeftBorderConstraint || !fTopBorderConstraint
		|| !fRightBorderConstraint || !fBottomBorderConstraint) {
		Uninit();
		return false;
	}

	return true;
}


void
GroupCookie::Uninit()
{
	delete fLeftBorder;
	delete fTopBorder;
	delete fRightBorder;
	delete fBottomBorder;
	fLeftBorder = NULL;
	fTopBorder = NULL;
	fRightBorder = NULL;
	fBottomBorder = NULL;
	
	delete fLeftBorderConstraint;
	delete fTopBorderConstraint;
	delete fRightBorderConstraint;
	delete fBottomBorderConstraint;
	fLeftBorderConstraint = NULL;
	fTopBorderConstraint = NULL;
	fRightBorderConstraint = NULL;
	fBottomBorderConstraint = NULL;

	delete fLeftConstraint;
	delete fTopConstraint;
	delete fMinWidthConstraint;
	delete fMinHeightConstraint;
	delete fWidthConstraint;
	delete fHeightConstraint;
	fLeftConstraint = NULL;
	fTopConstraint = NULL;
	fMinWidthConstraint = NULL;
	fMinHeightConstraint = NULL;
	fWidthConstraint = NULL;
	fHeightConstraint = NULL;

	fSATGroup.Unset();
	fWindowArea = NULL;
}


bool
GroupCookie::PropagateToGroup(SATGroup* group, WindowArea* area)
{
	if (!fSATGroup->fSATWindowList.RemoveItem(fSATWindow))
		return false;
	Uninit();

	if (!Init(group, area))
		return false;

	if (!group->fSATWindowList.AddItem(fSATWindow)) {
		Uninit();
		return false;
	}

	return true;
}


SATWindow::SATWindow(StackAndTile* sat, Window* window)
	:
	fWindow(window),
	fStackAndTile(sat),

	fOwnGroupCookie(this),
	fForeignGroupCookie(this),

	fOngoingSnapping(NULL),
	fSATStacking(this),
	fSATTiling(this),
	fShutdown(false)
{
	fDecorator = dynamic_cast<SATDecorator*>(fWindow->Decorator());
	fDesktop = fWindow->Desktop();

	fGroupCookie = &fOwnGroupCookie;
	_InitGroup();

	fSATSnappingBehaviourList.AddItem(&fSATStacking);
	fSATSnappingBehaviourList.AddItem(&fSATTiling);
}


SATWindow::~SATWindow()
{
	fShutdown = true;

	if (fForeignGroupCookie.GetGroup())
		fForeignGroupCookie.GetGroup()->RemoveWindow(this);
	if (fOwnGroupCookie.GetGroup())
		fOwnGroupCookie.GetGroup()->RemoveWindow(this);
}


SATGroup*
SATWindow::GetGroup()
{
	if (!fGroupCookie->GetGroup())
		_InitGroup();

	// manually set the tabs of the single window
	WindowArea* windowArea = fGroupCookie->GetWindowArea();
	if (!PositionManagedBySAT() && windowArea) {
		BRect frame = CompleteWindowFrame();
		windowArea->LeftTopCrossing()->VerticalTab()->SetPosition(frame.left);
		windowArea->LeftTopCrossing()->HorizontalTab()->SetPosition(frame.top);
		windowArea->RightBottomCrossing()->VerticalTab()->SetPosition(
			frame.right);
		windowArea->RightBottomCrossing()->HorizontalTab()->SetPosition(
			frame.bottom);
	}

	return fGroupCookie->GetGroup();
}


bool
SATWindow::HandleMessage(SATWindow* sender, BPrivate::ServerLink& link)
{
	return StackingEventHandler::HandleMessage(sender, link);
}


bool
SATWindow::PropagateToGroup(SATGroup* group, WindowArea* area)
{
	return fGroupCookie->PropagateToGroup(group, area);
}


void
SATWindow::UpdateGroupWindowsSize()
{
	SATGroup* group = GetGroup();
	if (!group)
		return;

	for (int i = 0; i < group->CountItems(); i++) {
		SATWindow* window = group->WindowAt(i);
		if (window == this)
			continue;
		window->UpdateWindowSize();
	}
}


void
SATWindow::UpdateWindowSize()
{
	fGroupCookie->UpdateWindowSize();
}


void
SATWindow::MoveWindowToSAT(int32 workspace)
{
	fGroupCookie->MoveWindow(workspace);
}


bool
SATWindow::AddedToGroup(SATGroup* group, WindowArea* area)
{
	STRACE_SAT("SATWindow::AddedToGroup group: %p window %s\n", group,
		fWindow->Title());
	if (fGroupCookie == &fForeignGroupCookie)
		return false;
	if (fOwnGroupCookie.GetGroup())
		fGroupCookie = &fForeignGroupCookie;

	if (!fGroupCookie->Init(group, area)) {
		fGroupCookie = &fOwnGroupCookie;
		return false;
	}

	return true;
}


bool
SATWindow::RemovedFromGroup(SATGroup* group)
{
	STRACE_SAT("SATWindow::RemovedFromGroup group: %p window %s\n", group,
		fWindow->Title());

	if (fShutdown) {
		fGroupCookie->Uninit();
		return true;
	}

	ASSERT(fGroupCookie->GetGroup() == group);
	fGroupCookie->Uninit();
	if (fGroupCookie == &fOwnGroupCookie)
		_InitGroup();
	else
		fGroupCookie = &fOwnGroupCookie;

	return true;
}


bool
SATWindow::StackWindow(SATWindow* child)
{
	SATGroup* group = GetGroup();
	WindowArea* area = GetWindowArea();
	if (!group || !area)
		return false;

	bool status = group->AddWindow(child, area, this);

	if (status) {
		area->WindowList().ItemAt(0)->SetStackedMode(true);
			// for the case we are the first added window
		child->SetStackedMode(true);
	}

	DoGroupLayout();
	return true;
}


void
SATWindow::RemovedFromArea(WindowArea* area)
{
	for (int i = 0; i < fSATSnappingBehaviourList.CountItems(); i++)
		fSATSnappingBehaviourList.ItemAt(i)->RemovedFromArea(area);
}


void
SATWindow::FindSnappingCandidates()
{
	fOngoingSnapping = NULL;
	GroupIterator groupIterator(fStackAndTile, GetWindow()->Desktop());
	for (SATGroup* group = groupIterator.NextGroup(); group;
		group = groupIterator.NextGroup()) {
		for (int i = 0; i < fSATSnappingBehaviourList.CountItems(); i++) {
			if (fSATSnappingBehaviourList.ItemAt(i)->FindSnappingCandidates(
				group)) {
				fOngoingSnapping = fSATSnappingBehaviourList.ItemAt(i);
				return;
			}
		}
	}
}


bool
SATWindow::JoinCandidates()
{
	if (!fOngoingSnapping)
		return false;
	bool status = fOngoingSnapping->JoinCandidates();
	fOngoingSnapping = NULL;
	return status;
}


void
SATWindow::DoGroupLayout()
{
	if (!PositionManagedBySAT())
		return;

	fGroupCookie->DoGroupLayout(this);

	for (int i = 0; i < fSATSnappingBehaviourList.CountItems(); i++)
		fSATSnappingBehaviourList.ItemAt(i)->DoGroupLayout();
}


BRect
SATWindow::CompleteWindowFrame()
{
	BRect frame = fWindow->Frame();
	if (fDesktop
		&& fDesktop->CurrentWorkspace() != fWindow->CurrentWorkspace()) {
		window_anchor& anchor = fWindow->Anchor(fWindow->CurrentWorkspace());
		if (anchor.position != kInvalidWindowPosition)
			frame.OffsetTo(anchor.position);
	}

	// TODO get this values from the decorator
	frame.left -= 5.;
	frame.right += 6.;
	frame.top -= 27;
	frame.bottom += 5;		

	return frame;
}


bool
SATWindow::PositionManagedBySAT()
{
	if (fGroupCookie->GetGroup() && fGroupCookie->GetGroup()->CountItems() == 1)
		return false;

	return true;
}


bool
SATWindow::HighlightTab(bool active)
{
	if (!fDecorator)
		return false;

	if (IsTabHighlighted() == active)
		return false;

	BRegion dirty;
	fDecorator->HighlightTab(active, &dirty);
	fWindow->ProcessDirtyRegion(dirty);

	return true;
}


bool
SATWindow::HighlightBorders(bool active)
{
	if (!fDecorator)
		return false;

	if (IsBordersHighlighted() == active)
		return false;

	BRegion dirty;
	fDecorator->HighlightBorders(active, &dirty);
	fWindow->ProcessDirtyRegion(dirty);
	return true;
}


bool
SATWindow::IsTabHighlighted()
{
	if (fDecorator)
		return fDecorator->IsTabHighlighted();
	return false;
}


bool
SATWindow::IsBordersHighlighted()
{
	if (fDecorator)
		return fDecorator->IsBordersHighlighted();
	return false;
}


bool
SATWindow::SetStackedMode(bool stacked)
{
	if (!fDecorator)
		return false;
	BRegion dirty;
	fDecorator->SetStackedMode(stacked, &dirty);
	fDesktop->RebuildAndRedrawAfterWindowChange(fWindow, dirty);
	return true;
}


bool
SATWindow::SetStackedTabLength(float length)
{
	if (!fDecorator)
		return false;
	BRegion dirty;
	fDecorator->SetStackedTabLength(length, &dirty);
	fDesktop->RebuildAndRedrawAfterWindowChange(fWindow, dirty);
	return true;
}


bool
SATWindow::SetStackedTabMoving(bool moving)
{
	if (!fDecorator)
		return false;

	if (!moving)
		DoGroupLayout();

	return true;
}


void
SATWindow::TabLocationMoved(float location, bool shifting)
{
	for (int i = 0; i < fSATSnappingBehaviourList.CountItems(); i++)
		fSATSnappingBehaviourList.ItemAt(i)->TabLocationMoved(location,
			shifting);
}


void
SATWindow::_InitGroup()
{
	ASSERT(fGroupCookie == &fOwnGroupCookie);
	ASSERT(fOwnGroupCookie.GetGroup() == NULL);
	STRACE_SAT("SATWindow::_InitGroup %s\n", fWindow->Title());
	SATGroup* group = new (std::nothrow)SATGroup;
	if (!group)
		return;
	BReference<SATGroup> groupRef;
	groupRef.SetTo(group, true);

	/* AddWindow also will trigger the window to hold a reference on the new
	group. */
	if (!groupRef->AddWindow(this, NULL, NULL, NULL, NULL))
		STRACE_SAT("SATWindow::_InitGroup(): adding window to group failed\n");
}
