#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "doomtype.h"

struct brokenlines_s {
	int width;
	char *string;
};
typedef struct brokenlines_s brokenlines_t;


// Output a line of text using the console font
void V_PrintStr (int x, int y, byte *str, int count, byte ormask);
void V_PrintStr2 (int x, int y, byte *str, int count, byte ormask);

// Output some text with wad heads-up font
void V_DrawText (int x, int y, byte *str);
void V_DrawWhiteText (int x, int y, byte *str);
void V_DrawRedText (int x, int y, byte *str);
void V_DrawTextClean (int x, int y, byte *str);		// <- Does not adjust x and y
void V_DrawWhiteTextClean (int x, int y, byte *str);
void V_DrawRedTextClean (int x, int y, byte *str);

int V_StringWidth (byte *str);

brokenlines_t *V_BreakLines (int maxwidth, const byte *str);
void V_FreeBrokenLines (brokenlines_t *lines);

void V_InitConChars (byte transcolor);

#endif //__V_TEXT_H__