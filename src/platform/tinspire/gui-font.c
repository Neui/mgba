#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/png-io.h>
#include <mgba-util/vfs.h>
#include <mgba/core/log.h>
#include <assert.h>
#include "common.h"
#include "draw.h"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

extern unsigned char ext_font_start[];
extern unsigned char ext_font_end[];
extern unsigned int ext_font_size;
extern unsigned char ext_icons_start[];
extern unsigned char ext_icons_end[];
extern unsigned int ext_icons_size;

extern struct ImageBuf *screenBuffer;

enum {
	GLYPH_HEIGHT = 16,
	GLYPH_WIDTH = 16,
	GLYPHS_COLUMNS = 16,
	GLYPHS_ROWS = 8,
	GLYPHS_ROWS_EFFECTIVE = 6,
};

struct GUIFont {
	struct ImageBuf *bg;
	struct ImageBuf glyphs[GLYPHS_COLUMNS * GLYPHS_ROWS];
	struct ImageBuf icons[GUI_ICON_MAX];
	unsigned height;
};

static bool _loadImage(struct GUIFont *font, void* mem, size_t size,
		bool (*cb)(struct GUIFont*, uint32_t*, unsigned, unsigned)) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _loadImage(%p, %p, %u", font, mem, (unsigned)size);
	struct VFile* vf = VFileFromConstMemory(mem, size);
	if (!vf) {
		return false;
	}
	png_structp png = PNGReadOpen(vf, 0);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	bool success = false;
	if (png && info && end) {
		success = PNGReadHeader(png, info);
	}
	uint32_t* pixels = NULL;
	if (success) {
		unsigned height = png_get_image_height(png, info);
		unsigned width = png_get_image_width(png, info);
		pixels = malloc(sizeof(*pixels) * width * height); // ABGR
		if (pixels) {
			png_set_expand(png);
			png_read_update_info(png, info); // Apply expanding
			success = PNGReadPixels8(png, info, pixels, width, height, width * sizeof(*pixels));
			success = success && PNGReadFooter(png, end);
		} else {
			success = false;
		}
		if (success) {
			success = cb(font, pixels, width, height);
		}
	}
	PNGReadClose(png, info, end);
	free(pixels);
	vf->close(vf);
	return success;
}

static void _extractBuffer(uint32_t *data, unsigned iw, unsigned ih,
		unsigned ix, unsigned iy,
		color_t *buf_data, char* mask, unsigned bw, unsigned bh) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _extractBuffer(%p, %u, %u, %u, %u, %p, %u, %u)", data, iw, ih, ix, iy, buf_data, bw, bh);
	UNUSED(ih);
	unsigned final_x = ix + bw;
	unsigned final_y = iy + bh;
	unsigned bx = 0;
	unsigned by = 0;
	unsigned init_ix = ix;
	for (; iy < final_y; iy++, by++) {
		ix = init_ix;
		bx = 0;
		for (; ix < final_x; ix++, bx++) {
			uint32_t pixel = data[iy * iw + ix]; // ABGR
			uint16_t c = 0;
			c |= (pixel & 0x000000F8UL) << 8; // R
			c |= (pixel & 0x0000FC00UL) >> 5; // G
			c |= (pixel & 0x00F80000UL) >> 19; // B
			buf_data[by * bw + bx] = c;
			mask[by * bw + bx] = (pixel & 0xFF000000UL) >> 24;
		}
	}
}

static bool _initFont(struct GUIFont *font, uint32_t *data, unsigned w, unsigned h) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _initFont(%p, %p, %u, %u)", font, data, w, h);

	assert((w % GLYPH_WIDTH) == 0);
	assert((h % GLYPH_HEIGHT) == 0);

	for (int y = 0; y < GLYPHS_ROWS; y++) {
		for (int x = 0; x < GLYPHS_COLUMNS; x++) {
			unsigned char c = (char)(y * GLYPHS_COLUMNS + x);
			struct GUIFontGlyphMetric metric = defaultFontMetrics[c];
			if (metric.width == 0 || metric.height == 0) {
				continue;
			}

			color_t *buf_data = malloc(sizeof(*buf_data) * metric.width * metric.height);
			char *mask = malloc(metric.width * metric.height);
			if (!mask || !buf_data) {
				free(buf_data);
				free(mask);
				return false;
			}

			_extractBuffer(data, w, h,
					x * GLYPH_WIDTH + metric.padding.left,
					y * GLYPH_HEIGHT + metric.padding.top,
					buf_data, mask, metric.width, metric.height);

			font->glyphs[c].w = metric.width;
			font->glyphs[c].h = metric.height;
			font->glyphs[c].data = buf_data;
			font->glyphs[c].mask = mask;
		}
	}
	return true;
}

