/*
 * Copyright 2001-2015, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Jérôme Duval, jerome.duval@free.fr
 *		Axel Dörfler, axeld@pinc-software.de
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Andrej Spielmann, <andrej.spielmann@seh.ox.ac.uk>
 */


#include <FontPrivate.h>
#include <ObjectList.h>
#include <ServerProtocol.h>
#include <String.h>
#include <truncate_string.h>
#include <utf8_functions.h>

#include <Autolock.h>
#include <Font.h>
#include <Locker.h>
#include <Message.h>
#include <Rect.h>
#include <Shape.h>
#include <String.h>
#include <UnicodeBlockObjects.h>

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <pango/pango-layout.h>
#include <pango/pangocairo.h>

using namespace std;

const float kUninitializedAscent = INFINITY;
const uint32 kUninitializedExtraFlags = 0xffffffff;

// The actual objects which the globals point to
static BFont sPlainFont;
static BFont sBoldFont;
static BFont sFixedFont;

const BFont* be_plain_font = &sPlainFont;
const BFont* be_bold_font = &sBoldFont;
const BFont* be_fixed_font = &sFixedFont;


struct style {
	BString	name;
	uint16	face;
	uint32	flags;
};

struct family {
	BString	name;
	uint32	flags;
	BObjectList<style> styles;
};

namespace {

float
LogicalLayoutWidth(PangoLayout* layout)
{
	PangoRectangle logicalRect;
	pango_layout_get_extents(layout, NULL, &logicalRect);
	return (float)PANGO_PIXELS_CEIL(logicalRect.width);
}


float
MeasuredLayoutWidth(PangoLayout* layout, cairo_t* cr, float displayScale)
{
	float width = LogicalLayoutWidth(layout);
	if (displayScale <= 1.0f)
		return width;

	pango_cairo_update_layout(cr, layout);

	int pixelWidth = 0;
	pango_layout_get_pixel_size(layout, &pixelWidth, NULL);
	if ((float)pixelWidth > width)
		return (float)pixelWidth;

	return width;
}

class FontList : public BLocker {
public:
								FontList();
	virtual						~FontList();

	static	FontList*			Default();

			bool				UpdatedOnServer();

			status_t			FamilyAt(int32 index, font_family* _family,
									uint32* _flags);
			status_t			StyleAt(font_family family, int32 index,
									font_style* _style, uint16* _face,
									uint32* _flags);

			int32				CountFamilies();
			int32				CountStyles(font_family family);

private:
			status_t			_UpdateIfNecessary();
			status_t			_Update();
			uint32				_RevisionOnServer();
			family*				_FindFamily(font_family name);
	static	void				_InitSingleton();

private:
			BObjectList<family>	fFamilies;
			family*				fLastFamily;
			bigtime_t			fLastUpdate;
			uint32				fRevision;

	static	pthread_once_t		sDefaultInitOnce;
	static	FontList*			sDefaultInstance;
};

pthread_once_t FontList::sDefaultInitOnce = PTHREAD_ONCE_INIT;
FontList* FontList::sDefaultInstance = NULL;

}	// unnamed namespace


//	#pragma mark -


static int
compare_families(const family* a, const family* b)
{
	// TODO: compare font names according to the user's locale settings
	return strcmp(a->name.String(), b->name.String());
}


