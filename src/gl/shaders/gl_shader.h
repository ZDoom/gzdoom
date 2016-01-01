
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_uniform.h"
#include "name.h"

extern bool gl_shaderactive;

enum
{
	VATTR_VERTEX = 0,
	VATTR_TEXCOORD = 1,
	VATTR_COLOR = 2,
	VATTR_VERTEX2 = 3
};


class FShader
{
	friend class FShaderManager;
	friend class FRenderState;

	unsigned int hShader;
	unsigned int hVertProg;
	unsigned int hFragProg;
	FName mName;

	FBufferedUniform1f muDesaturation;
	FBufferedUniform1i muFogEnabled;
	FBufferedUniform1i muTextureMode;
	FBufferedUniform4f muCameraPos;
	FBufferedUniform4f muLightParms;
	FBufferedUniform2f muClipSplit;
	FUniform1i muFixedColormap;
	FUniform4f muColormapStart;
	FUniform4f muColormapRange;
	FBufferedUniform1i muLightIndex;
	FBufferedUniformPE muFogColor;
	FBufferedUniform4f muDynLightColor;
	FBufferedUniformPE muObjectColor;
	FUniform4f muGlowBottomColor;
	FUniform4f muGlowTopColor;
	FUniform4f muGlowBottomPlane;
	FUniform4f muGlowTopPlane;
	FBufferedUniform1f muInterpolationFactor;
	FBufferedUniform1f muClipHeightTop;
	FBufferedUniform1f muClipHeightBottom;
	FBufferedUniform1f muAlphaThreshold;
	FBufferedUniform1f muTimer;
	
	int lights_index;
	int projectionmatrix_index;
	int viewmatrix_index;
	int modelmatrix_index;
	int texturematrix_index;
public:
	int fakevb_index;
private:
	int currentglowstate;
	int currentfixedcolormap;
	bool currentTextureMatrixState;
	bool currentModelMatrixState;

	TArray<FString> mTexUnitNames;

public:
	FShader(const char *name)
		: mName(name)
	{
		hShader = hVertProg = hFragProg = 0;
		currentglowstate = 0;
		currentfixedcolormap = 0;
		currentTextureMatrixState = true;	// by setting the matrix state to 'true' it is guaranteed to be set the first time the render state gets applied.
		currentModelMatrixState = true;
	}

	~FShader();

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char *fragprog3, const char *fragprog4, const char *defines);

	void SetColormapColor(float r, float g, float b, float r1, float g1, float b1);
	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
	void SetLightRange(int start, int end, int forceadd);

	bool Bind();
	unsigned int GetHandle() const { return hShader; }

	void ApplyMatrices(VSMatrix *proj, VSMatrix *view);
	const TArray<FString> &GetTexUnits() const
	{
		return mTexUnitNames; 
	}

};


enum EUniformType
{
	UT_INT,
	UT_IVEC2,
	UT_IVEC3,
	UT_IVEC4,
	UT_FLOAT,
	UT_VEC2,
	UT_VEC3,
	UT_VEC4
};

struct FUniformDefinition
{
	FString mName;
	BYTE mType;
	BYTE mRefPos;
	unsigned int mRef;
};

struct FShaderDefinition
{
	FName mName;
	FString mSourceFile;
	bool mNoLightShader;
	bool mNoCoordinateShader;
	bool mCoreLump;
	bool bRequireAlphaTest;
	TArray<FString> mTextureUnitNames;
	TArray<FUniformDefinition> mUniforms;

	FShaderDefinition()
	{
		mNoCoordinateShader = false;
		mNoLightShader = false;
		mCoreLump = false;
		bRequireAlphaTest = false;
	}
};

//==========================================================================
//
// The global shader manager
//
//==========================================================================
class FShaderManager
{
	struct FShaderContainer
	{
		FName mCoordinateName;
		FName mTexelName;
		FName mLightName;
		FShader *mShader;
		FShader *mShaderNAT;
	};

	TArray<FShaderContainer> mShaders;

	FShader *mActiveShader;
	FShader *mEffectShaders[MAX_EFFECTS];

	void CompileShaders();
	void FindAllUsedShaders();
	FShader *Compile(const char *ShaderName, FShaderDefinition *CoordShader, FShaderDefinition *TexShader, FShaderDefinition *LightShader, bool usediscard);


	
public:
	FShaderManager();
	~FShaderManager();
	FShader *BindEffect(int effect);
	void SetActiveShader(FShader *sh);
	void ApplyMatrices(VSMatrix *proj, VSMatrix *view);
	unsigned int GetShaderIndex(FName coord, FName tex, FName light);
	void Clean();
	FShader *GetActiveShader() const
	{
		return mActiveShader;
	}

	void ResetFixedColormap()
	{
		for (unsigned i = 0; i < mShaders.Size(); i++)
		{
			if (mShaders[i].mShader != NULL) mShaders[i].mShader->currentfixedcolormap = -1;
			if (mShaders[i].mShaderNAT != NULL) mShaders[i].mShaderNAT->currentfixedcolormap = -1;
		}
	}

	FShader *Get(unsigned int eff, bool alphateston)
	{
		if (eff < mShaders.Size())
		{
			if (!alphateston && mShaders[eff].mShaderNAT != NULL) return mShaders[eff].mShaderNAT;
			return mShaders[eff].mShader;
		}
		return NULL;
	}

	void Validate()
	{
		if (mShaders.Size() == 0)
		{
			CompileShaders();
		}
	}

};

enum
{
	LIGHTBUF_BINDINGPOINT = 1
};


extern TArray<FShaderDefinition *> CoordinateShaders;
extern TArray<FShaderDefinition *> TexelShaders;
extern TArray<FShaderDefinition *> LightShaders;

#endif

