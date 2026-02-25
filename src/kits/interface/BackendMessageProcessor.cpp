/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Backend message processor - handles PortLink protocol for all backends
 */

#include "BackendMessageProcessor.h"

#include <Application.h>
#include <LinkReceiver.h>
#include <LinkSender.h>
#include <ServerProtocol.h>
#include <Rect.h>
#include <Window.h>
#include <WindowPrivate.h>
#include <CosmoeBackend.h>

#include "rectangle.h"

#include <stdlib.h>
#include <stdio.h>

using BPrivate::LinkReceiver;
using BPrivate::LinkSender;

namespace BPrivate {

void
BackendMessageProcessor::ProcessMessages(CosmoeBackend* backend, 
                                         port_id backend_port, 
                                         port_id app_port)
{
	if (!backend)
		return;

	LinkReceiver link(backend_port);
	int32 code;
	
	// Non-blocking check for messages
	status_t status = link.GetNextMessage(code, 0);
	if (status != B_OK)
		return;  // No message available
	
	printf("Backend: Processing message code=%d\n", code);
	
	switch (code) {
		case AS_SET_WINDOW_TITLE: {
			int32_t token;
			char* title = NULL;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.ReadString(&title) == B_OK) {
				
				printf("Backend: SetTitle token=%d, title='%s'\n", (int)token, title);

				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win != NULL && title != NULL)
					backend->WindowSetTitle(win, title);
				
				free(title);
			} else {
				printf("Backend: Failed to read AS_SET_WINDOW_TITLE\n");
			}
			break;
		}
		
