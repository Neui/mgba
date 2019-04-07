#ifndef TINSPIRE_KEYS_H
#define TINSPIRE_KEYS_H
#include <keys.h>
#include <libndls.h>

/* Ndless defines contstants structs based on rows and colums. However, this
 * isn't very practical, so here's an "translated" set.
 * In addition, this is using the X-Macro pattern for easy overview.
 * Re-Sorted and modified by hand. */

/* #define X(name, display) */
#define X_TINSPIRE_KEYS \
	X(NEGATIVE, "Negative (-)") \
	X(PERIOD, "Period .") \
	X(0, "0") \
	X(1, "1") \
	X(2, "2") \
	X(3, "3") \
	X(5, "5") \
	X(4, "4") \
	X(6, "6") \
	X(7, "7") \
	X(8, "8") \
	X(9, "9") \
	X(A, "A") \
	X(B, "B") \
	X(C, "C") \
	X(D, "D") \
	X(E, "E") \
	X(F, "F") \
	X(G, "G") \
	X(H, "H") \
	X(I, "I") \
	X(J, "J") \
	X(K, "K") \
	X(L, "L") \
	X(M, "M") \
	X(N, "N") \
	X(O, "O") \
	X(P, "P") \
	X(Q, "Q") \
	X(R, "R") \
	X(S, "S") \
	X(T, "T") \
	X(U, "U") \
	X(V, "V") \
	X(W, "W") \
	X(X, "X") \
	X(Y, "Y") \
	X(Z, "Z") \
	X(SPACE, "Space") \
	X(EE, "EE") \
	X(PI, "Pi") \
	X(COMMA, "Comma ,") \
	X(QUESEXCL, "?!>") \
	X(FLAG, "Flag") \
	X(RET, "Return") \
	X(ESC, "esc") \
	X(SCRATCHPAD, "scratchpad / save") \
	X(TAB, "tab") \
	X(HOME, "home / on") \
	X(DOC, "doc") \
	X(MENU, "menu") \
	X(CTRL, "ctrl") \
	X(SHIFT, "shift") \
	X(VAR, "var / sto->") \
	X(DEL, "del / clear") \
	X(EQU, "Equals =") \
	X(TRIG, "trig>") \
	X(EXP, "Exponent ^") \
	\
	X(SQU, "Square (x^2)") \
	X(eEXP, "e exponent (e^x)") \
	X(TENX, "10^x") \
	\
	X(LP, "Left paren (") \
	X(RP, "Right paren )") \
	X(FRAC, "|O|{B") /* Left of book */ \
	X(CAT, "book") \
	X(MULTIPLY, "Multiply *") \
	X(DIVIDE, "Divide /") \
	X(PLUS, "Plus +") \
	X(MINUS, "Minus -") \
	X(ENTER, "Enter") \
	X(LEFT, "Touchpad (left)") \
	X(DOWN, "Touchpad (down)") \
	X(RIGHT, "Touchpad (right)") \
	X(UP, "Touchpad (Up)") \
	X(CLICK, "Touchpad (middle)") \

/* These seem to be for some other models I don't have */
#define X_TINSPIRE_KEYS_OTHER \
	X(QUES, "QUES") \
	X(COLON, "COLON") \
	X(II, "II") \
	X(GTHAN, "GTHAN") \
	X(BAR, "BAR") \
	X(APOSTROPHE, "APOSTROPHE") \
	X(QUOTE, "QUOTE") \
	X(TAN, "TAN") \
	X(COS, "COS") \
	X(SIN, "SIN") \
	X(LTHAN, "LTHAN") \
	X(THETA, "THETA") \

/* Used by touchpad, but not exposed to users */
#define X_TINSPIRE_KEYS_TP_SPECIAL \
	X(DOWNLEFT, "DOWNLEFT") \
	X(RIGHTDOWN, "RIGHTDOWN") \
	X(UPRIGHT, "UPRIGHT") \
	X(LEFTUP, "LEFTUP") \

enum tinspire_key {
#define X(name, us) TIN_KEY_ ## name,
	X_TINSPIRE_KEYS
#undef X
	TIN_KEY__LAST
};

extern const t_key * const TINspireKeyToNdlessKey[];
t_key *TINKeyToNdlessKey(enum tinspire_key key);
int TINKeyPressed(enum tinspire_key key, touchpad_report_t *tp_report);
#endif
