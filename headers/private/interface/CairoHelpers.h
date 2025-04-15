
#include <GraphicsDefs.h>
#include <ViewState.h>


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
			return CAIRO_OPERATOR_DIFFERENCE;
		case B_OP_BLEND:
			return CAIRO_OPERATOR_OVERLAY;
		case B_OP_MIN:
			return CAIRO_OPERATOR_LIGHTEN;
		case B_OP_MAX:
			return CAIRO_OPERATOR_DARKEN;
		case B_OP_ALPHA:
			return CAIRO_OPERATOR_ATOP;
		case B_OP_INVERT:
			// This will work with the addition of a white source
			// e.g. cairo_set_source_rgb (cr, 1., 1., 1.);
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
	}

	return CAIRO_FORMAT_INVALID;
}


class CairoContext {
	public:

    CairoContext(widget* widget, ::BPrivate::ViewState* state, BRegion* viewClipping, BRect* bounds)
    {
		rectangle allocation;

		widget_get_allocation(widget, &allocation);
        cr = widget_cairo_create(widget);
		SetState(state, viewClipping, allocation, bounds);
    }

	void AddGradient(const BGradient& gradient)
	{
		cairo_pattern_t *cairoGradient;

		switch(gradient.GetType()) {
			case BGradient::TYPE_LINEAR:
			{
				const BGradientLinear* linear = dynamic_cast<const BGradientLinear *>(&gradient);
		
				cairoGradient = cairo_pattern_create_linear(
					linear->Start().x,
					linear->Start().y,
					linear->End().x,
					linear->End().y);
			}
			break;
			
			case BGradient::TYPE_RADIAL:
			case BGradient::TYPE_RADIAL_FOCUS:	// should have it's own, but this is "good enough" for now
			{
				const BGradientRadial* radial = dynamic_cast<const BGradientRadial *>(&gradient);

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
        cairo_destroy(cr);

		if (cairoGradient)
			cairo_pattern_destroy(cairoGradient);
    }

    private:

	void SetState(::BPrivate::ViewState* state, BRegion* viewClipping, rectangle allocation, BRect* bounds)
	{
		if (state->pattern == B_SOLID_HIGH) {
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

        cairo_set_line_width(cr, state->pen_size);
        cairo_set_operator(cr, drawing_mode_to_cairo_operator(state->drawing_mode));

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

		// Do not put BeOS-centric x/y coordinates into Cairo drawing operations before this translation
		cairo_translate(cr, allocation.x + combinedOrigin.x - 0.5, allocation.y + combinedOrigin.y - 0.5);
		cairo_move_to(cr, state->pen_location.x, state->pen_location.y);

		uint32 rects = combinedClippingArea.CountRects();

		for (uint32 i = 0; i < rects; i++) {
			cairo_rectangle(cr, combinedClippingArea.RectAt(i).left - 0.5,
								combinedClippingArea.RectAt(i).top - 0.5,
								combinedClippingArea.RectAt(i).Width() + 1,
								combinedClippingArea.RectAt(i).Height() + 1);
		}

		cairo_clip(cr);

		// Translate for scrolling
		cairo_translate(cr, -bounds->left, -bounds->top);

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
		}

		cairo_set_fill_rule(cr, state->fill_rule == B_EVEN_ODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
	}

    cairo_t *cr;
	cairo_pattern_t *cairoGradient = NULL;
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


