//------------------------------------------------------------------------------
//	Copyright (c) 2001-2003, OpenBeOS
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		DesktopClasses.h
//	Author:			DarkWyrm <bpmagic@columbus.rr.com>
//					Gabe Yoder <gyoder@stny.rr.com>
//	Description:	Classes for managing workspaces and screens
//  
//------------------------------------------------------------------------------
#ifndef DESKTOPCLASSES_H
#define DESKTOPCLASSES_H

#include <SupportDefs.h>
#include <GraphicsCard.h>
#include <Window.h>	// for workspace defs
#include "RootLayer.h"

class DisplayDriver;
class ServerWindow;
class RGBColor;

class Workspace
{
public:
	Workspace(const graphics_card_info &gcinfo, const frame_buffer_info &fbinfo, DisplayDriver *gfxdriver);
	~Workspace(void);
	void SetBGColor(const RGBColor &c);
	RGBColor BGColor();
	RootLayer *GetRoot(void);
	void SetData(const graphics_card_info &gcinfo, const frame_buffer_info &fbinfo);
	void GetData(graphics_card_info *gcinfo, frame_buffer_info *fbinfo);

protected:
	RootLayer *_rootlayer;
	graphics_card_info _gcinfo;
	frame_buffer_info _fbinfo;
};

class Screen
{
public:
	Screen(DisplayDriver *gfxmodule, uint8 workspaces);
	~Screen(void);
	void AddWorkspace(int32 index=-1);
	void DeleteWorkspace(int32 index);
	int32 CountWorkspaces(void);
	void SetWorkspaceCount(int32 count);
	int32 CurrentWorkspace(void);
	void SetWorkspace(int32 index);
	void Activate(bool active=true);
	DisplayDriver *GetGfxDriver(void);
	status_t SetSpace(int32 index, int32 res,bool stick=true);
	void AddWindow(ServerWindow *win, int32 workspace=B_CURRENT_WORKSPACE);
	void RemoveWindow(ServerWindow *win);
	ServerWindow *ActiveWindow(void);
	void SetActiveWindow(ServerWindow *win);
	Layer *GetRootLayer(int32 workspace=B_CURRENT_WORKSPACE);
	bool IsInitialized(void);
	Workspace *GetActiveWorkspace(void);

protected:
	int32 _resolution;
	ServerWindow *_activewin;
	int32 _currentworkspace;
	int32 _workspacecount;
	BList *_workspacelist;
	DisplayDriver *_driver;
	bool _init, _active;
	Workspace *_activeworkspace;
	graphics_card_info _gcinfo;
	frame_buffer_info _fbinfo;
};

#endif
