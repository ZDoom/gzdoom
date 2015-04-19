/*
** p_tags.cpp
** everything to do with tags and their management
**
**---------------------------------------------------------------------------
** Copyright 2015 Christoph Oelckers
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
**
*/


#include "p_tags.h"

//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//

// Find the next sector with a specified tag.
// Rewritten by Lee Killough to use chained hashing to improve speed

int FSectorTagIterator::Next()
{
	int ret;
	if (searchtag == INT_MIN)
	{
		ret = start;
		start = -1;
	}
	else
	{
		while (start != -1 && sectors[start].tag != searchtag) start = sectors[start].nexttag;
		if (start == -1) return -1;
		ret = start;
		start = sectors[start].nexttag;
	}
	return ret;
}

int FSectorTagIterator::NextCompat(bool compat, int start)
{
	if (!compat) return Next();

	for (int i = start + 1; i < numsectors; i++)
	{
		if (sectors[i].HasTag(searchtag)) return i;
	}
	return -1;
}


// killough 4/16/98: Same thing, only for linedefs

int FLineIdIterator::Next()
{
	while (start != -1 && lines[start].id != searchtag) start = lines[start].nextid;
	if (start == -1) return -1;
	int ret = start;
	start = lines[start].nextid;
	return ret;
}


