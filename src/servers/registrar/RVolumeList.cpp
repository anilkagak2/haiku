//----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//---------------------------------------------------------------------

#include <string.h>

#include <Directory.h>
#include <Entry.h>
#include <Locker.h>
#include <Message.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <Volume.h>

#include "RVolumeList.h"
#include "Debug.h"

namespace RVolumeListPredicates {

// CompareIDPredicate
struct CompareIDPredicate : public UnaryPredicate<RVolume> {
	CompareIDPredicate(dev_t id) : fID(id) {}
	CompareIDPredicate(const RVolume *volume) : fID(volume->ID()) {}

	virtual int operator()(const RVolume *volume) const
	{
		dev_t id = volume->ID();
		if (fID < id)
			return 1;
		if (fID > id)
			return -1;
		return 0;
	}
	
private:
	dev_t	fID;
};

// CompareDevicePathPredicate
struct CompareDevicePathPredicate : public UnaryPredicate<RVolume> {
	CompareDevicePathPredicate(const char *path) : fPath(path) {}

	virtual int operator()(const RVolume *volume) const
	{
		return strcmp(fPath, volume->DevicePath());
	}
	
private:
	const char	*fPath;
};

}	// namespace RVolumeListPredicates

using namespace RVolumeListPredicates;


// RVolume

// constructor
RVolume::RVolume()
	: fID(-1),
	  fRootNode(-1),
	  fRootEntry()
{
}

// SetTo
status_t
RVolume::SetTo(dev_t id)
{
	status_t error = B_OK;
	// get general info
	fs_info info;
	error = fs_stat_dev(id, &info);
	if (error == B_OK) {
		fID = id;
		fRootNode = info.root;
		if (info.device_name[0] != '\0')
			strcpy(fDevicePath, info.device_name);
		else	// generate a pseudo name -- needed for the list
			sprintf(fDevicePath, "%ld", id);
	}
	// get root dir entry_ref
	if (error == B_OK) {
		node_ref ref;
		GetRootDirNode(&ref);
		BDirectory dir;
		error = dir.SetTo(&ref);
		BEntry entry;
		if (error == B_OK)
			error = dir.GetEntry(&entry);
		if (error == B_OK)
			error = entry.GetRef(&fRootEntry);
	}
	// cleanup on error
	if (error != B_OK)
		Unset();
	return error;
}

// Unset
void
RVolume::Unset()
{
	fID = -1;
	fRootNode = -1;
}

// StartWatching
status_t
RVolume::StartWatching(BMessenger target) const
{
	node_ref ref;
	ref.device = fID;
	ref.node = fRootNode;
	return watch_node(&ref, B_WATCH_NAME, target);
}

// StopWatching
status_t
RVolume::StopWatching(BMessenger target) const
{
	node_ref ref;
	ref.device = fID;
	ref.node = fRootNode;
	return watch_node(&ref, B_STOP_WATCHING, target);
}


// RVolumeListListener

// constructor
RVolumeListListener::RVolumeListListener()
{
}

// destructor
RVolumeListListener::~RVolumeListListener()
{
}

// VolumeMounted
void
RVolumeListListener::VolumeMounted(const RVolume *volume)
{
}

// VolumeUnmounted
void
RVolumeListListener::VolumeUnmounted(const RVolume *volume)
{
}

// MountPointMoved
void
RVolumeListListener::MountPointMoved(const RVolume *volume,
									 const entry_ref *oldRoot,
									 const entry_ref *newRoot)
{
}


// RVolumeList

// constructor
RVolumeList::RVolumeList(BMessenger target, BLocker &lock)
	: MessageHandler(),
	  fLock(lock),
	  fTarget(target),
	  fVolumes(20, true),
	  fRoster(),
	  fListener(NULL)
{
// TODO: Remove work-around. At this time R5 libbe's watching functions don't
// work for us. We have an own implementation in `NodeMonitoring.cpp'.
//		fRoster.StartWatching(BMessenger(this));
		watch_node(NULL, B_WATCH_MOUNT, fTarget);
		Rescan();
}

// destructor
RVolumeList::~RVolumeList()
{
// TODO: Remove work-around. At this time R5 libbe's watching functions don't
// work for us. We have an own implementation in `NodeMonitoring.cpp'.
//		fRoster.StopWatching();
		stop_watching(fTarget);
}

// HandleMessage
void
RVolumeList::HandleMessage(BMessage *message)
{
	if (Lock()) {
		switch (message->what) {
			case B_NODE_MONITOR:
			{
				int32 opcode = 0;
				if (message->FindInt32("opcode", &opcode) == B_OK) {
					switch (opcode) {
						case B_DEVICE_MOUNTED:
							_DeviceMounted(message);
							break;
						case B_DEVICE_UNMOUNTED:
							_DeviceUnmounted(message);
							break;
						case B_ENTRY_MOVED:
							_MountPointMoved(message);
							break;
					}
				}
				break;
			}
			default:
				MessageHandler::HandleMessage(message);
				break;
		}
		Unlock();
	}
}

