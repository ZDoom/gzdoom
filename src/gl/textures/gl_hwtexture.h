
#ifndef __GLTEXTURE_H
#define __GLTEXTURE_H

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"

class FCanvasTexture;
class AActor;

enum
{
	GLT_CLAMPX=1,
	GLT_CLAMPY=2
};

class FHardwareTexture
{
	enum
	{
		MAX_TEXTURES = 16
	};

	struct TranslatedTexture
	{
		unsigned int glTexID;
		int translation;
		int cm;
	};

public:

	static unsigned int lastbound[MAX_TEXTURES];
	static int lastactivetexture;
	static bool supportsNonPower2;
	static int max_texturesize;

	static int GetTexDimension(int value);

private:

	short texwidth, texheight;
	//float scalexfac, scaleyfac;
	bool mipmap;
	BYTE clampmode;
	bool forcenofiltering;
	bool forcenocompression;

	unsigned int * glTexID;
	TArray<TranslatedTexture> glTexID_Translated;
	unsigned int glDepthID;	// only used by camera textures

	void LoadImage(unsigned char * buffer,int w, int h, unsigned int & glTexID,int wrapparam, bool alphatexture, int texunit);
	unsigned * GetTexID(int cm, int translation);

	int GetDepthBuffer();
	void DeleteTexture(unsigned int texid);

public:
	FHardwareTexture(int w, int h, bool mip, bool wrap, bool nofilter, bool nocompress);
	~FHardwareTexture();

	static void Unbind(int texunit);
	static void UnbindAll();

	void BindToFrameBuffer();

	unsigned int Bind(int texunit, int cm, int translation=0);
	unsigned int CreateTexture(unsigned char * buffer, int w, int h,bool wrap, int texunit, int cm, int translation=0);
	void Resize(int _width, int _height) ;

	void Clean(bool all);
};

#endif
