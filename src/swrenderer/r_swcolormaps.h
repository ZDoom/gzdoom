#pragma once



struct FSWColormap
{
	uint8_t *Maps = nullptr;
	PalEntry Color = 0xffffffff;
	PalEntry Fade = 0xff000000;
	int Desaturate = 0;
};

struct FDynamicColormap : FSWColormap
{
	void ChangeFade (PalEntry fadecolor);
	void ChangeColor (PalEntry lightcolor, int desaturate);
	void ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor);
	void ChangeFogDensity(int newdensity);
	void BuildLights ();
	static void RebuildAllLights();

	FDynamicColormap *Next;
};

extern FSWColormap realcolormaps;					// [RH] make the colormaps externally visible

extern FDynamicColormap NormalLight;
extern FDynamicColormap FullNormalLight;
extern bool NormalLightHasFixedLights;
extern TArray<FSWColormap> SpecialSWColormaps;

void InitSWColorMaps();
FDynamicColormap *GetSpecialLights (PalEntry lightcolor, PalEntry fadecolor, int desaturate);
void SetDefaultColormap (const char *name);


// Give the compiler a strong hint we want these functions inlined:
#ifndef FORCEINLINE
#if defined(_MSC_VER)
#define FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define FORCEINLINE __attribute__((always_inline)) inline
#else
#define FORCEINLINE inline
#endif
#endif

// MSVC needs the forceinline here.
FORCEINLINE FDynamicColormap *GetColorTable(const FColormap &cm)
{
	auto p = &NormalLight;
	if (cm.LightColor == p->Color &&
		cm.FadeColor == p->Fade &&
		cm.Desaturation == p->Desaturate)
	{
		return p;
	}

	return GetSpecialLights(cm.LightColor, cm.FadeColor, cm.Desaturation);
}