// Rescan
status_t
RVolumeList::Rescan()
{
PRINT(("RVolumeList::Rescan()\n"));
	for (int32 i = fVolumes.CountItems() - 1; i >= 0; i--)
		_RemoveVolumeAt(i);
	fRoster.Rewind();
	BVolume bVolume;
	while (fRoster.GetNextVolume(&bVolume) == B_OK)
		_AddVolume(bVolume.Device());
PRINT(("RVolumeList::Rescan() done\n"));
	return B_OK;
}

// VolumeForDevicePath
const RVolume *
RVolumeList::VolumeForDevicePath(const char *devicePath) const
{
	const RVolume *volume = NULL;
	if (devicePath && fLock.Lock()) {
		bool found = false;
		int32 index = fVolumes.FindBinaryInsertionIndex(
			CompareDevicePathPredicate(devicePath), &found);
		if (found)
			volume = fVolumes.ItemAt(index);
		fLock.Unlock();
	}
	return volume;
}

// Lock
bool
RVolumeList::Lock()
{
	return fLock.Lock();
}

// Unlock
void
RVolumeList::Unlock()
{
	fLock.Unlock();
}

// SetListener
void
RVolumeList::SetListener(RVolumeListListener *listener)
{
	fListener = listener;
}

// Dump
void
RVolumeList::Dump() const
{
	for (int32 i = 0; RVolume *volume = fVolumes.ItemAt(i); i++) {
		printf("volume %ld:\n", volume->ID());
		printf("  root node: %lld", volume->RootNode());
		printf("  device:    `%s'\n", volume->DevicePath());
	}
}

// _AddVolume
RVolume *
RVolumeList::_AddVolume(dev_t id)
{
	RVolume *volume = new RVolume;
	if (volume) {
		status_t error = volume->SetTo(id);
		if (error == B_OK) {
			fVolumes.BinaryInsert(volume,
				CompareDevicePathPredicate(volume->DevicePath()));
			volume->StartWatching(fTarget);
		} else {
			delete volume;
			volume = NULL;
			PRINT(("RVolumeList::_AddVolume(): initializing RVolume failed: "
				   "%s\n", strerror(error)));
		}
	} else
		PRINT(("RVolumeList::_AddVolume(): no memory!\n"));
	return volume;
}

// _RemoveVolumeAt
bool
RVolumeList::_RemoveVolumeAt(int32 index)
{
	bool result = false;
	if (RVolume *volume = fVolumes.RemoveItemAt(index)) {
		volume->StopWatching(fTarget);
		delete volume;
	}
	return result;
}

// _DeviceMounted
void
RVolumeList::_DeviceMounted(BMessage *message)
{
	dev_t device;
	if (message->FindInt32("new device", &device) == B_OK) {
		PRINT(("RVolumeList::_DeviceMounted(%ld)\n", device));
		if (RVolume *volume = _AddVolume(device)) {
			if (fListener)
				fListener->VolumeMounted(volume);
		}
	}
}

// _DeviceUnmounted
void
RVolumeList::_DeviceUnmounted(BMessage *message)
{
	dev_t device;
	if (message->FindInt32("device", &device) == B_OK) {
		PRINT(("RVolumeList::_DeviceUnmounted(%ld)\n", device));
		if (RVolume *volume = fVolumes.FindIf(CompareIDPredicate(device))) {
			volume->StopWatching(fTarget);
			fVolumes.RemoveItem(volume, false);
			if (fListener)
				fListener->VolumeUnmounted(volume);
			delete volume;
		}
	}
}

// _MountPointMoved
void
RVolumeList::_MountPointMoved(BMessage *message)
{
	// Note: The "device" field of the message refers to the device of the
	// parent directory. Therefore we need to get an entry_ref, turn it into
	// a path and get the device for that path.
	// get the message fields
	dev_t device;
	ino_t directory;
	const char *name;
	if (message->FindInt32("device", &device) == B_OK
		&& message->FindInt64("to directory", &directory) == B_OK
		&& message->FindString("name", &name) == B_OK) {
		// init an entry_ref
		entry_ref ref;
		ref.device = device;
		ref.directory = directory;
		ref.set_name(name);
		// get the path and the device for that path
		BPath path;
		if (path.SetTo(&ref) == B_OK
			&& (device = dev_for_path(path.Path())) >= 0) {
			PRINT(("RVolumeList::_MountPointMoved(%ld)\n", device));
			if (RVolume *volume = fVolumes.FindIf(CompareIDPredicate(device))) {
				entry_ref oldRoot;
				volume->GetRootDirEntry(&oldRoot);
				volume->SetRootDirEntry(&ref);
				if (fListener)
					fListener->MountPointMoved(volume, &oldRoot, &ref);
			}
		}
	}
}

