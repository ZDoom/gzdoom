/*
** v_collection.cpp
** Holds a collection of images
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

#include "v_collection.h"
#include "v_font.h"
#include "v_video.h"
#include "filesystem.h"
#include "texturemanager.h"

FImageCollection::FImageCollection ()
{
}

FImageCollection::FImageCollection (const char **patchNames, int numPatches)
{
	Add (patchNames, numPatches);
}

void FImageCollection::Init (const char **patchNames, int numPatches, ETextureType namespc)
{
	ImageMap.Clear();
	Add(patchNames, numPatches, namespc);
}

// [MH] Mainly for mugshots with skins and SBARINFO
void FImageCollection::Add (const char **patchNames, int numPatches, ETextureType namespc)
{
	int OldCount = ImageMap.Size();

	ImageMap.Resize(OldCount + numPatches);

	for (int i = 0; i < numPatches; ++i)
	{
		FTextureID picnum = TexMan.CheckForTexture(patchNames[i], namespc);
		ImageMap[OldCount + i] = picnum;
	}
}

void FImageCollection::Uninit ()
{
	ImageMap.Clear();
}

FGameTexture *FImageCollection::operator[] (int index) const
{
	if ((unsigned int)index >= ImageMap.Size())
	{
		return NULL;
	}
	return ImageMap[index].Exists()? TexMan.GetGameTexture(ImageMap[index], true) : NULL;
}
