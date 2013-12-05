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
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"

// these will only have an effect on SM3 cards.
// For SM4 they are always on and for SM2 always off
CVAR(Bool, gl_warp_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_fog_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_colormap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_brightmap_shader, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
CVAR(Bool, gl_glow_shader, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)


extern long gl_frameMS;

//==========================================================================
//
//
//
//==========================================================================

bool FShader::Load(const char * name, const char * vert_prog_lump, const char * frag_prog_lump, const char * proc_prog_lump, const char * defines)
{
	static char buffer[10000];
	FString error;

	if (gl.shadermodel > 0)
	{
		int vp_lump = Wads.CheckNumForFullName(vert_prog_lump);
		if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump);
		FMemLump vp_data = Wads.ReadLump(vp_lump);

		int fp_lump = Wads.CheckNumForFullName(frag_prog_lump);
		if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump);
		FMemLump fp_data = Wads.ReadLump(fp_lump);


		FString vp_comb;
		FString fp_comb;
		vp_comb = defines;
		if (gl.shadermodel < 4) 
		{
			vp_comb << "#define NO_SM4\n";
		}

		fp_comb = vp_comb;
		// This uses GetChars on the strings to get rid of terminating 0 characters.
		vp_comb << vp_data.GetString().GetChars() << "\n";
		fp_comb << fp_data.GetString().GetChars() << "\n";

		if (proc_prog_lump != NULL)
		{
			if (*proc_prog_lump != '#')
			{
				int pp_lump = Wads.CheckNumForFullName(proc_prog_lump);
				if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump);
				FMemLump pp_data = Wads.ReadLump(pp_lump);

				fp_comb << pp_data.GetString().GetChars();
			}
			else 
			{
				// Proc_prog_lump is not a lump name but the source itself (from generated shaders)
				fp_comb << proc_prog_lump+1;
			}
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

		glBindAttribLocation(hShader, VATTR_GLOWDISTANCE, "glowdistance");
		glBindAttribLocation(hShader, VATTR_FOGPARAMS, "fogparams");
		glBindAttribLocation(hShader, VATTR_LIGHTLEVEL, "lightlevel_in"); // Korshun.

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
		glGetObjectParameteriv(hShader, GL_LINK_STATUS, &linked);
		if (linked == 0)
		{
			// only print message if there's an error.
			Printf("Init Shader '%s':\n%s\n", name, error.GetChars());
		}
		timer_index = glGetUniformLocation(hShader, "timer");
		desaturation_index = glGetUniformLocation(hShader, "desaturation_factor");
		fogenabled_index = glGetUniformLocation(hShader, "fogenabled");
		texturemode_index = glGetUniformLocation(hShader, "texturemode");
		camerapos_index = glGetUniformLocation(hShader, "camerapos");
		lightparms_index = glGetUniformLocation(hShader, "lightparms");
		colormapstart_index = glGetUniformLocation(hShader, "colormapstart");
		colormaprange_index = glGetUniformLocation(hShader, "colormaprange");
		lightrange_index = glGetUniformLocation(hShader, "lightrange");
		fogcolor_index = glGetUniformLocation(hShader, "fogcolor");
		lights_index = glGetUniformLocation(hShader, "lights");
		dlightcolor_index = glGetUniformLocation(hShader, "dlightcolor");

		glowbottomcolor_index = glGetUniformLocation(hShader, "bottomglowcolor");
		glowtopcolor_index = glGetUniformLocation(hShader, "topglowcolor");
		
		glUseProgram(hShader);

		int texture_index = glGetUniformLocation(hShader, "texture2");
		if (texture_index > 0) glUniform1i(texture_index, 1);

		glUseProgram(0);
		return !!linked;
	}
	return false;
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

