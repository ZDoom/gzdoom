
#pragma once

#include "p_3dfloors.h"

EXTERN_CVAR(Int, r_3dfloors);

namespace swrenderer
{
	struct HeightLevel
	{
		double height;
		struct HeightLevel *prev;
		struct HeightLevel *next;
	};

	struct HeightStack
	{
		HeightLevel *height_top;
		HeightLevel *height_cur;
		int height_max;
	};

	struct ClipStack
	{
		short floorclip[MAXWIDTH];
		short ceilingclip[MAXWIDTH];
		F3DFloor *ffloor;
		ClipStack *next;
	};

	enum Fake3DOpaque
	{
		// BSP stage:
		FAKE3D_FAKEFLOOR    = 1,  // fake floor, mark seg as FAKE
		FAKE3D_FAKECEILING  = 2,  // fake ceiling, mark seg as FAKE
		FAKE3D_FAKEBACK     = 4,  // RenderLine with fake backsector, mark seg as FAKE
		FAKE3D_FAKEMASK     = 7,
		FAKE3D_CLIPBOTFRONT = 8,  // use front sector clipping info (bottom)
		FAKE3D_CLIPTOPFRONT = 16, // use front sector clipping info (top)
	};

	enum Fake3DTranslucent
	{
		// sorting stage:
		FAKE3D_CLIPBOTTOM   = 1,  // clip bottom
		FAKE3D_CLIPTOP      = 2,  // clip top
		FAKE3D_REFRESHCLIP  = 4,  // refresh clip info
		FAKE3D_DOWN2UP      = 8,  // rendering from down to up (floors)
	};

	class Clip3DFloors
	{
	public:
		static Clip3DFloors *Instance();

		void Cleanup();

		void DeleteHeights();
		void AddHeight(secplane_t *add, sector_t *sec);
		void NewClip();
		void ResetClip();
		void EnterSkybox();
		void LeaveSkybox();

		int fake3D = 0;

		F3DFloor *fakeFloor = nullptr;
		fixed_t fakeAlpha = 0;
		bool fakeActive = false;
		double sclipBottom = 0;
		double sclipTop = 0;
		HeightLevel *height_top = nullptr;
		HeightLevel *height_cur = nullptr;
		int CurrentSkybox = 0;

	private:
		int height_max = -1;
		TArray<HeightStack> toplist;
		ClipStack *clip_top = nullptr;
		ClipStack *clip_cur = nullptr;
	};
}
