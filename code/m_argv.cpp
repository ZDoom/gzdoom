// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

#include <string.h>
#include "m_argv.h"
#include "cmdlib.h"

IMPLEMENT_CLASS (DArgs, DObject)

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
int DArgs::CheckParm (const char *check) const
{
	int i;

	for (i = 1; i < m_ArgC; i++)
		if (!stricmp (check, m_ArgV[i]))
			return i;

	return 0;
}

char *DArgs::CheckValue (const char *check) const
{
	int i = CheckParm (check);

	if (i > 0 && i < m_ArgC - 1)
		return m_ArgV[i+1];
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
	int extlen = strlen (extension);

	for (i = 1; i < m_ArgC && *m_ArgV[i] != '-' && *m_ArgV[i] != '+'; i++)
	{
		int len = strlen (m_ArgV[i]);
		if (len >= extlen && stricmp (m_ArgV[i] + len - extlen, extension) == 0)
			out->AppendArg (m_ArgV[i]);
		else if (acceptNoExt && !strrchr (m_ArgV[i], '.'))
			out->AppendArg (m_ArgV[i]);
	}
	i = CheckParm (param);
	if (i)
	{
		for (i++; i < m_ArgC && *m_ArgV[i] != '-' && *m_ArgV[i] != '+'; i++)
			out->AppendArg (m_ArgV[i]);
	}
	return out;
}
