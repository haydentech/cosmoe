
#include <GraphicsDefs.h>
#include <ViewState.h>

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


class CairoContext {
	public:

    CairoContext(widget* widget, ::BPrivate::ViewState* state)
    {
		rectangle allocation;

		widget_get_allocation(widget, &allocation);
        cr = widget_cairo_create(widget);
		SetState(state, allocation);
    }

	void AddGradient(const BGradient& gradient)
	{
		cairo_pattern_t *cairoGradient;

		if (gradient.GetType() == BGradient::TYPE_LINEAR) {
			const BGradientLinear* linear
					= dynamic_cast<const BGradientLinear *>(&gradient);
	
			cairoGradient = cairo_pattern_create_linear(
				linear->Start().x,
				linear->Start().y,
				linear->End().x,
				linear->End().y);
		} else {
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

	void SetState(::BPrivate::ViewState* state, rectangle allocation)
	{
        cairo_set_source_rgba(cr, rgb_to_cairo_color(state->high_color.red),
                                    rgb_to_cairo_color(state->high_color.green),
                                    rgb_to_cairo_color(state->high_color.blue),
                                    rgb_to_cairo_color(state->high_color.alpha));
        cairo_set_line_width(cr, state->pen_size);
        cairo_set_operator(cr, drawing_mode_to_cairo_operator(state->drawing_mode));

		// Do not put BeOS-centric x/y coordinates before this translation
        cairo_translate(cr, allocation.x + state->origin.x,
							allocation.y + state->origin.y);
		cairo_move_to(cr, state->pen_location.x, state->pen_location.y);

		if (state->clipping_region_used) {
			uint32 rects = state->clipping_region.CountRects();

			for (uint32 i = 0; i < rects; i++) {
				cairo_rectangle(cr, state->clipping_region.RectAt(i).left,
					state->clipping_region.RectAt(i).top,
					state->clipping_region.RectAt(i).IntegerWidth(),
					state->clipping_region.RectAt(i).IntegerHeight());
			}
		
			cairo_clip(cr);
		}
		//cairo_scale(cr, 2.0, 2.0);

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
				// SQUARE is intentional.  Cairo and Be seems to disagree on the meaning
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


