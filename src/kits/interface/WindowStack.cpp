/*
 * Copyright 2010, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Clemens Zeidler <haiku@clemens-zeidler.de>
 */


#include "WindowStack.h"

#include <algorithm>
#include <mutex>
#include <new>
#include <vector>

#include <Window.h>


using namespace BPrivate;


namespace {

typedef std::vector<BMessenger> WindowGroup;

struct WindowGroups {
	std::mutex lock;
	std::vector<WindowGroup> groups;
};

status_t WindowMessenger(const BMessenger& messenger, BMessenger& window);


bool
IsLiveWindow(const BMessenger& messenger)
{
	BMessenger window;
	return WindowMessenger(messenger, window) == B_OK;
}


WindowGroups&
Groups()
{
	static WindowGroups groups;
	return groups;
}


void
PruneGroups(WindowGroups& state)
{
	for (size_t index = 0; index < state.groups.size();) {
		WindowGroup& group = state.groups[index];
		group.erase(std::remove_if(group.begin(), group.end(),
			[](const BMessenger& messenger) { return !IsLiveWindow(messenger); }),
			group.end());
		if (group.empty())
			state.groups.erase(state.groups.begin() + index);
		else
			index++;
	}
}


size_t
FindGroup(const WindowGroups& state, const BMessenger& window)
{
	for (size_t index = 0; index < state.groups.size(); index++) {
		const WindowGroup& group = state.groups[index];
		if (std::find(group.begin(), group.end(), window) != group.end())
			return index;
	}
	return state.groups.size();
}


status_t
WindowMessenger(const BMessenger& messenger, BMessenger& window)
{
	if (!messenger.IsValid())
		return B_BAD_VALUE;
	if (!messenger.IsTargetLocal())
		return B_NOT_SUPPORTED;

	BLooper* looper = NULL;
	BHandler* handler = messenger.Target(&looper);
	BWindow* target = dynamic_cast<BWindow*>(looper);
	if (target == NULL || (handler != target
		&& messenger != BMessenger(NULL, target))) {
		return B_BAD_VALUE;
	}
	window = BMessenger(target);
	return window.IsValid() ? B_OK : B_BAD_VALUE;
}

} // namespace


BWindowStack::BWindowStack(BWindow* window)
	:
	fWindow(window)
{
}


BWindowStack::~BWindowStack()
{

}


status_t
BWindowStack::AddWindow(const BWindow* window)
{
	return AddWindowAt(window, -1);
}


status_t
BWindowStack::AddWindow(const BMessenger& window)
{
	return AddWindowAt(window, -1);
}


status_t
BWindowStack::AddWindowAt(const BWindow* window, int32 position)
{
	if (window == NULL)
		return B_BAD_VALUE;
	return AddWindowAt(BMessenger(window), position);
}


status_t
BWindowStack::AddWindowAt(const BMessenger& window, int32 position)
{
	if (!IsLiveWindow(fWindow))
		return B_BAD_VALUE;
	BMessenger target;
	status_t status = WindowMessenger(window, target);
	if (status != B_OK)
		return status;

	WindowGroups& state = Groups();
	std::lock_guard<std::mutex> guard(state.lock);
	PruneGroups(state);
	size_t destination = FindGroup(state, fWindow);
	size_t source = FindGroup(state, target);
	bool sourceExists = source != state.groups.size();
	if (target == fWindow || (destination != state.groups.size()
		&& source == destination)) {
		return B_NAME_IN_USE;
	}

	size_t count = destination == state.groups.size()
		? 1 : state.groups[destination].size();
	if (position < -1 || (position >= 0 && (size_t)position > count))
		return B_BAD_INDEX;
	if (position == -1)
		position = count;

	try {
		if (destination == state.groups.size()) {
			WindowGroup group;
			group.push_back(fWindow);
			state.groups.push_back(group);
		}
		WindowGroup& group = state.groups[destination];
		group.insert(group.begin() + position, target);
	} catch (const std::bad_alloc&) {
		return B_NO_MEMORY;
	}

	if (sourceExists && source != destination) {
		WindowGroup& group = state.groups[source];
		group.erase(std::find(group.begin(), group.end(), target));
		if (group.empty())
			state.groups.erase(state.groups.begin() + source);
	}
	return B_OK;
}


