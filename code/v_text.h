#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "doomtype.h"
#include "v_font.h"

struct brokenlines_s {
	short width;
	byte nlterminated;
	byte pad;
	char *string;
};
typedef struct brokenlines_s brokenlines_t;

#define TEXTCOLOR_ESCAPE	'\x81'

#define TEXTCOLOR_BRICK		"\x81""A"
#define TEXTCOLOR_TAN		"\x81""B"
#define TEXTCOLOR_GRAY		"\x81""C"
#define TEXTCOLOR_GREY		"\x81""C"
#define TEXTCOLOR_GREEN		"\x81""D"
#define TEXTCOLOR_BROWN		"\x81""E"
#define TEXTCOLOR_GOLD		"\x81""F"
#define TEXTCOLOR_RED		"\x81""G"
#define TEXTCOLOR_BLUE		"\x81""H"
#define TEXTCOLOR_ORANGE	"\x81""I"
#define TEXTCOLOR_WHITE		"\x81""J"
#define TEXTCOLOR_YELLOW	"\x81""k"

#define TEXTCOLOR_NORMAL	"\x81-"
#define TEXTCOLOR_BOLD		"\x81+"

brokenlines_t *V_BreakLines (int maxwidth, const byte *str, bool keepspace=false);
void V_FreeBrokenLines (brokenlines_t *lines);
inline brokenlines_t *V_BreakLines (int maxwidth, const char *str, bool keepspace=false)
 { return V_BreakLines (maxwidth, (const byte *)str, keepspace); }

#endif //__V_TEXT_H__