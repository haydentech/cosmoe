
#include <GraphicsDefs.h>
#include <ViewState.h>
#include <CosmoeBackendAPI.h>


class BRegion;

#include <cairo.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>

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
			return CAIRO_OPERATOR_OVER;
		case B_OP_ADD:
			return CAIRO_OPERATOR_ADD;
		case B_OP_SUBTRACT:
			// Note: Cairo doesn't have a true subtract operator
			// DIFFERENCE gives |src - dest|, but BeOS wants dest - src
			// This is an approximation
			return CAIRO_OPERATOR_DIFFERENCE;
		case B_OP_BLEND:
			// B_OP_BLEND averages source and destination colors
			// We approximate this by using OVER with 50% alpha on the source
			// The actual blending is handled in SetState() by modifying source alpha
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


static cairo_fill_rule_t
fill_rule_to_cairo_fill_rule(int32 fillRule)
{
	return fillRule == B_EVEN_ODD
		? CAIRO_FILL_RULE_EVEN_ODD
		: CAIRO_FILL_RULE_WINDING;
}


static void append_shape_path(cairo_t* context, BShape* shape,
	const BPoint& offset);


class CairoContext {
	public:

	CairoContext(cairo_surface_t* surface, ::BPrivate::ViewState* state, BRegion* viewClipping, BRect* bounds, BRect* viewFrame, bool usePattern, float displayScale = 1.0, BRect* updateRect = NULL)
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
		SetState(state, viewClipping, allocation, bounds, viewFrame, usePattern, displayScale, updateRect);
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

			case BGradient::TYPE_RADIAL_FOCUS:
			{
				const BGradientRadialFocus* radialFocus = dynamic_cast<const BGradientRadialFocus*>(&gradient);

				// Create a radial gradient with focal point
				// The inner circle (radius 0) is at the focal point
				// The outer circle is at the center with the specified radius
				cairoGradient = cairo_pattern_create_radial(
					radialFocus->Focal().x,
					radialFocus->Focal().y,
					0,
					radialFocus->Center().x,
					radialFocus->Center().y,
					radialFocus->Radius());
			}
			break;

			case BGradient::TYPE_DIAMOND:
			{
				const BGradientDiamond* diamond = dynamic_cast<const BGradientDiamond*>(&gradient);
				
				// Diamond gradient: approximated using a radial gradient rotated 45 degrees
				// Cairo doesn't have native diamond support, so we create a radial that looks diamond-like
				// A true diamond would require mesh patterns or custom rendering
				// For now, use radial as a reasonable approximation
				float radius = 100.0f; // Default radius, should ideally be based on bounds
				cairoGradient = cairo_pattern_create_radial(
					diamond->Center().x,
					diamond->Center().y,
					0,
					diamond->Center().x,
					diamond->Center().y,
					radius);
				
				// Note: This is a simplified approximation. A proper implementation would
				// use cairo_pattern_create_mesh() to create a diamond-shaped gradient
			}
			break;

			case BGradient::TYPE_CONIC:
			{
				const BGradientConic* conic = dynamic_cast<const BGradientConic*>(&gradient);
				
				// Conic (angular/sweep) gradient: not directly supported by Cairo
				// We approximate it using multiple radial gradients or mesh patterns
				// For now, use a radial gradient as a placeholder
				// A proper implementation would require custom rendering with cairo_mesh_pattern
				float radius = 100.0f; // Default radius
				cairoGradient = cairo_pattern_create_radial(
					conic->Center().x,
					conic->Center().y,
					0,
					conic->Center().x,
					conic->Center().y,
					radius);
				
				// Note: This is a placeholder. Proper conic gradients require mesh patterns
				// or pixel-by-pixel rendering based on angle from center
			}
			break;
			