static bool _initIcons(struct GUIFont *font, uint32_t *data, unsigned w, unsigned h) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _initIcons(%p, %p, %u, %u)", font, data, w, h);

	for (enum GUIIcon i = 0; i < GUI_ICON_MAX; i++) {
		struct GUIIconMetric metric = defaultIconMetrics[i];
		assert(metric.width != 0);
		assert(metric.height != 0);

		color_t *buf_data = malloc(sizeof(*buf_data) * metric.width * metric.height);
		char *mask = malloc(metric.width * metric.height);
		if (!mask || !buf_data) {
			free(buf_data);
			free(mask);
			return false;
		}

		_extractBuffer(data, w, h, metric.x, metric.y,
				buf_data, mask, metric.width, metric.height);

		font->icons[i].w = metric.width;
		font->icons[i].h = metric.height;
		font->icons[i].data = buf_data;
		font->icons[i].mask = mask;
	}
	return true;
}
struct GUIFont* GUIFontCreate(void) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called GUIFontCreate()");
	struct GUIFont* font = malloc(sizeof(struct GUIFont));
	if (!font) {
		return NULL;
	}
	
	for (unsigned i = 0; i < ARRAY_COUNT(font->icons); i++) {
		font->icons[i] = (struct ImageBuf)EMPTY_IMAGEBUF;
	}
	for (unsigned i = 0; i < ARRAY_COUNT(font->glyphs); i++) {
		font->glyphs[i] = (struct ImageBuf)EMPTY_IMAGEBUF;
	}
	
	if (!_loadImage(font, ext_font_start, ext_font_size, _initFont)) {
		GUIFontDestroy(font);
		return NULL;
	}
	if (!_loadImage(font, ext_icons_start, ext_icons_size, _initIcons)) {
		GUIFontDestroy(font);
		return NULL;
	}

	font->bg = screenBuffer;
	font->height = 12;
	
	return font;
}

void GUIFontDestroy(struct GUIFont *font) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called GUIFontDestroy(%p)", font);
	if (!font) {
		return; /* Make it act like free() */
	}
	for (unsigned i = 0; i < ARRAY_COUNT(font->icons); i++) {
		free(font->icons[i].data);
		free(font->icons[i].mask);
	}
	for (unsigned i = 0; i < ARRAY_COUNT(font->glyphs); i++) {
		free(font->glyphs[i].data);
		free(font->glyphs[i].mask);
	}
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	return font->height;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	UNUSED(font);
	if (glyph > 127) {
		return 0;
	}
	return defaultFontMetrics[glyph].width;
}

void GUIFontIconMetrics(const struct GUIFont* font, enum GUIIcon icon, unsigned* w, unsigned* h) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called GUIFontIconMetrics(%p, %i, %p, %p)", font, icon, w, h);
	UNUSED(font);
	assert(icon < GUI_ICON_MAX);
	if (w) {
		*w = defaultIconMetrics[icon].width;
	}
	if (h) {
		*h = defaultIconMetrics[icon].height;
	}
}

void GUIFontDrawGlyph(const struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called GUIFontDrawGlyph(%p, %d, %d, %lx, %lx)", font, x, y, color, glyph);
	if (glyph <= 32 || glyph > 126) {
		return; // Don't draw unsupported glyphs
	}
	
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];
	struct ImageBuf buf = font->glyphs[glyph];
	DrawOnTinted(*font->bg, buf,
			x, y - font->height + metric.padding.top,
			M_RGB8_TO_RGB565(color), color >> 24);
}

void GUIFontDrawIcon(const struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called GUIFontDrawIcon(%p, %i, %i, %i, %i, %lx, %i)", font, x, y, align, orient, color, icon);
	if (icon >= GUI_ICON_MAX) {
		return;
	}

	struct GUIIconMetric metric = defaultIconMetrics[icon];
	struct ImageBuf buf = font->icons[icon];
	switch (align & GUI_ALIGN_HCENTER) {
	case GUI_ALIGN_HCENTER:
		x -= metric.width / 2;
		break;
	case GUI_ALIGN_RIGHT:
		x -= metric.width;
		break;
	}
	switch (align & GUI_ALIGN_VCENTER) {
	case GUI_ALIGN_VCENTER:
		y -= metric.height / 2;
		break;
	case GUI_ALIGN_BOTTOM:
		y -= metric.height;
		break;
	}
	switch (orient) {
	case GUI_ORIENT_HMIRROR:
		// TODO: Implement HMIRROR. Fall through for now
	case GUI_ORIENT_VMIRROR:
		// TODO: Implement VMIRROR. Fall through for now
	case GUI_ORIENT_0:
	default:
		// TODO: Rotation // Originally from 3ds port
		DrawOnTinted(*font->bg, buf, x, y, M_RGB8_TO_RGB565(color), color >> 24);
		break;
	}
}

void GUIFontDrawIconSize(const struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called GUIFontDrawIconSize(%p, %i, %i, %i, %i, %lx, %i)", font, x, y, w, h, color, icon);
	if (icon >= GUI_ICON_MAX) {
		mLOG(GUI_TINSPIRE, DEBUG, "Trying to draw unsupported icon! %i %x",
				icon, icon);
		return;
	}

	UNUSED(color); // TODO: Tinted version of DrawOnresized

	struct ImageBuf buf = font->icons[icon];
	
	if (w == 0 || h == 0) {
		//mLOG(GUI_TINSPIRE, DEBUG, "Trying to draw with w=%i h=%i", w, h);
		return;
	}
	
	assert(w == (int)buf.w); // Resizing is currently not supported
	assert(h == (int)buf.h); // Resizing is currently not supported
	
	DrawOnResized(*font->bg, buf, x, y, w, h);
}
