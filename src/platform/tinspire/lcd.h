#ifndef TINSPIRE_LCD_H
#define TINSPIRE_LCD_H
#include "libndls.h"

#if !defined(COLOR_16_BIT) || !defined(COLOR_5_6_5)
#error "TI Nspire platform requires -DCOLOR_16_BIT and -DCOLOR_5_6_5"
#endif

struct ImageBuf; /* See draw.h */

struct LCDInfo {
	scr_type_t type;
	unsigned width, height;
	int grayBBP; /* Gray bits ber pixel */
	int redBBP, greenBBP, blueBBP;
};

extern const struct LCDInfo lcdinfos[];

bool LCDConvertAndBlit(struct LCDInfo info, struct ImageBuf buf, char *out);

struct LCDInfo GetLCDInfo(const scr_type_t type);
#endif
