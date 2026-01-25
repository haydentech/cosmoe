
#include <GraphicsDefs.h>
#include <ViewState.h>
#include <CosmoeBackendAPI.h>


class BRegion;

#include <cairo.h>

static double rgb_to_cairo_color(uint8_t rgb) {
    return (double)rgb / 255.0;
}

static cairo_operator_t drawing_mode_to_cairo_operator(drawing_mode mode)
{
	switch(mode)
	{
		case B_OP_SELECT:
			// Unhandled, fall through
		case B_OP_COPY:
			return CAIRO_OPERATOR_SOURCE;
		case B_OP_OVER:
			return CAIRO_OPERATOR_OVER;
		case B_OP_ERASE:
			return CAIRO_OPERATOR_CLEAR;
		case B_OP_ADD:
			return CAIRO_OPERATOR_ADD;
		case B_OP_SUBTRACT:
			// Note: Cairo doesn't have a true subtract operator
			// DIFFERENCE gives |src - dest|, but BeOS wants dest - src
			// This is an approximation
			return CAIRO_OPERATOR_DIFFERENCE;
		case B_OP_BLEND:
			// BeOS B_OP_BLEND uses alpha blending
			return CAIRO_OPERATOR_OVER;
		case B_OP_MIN:
			// Minimum (darker) values
			return CAIRO_OPERATOR_DARKEN;
		case B_OP_MAX:
			// Maximum (lighter) values
			return CAIRO_OPERATOR_LIGHTEN;
		case B_OP_ALPHA:
			// Alpha channel blending
			return CAIRO_OPERATOR_OVER;
		case B_OP_INVERT:
			// This will work only with the addition of a white source
			return CAIRO_OPERATOR_DIFFERENCE;
	}

	return CAIRO_OPERATOR_SOURCE;
}

static cairo_format_t color_space_to_cairo_format(color_space space)
{
	/*
	CAIRO_FORMAT_INVALID   = -1,
    CAIRO_FORMAT_ARGB32    = 0,
    CAIRO_FORMAT_RGB24     = 1,
    CAIRO_FORMAT_A8        = 2,
    CAIRO_FORMAT_A1        = 3,
    CAIRO_FORMAT_RGB16_565 = 4,
    CAIRO_FORMAT_RGB30     = 5,
    CAIRO_FORMAT_RGB96F    = 6,
    CAIRO_FORMAT_RGBA128F  = 7
	*/

	switch(space)
	{
		case B_RGB24:
		case B_RGB32:
			return CAIRO_FORMAT_RGB24;
		case B_RGBA32:
			return CAIRO_FORMAT_ARGB32;
		case B_GRAY8:
			return CAIRO_FORMAT_A8;
		case B_GRAY1:
			return CAIRO_FORMAT_A1;
		case B_RGB16:
		case B_RGB15:
		case B_RGBA15:
			return CAIRO_FORMAT_RGB16_565;

		default:
			printf("BUG: you cannot draw in color_space %d in Cosmoe.  Change your bitmap to a supported color_space.\n", space);
			break;
	}

	return CAIRO_FORMAT_INVALID;
}


class CairoContext {
	public:

	CairoContext(cairo_surface_t* surface, ::BPrivate::ViewState* state, BRegion* viewClipping, BRect* bounds, BRect* viewFrame, bool usePattern, float displayScale = 1.0)
		: cairoGradient(NULL), cairoSourcePattern(NULL)
    {
		if (!surface) {
			printf("ERROR: NULL surface passed to CairoContext\n");
			cr = NULL;
			return;
		}

		rectangle allocation;
		allocation.x = 0;
		allocation.y = 0;
		allocation.width = cairo_image_surface_get_width(surface);
		allocation.height = cairo_image_surface_get_height(surface);
        cr = cairo_create(surface);
		SetState(state, viewClipping, allocation, bounds, viewFrame, usePattern, displayScale);
    }

	// Delete copy constructor and assignment operator to prevent double-free
	CairoContext(const CairoContext&) = delete;
	CairoContext& operator=(const CairoContext&) = delete;

