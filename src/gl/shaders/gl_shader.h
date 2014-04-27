
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

#include "gl/renderer/gl_renderstate.h"
#include "name.h"

extern bool gl_shaderactive;

const int VATTR_ATTRIBCOLOR = 15;
const int VATTR_FOGPARAMS = 14;
const int VATTR_LIGHTLEVEL = 13; // Korshun.

//==========================================================================
//
//
//==========================================================================

class FShader
{
	friend class FShaderContainer;
	friend class FRenderState;
	friend class FShaderManager;

	unsigned int hShader;
	unsigned int hVertProg;
	unsigned int hFragProg;
	FName mName;

	int alphathreshold_index;

	int timer_index;
	int desaturation_index;
	int fogenabled_index;
	int texturemode_index;
	int lightparms_index;
	int lightrange_index;
	int fogcolor_index;
	int lights_index;
	int dlightcolor_index;
	int glowbottomcolor_index;
	int glowtopcolor_index;
	int glowbottomplane_index;
	int glowtopplane_index;
	int objectcolor_index;
	int buffercolor_index;
	int colorcontrol_index;

	int mModelMatLocation;
	int mTexMatLocation;


	PalEntry currentobjectcolor;
	int currentwarpphase;
	int currentglowstate;
	int currentfogenabled;
	int currenttexturemode;
	float currentlightfactor;
	float currentlightdist;
	float currentalphathreshold;
	float currentdesaturation;
	int currentColorControl;

	PalEntry currentfogcolor;
	float currentfogdensity;
	unsigned int mMatrixTick[4];

public:
	FShader(const char *name)
		: mName(name)
	{
		hShader = hVertProg = hFragProg = 0;
		currentColorControl = currentfogenabled = currenttexturemode = currentglowstate = 0;
		currentdesaturation = currentlightfactor = currentlightdist = currentalphathreshold = currentwarpphase = 0.0f;
		currentfogdensity = -1;
		currentobjectcolor = currentfogcolor = 0;

		timer_index = -1;
		desaturation_index = -1;
		fogenabled_index = -1;
		texturemode_index = -1;
		lightparms_index = -1;
		lightrange_index = -1;
		fogcolor_index = -1;
		lights_index = -1;
		dlightcolor_index = -1;
		alphathreshold_index = -1;
		glowtopplane_index = -1;
		glowbottomplane_index = -1;
		glowtopcolor_index = -1;
		glowbottomcolor_index = -1;
		buffercolor_index = -1;
		colorcontrol_index = -1;


		mMatrixTick[0] = mMatrixTick[1] = mMatrixTick[2] = mMatrixTick[3] = 0;

	}

	~FShader();

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char *defines);

	void SetColormapColor(float r, float g, float b, float r1, float g1, float b1);
	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
	void SetLightRange(int start, int end, int forceadd);

	bool Bind();
	unsigned int GetHandle() const { return hShader; }

};


//==========================================================================
//
// The global shader manager
//
//==========================================================================
class FShaderManager
{
	TArray<FShader*> mTextureEffects;
	FShader *mActiveShader;
	FShader *mEffectShaders[MAX_EFFECTS];

	void Clean();
	void CompileShaders();
	
public:
	FShaderManager();
	~FShaderManager();
	FShader *Compile(const char *ShaderName, const char *ShaderPath);
	int Find(const char *mame);
	FShader *BindEffect(int effect);
	void SetActiveShader(FShader *sh);
	void SetWarpSpeed(unsigned int eff, float speed);

	FShader *Get(unsigned int eff)
	{
		// indices 0-2 match the warping modes, 3 is brightmap, 4 no texture, the following are custom
		if (eff < mTextureEffects.Size())
		{
			return mTextureEffects[eff];
		}
		return NULL;
	}
};

#define FIRST_USER_SHADER 12


#endif