namespace {

FontList::FontList()
	: BLocker("font list"),
	fLastFamily(NULL),
	fLastUpdate(0),
	fRevision(0)
{
}


FontList::~FontList()
{
}


/*static*/ FontList*
FontList::Default()
{
	if (sDefaultInstance == NULL)
		pthread_once(&sDefaultInitOnce, &_InitSingleton);

	return sDefaultInstance;
}


bool
FontList::UpdatedOnServer()
{
	return _RevisionOnServer() != fRevision;
}


status_t
FontList::FamilyAt(int32 index, font_family* _family, uint32* _flags)
{
	BAutolock locker(this);

	status_t status = _UpdateIfNecessary();
	if (status < B_OK)
		return status;

	::family* family = fFamilies.ItemAt(index);
	if (family == NULL)
		return B_BAD_VALUE;

	memcpy(*_family, family->name.String(), family->name.Length() + 1);
	if (_flags)
		*_flags = family->flags;
	return B_OK;
}


status_t
FontList::StyleAt(font_family familyName, int32 index, font_style* _style,
	uint16* _face, uint32* _flags)
{
	BAutolock locker(this);

	status_t status = _UpdateIfNecessary();
	if (status < B_OK)
		return status;

	::family* family = _FindFamily(familyName);
	if (family == NULL)
		return B_BAD_VALUE;

	::style* style = family->styles.ItemAt(index);
	if (style == NULL)
		return B_BAD_VALUE;

	memcpy(*_style, style->name.String(), style->name.Length() + 1);
	if (_face)
		*_face = style->face;
	if (_flags)
		*_flags = style->flags;
	return B_OK;
}


int32
FontList::CountFamilies()
{
	BAutolock locker(this);

	_UpdateIfNecessary();
	return fFamilies.CountItems();
}


int32
FontList::CountStyles(font_family familyName)
{
	BAutolock locker(this);

	_UpdateIfNecessary();

	::family* family = _FindFamily(familyName);
	if (family == NULL)
		return 0;

	return family->styles.CountItems();
}


status_t
FontList::_Update()
{
	// check version

	uint32 revision = _RevisionOnServer();
	fLastUpdate = system_time();

	// are we up-to-date already?
	if (revision == fRevision)
		return B_OK;

	fFamilies.MakeEmpty();
	fLastFamily = NULL;

	// Use Pango/FontConfig to enumerate system fonts
	PangoFontMap* fontmap = pango_cairo_font_map_get_default();
	if (fontmap == NULL) {
		fprintf(stderr, "FontList::_Update: pango_cairo_font_map_get_default returned NULL!\n");
		return B_ERROR;
	}

	// List all font families
	PangoFontFamily** families;
	int n_families;
	pango_font_map_list_families(fontmap, &families, &n_families);

	for (int i = 0; i < n_families; i++) {
		const char* familyName = pango_font_family_get_name(families[i]);
		
		::family* family = new (nothrow) ::family;
		if (family == NULL) {
			g_free(families);
			return B_NO_MEMORY;
		}

		family->name = familyName;
		family->flags = B_IS_FIXED; // Will be updated if we find non-fixed styles
		bool hasNonFixed = false;

		// Get all faces/styles for this family
		PangoFontFace** faces;
		int n_faces;
		pango_font_family_list_faces(families[i], &faces, &n_faces);

		for (int j = 0; j < n_faces; j++) {
			const char* faceName = pango_font_face_get_face_name(faces[j]);
			
			::style* style = new (nothrow) ::style;
			if (style == NULL) {
				g_free(faces);
				g_free(families);
				delete family;
				return B_NO_MEMORY;
			}

			style->name = faceName;
			style->flags = 0;
			
			// Determine face flags from Pango font description
			PangoFontDescription* desc = pango_font_face_describe(faces[j]);
			
			if (desc == NULL) {
				// Skip faces without descriptions (shouldn't happen, but be safe)
				fprintf(stderr, "Warning: pango_font_face_describe returned NULL for face '%s'\n", faceName);
				delete style;
				continue;
			}
			
			// Check weight for bold
			PangoWeight weight = pango_font_description_get_weight(desc);
			if (weight >= PANGO_WEIGHT_BOLD)
				style->face = B_BOLD_FACE;
			else
				style->face = B_REGULAR_FACE;
			
			// Check style for italic
			PangoStyle pangoStyle = pango_font_description_get_style(desc);
			if (pangoStyle == PANGO_STYLE_ITALIC || pangoStyle == PANGO_STYLE_OBLIQUE)
				style->face |= B_ITALIC_FACE;
			
			// Check if monospace
			if (pango_font_family_is_monospace(families[i]))
				style->flags |= B_IS_FIXED;
			else
				hasNonFixed = true;

			pango_font_description_free(desc);
			
			family->styles.AddItem(style);
		}
		
		g_free(faces);

		// Update family flags
		if (hasNonFixed)
			family->flags = 0;

		fFamilies.BinaryInsert(family, compare_families);
	}

	g_free(families);

	fRevision = revision;

	// if the font list has been changed in the mean time, just update again
	if (UpdatedOnServer())
		_Update();

	return B_OK;
}


status_t
FontList::_UpdateIfNecessary()
{
	// an updated font list is at least valid for 1 second
	if (fLastUpdate > system_time() - 1000000)
		return B_OK;

	return _Update();
}


uint32
FontList::_RevisionOnServer()
{
	return 1;
}


family*
FontList::_FindFamily(font_family name)
{
	if (fLastFamily != NULL && fLastFamily->name == name)
		return fLastFamily;

	::family family;
	family.name = name;
	fLastFamily = const_cast< ::family*>(fFamilies.BinarySearch(family,
		compare_families));
	return fLastFamily;
}


/*static*/ void
FontList::_InitSingleton()
{
	sDefaultInstance = new FontList;
}

}	// unnamed namespace


//	#pragma mark -


void
_init_global_fonts_()
{
	sPlainFont.SetFamilyAndStyle(DEFAULT_PLAIN_FONT_FAMILY, DEFAULT_PLAIN_FONT_STYLE);
	sPlainFont.SetFlags(B_REGULAR_FACE);
	sPlainFont.SetSize(DEFAULT_FONT_SIZE);
	sPlainFont.fExtraFlags =
			(uint32)B_FONT_LEFT_TO_RIGHT << B_PRIVATE_FONT_DIRECTION_SHIFT;

	sBoldFont.SetFamilyAndStyle(DEFAULT_BOLD_FONT_FAMILY, DEFAULT_BOLD_FONT_STYLE);
	sBoldFont.SetFlags(B_BOLD_FACE);
	sBoldFont.SetSize(DEFAULT_FONT_SIZE);
	sBoldFont.fExtraFlags = kUninitializedExtraFlags;
	sBoldFont.fExtraFlags =
			(uint32)B_FONT_LEFT_TO_RIGHT << B_PRIVATE_FONT_DIRECTION_SHIFT;

	sFixedFont.SetFamilyAndStyle(DEFAULT_FIXED_FONT_FAMILY, DEFAULT_FIXED_FONT_STYLE);
	sFixedFont.SetFlags(B_REGULAR_FACE);
	sFixedFont.SetSize(DEFAULT_FONT_SIZE);
	sFixedFont.fExtraFlags = B_IS_FIXED |
			(uint32)B_FONT_LEFT_TO_RIGHT << B_PRIVATE_FONT_DIRECTION_SHIFT;
}



status_t
get_font_cache_info(uint32 id, void* set)
{
	return B_ERROR;
}


status_t
set_font_cache_info(uint32 id, void* set)
{
	return B_ERROR;
}


// Returns the number of installed font families
int32
count_font_families()
{
	return FontList::Default()->CountFamilies();
}


// Returns the number of styles available for a font family
int32
count_font_styles(font_family family)
{
	return FontList::Default()->CountStyles(family);
}


// Retrieves the family name at the specified index
status_t
get_font_family(int32 index, font_family* _name, uint32* _flags)
{
	if (_name == NULL)
		return B_BAD_VALUE;

	return FontList::Default()->FamilyAt(index, _name, _flags);
}


// Retrieves the family name at the specified index
status_t
get_font_style(font_family family, int32 index, font_style* _name,
	uint32* _flags)
{
	return get_font_style(family, index, _name, NULL, _flags);
}