	void AddGradient(const BGradient& gradient)
	{
		// Clean up any existing gradient first
		if (cairoGradient) {
			cairo_pattern_destroy(cairoGradient);
			cairoGradient = NULL;
		}

		switch(gradient.GetType()) {
			case BGradient::TYPE_LINEAR:
			{
				const BGradientLinear* linear = dynamic_cast<const BGradientLinear*>(&gradient);
		
				cairoGradient = cairo_pattern_create_linear(
					linear->Start().x,
					linear->Start().y,
					linear->End().x,
					linear->End().y);
			}
			break;
			
			case BGradient::TYPE_RADIAL:
			{
				const BGradientRadial* radial = dynamic_cast<const BGradientRadial*>(&gradient);

				cairoGradient = cairo_pattern_create_radial(
					radial->Center().x,
					radial->Center().y,
					0,
					radial->Center().x,
					radial->Center().y,
					radial->Radius());
			}
			break;

			// TODO: needs additional work to support focal point
			case BGradient::TYPE_RADIAL_FOCUS:
			{
				const BGradientRadialFocus* radialFocus = dynamic_cast<const BGradientRadialFocus*>(&gradient);

				cairoGradient = cairo_pattern_create_radial(
					radialFocus->Center().x,
					radialFocus->Center().y,
					0,
					radialFocus->Center().x,
					radialFocus->Center().y,
					radialFocus->Radius());
			}
			break;
			
			default:
				printf("*** Unsupported gradient type\n");
				return;
		}
	
		for (int32 i = 0; BGradient::ColorStop* stop = gradient.ColorStopAt(i); i++) {
			cairo_pattern_add_color_stop_rgb(cairoGradient,
				rgb_to_cairo_color(stop->offset),
				rgb_to_cairo_color(stop->color.red),
				rgb_to_cairo_color(stop->color.green),
				rgb_to_cairo_color(stop->color.blue));
		}

		cairo_set_source(cr, cairoGradient);	
	}

	operator cairo_t*()
	{
		return cr;
	}

	cairo_t* Context()
	{
		return cr;
	}

    ~CairoContext()
    {
		if (cr)
			cairo_destroy(cr);

		if (cairoGradient)
			cairo_pattern_destroy(cairoGradient);
			
		if (cairoSourcePattern)
			cairo_pattern_destroy(cairoSourcePattern);
    }

    private:

