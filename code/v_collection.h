#ifndef __V_COLLECTION_H__
#define __V_COLLECTION_H__

#include "doomtype.h"

struct patch_s;

struct RawImageHeader
{
	BYTE Magic[4];
	WORD Width;
	WORD Height;
	SWORD LeftOffset;
	SWORD TopOffset;
	BYTE Compression;
	BYTE Reserved[11];
};

class FImageCollection
{
public:
	FImageCollection ();
	FImageCollection (const char **patchNames, int numPatches);
	~FImageCollection ();

	void Init (const char **patchnames, int numPatches, int namespc=0);
	void Uninit ();

	byte *GetImage (int index, int *const width, int *const height, int *const xoffs, int *const yoffs) const;
	int GetImageWidth (int index) const;
	int GetImageHeight (int index) const;

protected:
	static patch_s *CachePatch (const char *name, int namespc);

	int NumImages;
	struct ImageData
	{
		WORD Width, Height;
		SWORD XOffs, YOffs;
		byte *Data;
	} *Images;
	byte *Bitmaps;
};

#endif //__V_COLLECTION_H__