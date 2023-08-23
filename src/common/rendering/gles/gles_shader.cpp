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
/*
** gl_shader.cpp
**
** GLSL shader handling
**
*/

#include "gles_system.h"
#include "c_cvars.h"
#include "v_video.h"
#include "filesystem.h"
#include "engineerrors.h"
#include "cmdlib.h"
#include "md5.h"
#include "gles_shader.h"
#include "hw_shaderpatcher.h"
#include "shaderuniforms.h"
#include "hw_viewpointuniforms.h"
#include "hw_lightbuffer.h"
#include "i_specialpaths.h"
#include "printf.h"
#include "version.h"

#include "matrix.h"
#include "gles_renderer.h"
#include <map>
#include <memory>

namespace OpenGLESRenderer
{

CVAR(Int, gles_glsl_precision, 2, 0); // 0 = low, 1 = medium, 2 = high

FString GetGLSLPrecision()
{
	FString str = "precision highp int;\n \
		           precision highp float;\n";

	if (gles_glsl_precision == 0)
		str.Substitute("highp", "lowp");
	else if (gles_glsl_precision == 1)
		str.Substitute("highp", "mediump");

	return str;
}

struct ProgramBinary
{
	uint32_t format;
	TArray<uint8_t> data;
};

static const char *ShaderMagic = "ZDSC";

static std::map<FString, std::unique_ptr<ProgramBinary>> ShaderCache; // Not a TMap because it doesn't support unique_ptr move semantics

bool IsShaderCacheActive()
{
	static bool active = true;
	static bool firstcall = true;

	if (firstcall)
	{
		const char *vendor = (const char *)glGetString(GL_VENDOR);
		active = !(strstr(vendor, "Intel") == nullptr);
		firstcall = false;
	}
	return active;
}

static FString CalcProgramBinaryChecksum(const FString &vertex, const FString &fragment)
{
	const GLubyte *vendor = glGetString(GL_VENDOR);
	const GLubyte *renderer = glGetString(GL_RENDERER);
	const GLubyte *version = glGetString(GL_VERSION);

	uint8_t digest[16];
	MD5Context md5;
	md5.Update(vendor, (unsigned int)strlen((const char*)vendor));
	md5.Update(renderer, (unsigned int)strlen((const char*)renderer));
	md5.Update(version, (unsigned int)strlen((const char*)version));
	md5.Update((const uint8_t *)vertex.GetChars(), (unsigned int)vertex.Len());
	md5.Update((const uint8_t *)fragment.GetChars(), (unsigned int)fragment.Len());
	md5.Final(digest);

	char hexdigest[33];
	for (int i = 0; i < 16; i++)
	{
		int v = digest[i] >> 4;
		hexdigest[i * 2] = v < 10 ? ('0' + v) : ('a' + v - 10);
		v = digest[i] & 15;
		hexdigest[i * 2 + 1] = v < 10 ? ('0' + v) : ('a' + v - 10);
	}
	hexdigest[32] = 0;
	return hexdigest;
}

static FString CreateProgramCacheName(bool create)
{
	FString path = M_GetCachePath(create);
	if (create) CreatePath(path);
	path << "/shadercache.zdsc";
	return path;
}

static void LoadShaders()
{
	static bool loaded = false;
	if (loaded)
		return;
	loaded = true;

	try
	{
		FString path = CreateProgramCacheName(false);
		FileReader fr;
		if (!fr.OpenFile(path))
			I_Error("Could not open shader file");

		char magic[4];
		fr.Read(magic, 4);
		if (memcmp(magic, ShaderMagic, 4) != 0)
			I_Error("Not a shader cache file");

		uint32_t count = fr.ReadUInt32();
		if (count > 512)
			I_Error("Too many shaders cached");

		for (uint32_t i = 0; i < count; i++)
		{
			char hexdigest[33];
			if (fr.Read(hexdigest, 32) != 32)
				I_Error("Read error");
			hexdigest[32] = 0;

			std::unique_ptr<ProgramBinary> binary(new ProgramBinary());
			binary->format = fr.ReadUInt32();
			uint32_t size = fr.ReadUInt32();
			if (size > 1024 * 1024)
				I_Error("Shader too big, probably file corruption");

			binary->data.Resize(size);
			if (fr.Read(binary->data.Data(), binary->data.Size()) != binary->data.Size())
				I_Error("Read error");

			ShaderCache[hexdigest] = std::move(binary);
		}
	}
	catch (...)
	{
		ShaderCache.clear();
	}
}

static void SaveShaders()
{
	FString path = CreateProgramCacheName(true);
	std::unique_ptr<FileWriter> fw(FileWriter::Open(path));
	if (fw)
	{
		uint32_t count = (uint32_t)ShaderCache.size();
		fw->Write(ShaderMagic, 4);
		fw->Write(&count, sizeof(uint32_t));
		for (const auto &it : ShaderCache)
		{
			uint32_t size = it.second->data.Size();
			fw->Write(it.first.GetChars(), 32);
			fw->Write(&it.second->format, sizeof(uint32_t));
			fw->Write(&size, sizeof(uint32_t));
			fw->Write(it.second->data.Data(), it.second->data.Size());
		}
	}
}

TArray<uint8_t> LoadCachedProgramBinary(const FString &vertex, const FString &fragment, uint32_t &binaryFormat)
{
	LoadShaders();

	auto it = ShaderCache.find(CalcProgramBinaryChecksum(vertex, fragment));
	if (it != ShaderCache.end())
	{
		binaryFormat = it->second->format;
		return it->second->data;
	}
	else
	{
		binaryFormat = 0;
		return {};
	}
}

void SaveCachedProgramBinary(const FString &vertex, const FString &fragment, const TArray<uint8_t> &binary, uint32_t binaryFormat)
{
	auto &entry = ShaderCache[CalcProgramBinaryChecksum(vertex, fragment)];
	entry.reset(new ProgramBinary());
	entry->format = binaryFormat;
	entry->data = binary;

	SaveShaders();
}

bool FShader::Configure(const char* name, const char* vert_prog_lump, const char* fragprog, const char* fragprog2, const char* light_fragprog, const char* defines)
{
	mVertProg = vert_prog_lump;
	mFragProg = fragprog;
	mFragProg2 = fragprog2;
	mLightProg = light_fragprog;
	mDefinesBase = defines;

	return true;
}

void FShader::LoadVariant()
{
	//mDefinesBase
	Load(mName.GetChars(), mVertProg, mFragProg, mFragProg2, mLightProg, mDefinesBase);
}

bool FShader::Load(const char * name, const char * vert_prog_lump_, const char * frag_prog_lump_, const char * proc_prog_lump_, const char * light_fragprog_, const char * defines)
{
	ShaderVariantData* shaderData = new ShaderVariantData();

	FString vert_prog_lump = vert_prog_lump_;
	FString frag_prog_lump = frag_prog_lump_;
	FString	proc_prog_lump = proc_prog_lump_;
	FString light_fragprog = light_fragprog_;

	vert_prog_lump.Substitute("shaders/", "shaders_gles/");
	frag_prog_lump.Substitute("shaders/", "shaders_gles/");
	proc_prog_lump.Substitute("shaders/", "shaders_gles/");
	light_fragprog.Substitute("shaders/", "shaders_gles/");

	//light_fragprog.Substitute("material_pbr", "material_normal");

	if(light_fragprog.Len())
		light_fragprog = "shaders_gles/glsl/material_normal.fp"; // NOTE: Always use normal material for now, ignore others


	static char buffer[10000];
	FString error;

	FString i_data = GetGLSLPrecision();

	i_data += R"(

		// light buffers
		uniform vec4 lights[MAXIMUM_LIGHT_VECTORS];

		// bone matrix buffers
		uniform mat4 bones[MAXIMUM_LIGHT_VECTORS];

		uniform	mat4 ProjectionMatrix;
		uniform	mat4 ViewMatrix;
		uniform	mat4 NormalViewMatrix;

		uniform	vec4 uCameraPos;
		uniform	vec4 uClipLine;

		uniform	float uGlobVis;			// uGlobVis = R_GetGlobVis(r_visibility) / 32.0
		uniform	int uPalLightLevels;	
		uniform	int uViewHeight;		// Software fuzz scaling
		uniform	float uClipHeight;
		uniform	float uClipHeightDirection;
		uniform	int uShadowmapFilter;

		uniform int uTextureMode;
		uniform vec2 uClipSplit;
		uniform float uAlphaThreshold;

		// colors
		uniform vec4 uObjectColor;
		uniform vec4 uObjectColor2;
		uniform vec4 uDynLightColor;
		uniform vec4 uAddColor;
		uniform vec4 uTextureBlendColor;
		uniform vec4 uTextureModulateColor;
		uniform vec4 uTextureAddColor;
		uniform vec4 uFogColor;
		uniform float uDesaturationFactor;
		uniform float uInterpolationFactor;

		// Glowing walls stuff
		uniform vec4 uGlowTopPlane;
		uniform vec4 uGlowTopColor;
		uniform vec4 uGlowBottomPlane;
		uniform vec4 uGlowBottomColor;

		uniform vec4 uGradientTopPlane;
		uniform vec4 uGradientBottomPlane;

		uniform vec4 uSplitTopPlane;
		uniform vec4 uSplitBottomPlane;

		uniform vec4 uDetailParms;
		// Lighting + Fog
		uniform vec4 uLightAttr;
		#define uLightLevel uLightAttr.a
		#define uFogDensity uLightAttr.b
		#define uLightFactor uLightAttr.g
		#define uLightDist uLightAttr.r
		//uniform int uFogEnabled;

		// dynamic lights
		uniform ivec4 uLightRange;

		// bone animation
		uniform int uBoneIndexBase;

		// Blinn glossiness and specular level
		uniform vec2 uSpecularMaterial;

		// matrices
		uniform mat4 ModelMatrix;
		uniform mat4 NormalModelMatrix;
		uniform mat4 TextureMatrix;

		uniform vec4 uFixedColormapStart;
		uniform vec4 uFixedColormapRange;

		// textures
		uniform sampler2D tex;
		uniform sampler2D ShadowMap;
		uniform sampler2D texture2;
		uniform sampler2D texture3;
		uniform sampler2D texture4;
		uniform sampler2D texture5;
		uniform sampler2D texture6;
		uniform sampler2D texture7;
		uniform sampler2D texture8;
		uniform sampler2D texture9;
		uniform sampler2D texture10;
		uniform sampler2D texture11;

		// timer data
		uniform float timer;

		// material types
		#if defined(SPECULAR)
		#define normaltexture texture2
		#define speculartexture texture3
		#define brighttexture texture4
		#define detailtexture texture5
		#define glowtexture texture6
		#elif defined(PBR)
		#define normaltexture texture2
		#define metallictexture texture3
		#define roughnesstexture texture4
		#define aotexture texture5
		#define brighttexture texture6
		#define detailtexture texture7
		#define glowtexture texture8
		#else
		#define brighttexture texture2
		#define detailtexture texture3
		#define glowtexture texture4
		#endif
	)";

#ifdef NPOT_EMULATION
	i_data += "#define NPOT_EMULATION\nuniform vec2 uNpotEmulation;\n";
#endif