bool FShader::Bind(float Speed)
{
	GLRenderer->mShaderManager->SetActiveShader(this);
	if (timer_index >=0 && Speed > 0.f) glUniform1f(timer_index, gl_frameMS*Speed/1000.f);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FShaderContainer::FShaderContainer(const char *ShaderName, const char *ShaderPath)
{
	const char * shaderdefines[] = {
		"#define NO_GLOW\n#define NO_DESATURATE\n",
		"#define NO_DESATURATE\n",
		"#define NO_GLOW\n",
		"\n",
		"#define NO_GLOW\n#define NO_DESATURATE\n#define DYNLIGHT\n",
		"#define NO_DESATURATE\n#define DYNLIGHT\n",
		"#define NO_GLOW\n#define DYNLIGHT\n",
		"\n#define DYNLIGHT\n",
		"#define NO_GLOW\n#define NO_DESATURATE\n#define SOFTLIGHT\n",
		"#define NO_DESATURATE\n#define SOFTLIGHT\n",
		"#define NO_GLOW\n#define SOFTLIGHT\n",
		"\n#define SOFTLIGHT\n",
		"#define NO_GLOW\n#define NO_DESATURATE\n#define DYNLIGHT\n#define SOFTLIGHT\n",
		"#define NO_DESATURATE\n#define DYNLIGHT\n#define SOFTLIGHT\n",
		"#define NO_GLOW\n#define DYNLIGHT\n#define SOFTLIGHT\n",
		"\n#define DYNLIGHT\n#define SOFTLIGHT\n"
	};

	const char * shaderdesc[] = {
		"::default",
		"::glow",
		"::desaturate",
		"::glow+desaturate",
		"::default+dynlight",
		"::glow+dynlight",
		"::desaturate+dynlight",
		"::glow+desaturate+dynlight",
		"::softlight",
		"::glow+softlight",
		"::desaturate+softlight",
		"::glow+desaturate+softlight",
		"::default+dynlight+softlight",
		"::glow+dynlight+softlight",
		"::desaturate+dynlight+softlight",
		"::glow+desaturate+dynlight+softlight",
	};

	FString name;

	name << ShaderName << "::colormap";

	try
	{
		shader_cm = new FShader;
		if (!shader_cm->Load(name, "shaders/glsl/main.vp", "shaders/glsl/main_colormap.fp", ShaderPath, "#define NO_FOG\n#define NO_GLOW\n"))
		{
			delete shader_cm;
			shader_cm = NULL;
		}
	}
	catch(CRecoverableError &err)
	{
		shader_cm = NULL;
		I_Error("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
	}

	name << ShaderName << "::foglayer";

	try
	{
		shader_fl = new FShader;
		if (!shader_fl->Load(name, "shaders/glsl/main.vp", "shaders/glsl/main_foglayer.fp", ShaderPath, "#define NO_GLOW\n"))
		{
			delete shader_fl;
			shader_fl = NULL;
		}
	}
	catch (CRecoverableError &err)
	{
		shader_fl = NULL;
		I_Error("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
	}

	if (gl.shadermodel > 2)
	{
		for(int i = 0;i < NUM_SHADERS; i++)
		{
			FString name;

			name << ShaderName << shaderdesc[i];

			try
			{
				FString str;
				if ((i&4) != 0)
				{
					if (gl.maxuniforms < 1024 || gl.shadermodel != 4)
					{
						shader[i] = NULL;
						continue;
					}
					// this can't be in the shader code due to ATI strangeness.
					str = "#version 120\n#extension GL_EXT_gpu_shader4 : enable\n";
					if (gl.MaxLights() == 128) str += "#define MAXLIGHTS128\n";
				}
				if ((i&8) == 0)
				{
					if (gl.shadermodel != 4)
					{
						shader[i] = NULL;
						continue;
					}
				}
				str += shaderdefines[i];
				shader[i] = new FShader;
				if (!shader[i]->Load(name, "shaders/glsl/main.vp", "shaders/glsl/main.fp", ShaderPath, str.GetChars()))
				{
					delete shader[i];
					shader[i] = NULL;
				}
			}
			catch(CRecoverableError &err)
			{
				shader[i] = NULL;
				I_Error("Unable to load shader %s:\n%s\n", name.GetChars(), err.GetMessage());
			}
		}
	}
	else memset(shader, 0, sizeof(shader));
}

//==========================================================================
//
//
//
//==========================================================================
FShaderContainer::~FShaderContainer()
{
	if (shader_cm != NULL) delete shader_cm;
	if (shader_fl != NULL) delete shader_fl;
	for (int i = 0; i < NUM_SHADERS; i++)
	{
		if (shader[i] != NULL)
		{
			delete shader[i];
			shader[i] = NULL;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

FShader *FShaderContainer::Bind(int cm, bool glowing, float Speed, bool lights)
{
	FShader *sh=NULL;

	if (cm == CM_FOGLAYER)
	{
		if (shader_fl)
		{
			shader_fl->Bind(Speed);
		}
		return shader_fl;
	}
	else if (cm >= CM_FIRSTSPECIALCOLORMAP && cm < CM_MAXCOLORMAP)
	{
		// these are never used with any kind of lighting or fog
		sh = shader_cm;
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
		{
			FSpecialColormap *map = &SpecialColormaps[cm - CM_FIRSTSPECIALCOLORMAP];
			sh->Bind(Speed);
			float m[3]= {map->ColorizeEnd[0] - map->ColorizeStart[0], 
				map->ColorizeEnd[1] - map->ColorizeStart[1], map->ColorizeEnd[2] - map->ColorizeStart[2]};

			glUniform3fv(sh->colormapstart_index, 1, map->ColorizeStart);
			glUniform3fv(sh->colormaprange_index, 1, m);
		}
	}
	else
	{
		bool desat = cm>=CM_DESAT1 && cm<=CM_DESAT31;
		sh = shader[glowing + 2*desat + 4*lights + (glset.lightmode & 8)];
		// [BB] If there was a problem when loading the shader, sh is NULL here.
		if( sh )
		{
			sh->Bind(Speed);
			if (desat)
			{
				glUniform1f(sh->desaturation_index, 1.f-float(cm-CM_DESAT0)/(CM_DESAT31-CM_DESAT0));
			}
		}
	}
	return sh;
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
	{"fogboundary", "shaders/glsl/main.vp", "shaders/glsl/fogboundary.fp", NULL, "#define NO_GLOW\n"},
	{"spheremap", "shaders/glsl/main.vp", "shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "#define NO_GLOW\n#define NO_DESATURATE\n#define SPHEREMAP\n#define SPHEREMAP_0\n"}
};


//==========================================================================
//
//
//
//==========================================================================

FShaderManager::FShaderManager()
{
	CompileShaders();
}

//==========================================================================
//
//
//
//==========================================================================

FShaderManager::~FShaderManager()
{
	Clean();
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderManager::Recompile()
{
	Clean();
	CompileShaders();
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderManager::CompileShaders()
{
	mActiveShader = mEffectShaders[0] = mEffectShaders[1] = NULL;
	if (gl.shadermodel > 0)
	{
		for(int i=0;defaultshaders[i].ShaderName != NULL;i++)
		{
			FShaderContainer * shc = new FShaderContainer(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc);
			mTextureEffects.Push(shc);
			if (gl.shadermodel <= 2) return;	// SM2 will only initialize the default shader
		}

		for(unsigned i = 0; i < usershaders.Size(); i++)
		{
			FString name = ExtractFileBase(usershaders[i]);
			FName sfn = name;

			if (gl.shadermodel > 2)
			{
				FShaderContainer * shc = new FShaderContainer(sfn, usershaders[i]);
				mTextureEffects.Push(shc);
			}
		}

		if (gl.shadermodel > 2)
		{
			for(int i=0;i<NUM_EFFECTS;i++)
			{
				FShader *eff = new FShader();
				if (!eff->Load(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
								effectshaders[i].fp2, effectshaders[i].defines))
				{
					delete eff;
				}
				else mEffectShaders[i] = eff;
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderManager::Clean()
{
	SetActiveShader(NULL);
	for(unsigned int i=0;i<mTextureEffects.Size();i++)
	{
		if (mTextureEffects[i] != NULL) delete mTextureEffects[i];
	}
	for(int i=0;i<NUM_EFFECTS;i++)
	{
		if (mEffectShaders[i] != NULL) delete mEffectShaders[i];
		mEffectShaders[i] = NULL;
	}
	mTextureEffects.Clear();
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
		if (mTextureEffects[i]->Name == sfn)
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
	// shadermodel needs to be tested here because without it UseProgram will be NULL.
	if (gl.shadermodel > 0 && mActiveShader != sh)
	{
		glUseProgram(sh == NULL? 0 : sh->GetHandle());
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
	if (effect > 0 && effect <= NUM_EFFECTS && mEffectShaders[effect-1] != NULL)
	{
		mEffectShaders[effect-1]->Bind(0);
		return mEffectShaders[effect-1];
	}
	return NULL;
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
			Printf("Cannot combine warping with hardware shader on texture '%s'\n", tex->Name);
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

