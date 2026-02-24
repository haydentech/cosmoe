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
			void* window_ptr;
			char* title = NULL;
			
			if (link.Read<void*>(&window_ptr) == B_OK
				&& link.ReadString(&title) == B_OK) {
				
				printf("Backend: SetTitle window=%p, title='%s'\n", 
				       window_ptr, title);
				
				if (window_ptr != NULL && title != NULL)
					backend->WindowSetTitle((backend_window_t)window_ptr, title);
				
				free(title);
			} else {
				printf("Backend: Failed to read AS_SET_WINDOW_TITLE\n");
			}
			break;
		}
		
		case AS_GET_POSITION: {
			void* window_ptr;
			
			if (link.Read<void*>(&window_ptr) == B_OK) {
				int32_t x = 0, y = 0;
				backend->WindowGetPosition((backend_window_t)window_ptr, 
				                          &x, &y);
				
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
			void* window_ptr;
			BRect frame;
			float minW, maxW, minH, maxH;
			
			if (link.Read<void*>(&window_ptr) == B_OK
				&& link.Read<BRect>(&frame) == B_OK
				&& link.Read<float>(&minW) == B_OK
				&& link.Read<float>(&maxW) == B_OK
				&& link.Read<float>(&minH) == B_OK
				&& link.Read<float>(&maxH) == B_OK) {
				
				float outMinW = minW, outMaxW = maxW, outMinH = minH, outMaxH = maxH;
				
				backend->WindowSetSizeLimits((backend_window_t)window_ptr,
				                            minW, maxW, minH, maxH,
				                            &frame, 
				                            &outMinW, &outMaxW, 
				                            &outMinH, &outMaxH);
				
				// Send reply with enforced limits
				LinkSender reply(app_port);
				reply.StartMessage(B_OK);
				reply.Attach<BRect>(frame);
				reply.Attach<float>(outMinW);
				reply.Attach<float>(outMaxW);
				reply.Attach<float>(outMinH);
				reply.Attach<float>(outMaxH);
				reply.Flush();
				
				printf("Backend: SetSizeLimits enforced limits (%.0f-%.0f, %.0f-%.0f)\n",
				       outMinW, outMaxW, outMinH, outMaxH);
			} else {
				printf("Backend: Failed to read AS_SET_SIZE_LIMITS\n");
			}
			break;
		}

		case AS_WINDOW_MOVE: {
			void* window_ptr;
			float x, y;
			
			if (link.Read<void*>(&window_ptr) == B_OK
				&& link.Read<float>(&x) == B_OK
				&& link.Read<float>(&y) == B_OK) {
				
				printf("Backend: WindowMoveTo to %.0fx%.0f\n", x, y);
				
				backend->WindowSetPosition((backend_window_t)window_ptr, x, y);
				
				// Send reply
				LinkSender reply(app_port);
				reply.StartMessage(B_OK);
				reply.Flush();
			} else {
				printf("Backend: Failed to read AS_WINDOW_MOVE\n");
			}
			break;
		}

		case AS_WINDOW_RESIZE: {
			void* window_ptr;
			float width, height;
			
			if (link.Read<void*>(&window_ptr) == B_OK
				&& link.Read<float>(&width) == B_OK
				&& link.Read<float>(&height) == B_OK) {
				
				printf("Backend: WindowResize to %.0fx%.0f\n", width, height);
				
				backend->WindowResize((backend_window_t)window_ptr, width, height);
				
				// Send reply
				LinkSender reply(app_port);
				reply.StartMessage(B_OK);
				reply.Flush();
			} else {
				printf("Backend: Failed to read AS_WINDOW_RESIZE\n");
			}
			break;
		}
		
		case AS_MINIMIZE_WINDOW: {
			void* window_ptr;
			bool minimize;
			
			if (link.Read<void*>(&window_ptr) == B_OK
				&& link.Read<bool>(&minimize) == B_OK) {
				
				printf("Backend: %s window %p\n", 
				       minimize ? "Minimizing" : "Restoring", window_ptr);
				
				backend->WindowMinimize((backend_window_t)window_ptr, 
				                       minimize);
			} else {
				printf("Backend: Failed to read AS_MINIMIZE_WINDOW\n");
			}
			// One-way message, no reply
			break;
		}
		
		case AS_ACTIVATE_WINDOW: {
			// void* window_ptr;
			// bool active;
			
			// if (link.Read<void*>(&window_ptr) == B_OK
			// 	&& link.Read<bool>(&active) == B_OK) {
				
			// 	printf("Backend: %s window %p\n", 
			// 	       active ? "Activating" : "Deactivating", window_ptr);
				
			// 	backend->WindowActivate((backend_window_t)window_ptr, active);
			// } else {
			// 	printf("Backend: Failed to read AS_ACTIVATE_WINDOW\n");
			// }
			// One-way message, no reply
			break;
		}
		
		case AS_DELETE_WINDOW: {
			void* window_ptr;
			void* widget_ptr;
			
			if (link.Read<void*>(&window_ptr) == B_OK
				&& link.Read<void*>(&widget_ptr) == B_OK)
				{
				
				printf("Backend: Deleting window=%p, widget=%p\n",
				       window_ptr, widget_ptr);

				backend->WidgetDestroy((backend_widget_t)widget_ptr);
				backend->WindowDestroy((backend_window_t)window_ptr);
				
				// Send reply
				LinkSender reply(app_port);
				reply.StartMessage(B_OK);
				reply.Flush();
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
			uint32 flags;
			uint32 feel;
			BRect frame;

			if (link.Read<BRect>(&frame) == B_OK
				&& link.Read<uint32>(&feel) == B_OK
				&& link.Read<void*>(&display_ptr) == B_OK
				&& link.Read<bool>(&offscreen) == B_OK
				&& link.Read<uint32>(&flags) == B_OK
				&& link.Read<void*>(&data) == B_OK
				&& link.ReadString(&title) == B_OK) {

				printf("Backend: CreateWindow display=%p offscreen=%d data=%p title='%s'\n",
				       display_ptr, (int)offscreen, data, title);

				backend_window_t window = backend->WindowCreate(
					(backend_display_t)display_ptr, offscreen, data);

				// Don't set title for popup/menu windows - they should have no title bar, and Wayland will 
				// automatically make a title bar if a name is provided
				if (window != NULL && title != NULL && feel != kMenuWindowFeel)
					backend->WindowSetTitle((backend_window_t)window, title);

				int32 minWidth = 0, maxWidth = 32767, minHeight = 0, maxHeight = 32767;

				if ((flags & B_NOT_RESIZABLE) || (flags & B_NOT_H_RESIZABLE && flags & B_NOT_V_RESIZABLE)) {
					minWidth = frame.IntegerWidth();
					minHeight = frame.IntegerHeight();
					maxWidth = frame.IntegerWidth();
					maxHeight = frame.IntegerHeight();
				} else if (flags & B_NOT_H_RESIZABLE) {
					minWidth = frame.IntegerWidth();
					minHeight = 0;
					maxWidth = frame.IntegerWidth();
					maxHeight = 32767;
				} else if (flags & B_NOT_V_RESIZABLE) {
					minWidth = 0;
					minHeight = frame.IntegerHeight();
					maxWidth = 32767;
					maxHeight = frame.IntegerHeight();
				}

				if (window != NULL)
					backend->WindowSetMinMaxAllocation(window, minWidth, minHeight, maxWidth, maxHeight);

				printf("Backend: Created window=%p\n", window);

				LinkSender reply(app_port);
				reply.StartMessage(B_OK);
				reply.Attach<void*>(window);
				reply.Attach<BRect>(frame);
				// Send as float to match what the client reads with Read<float>
				reply.Attach<float>((float)minWidth);
				reply.Attach<float>((float)maxWidth);
				reply.Attach<float>((float)minHeight);
				reply.Attach<float>((float)maxHeight);
				reply.Flush();

				free(title);
			} else {
				printf("Backend: Failed to read AS_CREATE_WINDOW\n");
			}
			break;
		}
		
		/* TODO: Implement these when backend methods are available
		case AS_CREATE_WINDOW: {
			void* bwindow_ptr;
			BRect frame;
			char* title = NULL;
			uint32 look, feel, flags;
			bool offscreen;
			float minW, maxW, minH, maxH;
			
			if (link.Read<void*>(&bwindow_ptr) == B_OK
				&& link.Read<BRect>(&frame) == B_OK
				&& link.ReadString(&title) == B_OK
				&& link.Read<uint32>(&look) == B_OK
				&& link.Read<uint32>(&feel) == B_OK
				&& link.Read<uint32>(&flags) == B_OK
				&& link.Read<bool>(&offscreen) == B_OK
				&& link.Read<float>(&minW) == B_OK
				&& link.Read<float>(&maxW) == B_OK
				&& link.Read<float>(&minH) == B_OK
				&& link.Read<float>(&maxH) == B_OK) {
				
				printf("Backend: Creating window frame=(%.0f,%.0f,%.0f,%.0f) "
				       "title='%s'\n",
				       frame.left, frame.top, frame.right, frame.bottom,
				       title);
				
				backend_window_t window = NULL;
				backend_windowframe_t windowframe = NULL;
				
				status_t result = backend->WindowCreateFromPortLink(
					bwindow_ptr, frame, title, 
					look, feel, flags, offscreen,
					minW, maxW, minH, maxH,
					&window, &windowframe);
				
				// Send reply with created pointers
				LinkSender reply(app_port);
				reply.StartMessage(result);
				if (result == B_OK) {
					reply.Attach<void*>(window);
					reply.Attach<void*>(windowframe);
					reply.Attach<void*>(windowframe);  // Same for topview on X11
				}
				reply.Flush();
				
				if (result == B_OK) {
					printf("Backend: Created window=%p, frame=%p\n", 
					       window, windowframe);
				} else {
					printf("Backend: Window creation failed with status=%d\n", 
					       (int)result);
				}
				
				free(title);
			} else {
				printf("Backend: Failed to read AS_CREATE_WINDOW\n");
			}
			break;
		}
*/
		case AS_FORCE_UPDATE: {
			void* window_ptr;
			
			if (link.Read<void*>(&window_ptr) == B_OK) {
				printf("Backend: ForceUpdate window=%p\n", window_ptr);
				
				// Trigger redraw - equivalent to display_trigger_redraw
				// Note: We pass NULL for widget since this triggers update for the whole window
				backend->DisplayTriggerRedraw(be_app->Display(), 
				                             (backend_window_t)window_ptr,
				                             NULL);
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
