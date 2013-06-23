#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#if 0
class ADynamicLight;

const int MAX_DYNLIGHTS = 40000;	// should hopefully be enough

struct FLightRGB
{
	unsigned char R,G,B,Type;	// Type is 0 for normal, 1 for additive and 2 for subtractive
};

struct FLightPosition
{
	float X,Z,Y,Distance;
};

class FLightBuffer
{
	unsigned int mIDbuf_RGB;
	unsigned int mIDbuf_Position;

	unsigned int mIDtex_RGB;
	unsigned int mIDtex_Position;

public:
	FLightBuffer();
	~FLightBuffer();
	//void MapBuffer();
	//void UnmapBuffer();
	void BindTextures(int uniloc1, int uniloc2);
	//void AddLight(ADynamicLight *light, bool foggy);
	void CollectLightSources();
};

class FLightIndexBuffer
{
	unsigned int mIDBuffer;
	unsigned int mIDTexture;

	TArray<unsigned short> mBuffer;

public:

	FLightIndexBuffer();
	~FLightIndexBuffer();
	void AddLight(ADynamicLight *light);
	void SendBuffer();
	void BindTexture(int loc1);

	void ClearBuffer()
	{
		mBuffer.Clear();
	}

	int GetLightIndex()
	{
		return mBuffer.Size();
	}

};

#endif

#endif