	int vp_lump = fileSystem.CheckNumForFullName(vert_prog_lump, 0);
	if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump.GetChars());
	auto vp_data = fileSystem.ReadFile(vp_lump);

	int fp_lump = fileSystem.CheckNumForFullName(frag_prog_lump, 0);
	if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump.GetChars());
	auto fp_data = fileSystem.ReadFile(fp_lump);



//
// The following code uses GetChars on the strings to get rid of terminating 0 characters. Do not remove or the code may break!
//
	FString vp_comb;

	assert(screen->mLights != NULL);
	assert(screen->mBones != NULL);

	unsigned int lightbuffersize = screen->mLights->GetBlockSize();

	vp_comb.Format("#version %s\n\n#define NO_CLIPDISTANCE_SUPPORT\n", gles.shaderVersionString);

	FString fp_comb = vp_comb;
	vp_comb << defines << i_data.GetChars();
	fp_comb << "$placeholder$\n" << defines << i_data.GetChars();

	vp_comb << "#line 1\n";
	fp_comb << "#line 1\n";

	vp_comb << RemoveLayoutLocationDecl(GetStringFromLump(vp_lump), "out").GetChars() << "\n";
	fp_comb << RemoveLayoutLocationDecl(GetStringFromLump(fp_lump), "in").GetChars() << "\n";
	FString placeholder = "\n";

	if (proc_prog_lump.Len())
	{
		fp_comb << "#line 1\n";

		if (*proc_prog_lump != '#')
		{
			int pp_lump = fileSystem.CheckNumForFullName(proc_prog_lump);
			if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump.GetChars());
			auto ppf = fileSystem.ReadFile(pp_lump);
			FString pp_data = GetStringFromLump(pp_lump);

			if (pp_data.IndexOf("ProcessMaterial") < 0 && pp_data.IndexOf("SetupMaterial") < 0)
			{
				// this looks like an old custom hardware shader.

				if (pp_data.IndexOf("GetTexCoord") >= 0)
				{
					int pl_lump = fileSystem.CheckNumForFullName("shaders_gles/glsl/func_defaultmat2.fp", 0);
					if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders_gles/glsl/func_defaultmat2.fp");
					fp_comb << "\n" << GetStringFromLump(pl_lump);
				}
				else
				{
					int pl_lump = fileSystem.CheckNumForFullName("shaders_gles/glsl/func_defaultmat.fp", 0);
					if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders_gles/glsl/func_defaultmat.fp");
					fp_comb << "\n" << GetStringFromLump(pl_lump);

					if (pp_data.IndexOf("ProcessTexel") < 0)
					{
						// this looks like an even older custom hardware shader.
						// We need to replace the ProcessTexel call to make it work.

						fp_comb.Substitute("material.Base = ProcessTexel();", "material.Base = Process(vec4(1.0));");
					}
				}

				if (pp_data.IndexOf("ProcessLight") >= 0)
				{
					// The ProcessLight signatured changed. Forward to the old one.
					fp_comb << "\nvec4 ProcessLight(vec4 color);\n";
					fp_comb << "\nvec4 ProcessLight(Material material, vec4 color) { return ProcessLight(color); }\n";
				}
			}

			fp_comb << RemoveLegacyUserUniforms(pp_data).GetChars();
			fp_comb.Substitute("gl_TexCoord[0]", "vTexCoord");	// fix old custom shaders.

			if (pp_data.IndexOf("ProcessLight") < 0)
			{
				int pl_lump = fileSystem.CheckNumForFullName("shaders_gles/glsl/func_defaultlight.fp", 0);
				if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders_gles/glsl/func_defaultlight.fp");
				fp_comb << "\n" << GetStringFromLump(pl_lump);
			}

			// ProcessMaterial must be considered broken because it requires the user to fill in data they possibly cannot know all about.
			if (pp_data.IndexOf("ProcessMaterial") >= 0 && pp_data.IndexOf("SetupMaterial") < 0)
			{
				// This reactivates the old logic and disables all features that cannot be supported with that method.
				placeholder << "#define LEGACY_USER_SHADER\n";
			}
		}
		else
		{
			// Proc_prog_lump is not a lump name but the source itself (from generated shaders)
			fp_comb << proc_prog_lump.GetChars() + 1;
		}
	}
	fp_comb.Substitute("$placeholder$", placeholder);

	if (light_fragprog.Len())
	{
		int pp_lump = fileSystem.CheckNumForFullName(light_fragprog, 0);
		if (pp_lump == -1) I_Error("Unable to load '%s'", light_fragprog.GetChars());
		fp_comb << GetStringFromLump(pp_lump) << "\n";
	}

	if (gles.flags & RFL_NO_CLIP_PLANES)
	{
		// On ATI's GL3 drivers we have to disable gl_ClipDistance because it's hopelessly broken.
		// This will cause some glitches and regressions but is the only way to avoid total display garbage.
		vp_comb.Substitute("gl_ClipDistance", "//");
	}

	shaderData->hShader = glCreateProgram();

	uint32_t binaryFormat = 0;
	TArray<uint8_t> binary;
	if (IsShaderCacheActive())
		binary = LoadCachedProgramBinary(vp_comb, fp_comb, binaryFormat);

	bool linked = false;

	if (!linked)
	{
		shaderData->hVertProg = glCreateShader(GL_VERTEX_SHADER);
		shaderData->hFragProg = glCreateShader(GL_FRAGMENT_SHADER);

		int vp_size = (int)vp_comb.Len();
		int fp_size = (int)fp_comb.Len();

		const char *vp_ptr = vp_comb.GetChars();
		const char *fp_ptr = fp_comb.GetChars();

		glShaderSource(shaderData->hVertProg, 1, &vp_ptr, &vp_size);
		glShaderSource(shaderData->hFragProg, 1, &fp_ptr, &fp_size);

		glCompileShader(shaderData->hVertProg);
		glCompileShader(shaderData->hFragProg);

		glAttachShader(shaderData->hShader, shaderData->hVertProg);
		glAttachShader(shaderData->hShader, shaderData->hFragProg);


		glBindAttribLocation(shaderData->hShader, VATTR_VERTEX, "aPosition");
		glBindAttribLocation(shaderData->hShader, VATTR_TEXCOORD, "aTexCoord");
		glBindAttribLocation(shaderData->hShader, VATTR_COLOR, "aColor");
		glBindAttribLocation(shaderData->hShader, VATTR_VERTEX2, "aVertex2");
		glBindAttribLocation(shaderData->hShader, VATTR_NORMAL, "aNormal");
		glBindAttribLocation(shaderData->hShader, VATTR_NORMAL2, "aNormal2");
		glBindAttribLocation(shaderData->hShader, VATTR_BONEWEIGHT, "aBoneWeight");
		glBindAttribLocation(shaderData->hShader, VATTR_BONESELECTOR, "aBoneSelector");


		glLinkProgram(shaderData->hShader);

		glGetShaderInfoLog(shaderData->hVertProg, 10000, NULL, buffer);
		if (*buffer)
		{
			error << "Vertex shader:\n" << buffer << "\n";
		}
		glGetShaderInfoLog(shaderData->hFragProg, 10000, NULL, buffer);
		if (*buffer)
		{
			error << "Fragment shader:\n" << buffer << "\n";
		}

		glGetProgramInfoLog(shaderData->hShader, 10000, NULL, buffer);
		if (*buffer)
		{
			error << "Linking:\n" << buffer << "\n";
		}
		GLint status = 0;
		glGetProgramiv(shaderData->hShader, GL_LINK_STATUS, &status);
		linked = (status == GL_TRUE);

		if (!linked)
		{
			// only print message if there's an error.
			I_Error("Init Shader '%s':\n%s\n", name, error.GetChars());
		}
	}
	else
	{
		shaderData->hVertProg = 0;
		shaderData->hFragProg = 0;
	}

	shaderData->muProjectionMatrix.Init(shaderData->hShader, "ProjectionMatrix");
	shaderData->muViewMatrix.Init(shaderData->hShader, "ViewMatrix");
	shaderData->muNormalViewMatrix.Init(shaderData->hShader, "NormalViewMatrix");

	shaderData->muCameraPos.Init(shaderData->hShader, "uCameraPos");
	shaderData->muClipLine.Init(shaderData->hShader, "uClipLine");

	shaderData->muGlobVis.Init(shaderData->hShader, "uGlobVis");
	shaderData->muPalLightLevels.Init(shaderData->hShader, "uPalLightLevels");
	shaderData->muViewHeight.Init(shaderData->hShader, "uViewHeight");
	shaderData->muClipHeight.Init(shaderData->hShader, "uClipHeight");
	shaderData->muClipHeightDirection.Init(shaderData->hShader, "uClipHeightDirection");
	shaderData->muShadowmapFilter.Init(shaderData->hShader, "uShadowmapFilter");

	////

	shaderData->muDesaturation.Init(shaderData->hShader, "uDesaturationFactor");
	shaderData->muFogEnabled.Init(shaderData->hShader, "uFogEnabled");
	shaderData->muTextureMode.Init(shaderData->hShader, "uTextureMode");
	shaderData->muLightParms.Init(shaderData->hShader, "uLightAttr");
	shaderData->muClipSplit.Init(shaderData->hShader, "uClipSplit");
	shaderData->muLightRange.Init(shaderData->hShader, "uLightRange");
	shaderData->muBoneIndexBase.Init(shaderData->hShader, "uBoneIndexBase");
	shaderData->muFogColor.Init(shaderData->hShader, "uFogColor");
	shaderData->muDynLightColor.Init(shaderData->hShader, "uDynLightColor");
	shaderData->muObjectColor.Init(shaderData->hShader, "uObjectColor");
	shaderData->muObjectColor2.Init(shaderData->hShader, "uObjectColor2");
	shaderData->muGlowBottomColor.Init(shaderData->hShader, "uGlowBottomColor");
	shaderData->muGlowTopColor.Init(shaderData->hShader, "uGlowTopColor");
	shaderData->muGlowBottomPlane.Init(shaderData->hShader, "uGlowBottomPlane");
	shaderData->muGlowTopPlane.Init(shaderData->hShader, "uGlowTopPlane");
	shaderData->muGradientBottomPlane.Init(shaderData->hShader, "uGradientBottomPlane");
	shaderData->muGradientTopPlane.Init(shaderData->hShader, "uGradientTopPlane");
	shaderData->muSplitBottomPlane.Init(shaderData->hShader, "uSplitBottomPlane");
	shaderData->muSplitTopPlane.Init(shaderData->hShader, "uSplitTopPlane");
	shaderData->muDetailParms.Init(shaderData->hShader, "uDetailParms");
	shaderData->muInterpolationFactor.Init(shaderData->hShader, "uInterpolationFactor");
	shaderData->muAlphaThreshold.Init(shaderData->hShader, "uAlphaThreshold");
	shaderData->muSpecularMaterial.Init(shaderData->hShader, "uSpecularMaterial");
	shaderData->muAddColor.Init(shaderData->hShader, "uAddColor");
	shaderData->muTextureAddColor.Init(shaderData->hShader, "uTextureAddColor");
	shaderData->muTextureModulateColor.Init(shaderData->hShader, "uTextureModulateColor");
	shaderData->muTextureBlendColor.Init(shaderData->hShader, "uTextureBlendColor");
	shaderData->muTimer.Init(shaderData->hShader, "timer");