	void SetState(::BPrivate::ViewState* state, BRegion* viewClipping, rectangle allocation, BRect* bounds, BRect* viewFrame, bool usePattern, float displayScale = 1.0)
	{
		// Destroy any existing source pattern before creating a new one
		if (cairoSourcePattern) {
			cairo_pattern_destroy(cairoSourcePattern);
			cairoSourcePattern = NULL;
		}
		
		// Create a pattern based on the state's pattern type
		cairo_pattern_t* sourcePattern = NULL;
		
		// When usePattern is false (e.g., for text drawing), always use high color
		// and skip pattern processing entirely
		if (!usePattern || state->pattern == B_SOLID_HIGH) {
			sourcePattern = cairo_pattern_create_rgba(
				rgb_to_cairo_color(state->high_color.red),
				rgb_to_cairo_color(state->high_color.green),
				rgb_to_cairo_color(state->high_color.blue),
				rgb_to_cairo_color(state->high_color.alpha));
		} else {
			// Determine if low color should be treated as transparent
			// Per BeOS documentation: B_OP_OVER, B_OP_ERASE, B_OP_INVERT, and B_OP_SELECT
			// treat the low color in a pattern as if it were transparent
			bool lowColorTransparent = (state->drawing_mode == B_OP_OVER ||
			                           state->drawing_mode == B_OP_ERASE ||
			                           state->drawing_mode == B_OP_INVERT ||
			                           state->drawing_mode == B_OP_SELECT);
		
			if (state->pattern != B_SOLID_HIGH && state->pattern != B_SOLID_LOW) {
				// Create a stipple pattern from the BeOS pattern
				// This creates an 8x8 ARGB32 surface with high color where bits are 1, low color where bits are 0
				cairo_surface_t* patternSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
				if (cairo_surface_status(patternSurface) == CAIRO_STATUS_SUCCESS) {
					unsigned char* data = cairo_image_surface_get_data(patternSurface);
					int stride = cairo_image_surface_get_stride(patternSurface);
					
					// Fill the surface based on the pattern data
					// Bits set to 1 = high color, bits set to 0 = low color
					for (int y = 0; y < 8; y++) {
						for (int x = 0; x < 8; x++) {
							bool useHigh = (state->pattern.data[y] & (1 << (7 - x))) != 0;
							rgb_color color = useHigh ? state->high_color : state->low_color;
							
							// Cairo ARGB32 format on little-endian stores as BGRA in memory
							unsigned char* pixel = data + y * stride + x * 4;
							pixel[0] = color.blue;   // B
							pixel[1] = color.green;  // G
							pixel[2] = color.red;    // R
							// For certain drawing modes, low color is transparent
							pixel[3] = (useHigh || !lowColorTransparent) ? color.alpha : 0;  // A
						}
					}
					
					cairo_surface_mark_dirty(patternSurface);
					
					sourcePattern = cairo_pattern_create_for_surface(patternSurface);
					cairo_pattern_set_extend(sourcePattern, CAIRO_EXTEND_REPEAT);
					cairo_pattern_set_filter(sourcePattern, CAIRO_FILTER_NEAREST);
					
					cairo_surface_destroy(patternSurface);
				}
			}
			
			// If we didn't create a stipple pattern, create a solid color pattern
			if (sourcePattern == NULL) {
				rgb_color color;
				if (state->pattern == B_SOLID_HIGH) {
					color = state->high_color;
				} else {
					// B_SOLID_LOW - always use the actual low color
					// The transparency rule only applies to low color PIXELS in stipple patterns,
					// not to B_SOLID_LOW which means "fill everything with low color"
					color = state->low_color;
				}
				
				sourcePattern = cairo_pattern_create_rgba(
					rgb_to_cairo_color(color.red),
					rgb_to_cairo_color(color.green),
					rgb_to_cairo_color(color.blue),
					rgb_to_cairo_color(color.alpha));
			}
		}
		
		// Set the pattern as the source
		if (sourcePattern != NULL) {
			cairo_set_source(cr, sourcePattern);
			// Store it so we can destroy it in the destructor
			cairoSourcePattern = sourcePattern;
		}

		// Handle alpha blending modes (SetBlendingMode)
		// Map BeOS alpha functions to Cairo composite operators
		cairo_operator_t composite_op = CAIRO_OPERATOR_OVER; // default
		
		switch(state->alpha_function_mode) {
			case B_ALPHA_OVERLAY:
			case B_ALPHA_COMPOSITE_SOURCE_OVER:
				composite_op = CAIRO_OPERATOR_OVER;
				break;
			case B_ALPHA_COMPOSITE_SOURCE_IN:
				composite_op = CAIRO_OPERATOR_IN;
				break;
			case B_ALPHA_COMPOSITE_SOURCE_OUT:
				composite_op = CAIRO_OPERATOR_OUT;
				break;
			case B_ALPHA_COMPOSITE_SOURCE_ATOP:
				composite_op = CAIRO_OPERATOR_ATOP;
				break;
			case B_ALPHA_COMPOSITE_DESTINATION_OVER:
				composite_op = CAIRO_OPERATOR_DEST_OVER;
				break;
			case B_ALPHA_COMPOSITE_DESTINATION_IN:
				composite_op = CAIRO_OPERATOR_DEST_IN;
				break;
			case B_ALPHA_COMPOSITE_DESTINATION_OUT:
				composite_op = CAIRO_OPERATOR_DEST_OUT;
				break;
			case B_ALPHA_COMPOSITE_DESTINATION_ATOP:
				composite_op = CAIRO_OPERATOR_DEST_ATOP;
				break;
			case B_ALPHA_COMPOSITE_XOR:
				composite_op = CAIRO_OPERATOR_XOR;
				break;
			case B_ALPHA_COMPOSITE_CLEAR:
				composite_op = CAIRO_OPERATOR_CLEAR;
				break;
			case B_ALPHA_COMPOSITE_DIFFERENCE:
				composite_op = CAIRO_OPERATOR_DIFFERENCE;
				break;
			case B_ALPHA_COMPOSITE_LIGHTEN:
				composite_op = CAIRO_OPERATOR_LIGHTEN;
				break;
			case B_ALPHA_COMPOSITE_DARKEN:
				composite_op = CAIRO_OPERATOR_DARKEN;
				break;
			default:
				composite_op = CAIRO_OPERATOR_OVER;
				break;
		}
		
		// Determine which Cairo operator to use
		// B_OP_ALPHA respects the alpha blending mode set by SetBlendingMode
		// B_OP_BLEND uses high color alpha but ignores bitmap's per-pixel alpha (handled in DrawBitmap)
		// All other drawing modes use their direct mapping
		if (state->drawing_mode == B_OP_ALPHA) {
			// Use the alpha function composite operator from SetBlendingMode
			cairo_set_operator(cr, composite_op);
		} else {
			// Use the operator corresponding to the drawing mode
			cairo_set_operator(cr, drawing_mode_to_cairo_operator(state->drawing_mode));
		}
		
		if (state->drawing_mode == B_OP_INVERT) {
			// For Cairo, this requires a white background for the invert to work
			cairo_set_source_rgb(cr, 1., 1., 1.);
		}

		// For B_CONSTANT_ALPHA, we use cairo_paint_with_alpha() in drawing operations
		// or modify the source pattern's alpha matrix. The constant alpha value comes
		// from state->high_color.alpha which is already set in the source color above.

		// Set the cumulative view state parameters: clipping, origin, and scale.
		// For clipping area, start with the view clipping region, which is 
		// the view rectangle minus the area of any visible child views.
		BRegion combinedClippingArea(*viewClipping);
		float combinedScale = state->scale;
		BPoint combinedOrigin(state->origin);

		// Set clipping area, scale, and origin from the current state...
		if (state->clipping_region_used)
			combinedClippingArea.IntersectWith(&state->clipping_region);

		combinedScale = state->scale;
		combinedOrigin = state->origin;

		// ...and then combine the clipping area, scale, and origin from all previous states.
		ViewState* previousState = state->previous_state;
		while (previousState != NULL) {
			if (previousState->clipping_region_used)
				combinedClippingArea.IntersectWith(&previousState->clipping_region);

			combinedScale *= previousState->scale;
			combinedOrigin += previousState->origin;

			previousState = previousState->previous_state;
		}

		// The allocation is always (0,0) and viewFrame contains the view's position
		// For the topview, viewFrame is (0,0) because frame offset is handled when copying
		// backing to widget surface, not here
		
		// Apply display scale for Retina/HiDPI rendering FIRST
		// This must come before translation so the translation is in logical coordinates
		if (displayScale != 1.0) {
			cairo_scale(cr, displayScale, displayScale);
		}
		
		// Do not put BeOS-centric x/y coordinates into Cairo drawing operations before this translation
		cairo_translate(cr, viewFrame->left + combinedOrigin.x + 0.5, 
						viewFrame->top + combinedOrigin.y + 0.5);
		cairo_move_to(cr, state->pen_location.x, state->pen_location.y);

		cairo_set_line_width(cr, state->pen_size * combinedScale);

		uint32 rects = combinedClippingArea.CountRects();

		for (uint32 i = 0; i < rects; i++) {
			cairo_rectangle(cr, combinedClippingArea.RectAt(i).left - 0.5 + bounds->left,
								combinedClippingArea.RectAt(i).top - 0.5 + bounds->top,
								combinedClippingArea.RectAt(i).Width() + 1,
								combinedClippingArea.RectAt(i).Height() + 1);
		}

		cairo_clip(cr);

		// Translate for scrolling
		cairo_translate(cr, -bounds->left, -bounds->top);

		// Apply view state scale
		cairo_scale(cr, combinedScale, combinedScale);

		switch(state->line_join) {
			case B_BUTT_JOIN:
			case B_MITER_JOIN:
			case B_SQUARE_JOIN:
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
				break;
			case B_ROUND_JOIN:
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
				break;
			case B_BEVEL_JOIN:
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
				break;
			default:
				printf("BUG: Invalid line join %d\n", state->line_join);
				break;
		}
	
		switch(state->line_cap) {
			case B_BUTT_CAP:
				// SQUARE is intentional.  Cairo and Be seem to disagree on this.
				// If we set Cairo to CAIRO_LINE_CAP_BUTT, all lines come out too short
				// by a pixel on both ends.
				cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
				break;
			case B_ROUND_CAP:
				cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
				break;
			case B_SQUARE_CAP:
				cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
				break;
			default:
				printf("BUG: Invalid line cap %d\n", state->line_cap);
				break;
		}

		switch(state->fill_rule) {
			case B_EVEN_ODD:
				cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
				break;
			case B_NONZERO:
				cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
				break;
			default:
				printf("BUG: Invalid fill rule %d\n", state->fill_rule);
				break;
		}
	}

    cairo_t *cr;
	cairo_pattern_t *cairoGradient = NULL;
	cairo_pattern_t *cairoSourcePattern = NULL;
};


class CairoShapeIterator : public BShapeIterator {
public:

	CairoShapeIterator(cairo_t* cr) : BShapeIterator()
	{
		this->cr = cr;
	}

	virtual status_t IterateMoveTo(BPoint* point)
	{
		cairo_move_to(cr, point->x, point->y);
		return B_OK;
	}

	virtual status_t IterateLineTo(int32 count, BPoint* points)
	{
		for (int32 i = 0; i < count; i++) {
			cairo_line_to(cr, points[i].x, points[i].y);
		}
		return B_OK;
	}
	
	virtual status_t IterateClose() {
		cairo_close_path(cr);
		return B_OK;
	}


private:

	cairo_t *cr;
};


