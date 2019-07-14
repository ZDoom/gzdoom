
#pragma once

#include "r_defs.h"
#include <unordered_map>

EXTERN_CVAR(Int, r_3dfloors);

namespace swrenderer
{
	class RenderThread;
	class FakeFloorClip;

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
		FakeFloorClip *ffloor;
		ClipStack *next;
	};

	// BSP stage
	struct Fake3DOpaque
	{
		enum Type
		{
			Normal,       // Not a 3D floor
			FakeFloor,    // fake floor, mark seg as FAKE
			FakeCeiling,  // fake ceiling, mark seg as FAKE
			FakeBack      // RenderLine with fake backsector, mark seg as FAKE
		};
		Type type = Normal;

		bool clipBotFront = false; // use front sector clipping info (bottom)
		bool clipTopFront = false; // use front sector clipping info (top)

		Fake3DOpaque() { }
		Fake3DOpaque(Type type) : type(type) { }
	};

	// Drawing stage
	struct Fake3DTranslucent
	{
		bool clipBottom = false;
		bool clipTop = false;
		bool down2Up = false; // rendering from down to up (floors)
		double sclipBottom = 0;
		double sclipTop = 0;
	};

	class FakeFloorClip
	{
	public:
		FakeFloorClip(F3DFloor *fakeFloor) : fakeFloor(fakeFloor) { }

		F3DFloor *fakeFloor = nullptr;
		short *floorclip = nullptr;
		short *ceilingclip = nullptr;
		int	validcount = -1;
	};

	class Clip3DFloors
	{
	public:
		Clip3DFloors(RenderThread *thread);

		void Cleanup();

		void DeleteHeights();
		void AddHeight(secplane_t *add, sector_t *sec);
		void NewClip();
		void ResetClip();
		void EnterSkybox();
		void LeaveSkybox();
		void SetFakeFloor(F3DFloor *fakeFloor);
		void ClearFakeFloors() { FakeFloors.clear(); }

		RenderThread *Thread = nullptr;

		FakeFloorClip *fakeFloor = nullptr;
		bool fakeActive = false;
		HeightLevel *height_top = nullptr;
		HeightLevel *height_cur = nullptr;
		int CurrentSkybox = 0;

	private:
		int height_max = -1;
		TArray<HeightStack> toplist;
		ClipStack *clip_top = nullptr;
		ClipStack *clip_cur = nullptr;

		std::unordered_map<F3DFloor *, FakeFloorClip *> FakeFloors;
	};
}
