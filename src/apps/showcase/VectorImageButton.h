/*
 * Copyright 2010 Stephan Aßmus <superstippi@gmx.de>. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef VECTOR_IMAGE_BUTTON_H
#define VECTOR_IMAGE_BUTTON_H

#include <Button.h>
#include <Size.h>


class BBitmap;


namespace BPrivate {

class BVectorImageButton : public BButton {
public:
	enum {
		BUTTON_BACKGROUND = 0,
		MENUBAR_BACKGROUND,
		NO_BACKGROUND
	};
	
								// Draw the image at a given fixed size
								BVectorImageButton(const char* resourceName, BSize imageSize, BMessage* message);

								// Draw the image at the full size of button, scaling as needed
								BVectorImageButton(const char* resourceName, BMessage* message);

	virtual						~BVectorImageButton();

	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();
	virtual	BSize				PreferredSize();
	virtual	void				Draw(BRect updateRect);

			status_t			InitCheck() const;

			status_t			LoadBitmap(const char* resourceName);
			status_t			LoadBitmap(const char* resourceName,
									BSize imageSize);
			void				SetBackgroundMode(uint32 mode);
			void				SetAutoscale(bool autoscale);

private:
			status_t			_LoadBitmap(const char* resourceName,
									BSize imageSize,
									bool updateImageSize = true);

			BBitmap*			fBitmap;
			uint32				fBackgroundMode;
			BSize				fImageSize;
			BSize				fRasterizedSize;
			bool				fAutoscale;
			BString				fResourceName;
			status_t			fInitStatus;
};

};

using BPrivate::BVectorImageButton;


#endif // VECTOR_IMAGE_BUTTON_H
