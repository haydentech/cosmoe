/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 */

#ifndef _NATIVE_MENU_PRIVATE_H
#define _NATIVE_MENU_PRIVATE_H


class BMenu;
class BMenuBar;
class BMessage;


namespace BPrivate {

bool attach_native_menus(BMenuBar* menuBar);
void detach_native_menu_bar(BMenuBar* menuBar);
void update_native_menu_bar(BMenu* menu);
bool has_native_menu_bar(const BMenuBar* menuBar);
bool dispatch_native_menu_message(BMenuBar* menuBar, BMessage* message);

}


#endif