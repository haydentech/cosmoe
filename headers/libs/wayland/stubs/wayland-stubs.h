#ifndef WL_POINTER_BUTTON_STATE_ENUM
#define WL_POINTER_BUTTON_STATE_ENUM
/**
 * physical button state
 *
 * Describes the physical state of a button that produced the button
 * event.
 */
enum wl_pointer_button_state {
	/**
	 * the button is not pressed
	 */
	WL_POINTER_BUTTON_STATE_RELEASED = 0,
	/**
	 * the button is pressed
	 */
	WL_POINTER_BUTTON_STATE_PRESSED = 1,
};
#endif /* WL_POINTER_BUTTON_STATE_ENUM */

#ifndef WL_KEYBOARD_KEY_STATE_ENUM
#define WL_KEYBOARD_KEY_STATE_ENUM
/**
 * @ingroup iface_wl_keyboard
 * physical key state
 *
 * Describes the physical state of a key that produced the key event.
 *
 * Since version 10, the key can be in a "repeated" pseudo-state which
 * means the same as "pressed", but is used to signal repetition in the
 * key event.
 *
 * The key may only enter the repeated state after entering the pressed
 * state and before entering the released state. This event may be
 * generated multiple times while the key is down.
 */
enum wl_keyboard_key_state {
	/**
	 * key is not pressed
	 */
	WL_KEYBOARD_KEY_STATE_RELEASED = 0,
	/**
	 * key is pressed
	 */
	WL_KEYBOARD_KEY_STATE_PRESSED = 1,
	/**
	 * key was repeated
	 * @since 10
	 */
	WL_KEYBOARD_KEY_STATE_REPEATED = 2,
};
#endif /* WL_KEYBOARD_KEY_STATE_ENUM */

#ifndef WL_POINTER_AXIS_ENUM
#define WL_POINTER_AXIS_ENUM
/**
 * @ingroup iface_wl_pointer
 * axis types
 *
 * Describes the axis types of scroll events.
 */
enum wl_pointer_axis {
	/**
	 * vertical axis
	 */
	WL_POINTER_AXIS_VERTICAL_SCROLL = 0,
	/**
	 * horizontal axis
	 */
	WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1,
};
#endif /* WL_POINTER_AXIS_ENUM */


/**
 * Fixed-point number
 *
 * A `wl_fixed_t` is a 24.8 signed fixed-point number with a sign bit, 23 bits
 * of integer precision and 8 bits of decimal precision. Consider `wl_fixed_t`
 * as an opaque struct with methods that facilitate conversion to and from
 * `double` and `int` types.
 */
typedef int32_t wl_fixed_t;

/**
 * Converts a fixed-point number to a floating-point number.
 *
 * \param f Fixed-point number to convert
 *
 * \return Floating-point representation of the fixed-point argument
 */
static inline double
cosmoe_fixed_to_double(wl_fixed_t f)
{
	return f / 256.0;
}