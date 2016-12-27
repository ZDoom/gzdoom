
#ifndef __GL_TEXSHADERS_H__
#define __GL_TEXSHADERS_H__


#include "tarray.h"
#include "zstring.h"
#include "gl/utility/gl_cycler.h"


enum
{
   SHADER_TexGen_None = 0,
   SHADER_TexGen_Sphere,
   NUM_TexGenTypes
};


//==========================================================================
//
//
//
//==========================================================================

class FShaderLayer
{
public:
	FShaderLayer();
	FShaderLayer(const FShaderLayer &layer);
	~FShaderLayer();
	void Update(float diff);
	CycleType ParseCycleType(FScanner &sc);
	bool ParseLayer(FScanner &sc);

	FTextureID texture;
	int warpspeed;
	unsigned char warp;
	bool animate;
	bool emissive;
	unsigned char texgen;
	float centerX, centerY;
	float rotation;
	float rotate;
	float offsetX, offsetY;
	FCycler adjustX, adjustY;
	FCycler vectorX, vectorY;
	FCycler scaleX, scaleY;
	FCycler alpha;
	FCycler r, g, b;
	FCycler srcFactor, dstFactor;
	unsigned int flags;
	unsigned int blendFuncSrc, blendFuncDst;
	FShaderLayer *layerMask;
};

//==========================================================================
//
//
//
//==========================================================================

class FTextureShader
{
public:
	FTextureShader();
	bool ParseShader(FScanner &sc, TArray<FTextureID> &names);
	bool Available();
	bool Setup(float time);
	void Update(int framems);
	void FakeUpdate(int framems);
	FString CreateName();
	FString GenerateCode();

	FName name;
	TDeletingArray <FShaderLayer *> layers; // layers for shader
	unsigned int lastUpdate;
};


/*
//extern TArray<FShader *> Shaders[NUM_ShaderClasses];
//extern TArray<FShader *> ShaderLookup[NUM_ShaderClasses];

void GL_InitShaders();
void GL_ReleaseShaders();
void GL_UpdateShaders();
void GL_FakeUpdateShaders();
//void GL_UpdateShader(FShader *shader);
void GL_DrawShaders();
//FShader *GL_ShaderForTexture(FTexture *tex);

bool GL_ParseShader();
*/


#endif // __GL_TEXSHADERS_H__
