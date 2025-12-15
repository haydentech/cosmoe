
#include <GraphicsDefs.h>
#include <ViewState.h>
#include <WindowBackendCAPI.h>


class BRegion;

#include <cairo/cairo.h>

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

	CairoContext(cairo_surface_t* surface, ::BPrivate::ViewState* state, BRegion* viewClipping, BRect* bounds, BRect* viewFrame, bool usePattern = false, float displayScale = 1.0)
		: cairoGradient(NULL), waylandSurface(false)
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
    }

    private:

	void SetState(::BPrivate::ViewState* state, BRegion* viewClipping, rectangle allocation, BRect* bounds, BRect* viewFrame, bool usePattern = false, float displayScale = 1.0)
	{
		if (usePattern == false || state->pattern == B_SOLID_HIGH) {
			cairo_set_source_rgba(cr, rgb_to_cairo_color(state->high_color.red),
										rgb_to_cairo_color(state->high_color.green),
										rgb_to_cairo_color(state->high_color.blue),
										rgb_to_cairo_color(state->high_color.alpha));
		} else if (state->pattern == B_SOLID_LOW) {
			cairo_set_source_rgba(cr, rgb_to_cairo_color(state->low_color.red),
										rgb_to_cairo_color(state->low_color.green),
										rgb_to_cairo_color(state->low_color.blue),
										rgb_to_cairo_color(state->low_color.alpha));			
		} else {
			// A quick hack, but good enough given how infrequently this is used
			cairo_set_source_rgba(cr, rgb_to_cairo_color((state->high_color.red + state->low_color.red) / 2),
										rgb_to_cairo_color((state->high_color.green + state->low_color.green) / 2),
										rgb_to_cairo_color((state->high_color.blue + state->low_color.blue) / 2),
										rgb_to_cairo_color((state->high_color.alpha + state->low_color.alpha) / 2));
		}

        cairo_set_operator(cr, drawing_mode_to_cairo_operator(state->drawing_mode));
		if (state->drawing_mode == B_OP_INVERT) {
			// For Cairo, this requires a white background for the invert to work
			cairo_set_source_rgb(cr, 1., 1., 1.);
		}

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

		// Scale the Cairo coordinate system to match display scale
		// This converts all subsequent logical coordinates to physical pixels
		cairo_scale(cr, displayScale, displayScale);

		// The allocation is always (0,0) and viewFrame contains the view's position
		// For the topview, viewFrame is (0,0) because frame offset is handled when copying
		// backing to widget surface, not here
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
	bool waylandSurface = false;
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


