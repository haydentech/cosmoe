#include <ColumnTypes.h>

//=====================================================================
// Field and column classes for strings.

class FilePanelFileField : public BStringField
{
public:
								FilePanelFileField(BBitmap* bitmap, const char* string);

	const	BBitmap*			Bitmap();
			void				SetBitmap(BBitmap* bitmap);

private:
			BBitmap*			fBitmap;
};


//--------------------------------------------------------------------

class FilePanelFileColumn : public BStringColumn
{
public:
								FilePanelFileColumn(const char* title, float width,
									float minWidth, float maxWidth, uint32 truncate,
									alignment align = B_ALIGN_LEFT);
	virtual	void				DrawField(BField* field, BRect rect, BView* parent);
	virtual	float				GetPreferredWidth(BField* field, BView* parent) const;
	virtual	bool				AcceptsField(const BField* field) const;

private:
			uint32				fTruncate;
};