#ifdef NPOT_EMULATION
	shaderData->muNpotEmulation.Init(shaderData->hShader, "uNpotEmulation");
#endif
	shaderData->muFixedColormapStart.Init(shaderData->hShader, "uFixedColormapStart");
	shaderData->muFixedColormapRange.Init(shaderData->hShader, "uFixedColormapRange");

	shaderData->lights_index = glGetUniformLocation(shaderData->hShader, "lights");
	shaderData->bones_index = glGetUniformLocation(shaderData->hShader, "bones");
	shaderData->modelmatrix_index = glGetUniformLocation(shaderData->hShader, "ModelMatrix");
	shaderData->texturematrix_index = glGetUniformLocation(shaderData->hShader, "TextureMatrix");
	shaderData->normalmodelmatrix_index = glGetUniformLocation(shaderData->hShader, "NormalModelMatrix");


	glUseProgram(shaderData->hShader);

	// set up other texture units (if needed by the shader)
	for (int i = 2; i<16; i++)
	{
		char stringbuf[20];
		mysnprintf(stringbuf, 20, "texture%d", i);
		int tempindex = glGetUniformLocation(shaderData->hShader, stringbuf);
		if (tempindex >= 0) glUniform1i(tempindex, i - 1);
	}

	int shadowmapindex = glGetUniformLocation(shaderData->hShader, "ShadowMap");
	if (shadowmapindex >= 0) glUniform1i(shadowmapindex, 16);

	glUseProgram(0);

	cur = shaderData;

	return linked;
}

