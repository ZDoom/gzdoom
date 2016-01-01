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
#include "gl/textures/gl_material.h"
#include "gl/dynlights/gl_lightbuffer.h"

TArray<FShaderDefinition *> CoordinateShaders;
TArray<FShaderDefinition *> TexelShaders;
TArray<FShaderDefinition *> LightShaders;

static TArray<FShaderDefinition *> *ShaderArrays[] = { &CoordinateShaders, &TexelShaders, &LightShaders };

//==========================================================================
//
//
//
//==========================================================================

bool FShader::Load(const char * name, const char * vert_prog_lump, const char * frag_prog_lump, const char * tex_proc_lump, const char *coord_proc_lump, const char *lite_proc_lump, const char * defines)
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
	unsigned int lightbuffertype = GLRenderer->mLights->GetBufferType();
	unsigned int lightbuffersize = GLRenderer->mLights->GetBlockSize();

	FString vp_comb;

	if (lightbuffertype == GL_UNIFORM_BUFFER)
	{
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

	vp_comb << defines << i_data.GetString().GetChars();
	FString fp_comb = vp_comb;

	vp_comb << vp_data.GetString().GetChars() << "\n";
	fp_comb << fp_data.GetString().GetChars() << "\n";

	if (tex_proc_lump != NULL)
	{
		if (*tex_proc_lump != '#')
		{
			int pp_lump = Wads.CheckNumForFullName(tex_proc_lump);
			if (pp_lump == -1) I_Error("Unable to load '%s'", tex_proc_lump);
			FMemLump pp_data = Wads.ReadLump(pp_lump);

			if (pp_data.GetString().IndexOf("ProcessTexel") < 0)
			{
				// this looks like an old custom hardware shader.
				// We need to replace the ProcessTexel call to make it work.

				fp_comb.Substitute("vec4 frag = ProcessTexel(coord);", "vec4 frag = Process(vec4(1.0));");
			}
			fp_comb << pp_data.GetString().GetChars();

			// fix old custom shaders which used the builtin variables. 
			// This replacement will essentially skip the coordinate function, but since that's 'none' anyway, it doesn't matter.
			fp_comb.Substitute("gl_TexCoord[0]", "vTexCoord");	
		}
		else
		{
			// Proc_prog_lump is not a lump name but the source itself (from generated shaders)
			fp_comb << tex_proc_lump + 1;
		}
	}
	if (coord_proc_lump != NULL)
	{
		int pp_lump = Wads.CheckNumForFullName(coord_proc_lump);
		if (pp_lump == -1) I_Error("Unable to load '%s'", coord_proc_lump);
		FMemLump pp_data = Wads.ReadLump(pp_lump);

		fp_comb << pp_data.GetString().GetChars();
	}

	if (lite_proc_lump != NULL)
	{
		int pp_lump = Wads.CheckNumForFullName(lite_proc_lump);
		if (pp_lump == -1) I_Error("Unable to load '%s'", lite_proc_lump);
		FMemLump pp_data = Wads.ReadLump(pp_lump);

		fp_comb << pp_data.GetString().GetChars();
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
	muFixedColormap.Init(hShader, "uFixedColormap");
	muInterpolationFactor.Init(hShader, "uInterpolationFactor");
	muClipHeightTop.Init(hShader, "uClipHeightTop");
	muClipHeightBottom.Init(hShader, "uClipHeightBottom");
	muAlphaThreshold.Init(hShader, "uAlphaThreshold");
	muTimer.Init(hShader, "timer");

	lights_index = glGetUniformLocation(hShader, "lights");
	fakevb_index = glGetUniformLocation(hShader, "fakeVB");
	projectionmatrix_index = glGetUniformLocation(hShader, "ProjectionMatrix");
	viewmatrix_index = glGetUniformLocation(hShader, "ViewMatrix");
	modelmatrix_index = glGetUniformLocation(hShader, "ModelMatrix");
	texturematrix_index = glGetUniformLocation(hShader, "TextureMatrix");

	int tempindex = glGetUniformBlockIndex(hShader, "LightBufferUBO");
	if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, LIGHTBUF_BINDINGPOINT);
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

FShader *FShaderManager::Compile (const char *ShaderName, FShaderDefinition *CoordShader, FShaderDefinition *TexShader, FShaderDefinition *LightShader, bool usediscard)
{
	FString defines;
	// this can't be in the shader code due to ATI strangeness.
	if (gl.MaxLights() == 128) defines += "#define MAXLIGHTS128\n";
	if (!usediscard) defines += "#define NO_ALPHATEST\n";

	FShader *shader = NULL;
	try
	{
		shader = new FShader(ShaderName);
		if (!shader->Load(ShaderName, "shaders/glsl/main.vp", "shaders/glsl/main.fp", TexShader->mSourceFile, CoordShader->mSourceFile, LightShader->mSourceFile, defines.GetChars()))
		{
			I_FatalError("Unable to load shader %s\n", ShaderName);
		}
		else
		{

			for (unsigned int i = 0; i<TexShader->mTextureUnitNames.Size(); i++)
			{
				shader->mTexUnitNames.Push(TexShader->mTextureUnitNames[i]);
	}
			for (unsigned int i = 0; i<LightShader->mTextureUnitNames.Size(); i++)
			{
				shader->mTexUnitNames.Push(LightShader->mTextureUnitNames[i]);
			}

			if (shader->mTexUnitNames.Size() > 0)
			{
				int texunit = 1;
				glUseProgram(shader->hShader);
				for (unsigned int i = 0; i < shader->mTexUnitNames.Size(); i++, texunit++)
				{
					int tempindex = glGetUniformLocation(shader->hShader, shader->mTexUnitNames[i]);
					if (tempindex >= 0) glUniform1i(tempindex, texunit);
					else I_Error("Unknown texture sampler name '%s'", shader->mTexUnitNames[i]);
				}
				glUseProgram(0);
			}
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

struct FEffectShader
{
	const char *ShaderName;
	const char *vp;
	const char *fp1;
	const char *fp2;
	const char *fp3;
	const char *defines;
};

static const FEffectShader effectshaders[]=
{
	{ "fogboundary", "shaders/glsl/main.vp", "shaders/glsl/fogboundary.fp", NULL, NULL, "#define NO_ALPHATEST\n" },
	{ "spheremap", "shaders/glsl/main.vp", "shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "shaders/glsl/func_defaultlight.fp", "#define SPHEREMAP\n#define NO_ALPHATEST\n" },
	{ "burn", "shaders/glsl/main.vp", "shaders/glsl/burn.fp", NULL, NULL, "#define SIMPLE\n#define NO_ALPHATEST\n" },
	{ "stencil", "shaders/glsl/main.vp", "shaders/glsl/stencil.fp", NULL, NULL, "#define SIMPLE\n#define NO_ALPHATEST\n" },
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

unsigned int FShaderManager::GetShaderIndex(FName coord, FName tex, FName lite)
{
	for (unsigned int i = 0; i < mShaders.Size(); i++)
	{
		if (mShaders[i].mCoordinateName == coord && mShaders[i].mTexelName == tex && mShaders[i].mLightName == lite)
		{
			return i;
		}
	}
	for (unsigned int k = 0; k < CoordinateShaders.Size(); k++)
	{
		if (CoordinateShaders[k]->mName == coord)
		{

			for (unsigned int i = 0; i < TexelShaders.Size(); i++)
			{
				if (TexelShaders[i]->mName == tex)
				{
					unsigned int j = 0;
					if (!TexelShaders[i]->mNoLightShader)
					{
						for (j = 0; j < LightShaders.Size(); j++)
						{
							if (LightShaders[j]->mName == lite) break;
						}
						if (j == LightShaders.Size()) j = 0;	// 0 is the default
					}

					FString shname;
					unsigned int ndx = mShaders.Reserve(1);
					FShaderContainer *cont = &mShaders[ndx];

					shname << CoordinateShaders[k]->mName << "::" << TexelShaders[i]->mName << "::" << LightShaders[j]->mName;
					DPrintf("Compiling shader %s\n", shname);

					cont->mCoordinateName = coord;
					cont->mTexelName = tex;
					cont->mLightName = lite;
					cont->mShader = cont->mShaderNAT = NULL;
					cont->mShader = Compile(shname, CoordinateShaders[k], TexelShaders[i], LightShaders[j], true);

					if (!TexelShaders[i]->bRequireAlphaTest)
					{
						cont->mShaderNAT = Compile(shname, CoordinateShaders[k], TexelShaders[i], LightShaders[j], false);
					}
					return ndx;
				}
			}
		}
	}
	// A shader with the required settings cannot be created so fall back to the default shader.
	return SHADER_DEFAULT;
}

//==========================================================================
//
//
//
//==========================================================================

static const char *defShaderNames[] =
{
	"No Texture",
	"None",
	"Fuzz Standard",
	"Fuzz Smooth",
	"Fuzz Swirly",
	"Fuzz Translucent",
	"Fuzz Jagged",
	"Fuzz Noise",
	"Fuzz Smooth Noise",
	NULL
};


void FShaderManager::CompileShaders()
{
	mActiveShader = NULL;

	mShaders.Clear();
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i] = NULL;
	}

	// load the ones the engine accesses directly in order first. The rest gets set up on a need to use basis.
	for (int i = 0; defShaderNames[i]; i++)
	{
		GetShaderIndex(NAME_None, defShaderNames[i], NAME_None);
	}

	for (int i = 0; i < TexMan.NumTextures(); i++)
	{
		FTexture *tex = TexMan.ByIndex(i);

		if (tex->bWarped == 1) tex->gl_info.coordinateFunction = "Warp 1";
		else if (tex->bWarped == 2) tex->gl_info.coordinateFunction = "Warp 2";

		GetShaderIndex(tex->gl_info.coordinateFunction, tex->gl_info.texelFunction, tex->gl_info.lightFunction);
	}

	for(int i=0;i<MAX_EFFECTS;i++)
	{
		FShader *eff = new FShader(effectshaders[i].ShaderName);
		if (!eff->Load(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
						effectshaders[i].fp2, effectshaders[i].fp3, NULL, effectshaders[i].defines))
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

	for (unsigned int i = 0; i < mShaders.Size(); i++)
	{
		if (mShaders[i].mShader != NULL) delete mShaders[i].mShader;
		if (mShaders[i].mShaderNAT != NULL) delete mShaders[i].mShaderNAT;
	}
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		if (mEffectShaders[i] != NULL) delete mEffectShaders[i];
		mEffectShaders[i] = NULL;
	}
	mShaders.Clear();
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

void FShaderManager::ApplyMatrices(VSMatrix *proj, VSMatrix *view)
{
	for (unsigned int i = 0; i < mShaders.Size(); i++)
	{
		if (mShaders[i].mShader != NULL) mShaders[i].mShader->ApplyMatrices(proj, view);
		if (mShaders[i].mShaderNAT != NULL) mShaders[i].mShaderNAT->ApplyMatrices(proj, view);
	}
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i]->ApplyMatrices(proj, view);
	}
	if (mActiveShader != NULL) mActiveShader->Bind();
}

//==========================================================================
//
// Find a shader definition
//
//==========================================================================

static unsigned int FindShaderDef(TArray<FShaderDefinition *> &defarray, FName name)
{
	for (unsigned int i = 0; i < defarray.Size(); i++)
	{
		if (defarray[i]->mName == name)
		{
			return i;
		}
	}
	return UINT_MAX;
}

//==========================================================================
//
// Parses an old hardware shader definition
// This feature is deprecated so its functionality is intentionally
// limited to what it was before.
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
	if (!tex)
	{
		sc.ScriptMessage("Texture '%s' not found\n", sc.String);
	}

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetString();
		if (sc.Compare("shader"))
		{
			sc.MustGetString();
			maplumpname = sc.String;
			maplumpname.ToLower();
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
			sc.ScriptMessage("Cannot combine warping with hardware shader on texture '%s'\n", tex->Name.GetChars());
			return;
		}
		tex->gl_info.shaderspeed = speed; 

		FShaderDefinition *def;
		FName nm = maplumpname + "@@@";
		unsigned int defindex = FindShaderDef(TexelShaders, nm);
		if (defindex == UINT_MAX)
		{
			def = new FShaderDefinition;
			def->mName = nm;
			def->mCoreLump = false;
			def->mNoLightShader = true;
			def->mNoCoordinateShader = false;
			def->mSourceFile = maplumpname;
			TexelShaders.Push(def);
		}
		else
		{
			def = TexelShaders[defindex];
			def->mName = nm;
			def->mCoreLump = false;
			def->mNoLightShader = true;
			def->mNoCoordinateShader = false;
			def->mSourceFile = maplumpname;
		}
		tex->gl_info.texelFunction = nm;
	}	
}

static const char * const uniformtypes[] = {
	"int",
	"ivec2",
	"ivec3",
	"ivec4",
	"float",
	"vec2",
	"vec3",
	"vec4",
	NULL
};

static const char * const xy[] = {
	"x",
	"y",
	NULL
};

static const char * const xyz[] = {
	"x",
	"y",
	"z",
	NULL
};

static const char * const xyzw[] = {
	"x",
	"y",
	"z",
	"w",
	NULL
};

static const char * const * const xyzarr[] = { NULL, xy, xyz, xyzw, NULL, xy, xyz, xyzw };

void gl_ParseShaderDef(FScanner &sc, int functiontype)
{
	FShaderDefinition *def = new FShaderDefinition;

	bool CoreLump = false;
	int ut;
	sc.SetCMode(true);
	sc.MustGetString();
	def->mName = sc.String;
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
	sc.MustGetString();
	if (sc.Compare("source"))
	{
		sc.MustGetString();
		def->mSourceFile = sc.String;
		while (sc.CheckString(","))
		{
			sc.MustGetString();
			if (sc.Compare("corelump"))
			{
				CoreLump = true;
				def->mCoreLump = true;
			}
		}
	}
	else if (sc.Compare("nolightfunction"))
	{
		def->mNoLightShader = true;
	}
	else if (sc.Compare("nocoordinatefunction"))
	{
		def->mNoCoordinateShader = true;
	}
	else if (sc.Compare("requirealphatest"))
	{
		def->bRequireAlphaTest = true;
	}
		else if (sc.Compare("textureunit"))
		{
			sc.MustGetString();
			def->mTextureUnitNames.Push(sc.String);
		}
		else if ((ut = sc.MatchString(uniformtypes)) >= 0)
		{
			sc.MustGetString();

			FUniformDefinition ud;

			ud.mName = sc.String;
			ud.mRefPos = 0;
			ud.mRef = NULL;
			ud.mType = (BYTE)ut;

			unsigned int udref = def->mUniforms.Push(ud);

			if (ut != UT_INT && ut != UT_FLOAT)
			{
				if (sc.CheckString("{"))
				{
					while (!sc.CheckString("}"))
					{
						sc.MustGetString();
						int utsub = sc.MatchString(xyzarr[ut]);
						if (utsub == -1) sc.ScriptError("Unknown identifier '%s'", sc.String);
						sc.MustGetString();
						ud.mName = sc.String;
						ud.mRefPos = (BYTE)utsub + 1;
						ud.mRef = udref + 1;
						def->mUniforms.Push(ud);
					}
				}
			}
		}
		else
		{
			sc.ScriptError("Unknown token '%s'", sc.String);
	}

	}

	int lumpnum = Wads.CheckNumForFullName(def->mSourceFile);
	if (lumpnum < 0)
	{
		sc.ScriptMessage("Unable to find shader source '%s'", def->mSourceFile);
	}
	else if (CoreLump)
	{
		int wadnum = Wads.GetLumpFile(lumpnum);
		if (wadnum > FWadCollection::IWAD_FILENUM)
		{
			I_FatalError("File %s is overriding core lump %s.",
				Wads.GetWadFullName(wadnum), def->mSourceFile);
		}
	}

	TArray<FShaderDefinition*> *pArr = ShaderArrays[functiontype];
	unsigned int defindex = FindShaderDef(*pArr, def->mName);
	if (defindex != UINT_MAX)
	{
		// replacing core shaders is prohibited.
		if ((*pArr)[defindex]->mCoreLump)
		{
			sc.ScriptError("Shader function %s is overriding core definition", def->mName.GetChars());
		}
		pArr->Delete(defindex);
	}
	pArr->Push(def);
}


void gl_DestroyUserShaders()
{
	if (GLRenderer != NULL && GLRenderer->mShaderManager != NULL)
	{
		GLRenderer->mShaderManager->Clean();
	}
	for (unsigned int i = 0; i < CoordinateShaders.Size(); i++)
	{
		delete CoordinateShaders[i];
	}
	for (unsigned int i = 0; i < TexelShaders.Size(); i++)
	{
		delete TexelShaders[i];
	}
	for (unsigned int i = 0; i < LightShaders.Size(); i++)
	{
		delete LightShaders[i];
	}
	CoordinateShaders.Clear();
	TexelShaders.Clear();
	LightShaders.Clear();
}
