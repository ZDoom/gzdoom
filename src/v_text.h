/*
** v_text.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "doomtype.h"
#include "v_font.h"

struct brokenlines_s
{
	short width;
	byte nlterminated;
	byte pad;
	char *string;
};
typedef struct brokenlines_s brokenlines_t;

#define TEXTCOLOR_ESCAPE	'\x1c'

#define TEXTCOLOR_BRICK		"\x1c""A"
#define TEXTCOLOR_TAN		"\x1c""B"
#define TEXTCOLOR_GRAY		"\x1c""C"
#define TEXTCOLOR_GREY		"\x1c""C"
#define TEXTCOLOR_GREEN		"\x1c""D"
#define TEXTCOLOR_BROWN		"\x1c""E"
#define TEXTCOLOR_GOLD		"\x1c""F"
#define TEXTCOLOR_RED		"\x1c""G"
#define TEXTCOLOR_BLUE		"\x1c""H"
#define TEXTCOLOR_ORANGE	"\x1c""I"
#define TEXTCOLOR_WHITE		"\x1c""J"
#define TEXTCOLOR_YELLOW	"\x1c""K"

#define TEXTCOLOR_NORMAL	"\x1c-"
#define TEXTCOLOR_BOLD		"\x1c+"

brokenlines_t *V_BreakLines (int maxwidth, const byte *str, bool keepspace=false);
void V_FreeBrokenLines (brokenlines_t *lines);
inline brokenlines_t *V_BreakLines (int maxwidth, const char *str, bool keepspace=false)
 { return V_BreakLines (maxwidth, (const byte *)str, keepspace); }

#endif //__V_TEXT_H__
