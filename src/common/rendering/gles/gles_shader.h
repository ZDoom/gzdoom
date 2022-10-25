// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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

#include <map>

#include "gles_renderstate.h"
#include "name.h"

extern bool gl_shaderactive;

struct HWViewpointUniforms;

namespace OpenGLESRenderer
{
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
	FVector4PalEntry mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar *name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(const FVector4PalEntry &newvalue)
	{
		if (newvalue != mBuffer)
		{
			mBuffer = newvalue;
			glUniform4f(mIndex, newvalue.r, newvalue.g, newvalue.b, newvalue.a);
		}
	}
};

class FBufferedUniformMat4fv
{
	VSMatrix mBuffer;
	int mIndex;

public:
	void Init(GLuint hShader, const GLchar* name)
	{
		mIndex = glGetUniformLocation(hShader, name);
		mBuffer = 0;
	}

	void Set(const VSMatrix* newvalue)
	{
		//if (memcmp(newvalue, &mBuffer, sizeof(mBuffer))) // Breaks 2D menu for some reason..
		{
			mBuffer = *newvalue;
			glUniformMatrix4fv(mIndex, 1, false, (float*)newvalue);
		}
	}
};

class ShaderFlavourData
{
public:
	int textureMode;
	int texFlags;
	int blendFlags;
	bool twoDFog;
	bool fogEnabled;
	bool fogEquationRadial;
	bool colouredFog;
	bool doDesaturate;
	bool dynLightsMod;
	bool dynLightsSub;
	bool dynLightsAdd;
	bool useULightLevel;
	bool useObjectColor2;
	bool useGlowTopColor;
	bool useGlowBottomColor;
	bool useColorMap;

	bool buildLighting;
	bool bandedSwLight;

#ifdef NPOT_EMULATION
	bool npotEmulation;
#endif

	bool hasSpotLight;
	bool paletteInterpolate;
};

class FShader
{
	friend class FShaderCollection;
	friend class FGLRenderState;

	FName mName;

	FString mVertProg;
	FString mFragProg;
	FString mFragProg2;
	FString mLightProg;
	FString mDefinesBase;

	/////
public: class ShaderVariantData
	{
	public:

		unsigned int hShader = 0;
		unsigned int hVertProg = 0;
		unsigned int hFragProg = 0;

		//int ProjectionMatrix_index = 0;
		//int ViewMatrix_index = 0;
		//int NormalViewMatrix_index = 0;

		FBufferedUniformMat4fv muProjectionMatrix;
		FBufferedUniformMat4fv muViewMatrix;
		FBufferedUniformMat4fv muNormalViewMatrix;

		FUniform4f muCameraPos;
		FUniform4f muClipLine;

		FBufferedUniform1f muGlobVis;
		FBufferedUniform1i muPalLightLevels;
		FBufferedUniform1i muViewHeight;
		FBufferedUniform1f muClipHeight;
		FBufferedUniform1f muClipHeightDirection;
		FBufferedUniform1i muShadowmapFilter;
		/////

		FBufferedUniform1f muDesaturation;
		FBufferedUniform1i muFogEnabled;
		FBufferedUniform1i muTextureMode;
		FBufferedUniform4f muLightParms;
		FBufferedUniform2f muClipSplit;
		FBufferedUniform4i muLightRange;
		FBufferedUniformPE muFogColor;
		FBufferedUniform4f muDynLightColor;
		FBufferedUniformPE muObjectColor;
		FBufferedUniformPE muObjectColor2;
		FBufferedUniformPE muAddColor;
		FBufferedUniformPE muTextureBlendColor;
		FBufferedUniformPE muTextureModulateColor;
		FBufferedUniformPE muTextureAddColor;
		FUniform4f muGlowBottomColor;
		FUniform4f muGlowTopColor;
		FUniform4f muGlowBottomPlane;
		FUniform4f muGlowTopPlane;
		FUniform4f muGradientBottomPlane;
		FUniform4f muGradientTopPlane;
		FUniform4f muSplitBottomPlane;
		FUniform4f muSplitTopPlane;
		FUniform4f muDetailParms;
		FBufferedUniform1f muInterpolationFactor;
		FBufferedUniform1f muAlphaThreshold;
		FBufferedUniform2f muSpecularMaterial;
		FBufferedUniform1f muTimer;
#ifdef NPOT_EMULATION
		FBufferedUniform2f muNpotEmulation;
#endif
		FUniform4f muFixedColormapStart;
		FUniform4f muFixedColormapRange;


		int lights_index = 0;
		int modelmatrix_index = 0;
		int normalmodelmatrix_index = 0;
		int texturematrix_index = 0;

		int currentglowstate = 0;
		int currentgradientstate = 0;
		int currentsplitstate = 0;
		int currentcliplinestate = 0;
		int currentfixedcolormap = 0;
		bool currentTextureMatrixState = true;// by setting the matrix state to 'true' it is guaranteed to be set the first time the render state gets applied.
		bool currentModelMatrixState = true;

		unsigned int GetHandle() const { return hShader; }
	};