// Retrieves the family name at the specified index
status_t
get_font_style(font_family family, int32 index, font_style* _name,
	uint16* _face, uint32* _flags)
{
	// The face value returned by this function is not very reliable. At the
	// same time, the value returned should be fairly reliable, returning the
	// proper flag for 90%-99% of font names.

	if (_name == NULL)
		return B_BAD_VALUE;

	return FontList::Default()->StyleAt(family, index, _name, _face, _flags);
}


// Updates the font family list
bool
update_font_families(bool /*checkOnly*/)
{
	return FontList::Default()->UpdatedOnServer();
}


//	#pragma mark -


BFont::BFont()
	:
	// initialise for be_plain_font (avoid circular definition)
	fSize(10.0),
	fShear(90.0),
	fRotation(0.0),
	fFalseBoldWidth(0.0),
	fSpacing(B_BITMAP_SPACING),
	fEncoding(B_UNICODE_UTF8),
	fFace(0),
	fFlags(0),
	fExtraFlags(kUninitializedExtraFlags)
{
	if (be_plain_font != NULL && this != &sPlainFont)
		*this = *be_plain_font;
	else {
		fHeight.ascent = 7.0;
		fHeight.descent = 2.0;
		fHeight.leading = 13.0;
	}

	fFamilyName[0] = '\0';
	fStyleName[0] = '\0';
}


BFont::BFont(const BFont& font)
{
	*this = font;
}


BFont::BFont(const BFont* font)
{
	if (font != NULL)
		*this = *font;
	else
		*this = *be_plain_font;
}


BFont::~BFont()
{
}


// Sets the font's family and style all at once
status_t
BFont::SetFamilyAndStyle(const font_family family, const font_style style)
{
	if (family == NULL && style == NULL)
		return B_BAD_VALUE;

	if (family != NULL)
		strlcpy(fFamilyName, family, sizeof(font_family));

	if (style != NULL)
		strlcpy(fStyleName, style, sizeof(font_style));

	fHeight.ascent = kUninitializedAscent;
	fExtraFlags = kUninitializedExtraFlags;

	return B_OK;
}


// Sets the font's family and face all at once
status_t
BFont::SetFamilyAndFace(const font_family family, uint16 face)
{
	// To comply with the BeBook, this function will only set valid values
	// i.e. passing a nonexistent family will cause only the face to be set.
	// Additionally, if a particular face does not exist in a family, the
	// closest match will be chosen.

	if (family != NULL) {
		strlcpy(fFamilyName, family, sizeof(font_family));
	}

	// FIXME: do what it says above
	fFace = face;

	fHeight.ascent = kUninitializedAscent;
	fExtraFlags = kUninitializedExtraFlags;

	return B_OK;
}


void
BFont::SetSize(float size)
{
	fSize = size;
	fHeight.ascent = kUninitializedAscent;
}


void
BFont::SetShear(float shear)
{
	fShear = shear;
	fHeight.ascent = kUninitializedAscent;
}


void
BFont::SetRotation(float rotation)
{
	fRotation = rotation;
	fHeight.ascent = kUninitializedAscent;
}


void
BFont::SetFalseBoldWidth(float width)
{
	fFalseBoldWidth = width;
}


void
BFont::SetSpacing(uint8 spacing)
{
	fSpacing = spacing;
}


void
BFont::SetEncoding(uint8 encoding)
{
	fEncoding = encoding;
}


void
BFont::SetFace(uint16 face)
{
	if (face == fFace)
		return;

	SetFamilyAndFace(NULL, face);
}


void
BFont::SetFlags(uint32 flags)
{
	fFlags = flags;
}


void
BFont::GetFamilyAndStyle(font_family* family, font_style* style) const
{
	if (family == NULL && style == NULL)
		return;

	// it's okay to call this function with either family or style set to NULL

	if (family != NULL)
		strlcpy(*family, fFamilyName, sizeof(font_family));

	if (style != NULL)
		strlcpy(*style, fStyleName, sizeof(font_style));
}


float
BFont::Size() const
{
	return fSize;
}


float
BFont::Shear() const
{
	return fShear;
}


float
BFont::Rotation() const
{
	return fRotation;
}


float
BFont::FalseBoldWidth() const
{
	return fFalseBoldWidth;
}


uint8
BFont::Spacing() const
{
	return fSpacing;
}


uint8
BFont::Encoding() const
{
	return fEncoding;
}


uint16
BFont::Face() const
{
	return fFace;
}


uint32
BFont::Flags() const
{
	return fFlags;
}


font_direction
BFont::Direction() const
{
	_GetExtraFlags();
	return (font_direction)(fExtraFlags >> B_PRIVATE_FONT_DIRECTION_SHIFT);
}


bool
BFont::IsFixed() const
{
	_GetExtraFlags();
	return (fExtraFlags & B_IS_FIXED) != 0;
}


// Returns whether or not the font is fixed-width and contains both
// full and half-width characters.
bool
BFont::IsFullAndHalfFixed() const
{
	// This was left unimplemented as of R5. It is a way to work with both
	// Kanji and Roman characters in the same fixed-width font.

	_GetExtraFlags();
	return (fExtraFlags & B_PRIVATE_FONT_IS_FULL_AND_HALF_FIXED) != 0;
}


