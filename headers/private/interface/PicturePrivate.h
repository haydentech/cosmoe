/*
 * Copyright 2012, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PICTURE_PRIVATE_H
#define _PICTURE_PRIVATE_H


#include <Picture.h>
#include <OS.h>

class BPicture::Private {
public:
								Private(BPicture* picture);
			const void*			Data() const;
			int32				Size() const;
			int32				CountPictures() const;
			BPicture*			PictureAt(int32 index) const;
			status_t			ImportData(const void* data, int32 size);
			void				ClearPictures();
			bool				AddPicture(BPicture* picture);
private:
			BPicture*			fPicture;
};


#endif // _PICTURE_PRIVATE_H
