//****************************************************************************************
//
//	File:		CPUButton.h
//
//	Written by:	Daniel Switkin
//
//	Copyright 1999, Be Incorporated
//
//****************************************************************************************

#ifndef CPUBUTTON_H
#define CPUBUTTON_H

#include <Control.h>
#include <MessageRunner.h>

class CPUButton : public BControl {
	public:
		CPUButton(BRect rect, const char *name, const char *label, BMessage *message);
		CPUButton(BMessage *message);
		virtual ~CPUButton();

		virtual void Draw(BRect rect);
		virtual void MouseDown(BPoint point);
		virtual void MouseUp(BPoint point);
		virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);

		
		virtual void MessageReceived(BMessage *message);
		virtual void AttachedToWindow();
		virtual void DetachedFromWindow();

		status_t Invoke(BMessage *message = NULL);
		static CPUButton *Instantiate(BMessage *data);
		status_t Archive(BMessage *data, bool deep = true) const;
		
		void UpdateColors(int32 color);

	private:
		rgb_color on_color, off_color;
		bool replicant;
		BMessageRunner *messagerunner;
};

#endif	// CPUBUTTON_H
