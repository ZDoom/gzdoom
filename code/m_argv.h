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
// DESCRIPTION:
//	Nil.
//	  
//-----------------------------------------------------------------------------

#ifndef __M_ARGV_H__
#define __M_ARGV_H__

#include "dobject.h"

//
// MISC
//
class DArgs : public DObject
{
	DECLARE_CLASS (DArgs, DObject)
public:
	DArgs ();
	DArgs (const DArgs &args);
	DArgs (int argc, char **argv);
	~DArgs ();

	DArgs &operator= (const DArgs &other);

	void AppendArg (const char *arg);
	void SetArgs (int argc, char **argv);
	DArgs *GatherFiles (const char *param, const char *extension, bool acceptNoExt) const;
	void SetArg (int argnum, const char *arg);

	// Returns the position of the given parameter
	// in the arg list (0 if not found).
	int CheckParm (const char *check) const;
	char *CheckValue (const char *check) const;
	char *GetArg (int arg) const;
	char **GetArgList (int arg) const;
	int NumArgs () const;
	void FlushArgs ();

private:
	int m_ArgC;
	char **m_ArgV;

	void CopyArgs (int argc, char **argv);
};

extern DArgs Args;

#endif //__M_ARGV_H__