	std::map<uint32_t, ShaderVariantData*> variants;

	ShaderVariantData* cur = 0;

public:
	FShader(const char *name)
		: mName(name)
	{

	}

	~FShader();

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char * light_fragprog, const char *defines);
	bool Configure(const char* name, const char* vert_prog_lump, const char* fragprog, const char* fragprog2, const char* light_fragprog, const char* defines);

	void LoadVariant();


	uint32_t CreateShaderTag(ShaderFlavourData &flavour)
	{
		uint32_t tag = 0;
		tag |= (flavour.textureMode & 0x7);

		tag |= (flavour.texFlags & 7) << 3;

		tag |= (flavour.blendFlags & 7) << 6;

		tag |= (flavour.twoDFog & 1) << 7;
		tag |= (flavour.fogEnabled & 1) << 8;
		tag |= (flavour.fogEquationRadial & 1) << 9;
		tag |= (flavour.colouredFog & 1) << 10;

		tag |= (flavour.doDesaturate & 1) << 11;

		tag |= (flavour.dynLightsMod & 1) << 12;
		tag |= (flavour.dynLightsSub & 1) << 13;
		tag |= (flavour.dynLightsAdd & 1) << 14;
		tag |= (flavour.useULightLevel & 1) << 15;
		tag |= (flavour.useObjectColor2 & 1) << 16;
		tag |= (flavour.useGlowTopColor & 1) << 17;
		tag |= (flavour.useGlowBottomColor & 1) << 18;
		tag |= (flavour.useColorMap & 1) << 19;
		tag |= (flavour.buildLighting & 1) << 20;
		tag |= (flavour.bandedSwLight & 1) << 21;

#ifdef NPOT_EMULATION
		tag |= (flavour.npotEmulation & 1) << 22;
#endif
		tag |= (flavour.hasSpotLight & 1) << 23;
		tag |= (flavour.paletteInterpolate & 1) << 24;

		return tag;
	}

	bool Bind(ShaderFlavourData& flavour);


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

	FShader *BindEffect(int effect, EPassType passType, ShaderFlavourData& flavour);
	FShader *Get(unsigned int eff, bool alphateston, EPassType passType);

	void SetActiveShader(FShader::ShaderVariantData *sh);
private:

	FShader::ShaderVariantData *mActiveShader = nullptr;
	TArray<FShaderCollection*> mPassShaders;

	friend class FShader;
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
	FShader *BindEffect(int effect, ShaderFlavourData& flavour);

	FShader *Get(unsigned int eff, bool alphateston)
	{
		// indices 0-2 match the warping modes, 3 no texture, the following are custom
		if (!alphateston && eff <= 2)
		{
			return mMaterialShadersNAT[eff];	// Non-alphatest shaders are only created for default, warp1+2 and brightmap. The rest won't get used anyway
		}
		if (eff < mMaterialShaders.Size())
		{
			return mMaterialShaders[eff];
		}
		else // This can happen if we try and active a user shader which is not loaded, so return default shader so it does not crash
		{
			return mMaterialShaders[0];
		}
	}
};

}
#endif

