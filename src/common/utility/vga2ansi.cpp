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

// Want to use this in another project?
// This file can be renamed to .c or .cpp and it will still work.

// this is a utf-8 file
#pragma execution_character_set("utf-8")

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

const char *ansi_esc[2] = {"\x1b[", ";"};
const char *ansi_end[2] = {"", "m"};

// Map DOS color to ANSI escape code
const char *ansi_fg[16] = 
{
	"30", "34", "32", "36", "31", "35", "33", "37", 
	"90", "94", "92", "96", "91", "95", "93", "97"
};
// Only standard backgrounds (no bright backgrounds in classic ANSI)
const char *ansi_bg[8] = 
{
	"40", "44", "42", "46",
	"41", "45", "43", "47"
};
// ANSI codes for truecolor DOS colors
const char *ansi_tc_fg[16] = 
{
	"38;2;0;0;0", "38;2;0;0;170", "38;2;0;170;0", "38;2;0;170;170",
	"38;2;170;0;0", "38;2;170;0;170", "38;2;170;85;0", "38;2;170;170;170",
	"38;2;85;85;85", "38;2;85;85;255", "38;2;85;255;0", "38;2;85;255;255",
	"38;2;255;85;85", "38;2;255;85;255", "38;2;255;255;85", "38;2;255;255;255"
};
const char *ansi_tc_bg[8] = 
{
	"48;2;0;0;0", "48;2;0;0;170", "48;2;0;170;0", "48;2;0;170;170",
	"48;2;170;0;0", "48;2;170;0;170", "48;2;170;85;0", "48;2;170;170;170"
};

const char *ansi_flash[2] = { "25", "5" };

const char *cp437_to_utf8[256] =
{
	" ", "☺", "☻", "♥", "♦", "♣", "♠", "•", "◘", "○", "◙", "♂", "♀", "♪", "♫", "☼", // 0 - 15
	"►", "◄", "↕", "‼", "¶", "§", "▬", "↨", "↑", "↓", "→", "←", "∟", "↔", "▲", "▼", // 16 - 31
	" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "‑", ".", "/", // 32 - 47
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?", // 48 - 63
	"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", // 64 - 79
	"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_", // 80 - 95
	"`", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", // 96 - 111
	"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "⌂", // 112 - 127
	"Ç", "ü", "é", "â", "ä", "à", "å", "ç", "ê", "ë", "è", "ï", "î", "ì", "Ä", "Å", // 128 - 143
	"É", "æ", "Æ", "ô", "ö", "ò", "û", "ù", "ÿ", "Ö", "Ü", "¢", "£", "¥", "₧", "ƒ", // 144 - 159
	"á", "í", "ó", "ú", "ñ", "Ñ", "ª", "º", "¿", "⌐", "¬", "½", "¼", "¡", "«", "»", // 160 - 175
	"░", "▒", "▓", "│", "┤", "╡", "╢", "╖", "╕", "╣", "║", "╗", "╝", "╜", "╛", "┐", // 176 - 191
	"└", "┴", "┬", "├", "─", "┼", "╞", "╟", "╚", "╔", "╩", "╦", "╠", "═", "╬", "╧", // 192 - 207
	"╨", "╤", "╥", "╙", "╘", "╒", "╓", "╫", "╪", "┘", "┌", "█", "▄", "▌", "▐", "▀", // 208 - 223
	"α", "ß", "Γ", "π", "Σ", "σ", "µ", "τ", "Φ", "Θ", "Ω", "δ", "∞", "φ", "ε", "∩", // 224 - 239
	"≡", "±", "≥", "≤", "⌠", "⌡", "÷", "≈", "°", "∙", "·", "√", "ⁿ", "²", "■", " " // 240 - 255
};

bool is_ansi_open = false;

void ansi_open()
{
	fputs(ansi_esc[is_ansi_open], stdout);
	is_ansi_open = true;
}

void ansi_close()
{
	fputs(ansi_end[is_ansi_open], stdout);
	is_ansi_open = false;
}

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
			bool spacer = cp437_to_utf8[ch][0] == 160;

			// Output color if changed
			if ((fg != last_fg) && !spacer)
			{
				ansi_open();
				if (truecolor)
					fputs(ansi_tc_fg[fg], stdout);
				else
					fputs(ansi_fg[fg], stdout);
				last_fg = fg;
			}
			if (bg != last_bg) 
			{
				ansi_open();
				if (truecolor)
					fputs(ansi_tc_bg[bg], stdout);
				else
					fputs(ansi_bg[bg], stdout);
				last_bg = bg;
			}
			if (blink != last_blink)
			{
				ansi_open();
				fputs(ansi_flash[blink], stdout);
				last_blink = blink;
			}
			ansi_close();

			// Output character, convert CP437 to UTF-8
			fputs(cp437_to_utf8[ch], stdout);
		}
		fputs("\x1b[0m\n", stdout); // Reset colors and end the line
	}
}

