/*
** gl_shader.cpp
**
** GLSL shader handling
**
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include "gl/system/gl_system.h"
#include "c_cvars.h"
#include "v_video.h"
#include "name.h"
#include "w_wad.h"
#include "i_system.h"
#include "doomerrors.h"
#include "v_palette.h"
#include "sc_man.h"
#include "cmdlib.h"

#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_matrix.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_shader.h"
#include "gl/shaders/gl_shaderprogram.h"
#include "gl/textures/gl_material.h"
#include "gl/dynlights/gl_lightbuffer.h"

//==========================================================================
//
//
//
//==========================================================================

bool FShader::Load(const char * name, const char * vert_prog_lump, const char * frag_prog_lump, const char * proc_prog_lump, const char * defines)
{
	static char buffer[10000];
	FString error;

	int i_lump = Wads.CheckNumForFullName("shaders/glsl/shaderdefs.i");
	if (i_lump == -1) I_Error("Unable to load 'shaders/glsl/shaderdefs.i'");
	FMemLump i_data = Wads.ReadLump(i_lump);

	int vp_lump = Wads.CheckNumForFullName(vert_prog_lump);
	if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump);
	FMemLump vp_data = Wads.ReadLump(vp_lump);

	int fp_lump = Wads.CheckNumForFullName(frag_prog_lump);
	if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump);
	FMemLump fp_data = Wads.ReadLump(fp_lump);



//
// The following code uses GetChars on the strings to get rid of terminating 0 characters. Do not remove or the code may break!
//
	FString vp_comb;

	if (gl.lightmethod == LM_SOFTWARE)
	{
		if (gl.glslversion >= 1.3)
		{
			vp_comb = "#version 130\n";
		}
		else
		{
			vp_comb = "#define GLSL12_COMPATIBLE\n";
		}
	}
	else
	{
		assert(GLRenderer->mLights != NULL);
		// On the shader side there is no difference between LM_DEFERRED and LM_DIRECT, it only matters which buffer type is used by the light buffer.
		unsigned int lightbuffertype = GLRenderer->mLights->GetBufferType();
		unsigned int lightbuffersize = GLRenderer->mLights->GetBlockSize();
		if (lightbuffertype == GL_UNIFORM_BUFFER)
		{
			// This differentiation is for some Intel drivers which fail on #extension, so use of #version 140 is necessary
			if (gl.glslversion < 1.4f || gl.version < 3.1f)
			{
				vp_comb.Format("#version 130\n#extension GL_ARB_uniform_buffer_object : require\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
			}
			else
			{
				vp_comb.Format("#version 140\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
			}
		}
		else
		{
			vp_comb = "#version 400 core\n#extension GL_ARB_shader_storage_buffer_object : require\n#define SHADER_STORAGE_LIGHTS\n";
		}
	}

	vp_comb << defines << i_data.GetString().GetChars();
	FString fp_comb = vp_comb;

	vp_comb << vp_data.GetString().GetChars() << "\n";
	fp_comb << fp_data.GetString().GetChars() << "\n";

	if (proc_prog_lump != NULL)
	{
		if (*proc_prog_lump != '#')
		{
			int pp_lump = Wads.CheckNumForFullName(proc_prog_lump);
			if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump);
			FMemLump pp_data = Wads.ReadLump(pp_lump);

			if (pp_data.GetString().IndexOf("ProcessTexel") < 0)
			{
				// this looks like an old custom hardware shader.
				// We need to replace the ProcessTexel call to make it work.

				fp_comb.Substitute("vec4 frag = ProcessTexel();", "vec4 frag = Process(vec4(1.0));");
			}
			fp_comb << pp_data.GetString().GetChars();
			fp_comb.Substitute("gl_TexCoord[0]", "vTexCoord");	// fix old custom shaders.

			if (pp_data.GetString().IndexOf("ProcessLight") < 0)
			{
				int pl_lump = Wads.CheckNumForFullName("shaders/glsl/func_defaultlight.fp");
				if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultlight.fp");
				FMemLump pl_data = Wads.ReadLump(pl_lump);
				fp_comb << "\n" << pl_data.GetString().GetChars();
			}
		}
		else
		{
			// Proc_prog_lump is not a lump name but the source itself (from generated shaders)
			fp_comb << proc_prog_lump + 1;
		}
	}

	if (gl.glslversion < 1.3)
	{
		FShaderProgram::PatchVertShader(vp_comb);
		FShaderProgram::PatchFragShader(fp_comb);
	}
	else if (gl.flags & RFL_NO_CLIP_PLANES)
	{
		// On ATI's GL3 drivers we have to disable gl_ClipDistance because it's hopelessly broken.
		// This will cause some glitches and regressions but is the only way to avoid total display garbage.
		vp_comb.Substitute("gl_ClipDistance", "//");
	}

	hVertProg = glCreateShader(GL_VERTEX_SHADER);
	hFragProg = glCreateShader(GL_FRAGMENT_SHADER);	


	int vp_size = (int)vp_comb.Len();
	int fp_size = (int)fp_comb.Len();

	const char *vp_ptr = vp_comb.GetChars();
	const char *fp_ptr = fp_comb.GetChars();

	glShaderSource(hVertProg, 1, &vp_ptr, &vp_size);
	glShaderSource(hFragProg, 1, &fp_ptr, &fp_size);

	glCompileShader(hVertProg);
	glCompileShader(hFragProg);

	hShader = glCreateProgram();

	glAttachShader(hShader, hVertProg);
	glAttachShader(hShader, hFragProg);

	glBindAttribLocation(hShader, VATTR_VERTEX, "aPosition");
	glBindAttribLocation(hShader, VATTR_TEXCOORD, "aTexCoord");
	glBindAttribLocation(hShader, VATTR_COLOR, "aColor");
	glBindAttribLocation(hShader, VATTR_VERTEX2, "aVertex2");

	glLinkProgram(hShader);

	glGetShaderInfoLog(hVertProg, 10000, NULL, buffer);
	if (*buffer) 
	{
		error << "Vertex shader:\n" << buffer << "\n";
	}
	glGetShaderInfoLog(hFragProg, 10000, NULL, buffer);
	if (*buffer) 
	{
		error << "Fragment shader:\n" << buffer << "\n";
	}

	glGetProgramInfoLog(hShader, 10000, NULL, buffer);
	if (*buffer) 
	{
		error << "Linking:\n" << buffer << "\n";
	}
	int linked;
	glGetProgramiv(hShader, GL_LINK_STATUS, &linked);
	if (linked == 0)
	{
		// only print message if there's an error.
		I_Error("Init Shader '%s':\n%s\n", name, error.GetChars());
	}


	muDesaturation.Init(hShader, "uDesaturationFactor");
	muFogEnabled.Init(hShader, "uFogEnabled");
	muTextureMode.Init(hShader, "uTextureMode");
	muCameraPos.Init(hShader, "uCameraPos");
	muLightParms.Init(hShader, "uLightAttr");
	muClipSplit.Init(hShader, "uClipSplit");
	muColormapStart.Init(hShader, "uFixedColormapStart");
	muColormapRange.Init(hShader, "uFixedColormapRange");
	muLightIndex.Init(hShader, "uLightIndex");
	muFogColor.Init(hShader, "uFogColor");
	muDynLightColor.Init(hShader, "uDynLightColor");
	muObjectColor.Init(hShader, "uObjectColor");
	muGlowBottomColor.Init(hShader, "uGlowBottomColor");
	muGlowTopColor.Init(hShader, "uGlowTopColor");
	muGlowBottomPlane.Init(hShader, "uGlowBottomPlane");
	muGlowTopPlane.Init(hShader, "uGlowTopPlane");
	muSplitBottomPlane.Init(hShader, "uSplitBottomPlane");
	muSplitTopPlane.Init(hShader, "uSplitTopPlane");
	muClipLine.Init(hShader, "uClipLine");
	muFixedColormap.Init(hShader, "uFixedColormap");
	muInterpolationFactor.Init(hShader, "uInterpolationFactor");
	muClipHeight.Init(hShader, "uClipHeight");
	muClipHeightDirection.Init(hShader, "uClipHeightDirection");
	muAlphaThreshold.Init(hShader, "uAlphaThreshold");
	muTimer.Init(hShader, "timer");

	lights_index = glGetUniformLocation(hShader, "lights");
	fakevb_index = glGetUniformLocation(hShader, "fakeVB");
	projectionmatrix_index = glGetUniformLocation(hShader, "ProjectionMatrix");
	viewmatrix_index = glGetUniformLocation(hShader, "ViewMatrix");
	modelmatrix_index = glGetUniformLocation(hShader, "ModelMatrix");
	texturematrix_index = glGetUniformLocation(hShader, "TextureMatrix");

	if (LM_SOFTWARE != gl.lightmethod && !(gl.flags & RFL_SHADER_STORAGE_BUFFER))
	{
		int tempindex = glGetUniformBlockIndex(hShader, "LightBufferUBO");
		if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, LIGHTBUF_BINDINGPOINT);
	}

	glUseProgram(hShader);

	// set up other texture units (if needed by the shader)
	for (int i = 2; i<16; i++)
	{
		char stringbuf[20];
		mysnprintf(stringbuf, 20, "texture%d", i);
		int tempindex = glGetUniformLocation(hShader, stringbuf);
		if (tempindex > 0) glUniform1i(tempindex, i - 1);
	}

	glUseProgram(0);
	return !!linked;
}

//==========================================================================
//
//
//
//==========================================================================

FShader::~FShader()
{
	glDeleteProgram(hShader);
	glDeleteShader(hVertProg);
	glDeleteShader(hFragProg);
}


//==========================================================================
//
//
//
//==========================================================================

bool FShader::Bind()
{
	GLRenderer->mShaderManager->SetActiveShader(this);
	return true;
}

//==========================================================================
//
// Since all shaders are REQUIRED, any error here needs to be fatal
//
//==========================================================================

FShader *FShaderManager::Compile (const char *ShaderName, const char *ShaderPath, bool usediscard)
{
	FString defines;
	// this can't be in the shader code due to ATI strangeness.
	if (gl.MaxLights() == 128) defines += "#define MAXLIGHTS128\n";
	if (!usediscard) defines += "#define NO_ALPHATEST\n";

	FShader *shader = NULL;
	try
	{
		shader = new FShader(ShaderName);
		if (!shader->Load(ShaderName, "shaders/glsl/main.vp", "shaders/glsl/main.fp", ShaderPath, defines.GetChars()))
		{
			I_FatalError("Unable to load shader %s\n", ShaderName);
		}
	}
	catch(CRecoverableError &err)
	{
		if (shader != NULL) delete shader;
		shader = NULL;
		I_FatalError("Unable to load shader %s:\n%s\n", ShaderName, err.GetMessage());
	}
	return shader;
}

//==========================================================================
//
//
//
//==========================================================================

void FShader::ApplyMatrices(VSMatrix *proj, VSMatrix *view)
{
	Bind();
	glUniformMatrix4fv(projectionmatrix_index, 1, false, proj->get());
	glUniformMatrix4fv(viewmatrix_index, 1, false, view->get());
}


//==========================================================================
//
//
//
//==========================================================================
struct FDefaultShader 
{
	const char * ShaderName;
	const char * gettexelfunc;
};

// Note: the FIRST_USER_SHADER constant in gl_shader.h needs 
// to be updated whenever the size of this array is modified.
static const FDefaultShader defaultshaders[]=
{	
	{"Default",	"shaders/glsl/func_normal.fp"},
	{"Warp 1",	"shaders/glsl/func_warp1.fp"},
	{"Warp 2",	"shaders/glsl/func_warp2.fp"},
	{"Brightmap","shaders/glsl/func_brightmap.fp"},
	{"No Texture", "shaders/glsl/func_notexture.fp"},
	{"Basic Fuzz", "shaders/glsl/fuzz_standard.fp"},
	{"Smooth Fuzz", "shaders/glsl/fuzz_smooth.fp"},
	{"Swirly Fuzz", "shaders/glsl/fuzz_swirly.fp"},
	{"Translucent Fuzz", "shaders/glsl/fuzz_smoothtranslucent.fp"},
	{"Jagged Fuzz", "shaders/glsl/fuzz_jagged.fp"},
	{"Noise Fuzz", "shaders/glsl/fuzz_noise.fp"},
	{"Smooth Noise Fuzz", "shaders/glsl/fuzz_smoothnoise.fp"},
	{NULL,NULL}
};

static TArray<FString> usershaders;

struct FEffectShader
{
	const char *ShaderName;
	const char *vp;
	const char *fp1;
	const char *fp2;
	const char *defines;
};

static const FEffectShader effectshaders[]=
{
	{ "fogboundary", "shaders/glsl/main.vp", "shaders/glsl/fogboundary.fp", NULL, "#define NO_ALPHATEST\n" },
	{ "spheremap", "shaders/glsl/main.vp", "shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "#define SPHEREMAP\n#define NO_ALPHATEST\n" },
	{ "burn", "shaders/glsl/main.vp", "shaders/glsl/burn.fp", NULL, "#define SIMPLE\n#define NO_ALPHATEST\n" },
	{ "stencil", "shaders/glsl/main.vp", "shaders/glsl/stencil.fp", NULL, "#define SIMPLE\n#define NO_ALPHATEST\n" },
};


//==========================================================================
//
//
//
//==========================================================================

FShaderManager::FShaderManager()
{
	if (gl.glslversion > 0) CompileShaders();
}

//==========================================================================
//
//
//
//==========================================================================

FShaderManager::~FShaderManager()
{
	if (gl.glslversion > 0) Clean();
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderManager::CompileShaders()
{
	mActiveShader = NULL;

	mTextureEffects.Clear();
	mTextureEffectsNAT.Clear();
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i] = NULL;
	}

	for(int i=0;defaultshaders[i].ShaderName != NULL;i++)
	{
		FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, true);
		mTextureEffects.Push(shc);
		if (i <= 3)
		{
			FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, false);
			mTextureEffectsNAT.Push(shc);
		}
	}

	for(unsigned i = 0; i < usershaders.Size(); i++)
	{
		FString name = ExtractFileBase(usershaders[i]);
		FName sfn = name;

		FShader *shc = Compile(sfn, usershaders[i], true);
		mTextureEffects.Push(shc);
	}

	for(int i=0;i<MAX_EFFECTS;i++)
	{
		FShader *eff = new FShader(effectshaders[i].ShaderName);
		if (!eff->Load(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
						effectshaders[i].fp2, effectshaders[i].defines))
		{
			delete eff;
		}
		else mEffectShaders[i] = eff;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderManager::Clean()
{
	glUseProgram(0);
	mActiveShader = NULL;

	for (unsigned int i = 0; i < mTextureEffectsNAT.Size(); i++)
	{
		if (mTextureEffectsNAT[i] != NULL) delete mTextureEffectsNAT[i];
	}
	for (unsigned int i = 0; i < mTextureEffects.Size(); i++)
	{
		if (mTextureEffects[i] != NULL) delete mTextureEffects[i];
	}
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		if (mEffectShaders[i] != NULL) delete mEffectShaders[i];
		mEffectShaders[i] = NULL;
	}
	mTextureEffects.Clear();
	mTextureEffectsNAT.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

int FShaderManager::Find(const char * shn)
{
	FName sfn = shn;

	for(unsigned int i=0;i<mTextureEffects.Size();i++)
	{
		if (mTextureEffects[i]->mName == sfn)
		{
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderManager::SetActiveShader(FShader *sh)
{
	if (mActiveShader != sh)
	{
		glUseProgram(sh!= NULL? sh->GetHandle() : 0);
		mActiveShader = sh;
	}
}


//==========================================================================
//
//
//
//==========================================================================

FShader *FShaderManager::BindEffect(int effect)
{
	if (effect >= 0 && effect < MAX_EFFECTS && mEffectShaders[effect] != NULL)
	{
		mEffectShaders[effect]->Bind();
		return mEffectShaders[effect];
	}
	return NULL;
}


//==========================================================================
//
//
//
//==========================================================================
EXTERN_CVAR(Int, gl_fuzztype)

void FShaderManager::ApplyMatrices(VSMatrix *proj, VSMatrix *view)
{
	if (gl.glslversion == 0)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(proj->get());
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(view->get());
	}
	else
	{
		for (int i = 0; i < 4; i++)
		{
			mTextureEffects[i]->ApplyMatrices(proj, view);
			mTextureEffectsNAT[i]->ApplyMatrices(proj, view);
		}
		mTextureEffects[4]->ApplyMatrices(proj, view);
		if (gl_fuzztype != 0)
		{
			mTextureEffects[4 + gl_fuzztype]->ApplyMatrices(proj, view);
		}
		for (unsigned i = 12; i < mTextureEffects.Size(); i++)
		{
			mTextureEffects[i]->ApplyMatrices(proj, view);
		}
		for (int i = 0; i < MAX_EFFECTS; i++)
		{
			mEffectShaders[i]->ApplyMatrices(proj, view);
		}
		if (mActiveShader != NULL) mActiveShader->Bind();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void gl_DestroyUserShaders()
{
	// todo
}

//==========================================================================
//
// Parses a shader definition
//
//==========================================================================

void gl_ParseHardwareShader(FScanner &sc, int deflump)
{
	int type = FTexture::TEX_Any;
	bool disable_fullbright=false;
	bool thiswad = false;
	bool iwad = false;
	int maplump = -1;
	FString maplumpname;
	float speed = 1.f;

	sc.MustGetString();
	if (sc.Compare("texture")) type = FTexture::TEX_Wall;
	else if (sc.Compare("flat")) type = FTexture::TEX_Flat;
	else if (sc.Compare("sprite")) type = FTexture::TEX_Sprite;
	else sc.UnGet();

	sc.MustGetString();
	FTextureID no = TexMan.CheckForTexture(sc.String, type);
	FTexture *tex = TexMan[no];

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetString();
		if (sc.Compare("shader"))
		{
			sc.MustGetString();
			maplumpname = sc.String;
		}
		else if (sc.Compare("speed"))
		{
			sc.MustGetFloat();
			speed = float(sc.Float);
		}
	}
	if (!tex)
	{
		return;
	}

	if (maplumpname.IsNotEmpty())
	{
		if (tex->bWarped != 0)
		{
			Printf("Cannot combine warping with hardware shader on texture '%s'\n", tex->Name.GetChars());
			return;
		}
		tex->gl_info.shaderspeed = speed; 
		for(unsigned i=0;i<usershaders.Size();i++)
		{
			if (!usershaders[i].CompareNoCase(maplumpname))
			{
				tex->gl_info.shaderindex = i + FIRST_USER_SHADER;
				return;
			}
		}
		tex->gl_info.shaderindex = usershaders.Push(maplumpname) + FIRST_USER_SHADER;
	}	
}

