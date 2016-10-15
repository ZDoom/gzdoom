
#pragma once

struct WorkerThreadData
{
	int32_t core;
	int32_t num_cores;
	int32_t pass_start_y;
	int32_t pass_end_y;
	uint32_t *temp;
};

struct DrawWallArgs
{
	uint32_t *dest;
	const uint32_t *source[4];
	const uint32_t *source2[4];
	int32_t pitch;
	int32_t count;
	int32_t dest_y;
	uint32_t texturefrac[4];
	uint32_t texturefracx[4];
	uint32_t iscale[4];
	uint32_t textureheight[4];
	uint32_t light[4];
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2
	};

	FString ToString()
	{
		FString info;
		info.Format("dest_y = %i, count = %i, flags = %i", dest_y, count, flags);
		return info;
	}
};

struct DrawSpanArgs
{
	uint32_t *destorg;
	const uint32_t *source;
	int32_t destpitch;
	int32_t xfrac;
	int32_t yfrac;
	int32_t xstep;
	int32_t ystep;
	int32_t x1;
	int32_t x2;
	int32_t y;
	int32_t xbits;
	int32_t ybits;
	uint32_t light;
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2
	};

	FString ToString()
	{
		FString info;
		info.Format("x1 = %i, x2 = %i, y = %i, flags = %i", x1, x2, y, flags);
		return info;
	}
};

struct DrawColumnArgs
{
	uint32_t *dest;
	const uint8_t *source;
	uint8_t *colormap;
	uint8_t *translation;
	const uint32_t *basecolors;
	int32_t pitch;
	int32_t count;
	int32_t dest_y;
	uint32_t iscale;
	uint32_t texturefrac;
	uint32_t light;
	uint32_t color;
	uint32_t srccolor;
	uint32_t srcalpha;
	uint32_t destalpha;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1
	};

	FString ToString()
	{
		FString info;
		info.Format("dest_y = %i, count = %i, flags = %i", dest_y, count, flags);
		return info;
	}
};

struct DrawSkyArgs
{
	uint32_t *dest;
	const uint32_t *source0[4];
	const uint32_t *source1[4];
	int32_t pitch;
	int32_t count;
	int32_t dest_y;
	uint32_t texturefrac[4];
	uint32_t iscale[4];
	uint32_t textureheight0;
	uint32_t textureheight1;
	uint32_t top_color;
	uint32_t bottom_color;

	FString ToString()
	{
		FString info;
		info.Format("dest_y = %i, count = %i", dest_y, count);
		return info;
	}
};

class LLVMDrawers
{
public:
	virtual ~LLVMDrawers() { }

	static void Create();
	static void Destroy();
	static LLVMDrawers *Instance();

	void(*DrawColumn)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnShaded)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnAddClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRevSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnTlatedAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnAddClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnSubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRevSubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumn)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnAddClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*FillColumnRevSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1Copy)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1Add)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1Shaded)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1AddClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1SubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1RevSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1Translated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1TlatedAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1AddClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1SubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt1RevSubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4Copy)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4Add)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4Shaded)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4AddClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4SubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4RevSubClamp)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4Translated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4TlatedAdd)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4AddClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4SubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawColumnRt4RevSubClampTranslated)(const DrawColumnArgs *, const WorkerThreadData *) = nullptr;

	void(*DrawSpan)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanMasked)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanTranslucent)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanMaskedTranslucent)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanAddClamp)(const DrawSpanArgs *) = nullptr;
	void(*DrawSpanMaskedAddClamp)(const DrawSpanArgs *) = nullptr;

	void(*vlinec1)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*vlinec4)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*mvlinec1)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*mvlinec4)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_add)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline4_add)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_addclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline4_addclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_subclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline4_subclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline1_revsubclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;
	void(*tmvline4_revsubclamp)(const DrawWallArgs *, const WorkerThreadData *) = nullptr;

	void(*DrawSky1)(const DrawSkyArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawSky4)(const DrawSkyArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawDoubleSky1)(const DrawSkyArgs *, const WorkerThreadData *) = nullptr;
	void(*DrawDoubleSky4)(const DrawSkyArgs *, const WorkerThreadData *) = nullptr;

private:
	static LLVMDrawers *Singleton;
};
