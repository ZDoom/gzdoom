#ifndef __V_PFX_H__
#define __V_PFX_H__

//
// Pixel format conversion routines, for use with DFrameBuffer implementations
//

union PfxUnion
{
	BYTE Pal8[256];
	WORD Pal16[256];
	DWORD Pal32[256];
};

struct PfxState
{
	union
	{
		struct
		{
			WORD Red;
			WORD Green;
			WORD Blue;
		} Bits16;
		struct
		{
			DWORD Red;
			DWORD Green;
			DWORD Blue;
		} Bits32;
	} Masks;
	BYTE RedShift;
	BYTE BlueShift;
	BYTE GreenShift;
	BITFIELD RedLeft:1;
	BITFIELD BlueLeft:1;
	BITFIELD GreenLeft:1;

	void SetFormat (int bits, DWORD redMask, DWORD greenMask, DWORD blueMask);
	void (*SetPalette) (const PalEntry *pal);
	void (*Convert) (BYTE *src, int srcpitch,
		void *dest, int destpitch, int destwidth, int destheight,
		fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);
};

extern "C"
{
	extern PfxUnion GPfxPal;
	extern PfxState GPfx;
}

#endif //__V_PFX_H__