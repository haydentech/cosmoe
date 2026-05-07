#include "FilePanelFileColumn.h"

#define kTEXT_MARGIN	8
#define kICON_WIDTH    24

// #pragma mark - FilePanelFileField


FilePanelFileField::FilePanelFileField(BBitmap* bitmap, const char* string)
	:
    BStringField(string),
    fBitmap(bitmap)
{
}


const BBitmap*
FilePanelFileField::Bitmap()
{
	return fBitmap;
}


void
FilePanelFileField::SetBitmap(BBitmap* bitmap)
{
	fBitmap = bitmap;
}



// #pragma mark - FilePanelFileColumn


FilePanelFileColumn::FilePanelFileColumn(const char* title, float width, float minWidth,
	float maxWidth, uint32 truncate, alignment align)
	:
	BStringColumn(title, width, minWidth, maxWidth, align),
	fTruncate(truncate)
{
}


void
FilePanelFileColumn::DrawField(BField* _field, BRect rect, BView* parent)
{
    FilePanelFileField* bitmapField = static_cast<FilePanelFileField*>(_field);
	const BBitmap* bitmap = bitmapField->Bitmap();

	if (bitmap != NULL) {
		float x = 0.0;
		float iconWidth = bitmap_logical_width(bitmap);
		float iconHeight = bitmap_logical_height(bitmap);
		float y = rect.top + ((rect.Height() - (iconHeight - 1)) / 2);

		switch (Alignment()) {
			default:
			case B_ALIGN_LEFT:
				x = rect.left + kTEXT_MARGIN;
				break;

			case B_ALIGN_CENTER:
				x = rect.left + ((rect.Width() - (iconWidth - 1)) / 2);
				break;

			case B_ALIGN_RIGHT:
				x = rect.right - kTEXT_MARGIN - (iconWidth - 1);
				break;
		}
		// setup drawing mode according to bitmap color space,
		// restore previous mode after drawing
		drawing_mode oldMode = parent->DrawingMode();
		if (bitmap->ColorSpace() == B_RGBA32
			|| bitmap->ColorSpace() == B_RGBA32_BIG) {
			parent->SetDrawingMode(B_OP_ALPHA);
			parent->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		} else {
			parent->SetDrawingMode(B_OP_OVER);
		}

		parent->DrawBitmap(bitmap, BPoint(x, y));

		parent->SetDrawingMode(oldMode);
	}

	float width = rect.Width() - (2 * kTEXT_MARGIN);
	FilePanelFileField* field = static_cast<FilePanelFileField*>(_field);
	float fieldWidth = field->Width();
	bool updateNeeded = width != fieldWidth;

	if (updateNeeded) {
		BString out_string(field->String());
		float preferredWidth = parent->StringWidth(out_string.String());
		if (width < preferredWidth) {
			parent->TruncateString(&out_string, fTruncate, width + 2);
			field->SetClippedString(out_string.String());
		} else
			field->SetClippedString("");
		field->SetWidth(width);
	}

	DrawString(field->HasClippedString()
		? field->ClippedString()
		: field->String(), parent, rect.OffsetByCopy(kICON_WIDTH, 0));
}


float
FilePanelFileColumn::GetPreferredWidth(BField *_field, BView* parent) const
{
	FilePanelFileField* field = static_cast<FilePanelFileField*>(_field);
	return kICON_WIDTH + parent->StringWidth(field->String()) + 2 * kTEXT_MARGIN;
}



bool
FilePanelFileColumn::AcceptsField(const BField *field) const
{
	return static_cast<bool>(dynamic_cast<const FilePanelFileField*>(field));
}