unicode_block
BFont::Blocks() const
{
	// Create a unicode_block that represents all blocks supported by this font
	// This requires querying the font's character coverage
	
	PangoFontDescription* desc = (PangoFontDescription*)GetPangoFontDescription();
	if (desc == NULL)
		return unicode_block();  // Return empty block
	
	PangoFontMap* fontmap = pango_cairo_font_map_get_default();
	PangoContext* context = pango_font_map_create_context(fontmap);
	PangoFont* font = pango_font_map_load_font(fontmap, context, desc);
	
	if (font == NULL) {
		g_object_unref(context);
		pango_font_description_free(desc);
		return unicode_block();
	}
	
	PangoCoverage* coverage = pango_font_get_coverage(font, pango_language_get_default());
	
	// Build a unicode_block by checking coverage of each defined Unicode block
	unicode_block result;
	
	// Check coverage for each Unicode block range defined in kUnicodeBlockMap
	// We'll use a simplified approach: check a few sample characters from each block
	for (size_t i = 0; i < kNumUnicodeBlockRanges; i++) {
		const unicode_block_range& range = kUnicodeBlockMap[i];
		
		// Sample 5 characters from this block to check coverage
		int samplesChecked = 0;
		int samplesFound = 0;
		
		for (uint32 ch = range.start; ch <= range.end && samplesChecked < 5; ch += (range.end - range.start) / 5 + 1) {
			PangoCoverageLevel level = pango_coverage_get(coverage, ch);
			if (level != PANGO_COVERAGE_NONE) {
				samplesFound++;
			}
			samplesChecked++;
		}
		
		// If at least 40% of samples have glyphs, consider the block supported
		if (samplesChecked > 0 && samplesFound >= samplesChecked * 2 / 5) {
			// OR this block into our result
			result = result | range.block;
		}
	}
	
	g_object_unref(coverage);
	g_object_unref(font);
	g_object_unref(context);
	pango_font_description_free(desc);
	
	return result;
}

bool
BFont::IncludesBlock(uint32 start, uint32 end) const
{
	// Check if the font has glyphs for characters in the range [start, end]
	
	PangoFontDescription* desc = (PangoFontDescription*)GetPangoFontDescription();
	if (desc == NULL)
		return false;
	
	PangoFontMap* fontmap = pango_cairo_font_map_get_default();
	PangoContext* context = pango_font_map_create_context(fontmap);
	PangoFont* font = pango_font_map_load_font(fontmap, context, desc);
	
	if (font == NULL) {
		g_object_unref(context);
		pango_font_description_free(desc);
		return false;
	}
	
	PangoCoverage* coverage = pango_font_get_coverage(font, pango_language_get_default());
	
	// Sample characters throughout the range to determine coverage
	// Check at least 10 samples or all characters if range is small
	uint32 rangeSize = end - start + 1;
	uint32 samplesToCheck = rangeSize < 10 ? rangeSize : 10;
	uint32 step = rangeSize / samplesToCheck;
	if (step == 0)
		step = 1;
	
	int totalSamples = 0;
	int coveredSamples = 0;
	
	for (uint32 ch = start; ch <= end && totalSamples < (int)samplesToCheck; ch += step) {
		PangoCoverageLevel level = pango_coverage_get(coverage, ch);
		totalSamples++;
		
		// Count characters with at least approximate coverage
		if (level != PANGO_COVERAGE_NONE)
			coveredSamples++;
	}
	
	g_object_unref(coverage);
	g_object_unref(font);
	g_object_unref(context);
	pango_font_description_free(desc);
	
	// Consider the block included if at least 50% of sampled characters are covered
	return totalSamples > 0 && coveredSamples >= totalSamples / 2;
}


int32
BFont::CountTuned() const
{
	// Cosmoe does not support tuned fonts
	return 0;
}


void
BFont::GetTunedInfo(int32 index, tuned_font_info* info) const
{
	// Cosmoe does not support tuned fonts
}


// Truncates a string to a given _pixel_ width based on the font and size
void
BFont::TruncateString(BString* inOut, uint32 mode, float width) const
{
	if (mode == B_NO_TRUNCATION)
		return;

	// NOTE: Careful, we cannot directly use "inOut->String()" as result
	// array, because the string length increases by 3 bytes in the worst
	// case scenario.
	const char* string = inOut->String();
	GetTruncatedStrings(&string, 1, mode, width, inOut);
}


void
BFont::GetTruncatedStrings(const char* stringArray[], int32 numStrings,
	uint32 mode, float width, BString resultArray[]) const
{
	if (stringArray != NULL && numStrings > 0) {
		// the width of the "…" glyph
		float ellipsisWidth = StringWidth(B_UTF8_ELLIPSIS);

		for (int32 i = 0; i < numStrings; i++) {
			resultArray[i] = stringArray[i];
			int32 numChars = resultArray[i].CountChars();

			// get the escapement of each glyph in font units
			float* escapementArray = new float[numChars];
			GetEscapements(stringArray[i], numChars, NULL, escapementArray);

			truncate_string(resultArray[i], mode, width, escapementArray,
				fSize, ellipsisWidth, numChars);

			delete[] escapementArray;
		}
	}
}


void
BFont::GetTruncatedStrings(const char* stringArray[], int32 numStrings,
	uint32 mode, float width, char* resultArray[]) const
{
	if (stringArray != NULL && numStrings > 0) {
		for (int32 i = 0; i < numStrings; i++) {
			BString* strings = new BString[numStrings];
			GetTruncatedStrings(stringArray, numStrings, mode, width, strings);

			for (int32 i = 0; i < numStrings; i++)
				strcpy(resultArray[i], strings[i].String());

			delete[] strings;
		}
	}
}


float
BFont::StringWidth(const char* string, float displayScale) const
{
	if (string == NULL)
		return 0.0;

	int32 length = strlen(string);
	float width;
	GetStringWidths(&string, &length, 1, &width, displayScale);

	return width;
}


float
BFont::StringWidth(const char* string, int32 length, float displayScale) const
{
	if (!string || length < 1)
		return 0.0f;

	float width = 0.0f;
	GetStringWidths(&string, &length, 1, &width, displayScale);

	return width;
}


