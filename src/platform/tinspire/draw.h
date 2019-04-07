#ifndef TINSPIRE_DRAW_H
#define TINSPIRE_DRAW_H
#include <mgba/core/core.h>

struct ImageBuf {
	unsigned w;
	unsigned h;
	color_t *data;
	char *mask;
};

#define EMPTY_IMAGEBUF {0, 0, NULL, NULL}

#define DrawVar(buf, x, y) ((buf).data[(buf).w * (y) + (x)])
#define DrawVarMask(buf, x, y) ((buf).mask[(buf).w * (y) + (x)])
#define DrawMemSize(buf) (sizeof((buf).data[0]) * (buf).w * (buf).h)

void DrawClear(struct ImageBuf bg, color_t fill);

void DrawOn(struct ImageBuf bg, struct ImageBuf fore, int x, int y);
void DrawOnResized(struct ImageBuf bg, struct ImageBuf fore, int x, int y,
		int newWidth, int newHeight);
void DrawFade(struct ImageBuf buf);
void DrawOnTinted(struct ImageBuf bg, struct ImageBuf fore, int x, int y, color_t tint, unsigned alpha);
void DrawOnTransparent(struct ImageBuf bg, struct ImageBuf fore, int x, int y, unsigned alpha);
#endif
