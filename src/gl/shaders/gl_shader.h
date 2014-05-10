
#ifndef __GL_SHADERS_H__
#define __GL_SHADERS_H__

#include "gl/renderer/gl_renderstate.h"
#include "name.h"

extern bool gl_shaderactive;

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

	unsigned int hShader;
	unsigned int hVertProg;
	unsigned int hFragProg;

	int timer_index;
	int desaturation_index;
	int fogenabled_index;
	int texturemode_index;
	int camerapos_index;
	int lightparms_index;
	int colormapstart_index;
	int colormaprange_index;
	int lightrange_index;
	int fogcolor_index;
	int lights_index;
	int dlightcolor_index;
	int glowbottomcolor_index;
	int glowtopcolor_index;
	int glowbottomplane_index;
	int glowtopplane_index;

	int currentglowstate;
	int currentfogenabled;
	int currenttexturemode;
	float currentlightfactor;
	float currentlightdist;

	PalEntry currentfogcolor;
	float currentfogdensity;

	FStateVec3 currentcamerapos;

public:
	FShader()
	{
		hShader = hVertProg = hFragProg = 0;
		currentglowstate = currentfogenabled = currenttexturemode = 0;
		currentlightfactor = currentlightdist = 0.0f;
		currentfogdensity = -1;
		currentfogcolor = 0;

		timer_index = -1;
		desaturation_index = -1;
		fogenabled_index = -1;
		texturemode_index = -1;
		camerapos_index = -1;
		lightparms_index = -1;
		colormapstart_index = -1;
		colormaprange_index = -1;
		lightrange_index = -1;
		fogcolor_index = -1;
		lights_index = -1;
		dlightcolor_index = -1;
		glowtopplane_index = -1;
		glowbottomplane_index = -1;
		glowtopcolor_index = -1;
		glowbottomcolor_index = -1;
	}

	~FShader();

	bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char *defines);

	void SetColormapColor(float r, float g, float b, float r1, float g1, float b1);
	void SetGlowParams(float *topcolors, float topheight, float *bottomcolors, float bottomheight);
	void SetLightRange(int start, int end, int forceadd);

	bool Bind(float Speed);
	unsigned int GetHandle() const { return hShader; }

};

//==========================================================================
//
// This class contains the shaders for the different lighting modes
// that are required (e.g. special colormaps etc.)
//
//==========================================================================

class FShaderContainer
{
	friend class FShaderManager;

	FName Name;

	enum { NUM_SHADERS = 16 };

	FShader *shader[NUM_SHADERS];
	FShader *shader_cm;	// the shader for fullscreen colormaps
	FShader *shader_fl;	// the shader for the fog layer

public:
	FShaderContainer(const char *ShaderName, const char *ShaderPath);
	~FShaderContainer();
	FShader *Bind(int cm, bool glowing, float Speed, bool lights);
};


//==========================================================================
//
// The global shader manager
//
//==========================================================================
class FShaderManager
{
	enum 
	{ 
		NUM_EFFECTS = 2 
	};

	TArray<FShaderContainer*> mTextureEffects;
	FShader *mActiveShader;
	FShader *mEffectShaders[NUM_EFFECTS];

	void Clean();
	void CompileShaders();
	
public:
	FShaderManager();
	~FShaderManager();
	int Find(const char *mame);
	FShader *BindEffect(int effect);
	void SetActiveShader(FShader *sh);

	FShaderContainer *Get(unsigned int eff)
	{
		// indices 0-2 match the warping modes, 3 is brightmap, 4 no texture, the following are custom
		if (eff < mTextureEffects.Size())
		{
			return mTextureEffects[eff];
		}
		return NULL;
	}

	void Recompile();
};

#define FIRST_USER_SHADER 12


#endif