void
BFont::GetStringWidths(const char* stringArray[], const int32 lengthArray[],
	int32 numStrings, float widthArray[], float displayScale) const
{
	if (stringArray == NULL || lengthArray == NULL || numStrings < 1
		|| widthArray == NULL) {
		return;
	}

	PangoFontDescription *desc = (PangoFontDescription*)GetPangoFontDescription();
    if (desc == NULL) {
		printf("BFont::GetStringWidths(): Failed to get PangoFontDescription\n");
        for (int32 i = 0; i < numStrings; i++)
            widthArray[i] = 0.0f;
        return;
    }

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		0, 0);
	cairo_t* cr = cairo_create(surface);
	if (displayScale > 1.0f)
		cairo_scale(cr, displayScale, displayScale);

	PangoLayout *layout = pango_cairo_create_layout(cr);
    
	// Set resolution BEFORE setting font description so font is loaded at correct DPI
	PangoContext *pctx = pango_layout_get_context(layout);
	pango_cairo_context_set_resolution(pctx, 72.0);
	
    pango_layout_set_font_description(layout, desc);

    for (int32 i = 0; i < numStrings; i++) {
        if (stringArray[i] == NULL || lengthArray[i] < 1) {
            widthArray[i] = 0.0f;
            continue;
        }

        pango_layout_set_text(layout, stringArray[i], lengthArray[i]);
		widthArray[i] = MeasuredLayoutWidth(layout, cr, displayScale);
    }

    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    pango_font_description_free(desc);
}


void*
BFont::GetPangoFontDescription() const
{
	const char* familyName = strlen(fFamilyName) > 0 ? fFamilyName : DEFAULT_PLAIN_FONT_FAMILY;
	const char* styleName = strlen(fStyleName) > 0 ? fStyleName : DEFAULT_PLAIN_FONT_STYLE;
	char fontDescriptor[256];

	sprintf(fontDescriptor, "%s %s", familyName, styleName);

	PangoFontDescription *desc = pango_font_description_from_string(fontDescriptor);
	if (desc == NULL) {
		printf("GetPangoFontDescription: Failed to create font description for '%s'\n", fontDescriptor);
		return NULL;
	}
	
	pango_font_description_set_size(desc, fSize * PANGO_SCALE);
	if (fFace & B_BOLD_FACE)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_style(desc, fFace & B_ITALIC_FACE ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);

	// Caller is responsible for freeing the returned PangoFontDescription
	return desc;
}


void
BFont::GetEscapements(const char charArray[], int32 numChars,
	float escapementArray[]) const
{
	GetEscapements(charArray, numChars, NULL, escapementArray);
}


void
BFont::GetEscapements(const char charArray[], int32 numChars,
	escapement_delta* delta, float escapementArray[]) const
{
	if (charArray == NULL || numChars < 1 || escapementArray == NULL)
		return;

	cairo_t *cr;
	cairo_surface_t *surface;
	PangoFontDescription *desc = (PangoFontDescription*)GetPangoFontDescription();

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create(surface);

	float nonspaceDelta = delta ? delta->nonspace : 0.0f;
	float spaceDelta = delta ? delta->space : 0.0f;

	const char* ptr = charArray;
	for (int32 i = 0; i < numChars && *ptr != '\0'; i++) {
		// Get the next UTF-8 character
		int32 charLen = UTF8NextCharLen(ptr);
		
		PangoLayout *layout = pango_cairo_create_layout(cr);
		
		// Set resolution BEFORE setting font description
		PangoContext *pctx = pango_layout_get_context(layout);
		pango_cairo_context_set_resolution(pctx, 72.0);
		
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, ptr, charLen);

		// Escapement is the character width normalized by font size
		escapementArray[i] = LogicalLayoutWidth(layout) / fSize;

		// Apply delta: space delta for space characters, nonspace for others
		bool isSpace = (*ptr == ' ' || *ptr == '\t');
		escapementArray[i] += (isSpace ? spaceDelta : nonspaceDelta) / fSize;

		g_object_unref(layout);
		ptr += charLen;
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	pango_font_description_free(desc);
}


void
BFont::GetEscapements(const char charArray[], int32 numChars,
	escapement_delta* delta, BPoint escapementArray[]) const
{
	GetEscapements(charArray, numChars, delta, escapementArray, NULL);
}


void
BFont::GetEscapements(const char charArray[], int32 numChars,
	escapement_delta* delta, BPoint escapementArray[],
	BPoint offsetArray[]) const
{
	if (charArray == NULL || numChars < 1 || escapementArray == NULL)
		return;

	cairo_t *cr;
	cairo_surface_t *surface;
	PangoFontDescription *desc = (PangoFontDescription*)GetPangoFontDescription();

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create(surface);

	float nonspaceDelta = delta ? delta->nonspace : 0.0f;
	float spaceDelta = delta ? delta->space : 0.0f;

	const char* ptr = charArray;
	for (int32 i = 0; i < numChars && *ptr != '\0'; i++) {
		// Get the next UTF-8 character
		int32 charLen = UTF8NextCharLen(ptr);
		
		PangoLayout *layout = pango_cairo_create_layout(cr);
		
		// Set resolution BEFORE setting font description
		PangoContext *pctx = pango_layout_get_context(layout);
		pango_cairo_context_set_resolution(pctx, 72.0);
		
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, ptr, charLen);

		PangoRectangle ink_rect, logical_rect;
		pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);

		// Escapement is horizontal advance (x direction)
		escapementArray[i].x = (float)logical_rect.width;
		escapementArray[i].y = 0.0f;  // No vertical advance for horizontal text

		// Apply delta: space delta for space characters, nonspace for others
		bool isSpace = (*ptr == ' ' || *ptr == '\t');
		escapementArray[i].x += (isSpace ? spaceDelta : nonspaceDelta);

		// Offset is the position offset from baseline (for rendered position)
		if (offsetArray) {
			offsetArray[i].x = (float)ink_rect.x;
			offsetArray[i].y = (float)ink_rect.y;
		}

		g_object_unref(layout);
		ptr += charLen;
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	pango_font_description_free(desc);
}


