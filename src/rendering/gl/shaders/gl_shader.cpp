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
/*
** gl_shader.cpp
**
** GLSL shader handling
**
*/

#include "gl_load/gl_system.h"
#include "c_cvars.h"
#include "v_video.h"
#include "w_wad.h"
#include "doomerrors.h"
#include "cmdlib.h"
#include "md5.h"
#include "m_misc.h"
#include "hwrenderer/utility/hw_shaderpatcher.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"

#include "gl_load/gl_interface.h"
#include "gl/system/gl_debug.h"
#include "matrix.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/shaders/gl_shader.h"
#include <map>
#include <memory>

namespace OpenGLRenderer
{

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

bool FShader::Load(const char * name, const char * vert_prog_lump, const char * frag_prog_lump, const char * proc_prog_lump, const char * light_fragprog, const char * defines)
{
	static char buffer[10000];
	FString error;

	FString i_data = R"(
		// these settings are actually pointless but there seem to be some old ATI drivers that fail to compile the shader without setting the precision here.
		precision highp int;
		precision highp float;

		// This must match the HWViewpointUniforms struct
		layout(std140) uniform ViewpointUBO {
			mat4 ProjectionMatrix;
			mat4 ViewMatrix;
			mat4 NormalViewMatrix;

			vec4 uCameraPos;
			vec4 uClipLine;

			float uGlobVis;			// uGlobVis = R_GetGlobVis(r_visibility) / 32.0
			int uPalLightLevels;	
			int uViewHeight;		// Software fuzz scaling
			float uClipHeight;
			float uClipHeightDirection;
			int uShadowmapFilter;
		};
	)";
	
	i_data += "uniform int uTextureMode;\n";
	i_data += "uniform vec2 uClipSplit;\n";
	i_data += "uniform float uAlphaThreshold;\n";

	// colors
	i_data += "uniform vec4 uObjectColor;\n";
	i_data += "uniform vec4 uObjectColor2;\n";
	i_data += "uniform vec4 uDynLightColor;\n";
	i_data += "uniform vec4 uAddColor;\n";
	i_data += "uniform vec4 uFogColor;\n";
	i_data += "uniform float uDesaturationFactor;\n";
	i_data += "uniform float uInterpolationFactor;\n";

	// Glowing walls stuff
	i_data += "uniform vec4 uGlowTopPlane;\n";
	i_data += "uniform vec4 uGlowTopColor;\n";
	i_data += "uniform vec4 uGlowBottomPlane;\n";
	i_data += "uniform vec4 uGlowBottomColor;\n";

	i_data += "uniform vec4 uGradientTopPlane;\n";
	i_data += "uniform vec4 uGradientBottomPlane;\n";

	i_data += "uniform vec4 uSplitTopPlane;\n";
	i_data += "uniform vec4 uSplitBottomPlane;\n";

	// Lighting + Fog
	i_data += "uniform vec4 uLightAttr;\n";
	i_data += "#define uLightLevel uLightAttr.a\n";
	i_data += "#define uFogDensity uLightAttr.b\n";
	i_data += "#define uLightFactor uLightAttr.g\n";
	i_data += "#define uLightDist uLightAttr.r\n";
	i_data += "uniform int uFogEnabled;\n";

	// dynamic lights
	i_data += "uniform int uLightIndex;\n";

	// Blinn glossiness and specular level
	i_data += "uniform vec2 uSpecularMaterial;\n";

	// matrices
	i_data += "uniform mat4 ModelMatrix;\n";
	i_data += "uniform mat4 NormalModelMatrix;\n";
	i_data += "uniform mat4 TextureMatrix;\n";

	// light buffers
	i_data += "#ifdef SHADER_STORAGE_LIGHTS\n";
	i_data += "layout(std430, binding = 1) buffer LightBufferSSO\n";
	i_data += "{\n";
	i_data += "    vec4 lights[];\n";
	i_data += "};\n";
	i_data += "#elif defined NUM_UBO_LIGHTS\n";
	i_data += "uniform LightBufferUBO\n";
	i_data += "{\n";
	i_data += "    vec4 lights[NUM_UBO_LIGHTS];\n";
	i_data += "};\n";
	i_data += "#endif\n";

