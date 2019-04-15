#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <assert.h>
#include "draw.h"
#include "lcd.h"
#include "libndls.h"

const struct LCDInfo lcdinfos[] = {
	/* type            w    h  bw  r  g  b */
	{SCR_320x240_4,   320, 240, 4, 0, 0, 0}, /* 4bit grayscale, classic */
	{SCR_320x240_8,   320, 240, 8, 0, 0, 0}, /* "8bit paletted mode" ??? */
	{SCR_320x240_16,  320, 240, 0, 4, 4, 4}, /* RGB444 */
	{SCR_320x240_565, 320, 240, 0, 5, 6, 5}, /* RGB565, CX before HW-W */
	{SCR_240x320_565, 240, 320, 0, 5, 6, 5}, /* CX on HW-W */
	{SCR_320x240_555, 240, 320, 0, 5, 5, 5}, /* (Unknown) */
	{SCR_240x320_555, 240, 320, 0, 5, 5, 5}, /* (Unknown) */
	{SCR_TYPE_INVALID,  0,   0, 0, 0, 0, 0}  /* Terminator */
};

struct LCDInfo GetLCDInfo(const scr_type_t type) {
	const struct LCDInfo *lcdinfo = lcdinfos;
	for (; lcdinfo->type != SCR_TYPE_INVALID
			&& lcdinfo->type != type; lcdinfo++);
	return *lcdinfo;
}

bool LCDConvertAndBlit(struct LCDInfo info, struct ImageBuf buf, char *out) {
	assert(info.width == buf.w);
	assert(info.height == buf.h);
	if (info.redBBP == 5 && info.greenBBP == 6 && info.blueBBP == 5) {
		lcd_blit(buf.data, info.type);
		return true;
	}
	
	if (info.redBBP == 5 && info.greenBBP == 5 && info.blueBBP == 5) {
		for (unsigned i = 0; i < DrawMemSize(buf); i++) {
			color_t c = DrawVar(buf, i, 1);
			((color_t*)out)[info.width + i] = (c & 0x1F) | ((c > 1) & 0x7FE0);
		}
	} else if (info.redBBP == 4 && info.greenBBP == 4 && info.blueBBP == 4) {
		for (unsigned i = 0; i < DrawMemSize(buf); i++) {
			color_t c = DrawVar(buf, i, 1);
			((color_t*)out)[info.width + i] = 0
				| (M_R565(c) >> 1)
				| ((M_G565(c) & 0xF) << 4)
				| ((M_B565(c) & 0xF) << 8);
		}
	} else if (info.grayBBP > 0) {
		for (unsigned i = 0; i < DrawMemSize(buf); i++) {
			/* We need to be fast, so don't do complicated stuff */
			color_t c = DrawVar(buf, i, 1);
			out[i] = (((M_R565(c) << 1) + M_G565(c) + (M_B565(c) << 1)) / 3);
			if ((6 - info.grayBBP) > 0) {
				out[i] >>= 6 - info.grayBBP;
			} else {
				out[i] <<= info.grayBBP - 6;
			}
		}
	} else {
		return false;
	}
	
	lcd_blit(out, info.type);
	return true;
}