		case AS_GET_POSITION: {
			int32_t token;
			
			if (link.Read<int32_t>(&token) == B_OK) {
				int32_t x = 0, y = 0;
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win)
					backend->WindowGetPosition(win, &x, &y);
				
				// Send reply
				LinkSender reply(app_port);
				reply.StartMessage(B_OK);
				reply.Attach<int32_t>(x);
				reply.Attach<int32_t>(y);
				reply.Flush();
				
				printf("Backend: GetPosition returned (%d, %d)\n", (int)x, (int)y);
			} else {
				printf("Backend: Failed to read AS_GET_POSITION\n");
			}
			break;
		}
		
		case AS_GET_SCREEN_FRAME: {
			// Get screen dimensions from display
			struct rectangle allocation;
			backend->DisplayGetScreenDimensions(be_app->Display(), &allocation);
			
			// Convert to BRect
			BRect frame(allocation.x, allocation.y,
			           allocation.x + allocation.width,
			           allocation.y + allocation.height);
			
			// Send reply
			LinkSender reply(app_port);
			reply.StartMessage(B_OK);
			reply.Attach<BRect>(frame);
			reply.Flush();
			
			printf("Backend: GetScreenFrame returned (%.0f,%.0f,%.0f,%.0f)\n",
			       frame.left, frame.top, frame.right, frame.bottom);
			break;
		}
		
		case AS_SET_SIZE_LIMITS: {
			// One-way: client enforces limits locally; just propagate to backend.
			int32_t token;
			BRect frame;
			float minW, maxW, minH, maxH;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<BRect>(&frame) == B_OK
				&& link.Read<float>(&minW) == B_OK
				&& link.Read<float>(&maxW) == B_OK
				&& link.Read<float>(&minH) == B_OK
				&& link.Read<float>(&maxH) == B_OK) {
				
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win) {
					backend->WindowSetSizeLimits(win, minW, maxW, minH, maxH,
					                            &frame, NULL, NULL, NULL, NULL);
				}
				// No reply.
			} else {
				printf("Backend: Failed to read AS_SET_SIZE_LIMITS\n");
			}
			break;
		}

		case AS_WINDOW_MOVE: {
			// One-way: client updates fFrame locally; we just inform the OS.
			int32_t token;
			float x, y;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<float>(&x) == B_OK
				&& link.Read<float>(&y) == B_OK) {
				
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win)
					backend->WindowSetPosition(win, x, y);
				// No reply.
			} else {
				printf("Backend: Failed to read AS_WINDOW_MOVE\n");
			}
			break;
		}

		case AS_WINDOW_RESIZE: {
			int32_t token;
			float width, height;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<float>(&width) == B_OK
				&& link.Read<float>(&height) == B_OK) {
				
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win)
					backend->WindowResize(win, width, height);
				// One-way: no reply.
			} else {
				printf("Backend: Failed to read AS_WINDOW_RESIZE\n");
			}
			break;
		}
		
		case AS_MINIMIZE_WINDOW: {
			int32_t token;
			bool minimize;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<bool>(&minimize) == B_OK) {
				
				printf("Backend: %s window token=%d\n", 
				       minimize ? "Minimizing" : "Restoring", (int)token);
				
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win)
					backend->WindowMinimize(win, minimize);
			} else {
				printf("Backend: Failed to read AS_MINIMIZE_WINDOW\n");
			}
			// One-way message, no reply
			break;
		}
		
		case AS_ACTIVATE_WINDOW: {
			int32_t token;
			bool active;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<bool>(&active) == B_OK) {
				
				printf("Backend: %s window token=%d\n",
				       active ? "Activating" : "Deactivating", (int)token);
				
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (win)
					backend->WindowActivate(win, active);
			} else {
				printf("Backend: Failed to read AS_ACTIVATE_WINDOW\n");
			}
			// One-way message, no reply
			break;
		}
		
		case AS_DELETE_WINDOW: {
			int32_t token;
			void* widget_ptr;
			
			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<void*>(&widget_ptr) == B_OK) {
				
				printf("Backend: Deleting window token=%d, widget=%p\n",
				       (int)token, widget_ptr);

				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				if (widget_ptr != NULL)
					backend->WidgetDestroy((backend_widget_t)widget_ptr);
				if (win)
					backend->WindowDestroy(win);
			} else {
				printf("Backend: Failed to read AS_DELETE_WINDOW\n");
			}
			break;
		}

		case AS_CREATE_WINDOW: {
			void* display_ptr;
			bool offscreen;
			void* data;
			char* title = NULL;
			char* appId = NULL;
			uint32 flags;
			uint32 feel;
			int32_t token;
			int32_t parent_token;
			BRect frame;
			void* topView;

			if (link.Read<BRect>(&frame) == B_OK
				&& link.Read<uint32>(&feel) == B_OK
				&& link.Read<void*>(&display_ptr) == B_OK
				&& link.Read<bool>(&offscreen) == B_OK
				&& link.Read<uint32>(&flags) == B_OK
				&& link.Read<int32_t>(&token) == B_OK
				&& link.Read<void*>(&data) == B_OK
				&& link.Read<int32_t>(&parent_token) == B_OK
				&& link.ReadString(&title) == B_OK
				&& link.ReadString(&appId) == B_OK
				&& link.Read<void*>(&topView) == B_OK) {

				printf("Backend: CreateWindow token=%d display=%p offscreen=%d title='%s'\n",
				       (int)token, display_ptr, (int)offscreen, title);

				backend->WindowCreate(
					(backend_display_t)display_ptr, token, offscreen, data);
				backend_window_t window = backend->WindowLookupByToken(
					(backend_display_t)display_ptr, token);

				if (window != NULL) {
					// Set title (not for popups)
					if (title != NULL && feel != kMenuWindowFeel)
						backend->WindowSetTitle(window, title);

					// Compute and apply size limits
					int32 minWidth = 0, maxWidth = 32767, minHeight = 0, maxHeight = 32767;
					if ((flags & B_NOT_RESIZABLE) ||
					    (flags & B_NOT_H_RESIZABLE && flags & B_NOT_V_RESIZABLE)) {
						minWidth = maxWidth = frame.IntegerWidth();
						minHeight = maxHeight = frame.IntegerHeight();
					} else if (flags & B_NOT_H_RESIZABLE) {
						minWidth = maxWidth = frame.IntegerWidth();
					} else if (flags & B_NOT_V_RESIZABLE) {
						minHeight = maxHeight = frame.IntegerHeight();
					}
					backend->WindowSetMinMaxAllocation(window, minWidth, minHeight, maxWidth, maxHeight);

					// Set app ID for icon lookup
					if (appId != NULL)
						backend->WindowSetAppId(window, appId);

					// Set parent for modal windows
					if (parent_token != B_NULL_TOKEN) {
						backend_window_t parent = backend->WindowLookupByToken(
							(backend_display_t)display_ptr, parent_token);
						if (parent != NULL)
							backend->WindowSetParent(window, parent);
					}

					// Create top widget at window creation time for normal windows.
					backend_widget_t widget = backend->WindowAddWidget(window, topView);
					if (widget != NULL)
						backend->WidgetSetAllocation(widget, 0, 0,
							frame.IntegerWidth() + 1, frame.IntegerHeight() + 1);
				}

				printf("Backend: Created window=%p for token=%d\n", window, (int)token);
				// One-way message — no reply.

				free(title);
				free(appId);
			} else {
				printf("Backend: Failed to read AS_CREATE_WINDOW\n");
			}
			break;
		}
		
		case AS_WINDOW_SHOW: {
			// Show window one-way: create/reuse widget, set all handlers, then map.
			// No reply — client gets widget ptr via direct cosmoe_window_add_widget().
			int32_t token;
			void* topView;
			void* bwindow;
			void* redraw_fn, *motion_fn, *button_fn, *axis_fn, *idle_fn;
			void* frame_resize_fn, *close_fn, *key_fn, *move_fn, *focus_fn;
			int32_t width, height;
			bool offscreen;

			if (link.Read<int32_t>(&token) == B_OK
				&& link.Read<void*>(&topView) == B_OK
				&& link.Read<void*>(&bwindow) == B_OK
				&& link.Read<void*>(&redraw_fn) == B_OK
				&& link.Read<void*>(&motion_fn) == B_OK
				&& link.Read<void*>(&button_fn) == B_OK
				&& link.Read<void*>(&axis_fn) == B_OK
				&& link.Read<void*>(&idle_fn) == B_OK
				&& link.Read<void*>(&frame_resize_fn) == B_OK
				&& link.Read<void*>(&close_fn) == B_OK
				&& link.Read<void*>(&key_fn) == B_OK
				&& link.Read<void*>(&move_fn) == B_OK
				&& link.Read<void*>(&focus_fn) == B_OK
				&& link.Read<int32_t>(&width) == B_OK
				&& link.Read<int32_t>(&height) == B_OK
				&& link.Read<bool>(&offscreen) == B_OK) {

				backend_window_t win = backend->WindowLookupByToken(
					(backend_display_t)be_app->Display(), token);

				if (win != NULL) {
					// WindowAddWidget is idempotent — creates widget on first show,
					// returns existing one on re-show or if client already called it.
					backend_widget_t widget = backend->WindowAddWidget(win, topView);
					if (widget != NULL) {
						backend->WidgetSetAllocation(widget, 0, 0, width + 1, height + 1);
						// (Re-)set widget event handlers
						backend->WidgetSetRedrawHandler(widget, (redraw_handler_t)redraw_fn);
						backend->WidgetSetMotionHandler(widget, (motion_handler_t)motion_fn);
						backend->WidgetSetButtonHandler(widget, (button_handler_t)button_fn);
						backend->WidgetSetAxisHandler(widget, (axis_handler_t)axis_fn);
						backend->WidgetSetIdleHandler(widget, (idle_handler_t)idle_fn);
					}

					// Set window-level handlers (idempotent, OK to set every show)
					if (frame_resize_fn != NULL)
						backend->WindowframeSetResizeHandler(win, (windowframe_resize_handler_t)frame_resize_fn);
					if (close_fn != NULL)
						backend->WindowSetCloseHandler(win, (close_handler_t)close_fn);
					if (key_fn != NULL)
						backend->WindowSetKeyHandler(win, (key_handler_t)key_fn);
					if (move_fn != NULL)
						backend->WindowSetMoveHandler(win, (move_handler_t)move_fn, bwindow);
					if (focus_fn != NULL)
						backend->WindowSetFocusHandler(win, (focus_handler_t)focus_fn, bwindow);

					// Schedule resize so surface is created at correct dimensions
					if (!offscreen)
						backend->WindowScheduleResize(win, width, height);

					// Show the window (no reply — this is one-way)
					backend->WindowShow(win);
				} else {
					printf("Backend: AS_WINDOW_SHOW - window not found for token=%d\n", (int)token);
				}
			} else {
				printf("Backend: Failed to read AS_WINDOW_SHOW\n");
			}
			break;
		}

		case AS_WINDOW_HIDE: {
			int32_t token;
			if (link.Read<int32_t>(&token) == B_OK) {
				backend_window_t win = backend->WindowLookupByToken(
					(backend_display_t)be_app->Display(), token);
				if (win != NULL)
					backend->WindowHide(win);
			} else {
				printf("Backend: Failed to read AS_WINDOW_HIDE\n");
			}
			break;
		}

		case AS_FORCE_UPDATE: {
			int32_t token;
			
			if (link.Read<int32_t>(&token) == B_OK) {
				printf("Backend: ForceUpdate token=%d\n", (int)token);
				
				backend_window_t win = backend->WindowLookupByToken(be_app->Display(), token);
				backend->DisplayTriggerRedraw(be_app->Display(), win, NULL);
			} else {
				printf("Backend: Failed to read AS_FORCE_UPDATE\n");
			}
			// One-way message, no reply
			break;
		}

		default:
			printf("Backend: Unknown message code %d\n", code);
			break;
	}
}

} // namespace BPrivate