	// textures
	i_data += "uniform sampler2D tex;\n";
	i_data += "uniform sampler2D ShadowMap;\n";
	i_data += "uniform sampler2D texture2;\n";
	i_data += "uniform sampler2D texture3;\n";
	i_data += "uniform sampler2D texture4;\n";
	i_data += "uniform sampler2D texture5;\n";
	i_data += "uniform sampler2D texture6;\n";

	// timer data
	i_data += "uniform float timer;\n";

	// material types
	i_data += "#if defined(SPECULAR)\n";
	i_data += "#define normaltexture texture2\n";
	i_data += "#define speculartexture texture3\n";
	i_data += "#define brighttexture texture4\n";
	i_data += "#elif defined(PBR)\n";
	i_data += "#define normaltexture texture2\n";
	i_data += "#define metallictexture texture3\n";
	i_data += "#define roughnesstexture texture4\n";
	i_data += "#define aotexture texture5\n";
	i_data += "#define brighttexture texture6\n";
	i_data += "#else\n";
	i_data += "#define brighttexture texture2\n";
	i_data += "#endif\n";

	int vp_lump = Wads.CheckNumForFullName(vert_prog_lump, 0);
	if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump);
	FMemLump vp_data = Wads.ReadLump(vp_lump);

	int fp_lump = Wads.CheckNumForFullName(frag_prog_lump, 0);
	if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump);
	FMemLump fp_data = Wads.ReadLump(fp_lump);



