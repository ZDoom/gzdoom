#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "doomtype.h"

struct brokenlines_s {
	int width;
	char *string;
};
typedef struct brokenlines_s brokenlines_t;

#define CR_BRICK			0
#define CR_TAN				1
#define CR_GRAY				2
#define CR_GREY				2
#define CR_GREEN			3
#define CR_BROWN			4
#define CR_GOLD				5
#define CR_RED				6
#define CR_BLUE				7
#define CR_ORANGE			8
#define NUM_TEXT_COLORS		9

#define TEXTCOLOR_BRICK		"\x8aA"
#define TEXTCOLOR_TAN		"\x8aB"
#define TEXTCOLOR_GRAY		"\x8aC"
#define TEXTCOLOR_GREY		"\x8aC"
#define TEXTCOLOR_GREEN		"\x8aD"
#define TEXTCOLOR_BROWN		"\x8aE"
#define TEXTCOLOR_GOLD		"\x8aF"
#define TEXTCOLOR_RED		"\x8aG"
#define TEXTCOLOR_BLUE		"\x8aH"
#define TEXTCOLOR_ORANGE	"\x8aI"

#define TEXTCOLOR_NORMAL	"\x8a-"
#define TEXTCOLOR_BOLD		"\x8a+"

// Output a line of text using the console font
void V_PrintStr (int x, int y, const byte *str, int count);
void V_PrintStr2 (int x, int y, const byte *str, int count);

// Output some text with wad heads-up font
void V_DrawText (int color, int x, int y, const byte *str);
void V_DrawTextLuc (int color, int x, int y, const byte *str);
void V_DrawTextClean (int color, int x, int y, const byte *str); // Does not adjust x and y
void V_DrawTextCleanLuc (int color, int x, int y, const byte *str); // ditto
void V_DrawTextCleanMove (int color, int x, int y, const byte *str); // This one does

int V_StringWidth (const byte *str);

brokenlines_t *V_BreakLines (int maxwidth, const byte *str);
void V_FreeBrokenLines (brokenlines_t *lines);

void V_InitConChars (byte transcolor);

#endif //__V_TEXT_H__