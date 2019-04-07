#include "keys.h"
#include <keys.h>
#include <libndls.h>

static const t_key _dummyTKey = KEY_(_KEY_DUMMY_ROW, _KEY_DUMMY_COL);
const t_key * const TINspireKeyToNdlessKey[] = {
#define X(name, us) [TIN_KEY_ ## name] = &(KEY_NSPIRE_ ## name),
	X_TINSPIRE_KEYS
#undef X
	[TIN_KEY__LAST] = &_dummyTKey
};

t_key *TINKeyToNdlessKey(enum tinspire_key key) {
	if (key < 0 || key >= TIN_KEY__LAST) {
		return (t_key*)TINspireKeyToNdlessKey[TIN_KEY__LAST];
	}
	return (t_key*)TINspireKeyToNdlessKey[key];
	/* Note: Casting to shut up the compiler */
}

int _keyIsFromTouchpad(enum tinspire_key key) {
	return key == TIN_KEY_CLICK
		|| key == TIN_KEY_UP
		|| key == TIN_KEY_DOWN
		|| key == TIN_KEY_LEFT
		|| key == TIN_KEY_RIGHT;
}

static const enum tinspire_key _MapTPadToTinKey[] = {
	[TPAD_ARROW_NONE] = 0,
	[TPAD_ARROW_UP] = TIN_KEY_UP,
	[TPAD_ARROW_UPRIGHT] = 0,
	[TPAD_ARROW_RIGHT] = TIN_KEY_RIGHT,
	[TPAD_ARROW_RIGHTDOWN] = 0,
	[TPAD_ARROW_DOWN] = TIN_KEY_DOWN,
	[TPAD_ARROW_DOWNLEFT] = 0,
	[TPAD_ARROW_LEFT] = TIN_KEY_LEFT,
	[TPAD_ARROW_LEFTUP] = 0,
	[TPAD_ARROW_CLICK] = TIN_KEY_CLICK
};

int TINKeyPressed(enum tinspire_key key, touchpad_report_t* tp_report) {
	if (key < 0 || key >= TIN_KEY__LAST) {
		return 0;
	}
	/* If it's an "touchpad key", then we do it ourselves so Ndless doesn't
	 * poll the touchpad and lose velocity information.
	 * See also _pollCursor in main.c */
	if (_keyIsFromTouchpad(key) && is_touchpad && tp_report) {
		tpad_arrow_t arrow = tp_report->arrow;
		if (arrow == TPAD_ARROW_UPRIGHT) {
			return key == TIN_KEY_UP || key == TIN_KEY_RIGHT;
		} else if (arrow == TPAD_ARROW_RIGHTDOWN) {
			return key == TIN_KEY_RIGHT || key == TIN_KEY_DOWN;
		} else if (arrow == TPAD_ARROW_DOWNLEFT) {
			return key == TIN_KEY_DOWN || key == TIN_KEY_LEFT;
		} else if (arrow == TPAD_ARROW_LEFTUP) {
			return key == TIN_KEY_LEFT || key == TIN_KEY_UP;
		}
		return key == _MapTPadToTinKey[arrow];
	}
	return !!isKeyPressed(*TINKeyToNdlessKey(key));
}

