/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 */


#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <Application.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Roster.h>
#include <Window.h>

#include <private/app/TokenSpace.h>
#include <private/interface/BMCPrivate.h>
#include <private/interface/CosmoeBackendAPI.h>
#include <private/interface/InterfacePrivate.h>
#include <private/interface/NativeMenuPrivate.h>

#include <map>
#include <list>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>


namespace {

const uint32 kMsgInvokeNativeMenu = 'nMnI';

struct NativeMenuEntryStorage {
	std::string label;
	std::string shortcut;
};

struct NativeMenuState {
	BMenuBar* menuBar;
	BWindow* window;
	bool installed;
	int32_t nextCommandID;
	std::unordered_map<int32_t, BMenuItem*> commands;
	std::list<NativeMenuEntryStorage> storage;
	std::vector<cosmoe_native_menu_item> model;
};

static std::unordered_map<const BMenuBar*, NativeMenuState*> sStates;

void
invalidate_native_menu_bar_layout(BMenuBar* menuBar)
{
	if (menuBar == NULL)
		return;

	menuBar->InvalidateLayout();
	menuBar->Invalidate();

	BView* parent = menuBar->Parent();
	if (parent != NULL)
		parent->InvalidateLayout();

	BWindow* window = menuBar->Window();
	if (window != NULL)
		window->InvalidateLayout();
}

bool
parse_bool_env(const char* value, bool& parsed)
{
	if (value == NULL || value[0] == '\0')
		return false;

	if (strcmp(value, "1") == 0
		|| strcasecmp(value, "true") == 0
		|| strcasecmp(value, "yes") == 0
		|| strcasecmp(value, "on") == 0) {
		parsed = true;
		return true;
	}

	if (strcmp(value, "0") == 0
		|| strcasecmp(value, "false") == 0
		|| strcasecmp(value, "no") == 0
		|| strcasecmp(value, "off") == 0) {
		parsed = false;
		return true;
	}

	return false;
}


static BMenuBar*
root_menu_bar_for(BMenu* menu)
{
	BMenu* current = menu;
	while (current != NULL && current->Supermenu() != NULL)
		current = current->Supermenu();

	BMenuBar* menuBar = dynamic_cast<BMenuBar*>(current);
	if (dynamic_cast<_BMCMenuBar_*>(menuBar) != NULL)
		return NULL;

	return menuBar;
}


static bool
is_native_menu_bar_candidate(BMenuBar* menuBar)
{
	return menuBar != NULL && dynamic_cast<_BMCMenuBar_*>(menuBar) == NULL;
}


static std::string
shortcut_string_for(BMenuItem* item)
{
	if (item == NULL)
		return std::string();

	uint32 modifiers = 0;
	char shortcut = item->Shortcut(&modifiers);
	if (shortcut == 0)
		return std::string();

	std::string result;
	if ((modifiers & B_SHIFT_KEY) != 0)
		result += "Shift+";
	if ((modifiers & B_CONTROL_KEY) != 0)
		result += "Windows+";
	if ((modifiers & B_OPTION_KEY) != 0)
		result += "Alt+";
	if ((modifiers & B_COMMAND_KEY) != 0)
		result += "Ctrl+";

	result += shortcut;
	return result;
}


static void
append_menu_items(BMenu* menu, int32_t parentID, NativeMenuState& state)
{
	if (menu == NULL)
		return;

	int32 count = menu->CountItems();
	for (int32 i = 0; i < count; i++) {
		BMenuItem* item = menu->ItemAt(i);
		if (item == NULL)
			continue;

		state.storage.emplace_back();
		NativeMenuEntryStorage& storage = state.storage.back();
		cosmoe_native_menu_item nativeItem = {};
		nativeItem.command_id = state.nextCommandID++;
		nativeItem.parent_id = parentID;

		if (dynamic_cast<BSeparatorItem*>(item) != NULL) {
			nativeItem.flags |= COSMOE_NATIVE_MENU_ITEM_SEPARATOR;
		} else {
			const char* label = item->Label();
			if (label != NULL)
				storage.label = label;
			nativeItem.label = storage.label.empty() ? NULL : storage.label.c_str();

			if (!item->IsEnabled())
				nativeItem.flags |= COSMOE_NATIVE_MENU_ITEM_DISABLED;
			if (item->IsMarked())
				nativeItem.flags |= COSMOE_NATIVE_MENU_ITEM_MARKED;
		}

		BMenu* submenu = item->Submenu();
		if (submenu != NULL) {
			nativeItem.flags |= COSMOE_NATIVE_MENU_ITEM_SUBMENU;
		} else if ((nativeItem.flags & COSMOE_NATIVE_MENU_ITEM_SEPARATOR) == 0) {
			storage.shortcut = shortcut_string_for(item);
			nativeItem.shortcut = storage.shortcut.empty()
				? NULL : storage.shortcut.c_str();
			state.commands[nativeItem.command_id] = item;
		}

		state.model.push_back(nativeItem);

		if (submenu != NULL)
			append_menu_items(submenu, nativeItem.command_id, state);
	}
}


static void
native_menu_callback(void* user_data, void* input, int index)
{
	(void)input;
	NativeMenuState* state = static_cast<NativeMenuState*>(user_data);
	if (state == NULL || state->menuBar == NULL)
		return;

	BMessage message(kMsgInvokeNativeMenu);
	message.AddInt32("command_id", index);
	BMessenger(state->menuBar).SendMessage(&message);
}


static bool
sync_native_menu_bar(NativeMenuState& state)
{
	if (state.window == NULL || be_app == NULL || be_app->Display() == NULL)
		return false;

	state.commands.clear();
	state.storage.clear();
	state.model.clear();
	state.nextCommandID = 1;

	append_menu_items(state.menuBar, -1, state);
	if (state.model.empty())
		return false;

	status_t status = cosmoe_window_set_native_menubar(be_app->Display(),
		state.window->WindowToken(), state.model.data(),
		(int32_t)state.model.size(), native_menu_callback, &state);
	state.installed = status == B_OK;
	return state.installed;
}

}


