//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//---------------------------------------------------------------------

#include <fcntl.h>
#include <new.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <disk_scanner.h>

#include "RDiskDevice.h"
#include "RDiskDeviceList.h"
#include "RPartition.h"
#include "RSession.h"

// constructor
RDiskDevice::RDiskDevice()
	: fSessions(10, true),
	  fDeviceList(NULL),
	  fID(-1),
	  fChangeCounter(0),
	  fTouched(false),
	  fPath(),
	  fFD(-1),
	  fMediaStatus(B_ERROR)
{
}

// destructor
RDiskDevice::~RDiskDevice()
{
	Unset();
}

// SetTo
status_t
RDiskDevice::SetTo(const char *path, int fd, const device_geometry *geometry,
				   status_t mediaStatus)
{
PRINT(("RDiskDevice::SetTo()\n"));
	Unset();
	status_t error = B_OK;
	fID = _NextID();
	fChangeCounter = 0;
	fTouched = true;
	fPath.SetTo(path);
	fFD = fd;
	fGeometry = *geometry;
	fMediaStatus = mediaStatus;
	// analyze the media status
	switch (fMediaStatus) {
		case B_NO_ERROR:
		case B_DEV_NO_MEDIA:
		case B_DEV_NOT_READY:
		case B_DEV_MEDIA_CHANGE_REQUESTED:
		case B_DEV_DOOR_OPEN:
			break;
		case B_DEV_MEDIA_CHANGED:
			// the user was fast enough to change the media between opening
			// and requesting the media status -- media is in, so we are fine
			fMediaStatus = B_OK;
			break;
		default:
			error = fMediaStatus;
			break;
	}
	// scan the device for sessions, if we have a media
	if (fMediaStatus == B_OK) {
		session_info sessionInfo;
		for (int32 i = 0; ; i++) {
			// get the session info
			status_t status = get_nth_session_info(fFD, i, &sessionInfo);
			if (status != B_OK) {
				if (status != B_ENTRY_NOT_FOUND)
					error = status;
				break;
			}
			// create and add a RSession
			if (RSession *session = new(nothrow) RSession) {
				error = session->SetTo(fFD, &sessionInfo);
				if (error == B_OK)
					AddSession(session);
				else
					delete session;
			} else
				error = B_NO_MEMORY;
		}
	}
	// cleanup on error
	if (error != B_OK)
		Unset();
RETURN_ERROR(error);
//	return error;
}

// Unset
void
RDiskDevice::Unset()
{
	for (int32 i = CountSessions() - 1; i >= 0; i--)
		RemoveSession(i);
	fID = -1;
	fPath.SetTo("");
	if (fFD >= 0) {
		close(fFD);
		fFD = -1;
	}
}

// Size
off_t
RDiskDevice::Size() const
{
	return (off_t)fGeometry.bytes_per_sector * fGeometry.sectors_per_track
			* fGeometry.cylinder_count * fGeometry.head_count;
}

// AddSession
bool
RDiskDevice::AddSession(RSession *session)
{
	bool success = false;
	if (session) {
		success = fSessions.AddItem(session);
		if (success) {
			session->SetDevice(this);
			if (RDiskDeviceList *deviceList = DeviceList())
				deviceList->SessionAdded(session);
		}
	}
	return success;
}

// RemoveSession
bool
RDiskDevice::RemoveSession(int32 index)
{
	RSession *session = SessionAt(index);
	if (session) {
		if (RDiskDeviceList *deviceList = DeviceList())
			deviceList->SessionRemoved(session);
		session->SetDevice(NULL);
		fSessions.RemoveItemAt(index);
		delete session;
	}
	return (session != NULL);
}

// RemoveSession
bool
RDiskDevice::RemoveSession(RSession *session)
{
	bool success = false;
	if (session) {
		int32 index = fSessions.IndexOf(session);
		if (index >= 0)
			success = RemoveSession(index);
	}
	return success;
}

// Update
status_t
RDiskDevice::Update()
{
	return B_ERROR;
}

// Archive
status_t
RDiskDevice::Archive(BMessage *archive) const
{
	status_t error = (archive ? B_OK : B_BAD_VALUE);
	// ID and change counter
	if (error == B_OK)
		error = archive->AddInt32("id", fID);
	if (error == B_OK)
		error = archive->AddInt32("change_counter", fChangeCounter);
	// geometry
	if (error == B_OK)
		error = archive->AddInt64("size", Size());
	if (error == B_OK)
		error = archive->AddInt32("block_size", BlockSize());
	if (error == B_OK)
		error = archive->AddInt8("type", (int8)fGeometry.device_type);
	if (error == B_OK)
		error = archive->AddBool("removable", fGeometry.removable);
	if (error == B_OK)
		error = archive->AddBool("read_only", fGeometry.read_only);
	if (error == B_OK)
		error = archive->AddBool("write_once", fGeometry.write_once);
	// other data
	if (error == B_OK)
		error = archive->AddString("path", fPath.String());
	if (error == B_OK)
		error = archive->AddInt32("media_status", fMediaStatus);
	// sessions
	for (int32 i = 0; const RSession *session = SessionAt(i); i++) {
		BMessage sessionArchive;
		error = session->Archive(&sessionArchive);
		if (error == B_OK)
			error = archive->AddMessage("sessions", &sessionArchive);
		if (error != B_OK)
			break;
	}
	return error;
}

// Dump
void
RDiskDevice::Dump() const
{
	printf("device `%s'\n", Path());
	for (int32 i = 0; RSession *session = SessionAt(i); i++)
		session->Dump();
}

// _NextID
int32
RDiskDevice::_NextID()
{
	return atomic_add(&fNextID, 1);
}

// fNextID
vint32 RDiskDevice::fNextID = 0;