void
BFont::GetEdges(const char charArray[], int32 numChars,
	edge_info edgeArray[]) const
{
	if (!charArray || numChars < 1 || !edgeArray)
		return;

	cairo_t *cr;
	cairo_surface_t *surface;
	PangoFontDescription *desc = (PangoFontDescription*)GetPangoFontDescription();

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create(surface);

	const char* ptr = charArray;
	for (int32 i = 0; i < numChars && *ptr != '\0'; i++) {
		// Get the next UTF-8 character
		int32 charLen = UTF8NextCharLen(ptr);
		
		PangoLayout *layout = pango_cairo_create_layout(cr);
		
		// Set resolution BEFORE setting font description
		PangoContext *pctx = pango_layout_get_context(layout);
		pango_cairo_context_set_resolution(pctx, 72.0);
		
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, ptr, charLen);

		PangoRectangle ink_rect, logical_rect;
		pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);

		// Edge info uses the ink rectangle to get actual drawn boundaries
		edgeArray[i].left = (float)ink_rect.x;
		edgeArray[i].right = (float)(ink_rect.x + ink_rect.width);

		g_object_unref(layout);
		ptr += charLen;
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	pango_font_description_free(desc);
}


void
BFont::GetHeight(font_height* _height) const
{
	if (_height == NULL)
		return;

	if (fHeight.ascent == kUninitializedAscent) {
		PangoFontMap* fontmap = pango_cairo_font_map_get_default();
		PangoFontDescription* fontdesc = (PangoFontDescription*)GetPangoFontDescription();
		PangoContext* context = pango_font_map_create_context(fontmap);
		
		// Set resolution BEFORE loading font so it loads at correct DPI
		pango_cairo_context_set_resolution(context, 72.0);
		
		PangoFont* font = pango_font_map_load_font(fontmap, context, fontdesc);
		PangoFontMetrics* m = pango_font_get_metrics(font, NULL);
				
		fHeight.ascent = pango_font_metrics_get_ascent(m) / PANGO_SCALE;
		fHeight.descent = pango_font_metrics_get_descent(m) / PANGO_SCALE;
		fHeight.leading = (pango_font_metrics_get_height(m) / PANGO_SCALE) - fHeight.ascent - fHeight.descent;
	
		//printf("leading: %f, ascent: %f, descent: %f\n", fHeight.leading, fHeight.ascent, fHeight.descent);
	
		pango_font_metrics_unref(m);
		g_object_unref(font);
		g_object_unref(context);
		pango_font_description_free(fontdesc);
		// Don't unref the default font map - it's a singleton managed by Pango
	}

	*_height = fHeight;
}


void
BFont::GetBoundingBoxesAsGlyphs(const char charArray[], int32 numChars,
	font_metric_mode mode, BRect boundingBoxArray[]) const
{
	_GetBoundingBoxes(charArray, numChars, mode, false, NULL,
		boundingBoxArray, false);
}


void
BFont::GetBoundingBoxesAsString(const char charArray[], int32 numChars,
	font_metric_mode mode, escapement_delta* delta,
	BRect boundingBoxArray[]) const
{
	_GetBoundingBoxes(charArray, numChars, mode, true, delta,
		boundingBoxArray, true);
}


