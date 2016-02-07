#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <new>

#include "src/codegen/label.h"
#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/parse/scanner.h"
#include "src/util/counter.h"

// used by Scanner::fatal_at and Scanner::fatalf
#if defined(_MSC_VER) && !defined(vsnprintf)
#    define vsnprintf _vsnprintf
#endif

namespace re2c {

const uint32_t Scanner::BSIZE = 8192;

ScannerState::ScannerState ()
	: tok (NULL)
	, ptr (NULL)
	, cur (NULL)
	, pos (NULL)
	, ctx (NULL)
	, bot (NULL)
	, lim (NULL)
	, top (NULL)
	, eof (NULL)
	, tchar (0)
	, tline (0)
	, cline (1)
	, in_parse (false)
	, lexer_state (LEX_NORMAL)
{}

ScannerState::ScannerState (const ScannerState & s)
	: tok (s.tok)
	, ptr (s.ptr)
	, cur (s.cur)
	, pos (s.pos)
	, ctx (s.ctx)
	, bot (s.bot)
	, lim (s.lim)
	, top (s.top)
	, eof (s.eof)
	, tchar (s.tchar)
	, tline (s.tline)
	, cline (s.cline)
	, in_parse (s.in_parse)
	, lexer_state (s.lexer_state)
{}

ScannerState & ScannerState::operator = (const ScannerState & s)
{
	this->~ScannerState ();
	new (this) ScannerState (s);
	return * this;
}

Scanner::Scanner (Input & i, OutputFile & o)
	: ScannerState ()
	, in (i)
	, out (o)
{}

void Scanner::fill (uint32_t need)
{
	if(!eof)
	{
		/* Do not get rid of anything when rFlag is active. Otherwise
		 * get rid of everything that was already handedout. */
		if (!opts->rFlag)
		{
			const ptrdiff_t diff = tok - bot;
			if (diff > 0)
			{
				const size_t move = static_cast<size_t> (top - tok);
				memmove (bot, tok, move);
				tok -= diff;
				ptr -= diff;
				cur -= diff;
				pos -= diff;
				lim -= diff;
				ctx -= diff;
			}
		}
		/* In crease buffer size. */
		if (BSIZE > need)
		{
			need = BSIZE;
		}
		if (static_cast<uint32_t> (top - lim) < need)
		{
			const size_t copy = static_cast<size_t> (lim - bot);
			char * buf = new char[copy + need];
			if (!buf)
			{
				fatal("Out of memory");
			}
			memcpy (buf, bot, copy);
			tok = &buf[tok - bot];
			ptr = &buf[ptr - bot];
			cur = &buf[cur - bot];
			pos = &buf[pos - bot];
			lim = &buf[lim - bot];
			top = &lim[need];
			ctx = &buf[ctx - bot];
			delete [] bot;
			bot = buf;
		}
		/* Append to buffer. */
		const size_t have = fread (lim, 1, need, in.file);
		if (have != need)
		{
			eof = &lim[have];
			*eof++ = '\0';
		}
		lim += have;
	}
}

void Scanner::set_in_parse(bool new_in_parse)
{
	in_parse = new_in_parse;
}

void Scanner::fatal_at(uint32_t line, ptrdiff_t ofs, const char *msg) const
{
	std::cerr << "re2c: error: "
		<< "line " << line << ", column " << (tchar + ofs + 1) << ": "
		<< msg << std::endl;
	exit(1);
}

void Scanner::fatal(ptrdiff_t ofs, const char *msg) const
{
	fatal_at(in_parse ? tline : cline, ofs, msg);
}

void Scanner::fatalf_at(uint32_t line, const char* fmt, ...) const
{
	char szBuf[4096];

	va_list args;
	
	va_start(args, fmt);
	vsnprintf(szBuf, sizeof(szBuf), fmt, args);
	va_end(args);
	
	szBuf[sizeof(szBuf)-1] = '0';
	
	fatal_at(line, 0, szBuf);
}

void Scanner::fatalf(const char *fmt, ...) const
{
	char szBuf[4096];

	va_list args;
	
	va_start(args, fmt);
	vsnprintf(szBuf, sizeof(szBuf), fmt, args);
	va_end(args);
	
	szBuf[sizeof(szBuf)-1] = '0';
	
	fatal(szBuf);
}

Scanner::~Scanner()
{
	delete [] bot;
}

void Scanner::reuse()
{
	out.label_counter.reset ();
	last_fill_index = 0;
	bWroteGetState = false;
	bWroteCondCheck = false;
	opts.reset_mapCodeName ();
}

void Scanner::restore_state(const ScannerState& state)
{
	ptrdiff_t diff = bot - state.bot;
	char *old_bot = bot;
	char *old_lim = lim;
	char *old_top = top;
	char *old_eof = eof;
	*(ScannerState*)this = state;
	if (diff)
	{
		tok -= diff;
		ptr -= diff;
		cur -= diff;
		pos -= diff;
		ctx -= diff;		
		bot = old_bot;
		lim = old_lim;
		top = old_top;
		eof = old_eof;
	}
}

} // namespace re2c
