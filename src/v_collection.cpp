/*
** v_collection.cpp
** Holds a collection of images
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

#include "v_collection.h"
#include "v_font.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"

FImageCollection::FImageCollection ()
{
	NumImages = 0;
	Bitmaps = NULL;
	Images = NULL;
}

FImageCollection::FImageCollection (const char **patchNames, int numPatches)
{
	Init (patchNames, numPatches);
}

FImageCollection::~FImageCollection ()
{
	Uninit ();
}

void FImageCollection::Init (const char **patchNames, int numPatches, int namespc)
{
	int neededsize;
	int i;
	byte translate[256];

	NumImages = numPatches;
	Bitmaps = NULL;
	Images = new ImageData[numPatches];

	for (i = neededsize = 0; i < numPatches; i++)
	{
		const patch_t *patch = CachePatch (patchNames[i], namespc);
		if (patch)
		{
			if (((byte *)patch)[0] == 'I' &&
				((byte *)patch)[1] == 'M' &&
				((byte *)patch)[2] == 'G' &&
				((byte *)patch)[3] == 'Z')
			{
				RawImageHeader *raw = (RawImageHeader *)patch;
				Images[i].Width = SHORT(raw->Width);
				Images[i].Height = SHORT(raw->Height);
				Images[i].XOffs = SHORT(raw->LeftOffset);
				Images[i].YOffs = SHORT(raw->TopOffset);
			}
			else
			{
				Images[i].Width = SHORT(patch->width);
				Images[i].Height = SHORT(patch->height);
				Images[i].XOffs = SHORT(patch->leftoffset);
				Images[i].YOffs = SHORT(patch->topoffset);
			}
			neededsize += Images[i].Width * Images[i].Height;
			W_UnMapLump (patch);
		}
		else
		{
			Images[i].Width = Images[i].Height = 0;
			Images[i].XOffs = Images[i].YOffs = 0;
		}
	}

	translate[0] = Near0;
	for (i = 1; i < 256; i++)
	{
		translate[i] = i;
	}

	Bitmaps = new byte[neededsize];
	memset (Bitmaps, 0, neededsize);

	for (i = neededsize = 0; i < numPatches; i++)
	{
		const patch_t *patch = CachePatch (patchNames[i], namespc);
		if (patch)
		{
			Images[i].Data = Bitmaps + neededsize;
			if (((byte *)patch)[0] == 'I' &&
				((byte *)patch)[1] == 'M' &&
				((byte *)patch)[2] == 'G' &&
				((byte *)patch)[3] == 'Z')
			{
				RawImageHeader *raw = (RawImageHeader *)patch;
				byte *source = (byte *)&raw[1];

				if (raw->Compression)
				{
					int destSize = Images[i].Width * Images[i].Height;
					byte *dest = Images[i].Data;

					do
					{
						SBYTE code = *source++;
						if (code >= 0)
						{
							memcpy (dest, source, code+1);
							dest += code+1;
							source += code+1;
							destSize -= code+1;
						}
						else if (code != -128)
						{
							memset (dest, *source, (-code)+1);
							dest += (-code)+1;
							source++;
							destSize -= (-code)+1;
						}
					} while (destSize > 0);
				}
				else
				{
					memcpy (Images[i].Data, source, Images[i].Width * Images[i].Height);
				}
			}
			else
			{
				RawDrawPatch (patch, Images[i].Data, translate);
			}
			neededsize += Images[i].Width * Images[i].Height;
			W_UnMapLump (patch);
		}
		else
		{
			Images[i].Data = NULL;
		}
	}
}

void FImageCollection::Uninit ()
{
	if (Bitmaps)
	{
		delete[] Bitmaps;
		Bitmaps = NULL;
	}
	if (Images)
	{
		delete[] Images;
		Images = NULL;
	}
	NumImages = 0;
}

const patch_t *FImageCollection::CachePatch (const char *name, int namespc)
{
	int lump;
	
	if (name == NULL)
		return NULL;
	lump = W_CheckNumForName (name, namespc);
	if (lump == -1)
		lump = W_CheckNumForName (name, ns_sprites);
	if (lump == -1)
		return NULL;
	return (const patch_t *)W_MapLumpNum (lump);
}

byte *FImageCollection::GetImage (int code, int *const width, int *const height,
								  int *const xoffs, int *const yoffs) const
{
	if ((unsigned)code < (unsigned)NumImages)
	{
		*width = Images[code].Width;
		*height = Images[code].Height;
		*xoffs = Images[code].XOffs;
		*yoffs = Images[code].YOffs;
		return Images[code].Data;
	}
	return NULL;
}

int FImageCollection::GetImageWidth (int code) const
{
	if ((unsigned)code < (unsigned)NumImages)
	{
		return Images[code].Width;
	}
	else
	{
		return 0;
	}
}

int FImageCollection::GetImageHeight (int code) const
{
	if ((unsigned)code < (unsigned)NumImages)
	{
		return Images[code].Height;
	}
	else
	{
		return 0;
	}
}