			default:
				printf("*** Unsupported gradient type %d\n", gradient.GetType());
				return;
		}
	
		for (int32 i = 0; BGradient::ColorStop* stop = gradient.ColorStopAt(i); i++) {
			cairo_pattern_add_color_stop_rgba(cairoGradient,
				rgb_to_cairo_color(stop->offset),
				rgb_to_cairo_color(stop->color.red),
				rgb_to_cairo_color(stop->color.green),
				rgb_to_cairo_color(stop->color.blue),
				rgb_to_cairo_color(stop->color.alpha));
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

	// Helper method to perform fill operation respecting drawing mode
	void Fill()
	{
		if (drawingMode == B_OP_BLEND) {
			// B_OP_BLEND averages colors: (src + dest) / 2
			// Push a group, fill it, then paint the group with 50% alpha
			cairo_push_group(cr);
			cairo_fill(cr);
			cairo_pop_group_to_source(cr);
			cairo_paint_with_alpha(cr, 0.5);
		} else if (drawingMode == B_OP_COPY) {
			cairo_save(cr);
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
			cairo_fill(cr);
			cairo_restore(cr);
		} else {
			cairo_fill(cr);
		}
	}

	// Helper method to perform stroke operation respecting drawing mode
	void Stroke()
	{
		if (drawingMode == B_OP_BLEND) {
			// B_OP_BLEND averages colors: (src + dest) / 2
			cairo_push_group(cr);
			cairo_stroke(cr);
			cairo_pop_group_to_source(cr);
			cairo_paint_with_alpha(cr, 0.5);
		} else if (drawingMode == B_OP_COPY) {
			cairo_save(cr);
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
			cairo_stroke(cr);
			cairo_restore(cr);
		} else {
			cairo_stroke(cr);
		}
	}

	// Helper method to draw text (Pango layout) respecting drawing mode
	void ShowLayout(PangoLayout* layout)
	{
		if (drawingMode == B_OP_BLEND) {
			// B_OP_BLEND averages colors: (src + dest) / 2
			// Push a group, draw text into it, then paint with 50% alpha
			cairo_push_group(cr);
			pango_cairo_show_layout(cr, layout);
			cairo_pop_group_to_source(cr);
			cairo_paint_with_alpha(cr, 0.5);
		} else if (drawingMode == B_OP_COPY) {
			cairo_save(cr);
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
			pango_cairo_show_layout(cr, layout);
			cairo_restore(cr);
		} else {
			pango_cairo_show_layout(cr, layout);
		}
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

	void SetState(::BPrivate::ViewState* state, BRegion* viewClipping, rectangle allocation, BRect* bounds, BRect* viewFrame, bool usePattern, float displayScale = 1.0, BRect* updateRect = NULL)
	{
		// Destroy any existing source pattern before creating a new one
		if (cairoSourcePattern) {
			cairo_pattern_destroy(cairoSourcePattern);
			cairoSourcePattern = NULL;
		}
		
		// Create a pattern based on the state's pattern type
		cairo_pattern_t* sourcePattern = NULL;
		bool eraseMode = state->drawing_mode == B_OP_ERASE;
		bool forceOpaque = state->drawing_mode == B_OP_COPY
			|| state->drawing_mode == B_OP_MIN
			|| state->drawing_mode == B_OP_MAX
			|| state->drawing_mode == B_OP_ADD
			|| state->drawing_mode == B_OP_SUBTRACT;

		if (!usePattern || state->pattern == B_SOLID_HIGH) {
			// When usePattern is false (e.g., for text drawing), skip pattern
			// processing entirely. When in B_OP_ERASE mode, paint with
			// low color, otherwise use high color.
			rgb_color solidColor = eraseMode ? state->low_color : state->high_color;
			sourcePattern = cairo_pattern_create_rgba(
				rgb_to_cairo_color(solidColor.red),
				rgb_to_cairo_color(solidColor.green),
				rgb_to_cairo_color(solidColor.blue),
				rgb_to_cairo_color(forceOpaque ? 255 : solidColor.alpha));
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
					// Default: Bits set to 1 = high color, bits set to 0 = low color.
					// B_OP_ERASE override for primitives:
					// Bits set to 1 = low color, bits set to 0 = transparent.
					for (int y = 0; y < 8; y++) {
						for (int x = 0; x < 8; x++) {
							bool bitIsOne = (state->pattern.data[y] & (1 << (7 - x))) != 0;
							bool useHigh = !eraseMode && bitIsOne;
							rgb_color color = useHigh ? state->high_color : state->low_color;
							bool transparent = eraseMode ? !bitIsOne
								: (!useHigh && lowColorTransparent);
							
							// Cairo ARGB32 format on little-endian stores as BGRA in memory
							unsigned char* pixel = data + y * stride + x * 4;
							pixel[0] = color.blue;   // B
							pixel[1] = color.green;  // G
							pixel[2] = color.red;    // R
							pixel[3] = transparent ? 0
								: (forceOpaque ? 255 : color.alpha);  // A
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
				if (eraseMode || state->pattern == B_SOLID_LOW) {
					color = state->low_color;
				} else if (state->pattern == B_SOLID_HIGH) {
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
					rgb_to_cairo_color(forceOpaque ? 255 : color.alpha));
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
		
		// Store the drawing mode for use in Fill() and Stroke() helper methods
		drawingMode = state->drawing_mode;
		
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

		// Set the cumulative view state parameters: clipping, origin, scale,
		// and affine transform.
		BRegion combinedClippingArea(*viewClipping);
		float combinedScale = 1.0f;
		BPoint combinedOrigin(0.0f, 0.0f);
		BAffineTransform combinedTransform;

		auto applyScaleAndOffsetToRegion = [](BRegion& region, float scale,
			const BPoint& offset) {
			if (scale == 1.0f) {
				region.OffsetBy(offset.x, offset.y);
				return;
			}

			BRegion converted;
			int32 count = region.CountRects();
			for (int32 i = 0; i < count; i++) {
				BRect rect = region.RectAt(i);
				BPoint leftTop(rect.LeftTop());
				BPoint rightBottom(rect.RightBottom());
				rightBottom.x += 1.0f;
				rightBottom.y += 1.0f;

				leftTop.x = leftTop.x * scale + offset.x;
				leftTop.y = leftTop.y * scale + offset.y;
				rightBottom.x = rightBottom.x * scale + offset.x;
				rightBottom.y = rightBottom.y * scale + offset.y;

				rightBottom.x -= 1.0f;
				rightBottom.y -= 1.0f;
				converted.Include(BRect(leftTop, rightBottom));
			}

			region = converted;
		};

		std::vector<ViewState*> stateStack;
		for (ViewState* currentState = state; currentState != NULL;
				currentState = currentState->previous_state) {
			stateStack.push_back(currentState);
		}

		for (std::vector<ViewState*>::reverse_iterator it = stateStack.rbegin();
				it != stateStack.rend(); ++it) {
			ViewState* currentState = *it;

			combinedOrigin.x += currentState->origin.x * combinedScale;
			combinedOrigin.y += currentState->origin.y * combinedScale;
			combinedScale *= currentState->scale;
			combinedTransform = combinedTransform * currentState->transform;

			if (!currentState->clipping_region_used)
				continue;

			BRegion transformedClip(currentState->clipping_region);
			transformedClip.OffsetBy(-(int32)bounds->left, -(int32)bounds->top);
			applyScaleAndOffsetToRegion(transformedClip, combinedScale,
				combinedOrigin);
			combinedClippingArea.IntersectWith(&transformedClip);
		}

		const double kTransformEpsilon = 1e-12;
		const bool hasScale = fabs(combinedScale - 1.0f) > kTransformEpsilon;
		const bool hasOriginTranslation = fabs(combinedOrigin.x) > kTransformEpsilon
			|| fabs(combinedOrigin.y) > kTransformEpsilon;
		const bool hasAffineTransform = !combinedTransform.IsIdentity();

		// For transformed content, update rect clipping can miss newly exposed
		// pixels due to rotated/sheared/scaled bounds. Disable it conservatively.
		const bool useConservativeUpdateClip = hasScale || hasOriginTranslation
			|| hasAffineTransform;

		// Keep a separate region for update rect optimization.
		// We'll apply this separately in Cairo after the view boundary clipping,
		// to avoid the boundary clip rectangles from shrinking.
		BRegion* updateRegion = NULL;
		if (!useConservativeUpdateClip && updateRect != NULL && updateRect->IsValid()) {
			// Convert from content coordinates to bounds coordinates by offsetting
			BRect boundsUpdateRect = *updateRect;
			boundsUpdateRect.OffsetBy(-bounds->left, -bounds->top);
			updateRegion = new BRegion(boundsUpdateRect);
		}

		// The allocation is always (0,0) and viewFrame contains the view's position
		// For the topview, viewFrame is (0,0) because frame offset is handled when copying
		// backing to widget surface, not here
		
		// Apply display scale for Retina/HiDPI rendering FIRST
		// This must come before translation so the translation is in logical coordinates
		if (displayScale != 1.0) {
			cairo_scale(cr, displayScale, displayScale);
		}
		
		// Do not put BeOS-centric x/y coordinates into Cairo drawing operations before this translation.
		cairo_translate(cr, viewFrame->left + 0.5, viewFrame->top + 0.5);

		uint32 rects = combinedClippingArea.CountRects();
		for (uint32 i = 0; i < rects; i++) {
			cairo_rectangle(cr,
				combinedClippingArea.RectAt(i).left - 0.5 + bounds->left,
				combinedClippingArea.RectAt(i).top - 0.5 + bounds->top,
				combinedClippingArea.RectAt(i).Width() + 1,
				combinedClippingArea.RectAt(i).Height() + 1);
		}

		cairo_clip(cr);

		// If we have an update region (invalidated area optimization), apply it as 
		// an additional clip. This happens after the scroll translation so it's in
		// bounds coordinates where the update region was defined.
		if (updateRegion != NULL) {
			uint32 updateRects = updateRegion->CountRects();
			for (uint32 i = 0; i < updateRects; i++) {
				BRect rect = updateRegion->RectAt(i);
				cairo_rectangle(cr, rect.left + bounds->left - 0.5,
									rect.top + bounds->top - 0.5,
									rect.Width() + 1,
									rect.Height() + 1);
			}
			cairo_clip(cr);
			delete updateRegion;
		}

		for (size_t i = 0; i < state->shape_clips.size(); i++) {
			const ::BPrivate::ShapeClipOperation& clip = state->shape_clips[i];
			BShape clipShape(clip.shape);
			cairo_set_fill_rule(cr, fill_rule_to_cairo_fill_rule(clip.fill_rule));

			if (clip.inverse) {
				cairo_rectangle(cr,
					bounds->left - 0.5,
					bounds->top - 0.5,
					bounds->Width() + 1,
					bounds->Height() + 1);
				cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
			}

			append_shape_path(cr, &clipShape, B_ORIGIN);
			cairo_clip(cr);
		}

		for (size_t i = 0; i < state->frozen_region_clips.size(); i++) {
			const BRegion& region = state->frozen_region_clips[i].region;
			uint32 rectCount = region.CountRects();
			for (uint32 rectIndex = 0; rectIndex < rectCount; rectIndex++) {
				BRect rect = region.RectAt(rectIndex);
				cairo_rectangle(cr, rect.left - 0.5, rect.top - 0.5,
					rect.Width() + 1, rect.Height() + 1);
			}
			cairo_clip(cr);
		}

		cairo_set_fill_rule(cr, fill_rule_to_cairo_fill_rule(state->fill_rule));

		// Match app_server transform order: non-affine scalar scale first,
		// then affine matrix transform, then origin translation.
		if (hasScale)
			cairo_scale(cr, combinedScale, combinedScale);

		if (hasAffineTransform) {
			cairo_matrix_t affineMatrix;
			affineMatrix.xx = combinedTransform.sx;
			affineMatrix.yx = combinedTransform.shy;
			affineMatrix.xy = combinedTransform.shx;
			affineMatrix.yy = combinedTransform.sy;
			affineMatrix.x0 = combinedTransform.tx;
			affineMatrix.y0 = combinedTransform.ty;
			cairo_transform(cr, &affineMatrix);
		}

		if (hasOriginTranslation)
			cairo_translate(cr, combinedOrigin.x, combinedOrigin.y);

		cairo_set_line_width(cr, state->pen_size);

		// Note: scroll offset is already applied via ConvertToWindow() in the
		// viewFrame calculation (_ConvertToParent subtracts fBounds.left/top).
		// The clip rectangles add bounds->left/top to compensate, placing the
		// clip at the correct physical position. No additional scroll translate
		// is needed here — adding one would double-apply the scroll offset.

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
	drawing_mode drawingMode = B_OP_COPY;
};


class CairoShapeIterator : public BShapeIterator {
public:
	CairoShapeIterator(cairo_t* context, BPoint offset = B_ORIGIN)
		:
		fContext(context),
		fOffset(offset),
		fCurrentPoint(0.0f, 0.0f),
		fSubpathStart(0.0f, 0.0f),
		fHasCurrentPoint(false)
	{
	}

	virtual status_t IterateMoveTo(BPoint* point)
	{
		if (point == NULL)
			return B_BAD_VALUE;

		BPoint offsetPoint = _Offset(*point);
		cairo_move_to(fContext, offsetPoint.x, offsetPoint.y);
		fCurrentPoint = offsetPoint;
		fSubpathStart = offsetPoint;
		fHasCurrentPoint = true;
		return B_OK;
	}

	virtual status_t IterateLineTo(int32 count, BPoint* points)
	{
		if (points == NULL)
			return B_BAD_VALUE;

		for (int32 i = 0; i < count; i++) {
			BPoint offsetPoint = _Offset(points[i]);
			cairo_line_to(fContext, offsetPoint.x, offsetPoint.y);
			fCurrentPoint = offsetPoint;
			if (!fHasCurrentPoint) {
				fSubpathStart = offsetPoint;
				fHasCurrentPoint = true;
			}
		}

		return B_OK;
	}

	virtual status_t IterateBezierTo(int32 bezierCount, BPoint* bezierPoints)
	{
		if (bezierPoints == NULL)
			return B_BAD_VALUE;

		for (int32 i = 0; i < bezierCount; i++) {
			BPoint* control = bezierPoints + i * 3;
			BPoint control0 = _Offset(control[0]);
			BPoint control1 = _Offset(control[1]);
			BPoint control2 = _Offset(control[2]);
			cairo_curve_to(fContext,
				control0.x, control0.y,
				control1.x, control1.y,
				control2.x, control2.y);
			fCurrentPoint = control2;
			if (!fHasCurrentPoint) {
				fSubpathStart = control2;
				fHasCurrentPoint = true;
			}
		}

		return B_OK;
	}

	virtual status_t IterateClose()
	{
		cairo_close_path(fContext);
		if (fHasCurrentPoint)
			fCurrentPoint = fSubpathStart;
		return B_OK;
	}

	virtual status_t IterateArcTo(float& rx, float& ry, float& angle, bool largeArc,
		bool counterClockWise, BPoint& point)
	{
		// Cairo and Haiku have different interpretations of the counterClockWise flag somehow
		const bool cairoCounterClockWise = !counterClockWise;

		BPoint offsetPoint = _Offset(point);
		if (!fHasCurrentPoint) {
			cairo_move_to(fContext, offsetPoint.x, offsetPoint.y);
			fCurrentPoint = offsetPoint;
			fSubpathStart = offsetPoint;
			fHasCurrentPoint = true;
			return B_OK;
		}

		const double x1 = fCurrentPoint.x;
		const double y1 = fCurrentPoint.y;
		const double x2 = offsetPoint.x;
		const double y2 = offsetPoint.y;

		double radiusX = fabs(rx);
		double radiusY = fabs(ry);

		if (radiusX <= 0.0 || radiusY <= 0.0) {
			cairo_line_to(fContext, x2, y2);
			fCurrentPoint = offsetPoint;
			return B_OK;
		}

		if (x1 == x2 && y1 == y2)
			return B_OK;

		const double phi = angle * M_PI / 180.0;
		const double cosPhi = cos(phi);
		const double sinPhi = sin(phi);

		const double dx2 = (x1 - x2) * 0.5;
		const double dy2 = (y1 - y2) * 0.5;
		const double x1Prime = cosPhi * dx2 + sinPhi * dy2;
		const double y1Prime = -sinPhi * dx2 + cosPhi * dy2;

		double rx2 = radiusX * radiusX;
		double ry2 = radiusY * radiusY;
		double x1Prime2 = x1Prime * x1Prime;
		double y1Prime2 = y1Prime * y1Prime;

		double radiiScale = x1Prime2 / rx2 + y1Prime2 / ry2;
		if (radiiScale > 1.0) {
			double scale = sqrt(radiiScale);
			radiusX *= scale;
			radiusY *= scale;
			rx2 = radiusX * radiusX;
			ry2 = radiusY * radiusY;
		}

		double numerator = rx2 * ry2 - rx2 * y1Prime2 - ry2 * x1Prime2;
		double denominator = rx2 * y1Prime2 + ry2 * x1Prime2;
		if (denominator <= 0.0) {
			cairo_line_to(fContext, x2, y2);
			fCurrentPoint = point;
			return B_OK;
		}

		double sign = (largeArc == cairoCounterClockWise) ? -1.0 : 1.0;
		double coeff = sign * sqrt(std::max(0.0, numerator / denominator));

		double cxPrime = coeff * (radiusX * y1Prime / radiusY);
		double cyPrime = coeff * (-radiusY * x1Prime / radiusX);

		double centerX = cosPhi * cxPrime - sinPhi * cyPrime + (x1 + x2) * 0.5;
		double centerY = sinPhi * cxPrime + cosPhi * cyPrime + (y1 + y2) * 0.5;

		double ux = (x1Prime - cxPrime) / radiusX;
		double uy = (y1Prime - cyPrime) / radiusY;
		double vx = (-x1Prime - cxPrime) / radiusX;
		double vy = (-y1Prime - cyPrime) / radiusY;

		double startAngle = atan2(uy, ux);
		double deltaAngle = atan2(ux * vy - uy * vx, ux * vx + uy * vy);

		if (!cairoCounterClockWise && deltaAngle > 0.0)
			deltaAngle -= 2.0 * M_PI;
		else if (cairoCounterClockWise && deltaAngle < 0.0)
			deltaAngle += 2.0 * M_PI;

		cairo_save(fContext);
		cairo_translate(fContext, centerX, centerY);
		cairo_rotate(fContext, phi);
		cairo_scale(fContext, radiusX, radiusY);
		if (cairoCounterClockWise)
			cairo_arc(fContext, 0.0, 0.0, 1.0, startAngle, startAngle + deltaAngle);
		else
			cairo_arc_negative(fContext, 0.0, 0.0, 1.0, startAngle,
				startAngle + deltaAngle);
		cairo_restore(fContext);

		fCurrentPoint = offsetPoint;
		return B_OK;
	}

private:
	BPoint _Offset(const BPoint& point) const
	{
		return BPoint(point.x + fOffset.x, point.y + fOffset.y);
	}

	cairo_t* fContext;
	BPoint fOffset;
	BPoint fCurrentPoint;
	BPoint fSubpathStart;
	bool fHasCurrentPoint;
};


static void
append_shape_path(cairo_t* context, BShape* shape, const BPoint& offset)
{
	CairoShapeIterator iterator(context, offset);
	iterator.Iterate(shape);
}