void
BFont::_GetBoundingBoxes(const char charArray[], int32 numChars,
	font_metric_mode mode, bool string_escapement, escapement_delta* delta,
	BRect boundingBoxArray[], bool asString) const
{
	if (charArray == NULL || numChars < 1 || boundingBoxArray == NULL)
		return;

	cairo_t *cr;
	cairo_surface_t *surface;
	PangoFontDescription *desc = (PangoFontDescription*)GetPangoFontDescription();

	// Create a small surface for measurement - 0x0 surface prevents path creation
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cr = cairo_create(surface);

	if (asString) {
		// Get bounding box for the entire string
		PangoLayout *layout = pango_cairo_create_layout(cr);
		
		// Set resolution BEFORE setting font description
		PangoContext *pctx = pango_layout_get_context(layout);
		pango_cairo_context_set_resolution(pctx, 72.0);
		
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, charArray, numChars);

		// Get baseline position for coordinate adjustment
		PangoLayoutIter *iter = pango_layout_get_iter(layout);
		int baseline = pango_layout_iter_get_baseline(iter) / PANGO_SCALE;
		pango_layout_iter_free(iter);

		// Create the layout path and get untransformed extents
		cairo_move_to(cr, 0, 0);
		pango_cairo_layout_path(cr, layout);
		
		// Get the path extents in layout space (before transformations)
		double x1, y1, x2, y2;
		cairo_path_extents(cr, &x1, &y1, &x2, &y2);
		
		// Adjust for baseline - convert to baseline-relative coordinates
		y1 -= baseline;
		y2 -= baseline;
		
		// Apply transformations if needed
		if (fRotation != 0.0 || fShear != 90.0) {
			double radians = -fRotation * M_PI / 180.0;
			double cosR = cos(radians);
			double sinR = sin(radians);
			double skew = tan((90.0 - fShear) * M_PI / 180.0);
			
			// Transform all four corners: rotation first, then shear
			double corners[4][2] = {
				{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}
			};
			
			for (int i = 0; i < 4; i++) {
				double x = corners[i][0];
				double y = corners[i][1];
				// Rotate
				double x_rot = x * cosR - y * sinR;
				double y_rot = x * sinR + y * cosR;
				// Shear - note: y is baseline-relative (negative = above baseline)
				// so we negate skew to match the direction of the actual glyph transformation
				corners[i][0] = x_rot - skew * y_rot;
				corners[i][1] = y_rot;
			}
			
			// Find axis-aligned bounding box
			x1 = x2 = corners[0][0];
			y1 = y2 = corners[0][1];
			for (int i = 1; i < 4; i++) {
				if (corners[i][0] < x1) x1 = corners[i][0];
				if (corners[i][0] > x2) x2 = corners[i][0];
				if (corners[i][1] < y1) y1 = corners[i][1];
				if (corners[i][1] > y2) y2 = corners[i][1];
			}
		}
		
		boundingBoxArray[0].Set((float)x1, (float)y1, (float)x2, (float)y2);
		g_object_unref(layout);
	} else {
		// Get bounding box for each character
		const char* ptr = charArray;
		float xOffset = 0.0f;  // Track cumulative x position for string layout
		
		// Precompute transformation values if needed
		double radians = -fRotation * M_PI / 180.0;
		double cosR = cos(radians);
		double sinR = sin(radians);
		double skew = tan((90.0 - fShear) * M_PI / 180.0);
		bool needsTransform = (fRotation != 0.0 || fShear != 90.0);
		
		for (int32 i = 0; i < numChars && *ptr != '\0'; i++) {
			// Get the next UTF-8 character
			int32 charLen = UTF8NextCharLen(ptr);
			
			PangoLayout *layout = pango_cairo_create_layout(cr);
			
			// Set resolution BEFORE setting font description
			PangoContext *pctx = pango_layout_get_context(layout);
			pango_cairo_context_set_resolution(pctx, 72.0);
			
			pango_layout_set_font_description(layout, desc);
			pango_layout_set_text(layout, ptr, charLen);

			// Get baseline position for coordinate adjustment
			PangoLayoutIter *iter = pango_layout_get_iter(layout);
			int baseline = pango_layout_iter_get_baseline(iter) / PANGO_SCALE;
			pango_layout_iter_free(iter);

			// Create the layout path at origin and get untransformed extents
			cairo_move_to(cr, 0, 0);
			pango_cairo_layout_path(cr, layout);
			
			// Get the path extents in layout space (before transformations)
			double x1, y1, x2, y2;
			cairo_path_extents(cr, &x1, &y1, &x2, &y2);
			
			// Adjust for baseline - convert to baseline-relative coordinates
			y1 -= baseline;
			y2 -= baseline;
			
			// Apply transformations if needed
			if (needsTransform) {
				// Transform all four corners: rotation first, then shear
				double corners[4][2] = {
					{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}
				};
				
				for (int j = 0; j < 4; j++) {
					double x = corners[j][0];
					double y = corners[j][1];
					// Rotate
					double x_rot = x * cosR - y * sinR;
					double y_rot = x * sinR + y * cosR;
					// Shear - note: y is baseline-relative (negative = above baseline)
					// so we negate skew to match the direction of the actual glyph transformation
					corners[j][0] = x_rot - skew * y_rot;
					corners[j][1] = y_rot;
				}
				
				// Find axis-aligned bounding box
				x1 = x2 = corners[0][0];
				y1 = y2 = corners[0][1];
				for (int j = 1; j < 4; j++) {
					if (corners[j][0] < x1) x1 = corners[j][0];
					if (corners[j][0] > x2) x2 = corners[j][0];
					if (corners[j][1] < y1) y1 = corners[j][1];
					if (corners[j][1] > y2) y2 = corners[j][1];
				}
			}
			
			// Now add the position offset for string_escapement mode
			float xPos = string_escapement ? xOffset : 0.0f;
			boundingBoxArray[i].Set((float)(x1 + xPos), (float)y1, (float)(x2 + xPos), (float)y2);
			
			// Clear the path for next iteration
			cairo_new_path(cr);
			
			// Update position for next character
			if (string_escapement) {
				PangoRectangle logical_rect;
				pango_layout_get_pixel_extents(layout, NULL, &logical_rect);
				xOffset += logical_rect.width;
			}
			
			g_object_unref(layout);
			ptr += charLen;
		}
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	pango_font_description_free(desc);
}


void
BFont::GetBoundingBoxesForStrings(const char* stringArray[], int32 numStrings,
	font_metric_mode mode, escapement_delta deltas[],
	BRect boundingBoxArray[]) const
{
	if (!stringArray || numStrings < 1 || !boundingBoxArray)
		return;

	cairo_t *cr;
	cairo_surface_t *surface;
	PangoFontDescription *desc = (PangoFontDescription*)GetPangoFontDescription();

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create(surface);

	// Precompute transformation values if needed
	double radians = -fRotation * M_PI / 180.0;
	double cosR = cos(radians);
	double sinR = sin(radians);
	double skew = tan((90.0 - fShear) * M_PI / 180.0);
	bool needsTransform = (fRotation != 0.0 || fShear != 90.0);

	for (int32 i = 0; i < numStrings; i++) {
		if (stringArray[i] == NULL) {
			boundingBoxArray[i].Set(0, 0, 0, 0);
			continue;
		}

		PangoLayout *layout = pango_cairo_create_layout(cr);
		
		// Set resolution BEFORE setting font description
		PangoContext *pctx = pango_layout_get_context(layout);
		pango_cairo_context_set_resolution(pctx, 72.0);
		
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout, stringArray[i], -1);

		// Get baseline position for coordinate adjustment
		PangoLayoutIter *iter = pango_layout_get_iter(layout);
		int baseline = pango_layout_iter_get_baseline(iter) / PANGO_SCALE;
		pango_layout_iter_free(iter);

		// Create the layout path and get untransformed extents
		cairo_move_to(cr, 0, 0);
		pango_cairo_layout_path(cr, layout);
		
		// Get the path extents in layout space (before transformations)
		double x1, y1, x2, y2;
		cairo_path_extents(cr, &x1, &y1, &x2, &y2);
		
		// Adjust for baseline - convert to baseline-relative coordinates
		y1 -= baseline;
		y2 -= baseline;
		
		// Apply transformations if needed
		if (needsTransform) {
			// Transform all four corners: rotation first, then shear
			double corners[4][2] = {
				{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}
			};
			
			for (int j = 0; j < 4; j++) {
				double x = corners[j][0];
				double y = corners[j][1];
				// Rotate
				double x_rot = x * cosR - y * sinR;
				double y_rot = x * sinR + y * cosR;
				// Shear - note: y is baseline-relative (negative = above baseline)
				// so we negate skew to match the direction of the actual glyph transformation
				corners[j][0] = x_rot - skew * y_rot;
				corners[j][1] = y_rot;
			}
			
			// Find axis-aligned bounding box
			x1 = x2 = corners[0][0];
			y1 = y2 = corners[0][1];
			for (int j = 1; j < 4; j++) {
				if (corners[j][0] < x1) x1 = corners[j][0];
				if (corners[j][0] > x2) x2 = corners[j][0];
				if (corners[j][1] < y1) y1 = corners[j][1];
				if (corners[j][1] > y2) y2 = corners[j][1];
			}
		}
		
		boundingBoxArray[i].Set((float)x1, (float)y1, (float)x2, (float)y2);
		
		// Clear the path for next iteration
		cairo_new_path(cr);
		g_object_unref(layout);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	pango_font_description_free(desc);
}