//
// The following code uses GetChars on the strings to get rid of terminating 0 characters. Do not remove or the code may break!
//
	FString vp_comb;

	assert(screen->mLights != NULL);

	bool lightbuffertype = screen->mLights->GetBufferType();
	unsigned int lightbuffersize = screen->mLights->GetBlockSize();
	if (!lightbuffertype)
	{
		vp_comb.Format("#version 330 core\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
	}
	else
	{
		// This differentiation is for Intel which do not seem to expose the full extension, even if marked as required.
		if (gl.glslversion < 4.3f)
			vp_comb = "#version 400 core\n#extension GL_ARB_shader_storage_buffer_object : require\n#define SHADER_STORAGE_LIGHTS\n";
		else
			vp_comb = "#version 430 core\n#define SHADER_STORAGE_LIGHTS\n";
	}

	if (gl.flags & RFL_SHADER_STORAGE_BUFFER)
	{
		vp_comb << "#define SUPPORTS_SHADOWMAPS\n";
	}

	vp_comb << defines << i_data.GetChars();
	FString fp_comb = vp_comb;

	vp_comb << "#line 1\n";
	fp_comb << "#line 1\n";

	vp_comb << RemoveLayoutLocationDecl(vp_data.GetString(), "out").GetChars() << "\n";
	fp_comb << RemoveLayoutLocationDecl(fp_data.GetString(), "in").GetChars() << "\n";

	if (proc_prog_lump != NULL)
	{
		fp_comb << "#line 1\n";

		if (*proc_prog_lump != '#')
		{
			int pp_lump = Wads.CheckNumForFullName(proc_prog_lump);
			if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump);
			FMemLump pp_data = Wads.ReadLump(pp_lump);

			if (pp_data.GetString().IndexOf("ProcessMaterial") < 0)
			{
				// this looks like an old custom hardware shader.

				// add ProcessMaterial function that calls the older ProcessTexel function
				int pl_lump = Wads.CheckNumForFullName("shaders/glsl/func_defaultmat.fp");
				if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultmat.fp");
				FMemLump pl_data = Wads.ReadLump(pl_lump);
				fp_comb << "\n" << pl_data.GetString().GetChars();

				if (pp_data.GetString().IndexOf("ProcessTexel") < 0)
				{
					// this looks like an even older custom hardware shader.
					// We need to replace the ProcessTexel call to make it work.

					fp_comb.Substitute("material.Base = ProcessTexel();", "material.Base = Process(vec4(1.0));");
				}

				if (pp_data.GetString().IndexOf("ProcessLight") >= 0)
				{
					// The ProcessLight signatured changed. Forward to the old one.
					fp_comb << "\nvec4 ProcessLight(vec4 color);\n";
					fp_comb << "\nvec4 ProcessLight(Material material, vec4 color) { return ProcessLight(color); }\n";
				}
			}

			fp_comb << RemoveLegacyUserUniforms(pp_data.GetString()).GetChars();
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

	if (light_fragprog)
	{
		int pp_lump = Wads.CheckNumForFullName(light_fragprog);
		if (pp_lump == -1) I_Error("Unable to load '%s'", light_fragprog);
		FMemLump pp_data = Wads.ReadLump(pp_lump);
		fp_comb << pp_data.GetString().GetChars() << "\n";
	}

	if (gl.flags & RFL_NO_CLIP_PLANES)
	{
		// On ATI's GL3 drivers we have to disable gl_ClipDistance because it's hopelessly broken.
		// This will cause some glitches and regressions but is the only way to avoid total display garbage.
		vp_comb.Substitute("gl_ClipDistance", "//");
	}

	hShader = glCreateProgram();
	FGLDebug::LabelObject(GL_PROGRAM, hShader, name);

	uint32_t binaryFormat = 0;
	TArray<uint8_t> binary;
	if (IsShaderCacheActive())
		binary = LoadCachedProgramBinary(vp_comb, fp_comb, binaryFormat);

	bool linked = false;
	if (binary.Size() > 0 && glProgramBinary)
	{
		glProgramBinary(hShader, binaryFormat, binary.Data(), binary.Size());
		GLint status = 0;
		glGetProgramiv(hShader, GL_LINK_STATUS, &status);
		linked = (status == GL_TRUE);
	}

	if (!linked)
	{
		hVertProg = glCreateShader(GL_VERTEX_SHADER);
		hFragProg = glCreateShader(GL_FRAGMENT_SHADER);

		FGLDebug::LabelObject(GL_SHADER, hVertProg, vert_prog_lump);
		FGLDebug::LabelObject(GL_SHADER, hFragProg, frag_prog_lump);

		int vp_size = (int)vp_comb.Len();
		int fp_size = (int)fp_comb.Len();

		const char *vp_ptr = vp_comb.GetChars();
		const char *fp_ptr = fp_comb.GetChars();

		glShaderSource(hVertProg, 1, &vp_ptr, &vp_size);
		glShaderSource(hFragProg, 1, &fp_ptr, &fp_size);

		glCompileShader(hVertProg);
		glCompileShader(hFragProg);

		glAttachShader(hShader, hVertProg);
		glAttachShader(hShader, hFragProg);

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
		GLint status = 0;
		glGetProgramiv(hShader, GL_LINK_STATUS, &status);
		linked = (status == GL_TRUE);
		if (!linked)
		{
			// only print message if there's an error.
			I_Error("Init Shader '%s':\n%s\n", name, error.GetChars());
		}
		else if (glProgramBinary && IsShaderCacheActive())
		{
			int binaryLength = 0;
			glGetProgramiv(hShader, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
			binary.Resize(binaryLength);
			glGetProgramBinary(hShader, binary.Size(), &binaryLength, &binaryFormat, binary.Data());
			binary.Resize(binaryLength);
			SaveCachedProgramBinary(vp_comb, fp_comb, binary, binaryFormat);
		}
	}
	else
	{
		hVertProg = 0;
		hFragProg = 0;
	}

	muDesaturation.Init(hShader, "uDesaturationFactor");
	muFogEnabled.Init(hShader, "uFogEnabled");
	muTextureMode.Init(hShader, "uTextureMode");
	muLightParms.Init(hShader, "uLightAttr");
	muClipSplit.Init(hShader, "uClipSplit");
	muLightIndex.Init(hShader, "uLightIndex");
	muFogColor.Init(hShader, "uFogColor");
	muDynLightColor.Init(hShader, "uDynLightColor");
	muObjectColor.Init(hShader, "uObjectColor");
	muObjectColor2.Init(hShader, "uObjectColor2");
	muGlowBottomColor.Init(hShader, "uGlowBottomColor");
	muGlowTopColor.Init(hShader, "uGlowTopColor");
	muGlowBottomPlane.Init(hShader, "uGlowBottomPlane");
	muGlowTopPlane.Init(hShader, "uGlowTopPlane");
	muGradientBottomPlane.Init(hShader, "uGradientBottomPlane");
	muGradientTopPlane.Init(hShader, "uGradientTopPlane");
	muSplitBottomPlane.Init(hShader, "uSplitBottomPlane");
	muSplitTopPlane.Init(hShader, "uSplitTopPlane");
	muInterpolationFactor.Init(hShader, "uInterpolationFactor");
	muAlphaThreshold.Init(hShader, "uAlphaThreshold");
	muSpecularMaterial.Init(hShader, "uSpecularMaterial");
	muAddColor.Init(hShader, "uAddColor");
	muTimer.Init(hShader, "timer");

	lights_index = glGetUniformLocation(hShader, "lights");
	modelmatrix_index = glGetUniformLocation(hShader, "ModelMatrix");
	texturematrix_index = glGetUniformLocation(hShader, "TextureMatrix");
	normalmodelmatrix_index = glGetUniformLocation(hShader, "NormalModelMatrix");

	if (!lightbuffertype)
	{
		int tempindex = glGetUniformBlockIndex(hShader, "LightBufferUBO");
		if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, LIGHTBUF_BINDINGPOINT);
	}
	int tempindex = glGetUniformBlockIndex(hShader, "ViewpointUBO");
	if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, VIEWPOINT_BINDINGPOINT);

	glUseProgram(hShader);

	// set up other texture units (if needed by the shader)
	for (int i = 2; i<16; i++)
	{
		char stringbuf[20];
		mysnprintf(stringbuf, 20, "texture%d", i);
		int tempindex = glGetUniformLocation(hShader, stringbuf);
		if (tempindex > 0) glUniform1i(tempindex, i - 1);
	}

	int shadowmapindex = glGetUniformLocation(hShader, "ShadowMap");
	if (shadowmapindex > 0) glUniform1i(shadowmapindex, 16);

	glUseProgram(0);
	return linked;
}

