/*
 * Copyright 2006, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus <superstippi@gmx.de>
 */

#ifndef PATH_CONTAINER_H
#define PATH_CONTAINER_H

#include <List.h>

class VectorPath;

#ifdef ICON_O_MATIC
class PathContainerListener {
 public:
								PathContainerListener();
	virtual						~PathContainerListener();

	virtual	void				PathAdded(VectorPath* path) = 0;
	virtual	void				PathRemoved(VectorPath* path) = 0;
};
#endif // ICON_O_MATIC

class PathContainer {
 public:
								PathContainer(bool ownsPaths);
	virtual						~PathContainer();

			bool				AddPath(VectorPath* path);
			bool				RemovePath(VectorPath* path);
			VectorPath*			RemovePath(int32 index);

			void				MakeEmpty();

			int32				CountPaths() const;
			bool				HasPath(VectorPath* path) const;
			int32				IndexOf(VectorPath* path) const;

			VectorPath*			PathAt(int32 index) const;
			VectorPath*			PathAtFast(int32 index) const;

 private:
			void				_MakeEmpty();

			BList				fPaths;
			bool				fOwnsPaths;

#ifdef ICON_O_MATIC
 public:
			bool				AddListener(PathContainerListener* listener);
			bool				RemoveListener(PathContainerListener* listener);

 private:
			void				_NotifyPathAdded(VectorPath* path) const;
			void				_NotifyPathRemoved(VectorPath* path) const;

			BList				fListeners;
#endif // ICON_O_MATIC
};

#endif // PATH_CONTAINER_H
