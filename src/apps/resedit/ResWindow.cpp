/*
 * Copyright (c) 2005-2010, Haiku, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *		DarkWyrm <darkwyrm@gmail.com>
 */
#include "ResWindow.h"
#include "ResView.h"
#include "App.h"

static int32 sWindowCount = 0;

ResWindow::ResWindow(const BRect &rect, const entry_ref *ref)
 :	BWindow(rect,"", B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS)
{
	atomic_add(&sWindowCount,1);
	
	ResView *child = new ResView(Bounds(), "resview", B_FOLLOW_ALL,
								B_WILL_DRAW, ref);
	AddChild(child);
	
	SetTitle(child->Filename());

	Show();
}


ResWindow::~ResWindow(void)
{
}


bool
ResWindow::QuitRequested(void)
{
	atomic_add(&sWindowCount,-1);
	
	if (sWindowCount == 0)
		be_app->PostMessage(B_QUIT_REQUESTED);
	//	be_app->PostMessage(M_UNREGISTER_WINDOW);
	return true;
}


