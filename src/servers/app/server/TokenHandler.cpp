//------------------------------------------------------------------------------
//	Copyright (c) 2001-2002, OpenBeOS
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
//	File Name:		TokenHandler.cpp
//	Author:			DarkWyrm <bpmagic@columbus.rr.com>
//	Description:	Class to provide tokens with excluded values possible.
//  
//------------------------------------------------------------------------------
#include "TokenHandler.h"

//! Does setup
TokenHandler::TokenHandler(void)
{
	_index=-1;
	_lock=create_sem(1,"tokenhandler_sem");
	
	_excludes=new BList(0);
}

//! Undoes the setup from the constructor
TokenHandler::~TokenHandler(void)
{
	ResetExcludes();
	delete_sem(_lock);
	delete _excludes;
}

/*!
	\brief	Returns a unique token
	\return A unique token
	
	If a value is excluded the next higher non-excluded value is returned.
*/
int32 TokenHandler::GetToken(void)
{
	int32 value;

	acquire_sem(_lock);

	_index++;
	while(IsExclude(_index))
		_index++;
	value=_index;
	release_sem(_lock);
	return value;
}

/*!
	\brief Excludes a value from being returned as a token.
	
	 This is quite useful for excluding values like B_ERROR and such.
*/
void TokenHandler::ExcludeValue(int32 value)
{
	acquire_sem(_lock);

	if(!IsExclude(value))
		_excludes->AddItem(new int32(value));
	release_sem(_lock);
}

//! Resets the token index
void TokenHandler::Reset(void)
{
	acquire_sem(_lock);
	_index=-1;
	release_sem(_lock);
}

//! Empties object of all assigned excluded values
void TokenHandler::ResetExcludes(void)
{
	acquire_sem(_lock);
	int32 *temp;
	for(int32 i=0; i<_excludes->CountItems();i++)
	{
		temp=(int32*)_excludes->ItemAt(i);
		if(temp)
			delete temp;
	}
	_excludes->MakeEmpty();
	release_sem(_lock);
}

/*!
	\brief Determines whether a value is excluded
	\param value The value to be checked
	\return True if the value is excluded from being a token, false if not.
*/
bool TokenHandler::IsExclude(int32 value)
{
	bool match=false;
	int32 *temp;
	
	acquire_sem(_lock);
	for(int32 i=0;i<_excludes->CountItems();i++)
	{
		temp=(int32*)_excludes->ItemAt(i);
		if(temp && *temp==value)
		{
			match=true;
			break;
		}
	}
	delete_sem(_lock);
	return match;
}
