
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

#include "gl/renderer/gl_renderstate.h"
#include "name.h"

extern bool gl_shaderactive;

enum
{
	VATTR_VERTEX = 0,
	VATTR_TEXCOORD = 1,
	VATTR_COLOR = 2,
	VATTR_VERTEX2 = 3
};


//==========================================================================
//
//
//==========================================================================

class FUniform1i
{
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
	}

	void Set(int newvalue)
	{
		glUniform1i(mIndex, newvalue);
	}
};

class FBufferedUniform1i
{
	int mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(int newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform1i(mIndex, newvalue);
		}
	}
};

class FBufferedUniform4i
{
	int mBuffer[4];
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		memset(mBuffer, 0, sizeof(mBuffer));
	}

	void Set(const int *newvalue)
	{
		if (memcmp(newvalue, mBuffer, sizeof(mBuffer)))
		{
			memcpy(mBuffer, newvalue, sizeof(mBuffer));
			glUniform4iv(mIndex, 1, newvalue);
		}
	}
};

class FBufferedUniform1f
{
	float mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(float newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform1f(mIndex, newvalue);
		}
	}
};

class FBufferedUniform2f
{
	float mBuffer[2];
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		memset(mBuffer, 0, sizeof(mBuffer));
	}

	void Set(const float *newvalue)
	{
		if (memcmp(newvalue, mBuffer, sizeof(mBuffer)))
		{
			memcpy(mBuffer, newvalue, sizeof(mBuffer));
			glUniform2fv(mIndex, 1, newvalue);
		}
	}
};

class FBufferedUniform4f
{
	float mBuffer[4];
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		memset(mBuffer, 0, sizeof(mBuffer));
	}

	void Set(const float *newvalue)
	{
		if (memcmp(newvalue, mBuffer, sizeof(mBuffer)))
		{
			memcpy(mBuffer, newvalue, sizeof(mBuffer));
			glUniform4fv(mIndex, 1, newvalue);
		}
	}
};

class FUniform4f
{
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
	}

	void Set(const float *newvalue)
	{
		glUniform4fv(mIndex, 1, newvalue);
	}

	void Set(float a, float b, float c, float d)
	{
		glUniform4f(mIndex, a, b, c, d);
	}
};

class FBufferedUniformPE
{
	PalEntry mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(PalEntry newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform4f(mIndex, newvalue.r/255.f, newvalue.g/255.f, newvalue.b/255.f, newvalue.a/255.f);
		}
	}
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

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char *fragprog3, const char *defines);

	void SetColormapColor(float r, float g, float b, float r1, float g1, float b1);
	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
	void SetLightRange(int start, int end, int forceadd);

	bool Bind();
	unsigned int GetHandle() const { return hShader; }

	void ApplyMatrices(VSMatrix *proj, VSMatrix *view);

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
	FShader *Compile(const char *ShaderName, const char *TexShaderPath, const char *LightShaderPath, bool usediscard);


	
public:
	FShaderManager();
	~FShaderManager();
	FShader *BindEffect(int effect);
	void SetActiveShader(FShader *sh);
	void ApplyMatrices(VSMatrix *proj, VSMatrix *view);
	unsigned int GetShaderIndex(FName tex, FName light);
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


struct FShaderDefinition
{
	FName mName;
	FString mSourceFile;
	bool mNoLightShader;
	bool mCoreLump;
	bool bRequireAlphaTest;

	FShaderDefinition()
	{
		mNoLightShader = false;
		mCoreLump = false;
		bRequireAlphaTest = false;
	}
};


extern TArray<FShaderDefinition *> TexelShaders;
extern TArray<FShaderDefinition *> LightShaders;

#endif

