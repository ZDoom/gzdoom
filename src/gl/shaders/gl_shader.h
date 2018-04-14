// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

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
	VATTR_VERTEX2 = 3,
	VATTR_NORMAL = 4
};

class FShaderCollection;

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

	void Set(float f1, float f2)
	{
		if (mBuffer[0] != f1 || mBuffer[1] != f2)
		{
			mBuffer[0] = f1;
			mBuffer[1] = f2;
			glUniform2fv(mIndex, 1, mBuffer);
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

	void Set(PalEntry newvalue)
	{
		glUniform4f(mIndex, newvalue.r / 255.f, newvalue.g / 255.f, newvalue.b / 255.f, newvalue.a / 255.f);
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

class FBufferedUniformSampler
{
	int mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = -1;
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


class FShader
{
	friend class FShaderCollection;
	friend class FRenderState;

	unsigned int hShader;
	unsigned int hVertProg;
	unsigned int hFragProg;
	FName mName;

	FBufferedUniform1f muDesaturation;
	FBufferedUniform1i muFogEnabled;
	FBufferedUniform1i muPalLightLevels;
	FBufferedUniform1f muGlobVis;
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
	FBufferedUniformPE muObjectColor2;
	FUniform4f muGlowBottomColor;
	FUniform4f muGlowTopColor;
	FUniform4f muGlowBottomPlane;
	FUniform4f muGlowTopPlane;
	FUniform4f muSplitBottomPlane;
	FUniform4f muSplitTopPlane;
	FUniform4f muClipLine;
	FBufferedUniform1f muInterpolationFactor;
	FBufferedUniform1f muClipHeight;
	FBufferedUniform1f muClipHeightDirection;
	FBufferedUniform1f muAlphaThreshold;
	FBufferedUniform1i muViewHeight;
	FBufferedUniform2f muSpecularMaterial;
	FBufferedUniform1f muTimer;
	
	int lights_index;
	int projectionmatrix_index;
	int viewmatrix_index;
	int normalviewmatrix_index;
	int modelmatrix_index;
	int normalmodelmatrix_index;
	int texturematrix_index;
public:
	int vertexmatrix_index;
	int texcoordmatrix_index;
	int quadmode_index;
	int fakevb_index;
private:
	int currentglowstate = 0;
	int currentsplitstate = 0;
	int currentcliplinestate = 0;
	int currentfixedcolormap = 0;
	bool currentTextureMatrixState = true;// by setting the matrix state to 'true' it is guaranteed to be set the first time the render state gets applied.
	bool currentModelMatrixState = true;

public:
	FShader(const char *name)
		: mName(name)
	{
		hShader = hVertProg = hFragProg = 0;
	}

	~FShader();

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char * light_fragprog, const char *defines);

	void SetColormapColor(float r, float g, float b, float r1, float g1, float b1);
	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
	void SetLightRange(int start, int end, int forceadd);

	bool Bind();
	unsigned int GetHandle() const { return hShader; }

	void ApplyMatrices(VSMatrix *proj, VSMatrix *view, VSMatrix *norm);

};

//==========================================================================
//
// The global shader manager
//
//==========================================================================
class FShaderManager
{
public:
	FShaderManager();
	~FShaderManager();

	void SetActiveShader(FShader *sh);
	FShader *GetActiveShader() const { return mActiveShader; }

	FShader *BindEffect(int effect, EPassType passType);
	FShader *Get(unsigned int eff, bool alphateston, EPassType passType);
	void ApplyMatrices(VSMatrix *proj, VSMatrix *view, EPassType passType);

	void ResetFixedColormap();

private:
	FShader *mActiveShader = nullptr;
	TArray<FShaderCollection*> mPassShaders;
};

class FShaderCollection
{
	TArray<FShader*> mMaterialShaders;
	TArray<FShader*> mMaterialShadersNAT;
	FShader *mEffectShaders[MAX_EFFECTS];

	void Clean();
	void CompileShaders(EPassType passType);
	
public:
	FShaderCollection(EPassType passType);
	~FShaderCollection();
	FShader *Compile(const char *ShaderName, const char *ShaderPath, const char *LightModePath, const char *shaderdefines, bool usediscard, EPassType passType);
	int Find(const char *mame);
	FShader *BindEffect(int effect);
	void ApplyMatrices(VSMatrix *proj, VSMatrix *view);

	void ResetFixedColormap()
	{
		for (unsigned i = 0; i < mMaterialShaders.Size(); i++)
		{
			mMaterialShaders[i]->currentfixedcolormap = -1;
		}
		for (unsigned i = 0; i < mMaterialShadersNAT.Size(); i++)
		{
			mMaterialShadersNAT[i]->currentfixedcolormap = -1;
		}
	}

	FShader *Get(unsigned int eff, bool alphateston)
	{
		// indices 0-2 match the warping modes, 3 is brightmap, 4 no texture, the following are custom
		if (!alphateston && eff <= 3)
		{
			return mMaterialShadersNAT[eff];	// Non-alphatest shaders are only created for default, warp1+2 and brightmap. The rest won't get used anyway
		}
		if (eff < mMaterialShaders.Size())
		{
			return mMaterialShaders[eff];
		}
		return NULL;
	}
};

enum
{
	LIGHTBUF_BINDINGPOINT = 1
};

#endif