//==========================================================================
//
//
//
//==========================================================================

FShader::~FShader()
{
	std::map<uint32_t, ShaderVariantData*>::iterator it = variants.begin();

	while (it != variants.end())
	{
		glDeleteProgram(it->second->hShader);
		if (it->second->hVertProg != 0)
			glDeleteShader(it->second->hVertProg);
		if (it->second->hFragProg != 0)
			glDeleteShader(it->second->hFragProg);
		it++;
	}
}


//==========================================================================
//
//
//
//==========================================================================

bool FShader::Bind(ShaderFlavourData& flavour)
{
	uint32_t tag = CreateShaderTag(flavour);

	auto pos = variants.find(tag);

	if (pos == variants.end())
	{
		FString variantConfig = "\n";

		variantConfig.AppendFormat("#define MAXIMUM_LIGHT_VECTORS %d\n", gles.numlightvectors);
		variantConfig.AppendFormat("#define DEF_TEXTURE_MODE %d\n", flavour.textureMode);
		variantConfig.AppendFormat("#define DEF_TEXTURE_FLAGS %d\n", flavour.texFlags);
		variantConfig.AppendFormat("#define DEF_BLEND_FLAGS %d\n", flavour.blendFlags & 0x7);
		variantConfig.AppendFormat("#define DEF_FOG_2D %d\n", flavour.twoDFog);
		variantConfig.AppendFormat("#define DEF_FOG_ENABLED %d\n", flavour.fogEnabled);
		variantConfig.AppendFormat("#define DEF_FOG_RADIAL %d\n", flavour.fogEquationRadial);
		variantConfig.AppendFormat("#define DEF_FOG_COLOURED %d\n", flavour.colouredFog);
		variantConfig.AppendFormat("#define DEF_USE_U_LIGHT_LEVEL %d\n", flavour.useULightLevel);

		variantConfig.AppendFormat("#define DEF_DO_DESATURATE %d\n", flavour.doDesaturate);

		variantConfig.AppendFormat("#define DEF_DYNAMIC_LIGHTS_MOD %d\n", flavour.dynLightsMod);
		variantConfig.AppendFormat("#define DEF_DYNAMIC_LIGHTS_SUB %d\n", flavour.dynLightsSub);
		variantConfig.AppendFormat("#define DEF_DYNAMIC_LIGHTS_ADD %d\n", flavour.dynLightsAdd);

		variantConfig.AppendFormat("#define DEF_USE_OBJECT_COLOR_2 %d\n", flavour.useObjectColor2);
		variantConfig.AppendFormat("#define DEF_USE_GLOW_TOP_COLOR %d\n", flavour.useGlowTopColor);
		variantConfig.AppendFormat("#define DEF_USE_GLOW_BOTTOM_COLOR %d\n", flavour.useGlowBottomColor);

		variantConfig.AppendFormat("#define DEF_USE_COLOR_MAP %d\n", flavour.useColorMap);
		variantConfig.AppendFormat("#define DEF_BUILD_LIGHTING %d\n", flavour.buildLighting);
		variantConfig.AppendFormat("#define DEF_BANDED_SW_LIGHTING %d\n", flavour.bandedSwLight);

		variantConfig.AppendFormat("#define USE_GLSL_V100 %d\n", gles.forceGLSLv100);

#ifdef NPOT_EMULATION
		variantConfig.AppendFormat("#define DEF_NPOT_EMULATION %d\n", flavour.npotEmulation);
#endif

		variantConfig.AppendFormat("#define DEF_HAS_SPOTLIGHT %d\n", flavour.hasSpotLight);
		variantConfig.AppendFormat("#define DEF_PALETTE_INTERPOLATE %d\n", flavour.paletteInterpolate);

		//Printf("Shader: %s, %08x %s", mFragProg2.GetChars(), tag, variantConfig.GetChars());

		Load(mName.GetChars(), mVertProg, mFragProg, mFragProg2, mLightProg, mDefinesBase + variantConfig);

		variants.insert(std::make_pair(tag, cur));
	}
	else
	{
		cur = pos->second;
	}

	GLRenderer->mShaderManager->SetActiveShader(this->cur);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FShader *FShaderCollection::Compile (const char *ShaderName, const char *ShaderPath, const char *LightModePath, const char *shaderdefines, bool usediscard, EPassType passType)
{
	FString defines;
	defines += shaderdefines;
	// this can't be in the shader code due to ATI strangeness.
	if (!usediscard) defines += "#define NO_ALPHATEST\n";

	FShader *shader = new FShader(ShaderName);
	shader->Configure(ShaderName, "shaders_gles/glsl/main.vp", "shaders_gles/glsl/main.fp", ShaderPath, LightModePath, defines.GetChars());
	return shader;
}

//==========================================================================
//
//
//
//==========================================================================

FShaderManager::FShaderManager()
{
	for (int passType = 0; passType < MAX_PASS_TYPES; passType++)
		mPassShaders.Push(new FShaderCollection((EPassType)passType));
}

FShaderManager::~FShaderManager()
{
	glUseProgram(0);
	mActiveShader = NULL;

	for (auto collection : mPassShaders)
		delete collection;
}

void FShaderManager::SetActiveShader(FShader::ShaderVariantData *sh)
{
	if (mActiveShader != sh)
	{
		glUseProgram(sh!= NULL? sh->GetHandle() : 0);
		mActiveShader = sh;
	}
}

FShader *FShaderManager::BindEffect(int effect, EPassType passType, ShaderFlavourData &flavour)
{
	if (passType < mPassShaders.Size())
		return mPassShaders[passType]->BindEffect(effect, flavour);
	else
		return nullptr;
}

FShader *FShaderManager::Get(unsigned int eff, bool alphateston, EPassType passType)
{
	if (passType < mPassShaders.Size())
		return mPassShaders[passType]->Get(eff, alphateston);
	else
		return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FShaderCollection::FShaderCollection(EPassType passType)
{
	CompileShaders(passType);
}

//==========================================================================
//
//
//
//==========================================================================

FShaderCollection::~FShaderCollection()
{
	Clean();
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderCollection::CompileShaders(EPassType passType)
{
	mMaterialShaders.Clear();
	mMaterialShadersNAT.Clear();
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i] = NULL;
	}

	for(int i=0;defaultshaders[i].ShaderName != NULL;i++)
	{
		FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, true, passType);
		mMaterialShaders.Push(shc);
		if (i < SHADER_NoTexture)
		{
			shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, false, passType);
			mMaterialShadersNAT.Push(shc);
		}
	}

