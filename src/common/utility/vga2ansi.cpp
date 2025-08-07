// Copyright (c) 2025 Rachael Alexanderson
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32
// windows platform code uses special stdout handling, let's make sure to use that
#include <windows.h>
extern HANDLE StdOut;
static void CPrint(const char* in)
{
	DWORD bytes_written;
	if (!StdOut)
		return;
	WriteFile(StdOut, in, strlen(in), &bytes_written, NULL);
}
#else
static void CPrint(const char* in)
{
	fputs(in, stdout);
}
#endif

static const char *ansi_esc[2] = {"\x1b[", ";"};
static const char *ansi_end[2] = {"", "m"};

// Map DOS color to ANSI escape code
static const char *ansi_fg[16] = 
{
	"30", "34", "32", "36", "31", "35", "33", "37", 
	"90", "94", "92", "96", "91", "95", "93", "97"
};
// Only standard backgrounds (no bright backgrounds in classic ANSI)
static const char *ansi_bg[8] = 
{
	"40", "44", "42", "46",
	"41", "45", "43", "47"
};
// ANSI codes for truecolor DOS colors
static const char *ansi_tc_fg[16] = 
{
	"38;2;0;0;0", "38;2;0;0;170", "38;2;0;170;0", "38;2;0;170;170",
	"38;2;170;0;0", "38;2;170;0;170", "38;2;170;85;0", "38;2;170;170;170",
	"38;2;85;85;85", "38;2;85;85;255", "38;2;85;255;0", "38;2;85;255;255",
	"38;2;255;85;85", "38;2;255;85;255", "38;2;255;255;85", "38;2;255;255;255"
};
static const char *ansi_tc_bg[8] = 
{
	"48;2;0;0;0", "48;2;0;0;170", "48;2;0;170;0", "48;2;0;170;170",
	"48;2;170;0;0", "48;2;170;0;170", "48;2;170;85;0", "48;2;170;170;170"
};

static const char *ansi_flash[2] = { "25", "5" };

inline void ansi_ctrl(bool open)
{
	static bool is_ansi_open = false;
	if (open)
		CPrint(ansi_esc[is_ansi_open]);
	else
		CPrint(ansi_end[is_ansi_open]);
	is_ansi_open = open;
}

void ibm437_to_utf8(char* result, char in);

void vga_to_ansi(const uint8_t *buf) 
{
#ifdef _WIN32
	bool truecolor = true;
#else
	const char *ct = getenv("COLORTERM");
	bool truecolor = ct && (strcmp(ct, "truecolor") || strcmp(ct, "24bit"));
#endif

	for (int row = 0; row < 25; ++row) 
	{
		int last_fg = -1, last_bg = -1;
		bool last_blink = false;
		for (int col = 0; col < 80; ++col) 
		{
			int off = (row * 80 + col) * 2;
			uint8_t ch = buf[off];
			uint8_t attr = buf[off + 1];
			int fg = attr & 0x0F;
			int bg = (attr >> 4) & 0x07;
			bool blink = !!(attr & 0x80);
			bool spacer = (ch == 0) || (ch == 32) || (ch == 255);

			// Output color if changed
			if ((fg != last_fg) && !spacer)
			{
				ansi_ctrl(1);
				if (truecolor)
					CPrint(ansi_tc_fg[fg]);
				else
					CPrint(ansi_fg[fg]);
				last_fg = fg;
			}
			if (bg != last_bg) 
			{
				ansi_ctrl(1);
				if (truecolor)
					CPrint(ansi_tc_bg[bg]);
				else
					CPrint(ansi_bg[bg]);
				last_bg = bg;
			}
			if (blink != last_blink)
			{
				ansi_ctrl(1);
				CPrint(ansi_flash[blink]);
				last_blink = blink;
			}
			ansi_ctrl(0);

			// Output character, convert CP437 to UTF-8
			if (spacer)
				CPrint(" ");
			else
			{
				char result[4];
				ibm437_to_utf8(result, ch);
				CPrint(result);
			}
		}
		CPrint("\x1b[0m\n"); // Reset colors and end the line
	}
}