//==========================================================================
//
//
//
//==========================================================================

FShader::~FShader()
{
	glDeleteProgram(hShader);
	if (hVertProg != 0)
		glDeleteShader(hVertProg);
	if (hFragProg != 0)
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

FShader *FShaderCollection::Compile (const char *ShaderName, const char *ShaderPath, const char *LightModePath, const char *shaderdefines, bool usediscard, EPassType passType)
{
	FString defines;
	defines += shaderdefines;
	// this can't be in the shader code due to ATI strangeness.
	if (!usediscard) defines += "#define NO_ALPHATEST\n";
	if (passType == GBUFFER_PASS) defines += "#define GBUFFER_PASS\n";

	FShader *shader = NULL;
	try
	{
		shader = new FShader(ShaderName);
		if (!shader->Load(ShaderName, "shaders/glsl/main.vp", "shaders/glsl/main.fp", ShaderPath, LightModePath, defines.GetChars()))
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

void FShaderManager::SetActiveShader(FShader *sh)
{
	if (mActiveShader != sh)
	{
		glUseProgram(sh!= NULL? sh->GetHandle() : 0);
		mActiveShader = sh;
	}
}

FShader *FShaderManager::BindEffect(int effect, EPassType passType)
{
	if (passType < mPassShaders.Size())
		return mPassShaders[passType]->BindEffect(effect);
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
			FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, false, passType);
			mMaterialShadersNAT.Push(shc);
		}
	}

	for(unsigned i = 0; i < usershaders.Size(); i++)
	{
		FString name = ExtractFileBase(usershaders[i].shader);
		FString defines = defaultshaders[usershaders[i].shaderType].Defines + usershaders[i].defines;
		FShader *shc = Compile(name, usershaders[i].shader, defaultshaders[usershaders[i].shaderType].lightfunc, defines, true, passType);
		mMaterialShaders.Push(shc);
	}

	for(int i=0;i<MAX_EFFECTS;i++)
	{
		FShader *eff = new FShader(effectshaders[i].ShaderName);
		if (!eff->Load(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
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

FShader *FShaderCollection::BindEffect(int effect)
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

void gl_DestroyUserShaders()
{
	// todo
}

}