#if 0
	for(unsigned i = 0; i < usershaders.Size(); i++)
	{
		FString name = ExtractFileBase(usershaders[i].shader);
		FString defines = defaultshaders[usershaders[i].shaderType].Defines + usershaders[i].defines;
		FShader *shc = Compile(name, usershaders[i].shader, defaultshaders[usershaders[i].shaderType].lightfunc, defines, true, passType);
		mMaterialShaders.Push(shc);
	}
#endif

	for(int i=0;i<MAX_EFFECTS;i++)
	{
		FShader *eff = new FShader(effectshaders[i].ShaderName);
		if (!eff->Configure(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
						effectshaders[i].fp2, effectshaders[i].fp3, effectshaders[i].defines))
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

void FShaderCollection::Clean()
{
	for (unsigned int i = 0; i < mMaterialShadersNAT.Size(); i++)
	{
		if (mMaterialShadersNAT[i] != NULL) delete mMaterialShadersNAT[i];
	}
	for (unsigned int i = 0; i < mMaterialShaders.Size(); i++)
	{
		if (mMaterialShaders[i] != NULL) delete mMaterialShaders[i];
	}
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		if (mEffectShaders[i] != NULL) delete mEffectShaders[i];
		mEffectShaders[i] = NULL;
	}
	mMaterialShaders.Clear();
	mMaterialShadersNAT.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

int FShaderCollection::Find(const char * shn)
{
	FName sfn = shn;

	for(unsigned int i=0;i<mMaterialShaders.Size();i++)
	{
		if (mMaterialShaders[i]->mName == sfn)
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

FShader *FShaderCollection::BindEffect(int effect, ShaderFlavourData& flavour)
{
	if (effect >= 0 && effect < MAX_EFFECTS && mEffectShaders[effect] != NULL)
	{
		mEffectShaders[effect]->Bind(flavour);
		return mEffectShaders[effect];
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

}