namespace BPrivate {

bool
app_requests_native_menus()
{
	bool envValue;
	if (parse_bool_env(getenv("COSMOE_NATIVE_MENUS"), envValue))
		return envValue;

	if (be_app == NULL)
		return false;

	app_info info;
	if (be_app->GetAppInfo(&info) != B_OK)
		return false;

	return (info.flags & B_NATIVE_MENUS) != 0;
}


bool
native_menus_enabled()
{
	return cosmoe_backend_supports_native_menus() && app_requests_native_menus();
}


bool
attach_native_menus(BMenuBar* menuBar)
{
	if (!is_native_menu_bar_candidate(menuBar) || !native_menus_enabled())
		return false;

	BWindow* window = menuBar->Window();
	if (window == NULL || window->WindowToken() == B_NULL_TOKEN)
		return false;

	std::unordered_map<const BMenuBar*, NativeMenuState*>::iterator existing
		= sStates.find(menuBar);
	if (existing != sStates.end())
		return existing->second != NULL && existing->second->installed;

	NativeMenuState* state = new(std::nothrow) NativeMenuState();
	if (state == NULL)
		return false;

	state->menuBar = menuBar;
	state->window = window;
	state->installed = false;
	state->nextCommandID = 1;

	if (!sync_native_menu_bar(*state)) {
		delete state;
		return false;
	}

	sStates[menuBar] = state;
	invalidate_native_menu_bar_layout(menuBar);
	return true;
}


void
detach_native_menu_bar(BMenuBar* menuBar)
{
	if (menuBar == NULL)
		return;

	std::unordered_map<const BMenuBar*, NativeMenuState*>::iterator it
		= sStates.find(menuBar);
	if (it == sStates.end())
		return;

	NativeMenuState* state = it->second;
	if (state != NULL && state->window != NULL && be_app != NULL
		&& be_app->Display() != NULL) {
		cosmoe_window_clear_native_menubar(be_app->Display(),
			state->window->WindowToken());
	}

	delete state;
	sStates.erase(it);
}


void
update_native_menu_bar(BMenu* menu)
{
	BMenuBar* menuBar = root_menu_bar_for(menu);
	if (menuBar == NULL)
		return;

	std::unordered_map<const BMenuBar*, NativeMenuState*>::iterator it
		= sStates.find(menuBar);
	if (it == sStates.end() || it->second == NULL)
		return;

	sync_native_menu_bar(*it->second);
}


bool
has_native_menu_bar(const BMenuBar* menuBar)
{
	std::unordered_map<const BMenuBar*, NativeMenuState*>::const_iterator it
		= sStates.find(menuBar);
	return it != sStates.end() && it->second != NULL && it->second->installed;
}


bool
dispatch_native_menu_message(BMenuBar* menuBar, BMessage* message)
{
	if (menuBar == NULL || message == NULL || message->what != kMsgInvokeNativeMenu)
		return false;

	std::unordered_map<const BMenuBar*, NativeMenuState*>::iterator it
		= sStates.find(menuBar);
	if (it == sStates.end() || it->second == NULL)
		return false;

	int32 commandID;
	if (message->FindInt32("command_id", &commandID) != B_OK)
		return false;

	NativeMenuState* state = it->second;
	std::unordered_map<int32_t, BMenuItem*>::iterator commandIt
		= state->commands.find(commandID);
	if (commandIt == state->commands.end() || commandIt->second == NULL)
		return true;

	message->AddPointer("_native_item", commandIt->second);
	return true;
}

}