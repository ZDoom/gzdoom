/*
** m_argv.cpp
** Manages command line arguments
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <string.h>
#include "m_argv.h"
#include "cmdlib.h"

IMPLEMENT_CLASS (DArgs)

DArgs::DArgs ()
{
	m_ArgC = 0;
	m_ArgV = NULL;
}

DArgs::DArgs (int argc, char **argv)
{
	CopyArgs (argc, argv);
}

DArgs::DArgs (const DArgs &other)
{
	CopyArgs (other.m_ArgC, other.m_ArgV);
}


DArgs::~DArgs ()
{
	FlushArgs ();
}

DArgs &DArgs::operator= (const DArgs &other)
{
	FlushArgs ();
	CopyArgs (other.m_ArgC, other.m_ArgV);
	return *this;
}

void DArgs::SetArgs (int argc, char **argv)
{
	FlushArgs ();
	CopyArgs (argc, argv);
}

void DArgs::CopyArgs (int argc, char **argv)
{
	int i;

	m_ArgC = argc;
	m_ArgV = new char *[argc];
	for (i = 0; i < argc; i++)
		m_ArgV[i] = copystring (argv[i]);
}

void DArgs::FlushArgs ()
{
	int i;

	for (i = 0; i < m_ArgC; i++)
		delete[] m_ArgV[i];
	delete[] m_ArgV;
	m_ArgC = 0;
	m_ArgV = NULL;
}

//
// CheckParm
// Checks for the given parameter in the program's command line arguments.
// Returns the argument number (1 to argc-1) or 0 if not present
//
int DArgs::CheckParm (const char *check, int start) const
{
	for (int i = start; i < m_ArgC; ++i)
		if (!stricmp (check, m_ArgV[i]))
			return i;

	return 0;
}

char *DArgs::CheckValue (const char *check) const
{
	int i = CheckParm (check);

	if (i > 0 && i < m_ArgC - 1)
		return m_ArgV[i+1][0] != '+' && m_ArgV[i+1][0] != '-' ? m_ArgV[i+1] : NULL;
	else
		return NULL;
}

char *DArgs::GetArg (int arg) const
{
	if (arg >= 0 && arg < m_ArgC)
		return m_ArgV[arg];
	else
		return NULL;
}

char **DArgs::GetArgList (int arg) const
{
	if (arg >= 0 && arg < m_ArgC)
		return &m_ArgV[arg];
	else
		return NULL;
}

int DArgs::NumArgs () const
{
	return m_ArgC;
}

void DArgs::AppendArg (const char *arg)
{
	char **temp = new char *[m_ArgC + 1];
	if (m_ArgV)
	{
		memcpy (temp, m_ArgV, sizeof(*m_ArgV) * m_ArgC);
		delete[] m_ArgV;
	}
	temp[m_ArgC] = copystring (arg);
	m_ArgV = temp;
	m_ArgC++;
}

DArgs *DArgs::GatherFiles (const char *param, const char *extension, bool acceptNoExt) const
{
	DArgs *out = new DArgs;
	int i;
	size_t extlen = strlen (extension);

	if (extlen > 0)
	{
		for (i = 1; i < m_ArgC && *m_ArgV[i] != '-' && *m_ArgV[i] != '+'; i++)
		{
			size_t len = strlen (m_ArgV[i]);
			if (len >= extlen && stricmp (m_ArgV[i] + len - extlen, extension) == 0)
				out->AppendArg (m_ArgV[i]);
			else if (acceptNoExt)
			{
				const char *src = m_ArgV[i] + len - 1;

				while (src != m_ArgV[i] && *src != '/'
			#ifdef _WIN32
					&& *src != '\\'
			#endif
					)
				{
					if (*src == '.')
						goto morefor;					// it has an extension
					src--;
				}
				out->AppendArg (m_ArgV[i]);
morefor:
				;
			}
		}
	}
	if (param != NULL)
	{
		i = 1;
		while (0 != (i = CheckParm (param, i)))
		{
			for (++i; i < m_ArgC && *m_ArgV[i] != '-' && *m_ArgV[i] != '+'; ++i)
				out->AppendArg (m_ArgV[i]);
		}
	}
	return out;
}
