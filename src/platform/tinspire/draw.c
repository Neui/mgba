#include "draw.h"
#include <string.h>

struct _drawInfo {
	unsigned forex;
	unsigned forey;
	unsigned forelx;
	unsigned forely;
	unsigned bgx;
	unsigned bgy;
};

static inline struct _drawInfo _drawInfo(struct ImageBuf bg, struct ImageBuf fore, int x, int y) {
	struct _drawInfo info;
	// Start inside image if it starts out of bounds
	info.forex = (x < 0 ? 0 - x : 0);
	info.forey = (y < 0 ? 0 - y : 0);
	// Now correct x and y
	x = info.bgx = (x < 0 ? 0 : x);
	y = info.bgy = (y < 0 ? 0 : y);
	// End inside image if it ends out of bounds
	unsigned forew = (fore.w < (bg.w - x) ? fore.w : bg.w - x);
	unsigned foreh = (fore.h < (bg.h - y) ? fore.h : bg.h - y);
	info.forelx = info.forex + forew;
	info.forely = info.forey + foreh;
	return info;
}

void DrawClear(struct ImageBuf bg, color_t fill) {
	if ((fill & 0xFF) == ((fill >> 8) & 0xFF)) {
		memset(bg.data, fill & 0xFF, DrawMemSize(bg));
	} else {
		for (unsigned y = 0; y < bg.h; y++) {
			for (unsigned x = 0; x < bg.w; x++) {
				DrawVar(bg, x, y) = fill;
			}
		}
	}
	if (bg.mask) {
		memset(bg.mask, 1, bg.w * bg.h);
	}
}

void DrawOn(struct ImageBuf bg, struct ImageBuf fore, int x, int y) {
	struct _drawInfo info = _drawInfo(bg, fore, x, y);
	unsigned forex, forey;
	
	if (fore.mask) {
		for (forey = info.forey; forey < info.forely; forey++) {
			for (forex = info.forex; forex < info.forelx; forex++) {
				if (!DrawVarMask(fore, forex, forey))
					continue;
				DrawVar(bg, x + forex, y + forey) = DrawVar(fore, forex, forey);
			}
		}
	} else { // fast path
		unsigned forew = info.forelx - info.forex;
		for (forey = info.forey; forey < info.forely; forey++) {
			memcpy(&DrawVar(bg, x, y + forey), &DrawVar(fore, info.forex, forey),
					sizeof(*bg.data) * forew);
		}
	}
}

void DrawOnResized(struct ImageBuf bg, struct ImageBuf fore, int x, int y,
		int newWidth, int newHeight) {
	struct _drawInfo info = _drawInfo(bg, fore, x, y);
	unsigned xratio = (info.forelx - info.forex) * 0x10000 / newWidth;
	unsigned yratio = (info.forely - info.forey) * 0x10000 / newHeight;
	
	for (y = info.bgy; y < (int)info.bgy + newHeight; y++) {
		unsigned ry = info.forey + (y - info.bgy) * yratio / 0x10000;
		for (x = info.bgx; x < (int)info.bgx + newWidth; x++) {
			unsigned rx = info.forex + (x - info.bgx) * xratio / 0x10000;
			if (fore.mask && !DrawVarMask(fore, rx, ry))
				continue;
			DrawVar(bg, x, y) = DrawVar(fore, rx, ry);
		}
	}
}

void DrawFade(struct ImageBuf buf) {
	for (unsigned y = 0; y < buf.h; y++) {
		for (unsigned x = 0; x < buf.w; x++) {
			color_t c = DrawVar(buf, x, y);
			DrawVar(buf, x, y) = (c >> 1) & 0x7BEF;
		}
	}
}

void DrawOnTinted(struct ImageBuf bg, struct ImageBuf fore, int x, int y, color_t tint, unsigned alpha) {
	struct _drawInfo info = _drawInfo(bg, fore, x, y);
	unsigned forex, forey;
	float a = (float)alpha / 255.0f;
	for (forey = info.forey; forey < info.forely; forey++) {
		for (forex = info.forex; forex < info.forelx; forex++) {
			if (fore.mask && !DrawVarMask(fore, forex, forey))
				continue;
			color_t c = DrawVar(fore, forex, forey);
			color_t bgc = DrawVar(bg, x + forex, y + forey);
			color_t new = 0
				| (unsigned)(M_R565(c) + (M_R565(tint) - M_R565(c)) * 0.3f)
				| (unsigned)(M_G565(c) + (M_G565(tint) - M_G565(c)) * 0.3f) << 5
				| (unsigned)(M_B565(c) + (M_B565(tint) - M_B565(c)) * 0.3f) << 11;
			DrawVar(bg, x + forex, y + forey) = 0
				| (int)(M_R565(bgc) + (M_R565(new) - M_R565(bgc)) * a)
				| (int)(M_G565(bgc) + (M_G565(new) - M_G565(bgc)) * a) << 5
				| (int)(M_B565(bgc) + (M_B565(new) - M_B565(bgc)) * a) << 11;
		}
	}
}


void DrawOnTransparent(struct ImageBuf bg, struct ImageBuf fore, int x, int y, unsigned alpha) {
	struct _drawInfo info = _drawInfo(bg, fore, x, y);
	unsigned forex, forey;
	float a = (float)alpha / 255.0f;
	for (forey = info.forey; forey < info.forely; forey++) {
		for (forex = info.forex; forex < info.forelx; forex++) {
			if (fore.mask && !DrawVarMask(fore, forex, forey))
				continue;
			color_t c = DrawVar(fore, forex, forey);
			color_t bgc = DrawVar(bg, x + forex, y + forey);
			DrawVar(bg, x + forex, y + forey) = 0
				| (unsigned)(M_R565(bgc) + (M_R565(c) - M_R565(bgc)) * a)
				| (unsigned)(M_G565(bgc) + (M_G565(c) - M_G565(bgc)) * a) << 5
				| (unsigned)(M_B565(bgc) + (M_B565(c) - M_B565(bgc)) * a) << 11;
		}
	}
}