void
BFont::GetHasGlyphs(const char charArray[], int32 numChars,
	bool hasArray[]) const
{
	GetHasGlyphs(charArray, numChars, hasArray, true);
}


void
BFont::GetHasGlyphs(const char charArray[], int32 numChars, bool hasArray[],
	bool useFallbacks) const
{
	if (!charArray || numChars < 1 || !hasArray)
		return;

	// Initialize all to false in case of early return
	for (int32 i = 0; i < numChars; i++)
		hasArray[i] = false;

	PangoFontDescription* desc = (PangoFontDescription*)GetPangoFontDescription();
	if (desc == NULL)
		return;

	PangoFontMap* fontmap = pango_cairo_font_map_get_default();
	PangoContext* context = pango_font_map_create_context(fontmap);
	PangoFont* font = pango_font_map_load_font(fontmap, context, desc);
	
	if (font == NULL) {
		g_object_unref(context);
		pango_font_description_free(desc);
		return;
	}
	
	PangoCoverage* coverage = pango_font_get_coverage(font, pango_language_get_default());
	
	// Iterate through each UTF-8 character
	const char* ptr = charArray;
	for (int32 i = 0; i < numChars && *ptr != '\0'; i++) {
		// Get the next UTF-8 character
		int32 charLen = UTF8NextCharLen(ptr);
		
		// Convert UTF-8 character to Unicode codepoint
		uint32 codepoint = 0;
		if (charLen == 1) {
			codepoint = (unsigned char)*ptr;
		} else if (charLen == 2) {
			codepoint = ((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F);
		} else if (charLen == 3) {
			codepoint = ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
		} else if (charLen == 4) {
			codepoint = ((ptr[0] & 0x07) << 18) | ((ptr[1] & 0x3F) << 12) 
			          | ((ptr[2] & 0x3F) << 6) | (ptr[3] & 0x3F);
		}
		
		// Check coverage for this codepoint
		PangoCoverageLevel level = pango_coverage_get(coverage, codepoint);
		
		if (useFallbacks) {
			// If fallbacks are allowed, consider approximate coverage as well
			hasArray[i] = (level != PANGO_COVERAGE_NONE);
		} else {
			// If no fallbacks, only exact coverage counts
			hasArray[i] = (level == PANGO_COVERAGE_EXACT);
		}
		
		ptr += charLen;
	}
	
	g_object_unref(coverage);
	g_object_unref(font);
	g_object_unref(context);
	pango_font_description_free(desc);
}


BFont&
BFont::operator=(const BFont& font)
{
	fSize = font.fSize;
	fShear = font.fShear;
	fRotation = font.fRotation;
	fFalseBoldWidth = font.fFalseBoldWidth;
	fSpacing = font.fSpacing;
	fEncoding = font.fEncoding;
	fFace = font.fFace;
	fHeight = font.fHeight;
	fFlags = font.fFlags;
	fExtraFlags = font.fExtraFlags;

	strlcpy(fFamilyName, font.fFamilyName, sizeof(font_family));
	strlcpy(fStyleName, font.fStyleName, sizeof(font_style));

	return *this;
}


bool
BFont::operator==(const BFont& font) const
{
	if (strcmp(fFamilyName, font.fFamilyName) != 0)
		return false;

	if (strcmp(fStyleName, font.fFamilyName) != 0)
		return false;

	return fSize == font.fSize
		&& fShear == font.fShear
		&& fRotation == font.fRotation
		&& fFalseBoldWidth == font.fFalseBoldWidth
		&& fSpacing == font.fSpacing
		&& fEncoding == font.fEncoding
		&& fFace == font.fFace;
}


bool
BFont::operator!=(const BFont& font) const
{
	bool familyDiffers = (strcmp(fFamilyName, font.fFamilyName) != 0);
	bool styleDiffers = (strcmp(fStyleName, font.fStyleName) != 0);
	
	return familyDiffers || styleDiffers
		|| fSize != font.fSize
		|| fShear != font.fShear
		|| fRotation != font.fRotation
		|| fFalseBoldWidth != font.fFalseBoldWidth
		|| fSpacing != font.fSpacing
		|| fEncoding != font.fEncoding
		|| fFace != font.fFace;
}


void
BFont::PrintToStream() const
{
	font_family family;
	font_style style;
	GetFamilyAndStyle(&family, &style);

	printf("BFont { %s, %s 0x%x %f/%f %fpt (%f %f %f), %d }\n",
		family, style, fFace, fShear, fRotation, fSize,
		fHeight.ascent, fHeight.descent, fHeight.leading, fEncoding);
}


void
BFont::_GetExtraFlags() const
{
	// TODO: this has to be const in order to allow other font getters to
	// stay const as well
	if (fExtraFlags != kUninitializedExtraFlags)
		return;

	// FIXME: set extra flags
}


