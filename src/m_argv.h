/*
** m_argv.h
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
	int CheckParm (const char *check, int start=1) const;
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

extern DArgs *Args;

#endif //__M_ARGV_H__