status_t
BWindowStack::RemoveWindow(const BWindow* window)
{
	if (window == NULL)
		return B_BAD_VALUE;
	return RemoveWindow(BMessenger(window));
}


status_t
BWindowStack::RemoveWindow(const BMessenger& window)
{
	if (!IsLiveWindow(fWindow))
		return B_BAD_VALUE;
	BMessenger target;
	status_t status = WindowMessenger(window, target);
	if (status != B_OK)
		return status;

	WindowGroups& state = Groups();
	std::lock_guard<std::mutex> guard(state.lock);
	PruneGroups(state);
	size_t index = FindGroup(state, fWindow);
	if (index == state.groups.size())
		return target == fWindow ? B_OK : B_ENTRY_NOT_FOUND;
	WindowGroup& group = state.groups[index];
	WindowGroup::iterator found = std::find(group.begin(), group.end(), target);
	if (found == group.end())
		return B_ENTRY_NOT_FOUND;
	group.erase(found);
	if (group.empty())
		state.groups.erase(state.groups.begin() + index);
	return B_OK;
}


status_t
BWindowStack::RemoveWindowAt(int32 position, BMessenger* window)
{
	if (!IsLiveWindow(fWindow))
		return B_BAD_VALUE;

	WindowGroups& state = Groups();
	std::lock_guard<std::mutex> guard(state.lock);
	PruneGroups(state);
	size_t index = FindGroup(state, fWindow);
	if (index == state.groups.size()) {
		if (position != 0)
			return B_BAD_INDEX;
		if (window != NULL)
			*window = fWindow;
		return B_OK;
	}
	WindowGroup& group = state.groups[index];
	if (position < 0 || (size_t)position >= group.size())
		return B_BAD_INDEX;
	if (window != NULL)
		*window = group[position];
	group.erase(group.begin() + position);
	if (group.empty())
		state.groups.erase(state.groups.begin() + index);
	return B_OK;
}


int32
BWindowStack::CountWindows()
{
	if (!IsLiveWindow(fWindow))
		return 0;
	WindowGroups& state = Groups();
	std::lock_guard<std::mutex> guard(state.lock);
	PruneGroups(state);
	size_t index = FindGroup(state, fWindow);
	return index == state.groups.size() ? 1 : state.groups[index].size();
}


status_t
BWindowStack::WindowAt(int32 position, BMessenger& messenger)
{
	if (!IsLiveWindow(fWindow))
		return B_BAD_VALUE;
	WindowGroups& state = Groups();
	std::lock_guard<std::mutex> guard(state.lock);
	PruneGroups(state);
	size_t index = FindGroup(state, fWindow);
	if (index == state.groups.size()) {
		if (position != 0)
			return B_BAD_INDEX;
		messenger = fWindow;
		return B_OK;
	}
	const WindowGroup& group = state.groups[index];
	if (position < 0 || (size_t)position >= group.size())
		return B_BAD_INDEX;
	messenger = group[position];
	return B_OK;
}


bool
BWindowStack::HasWindow(const BWindow* window)
{
	return window != NULL && HasWindow(BMessenger(window));
}


bool
BWindowStack::HasWindow(const BMessenger& window)
{
	BMessenger target;
	if (!IsLiveWindow(fWindow) || WindowMessenger(window, target) != B_OK)
		return false;
	WindowGroups& state = Groups();
	std::lock_guard<std::mutex> guard(state.lock);
	PruneGroups(state);
	size_t index = FindGroup(state, fWindow);
	if (index == state.groups.size())
		return target == fWindow;
	const WindowGroup& group = state.groups[index];
	return std::find(group.begin(), group.end(), target) != group.end();
}
