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

#include "gl_system.h"
#include "c_cvars.h"
#include "v_video.h"
#include "filesystem.h"
#include "engineerrors.h"
#include "cmdlib.h"
#include "md5.h"
#include "gl_shader.h"
#include "hw_shaderpatcher.h"
#include "shaderuniforms.h"
#include "hw_viewpointuniforms.h"
#include "hw_lightbuffer.h"
#include "hw_bonebuffer.h"
#include "i_specialpaths.h"
#include "printf.h"
#include "version.h"

#include "gl_interface.h"
#include "gl_debug.h"
#include "matrix.h"
#include "gl_renderer.h"
#include <map>
#include <memory>

EXTERN_CVAR(Bool, r_skipmats)

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
		uniform int uFogEnabled;

		// dynamic lights
		uniform int uLightIndex;

		// bone animation
		uniform int uBoneIndexBase;

		// Blinn glossiness and specular level
		uniform vec2 uSpecularMaterial;

		// matrices
		uniform mat4 ModelMatrix;
		uniform mat4 NormalModelMatrix;
		uniform mat4 TextureMatrix;

		// light buffers
		#ifdef SHADER_STORAGE_LIGHTS
		layout(std430, binding = 1) buffer LightBufferSSO
		{
			vec4 lights[];
		};
		#elif defined NUM_UBO_LIGHTS
		uniform LightBufferUBO
		{
			vec4 lights[NUM_UBO_LIGHTS];
		};
		#endif

		// bone matrix buffers
		#ifdef SHADER_STORAGE_BONES
		layout(std430, binding = 7) buffer BoneBufferSSO
		{
			mat4 bones[];
		};
		#elif defined NUM_UBO_BONES
		uniform BoneBufferUBO
		{
			mat4 bones[NUM_UBO_BONES];
		};
		#endif

		// textures
		uniform sampler2D tex;
		uniform sampler2D ShadowMap;
		uniform sampler2DArray LightMap;
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


#ifdef __APPLE__
	// The noise functions are completely broken in macOS OpenGL drivers
	// Garbage values are returned, and their infrequent usage causes extreme slowdown
	// Also, these functions must return zeroes since GLSL 4.4
	i_data += "#define noise1(unused) 0.0\n";
	i_data += "#define noise2(unused) vec2(0)\n";
	i_data += "#define noise3(unused) vec3(0)\n";
	i_data += "#define noise4(unused) vec4(0)\n";
#endif // __APPLE__

#ifdef NPOT_EMULATION
	i_data += "#define NPOT_EMULATION\nuniform vec2 uNpotEmulation;\n";
#endif

	int vp_lump = fileSystem.CheckNumForFullName(vert_prog_lump, 0);
	if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump);
	FileData vp_data = fileSystem.ReadFile(vp_lump);

	int fp_lump = fileSystem.CheckNumForFullName(frag_prog_lump, 0);
	if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump);
	FileData fp_data = fileSystem.ReadFile(fp_lump);



//
// The following code uses GetChars on the strings to get rid of terminating 0 characters. Do not remove or the code may break!
//
	FString vp_comb;

	assert(screen->mLights != NULL);
	assert(screen->mBones != NULL);

	bool lightbuffertype = screen->mLights->GetBufferType();
	unsigned int lightbuffersize = screen->mLights->GetBlockSize();
	if (!lightbuffertype)
	{
		vp_comb.Format("#version 330 core\n#define NUM_UBO_LIGHTS %d\n#define NUM_UBO_BONES %d\n", lightbuffersize, screen->mBones->GetBlockSize());
	}
	else
	{
		// This differentiation is for Intel which do not seem to expose the full extension, even if marked as required.
		if (gl.glslversion < 4.3f)
			vp_comb = "#version 400 core\n#extension GL_ARB_shader_storage_buffer_object : require\n#define SHADER_STORAGE_LIGHTS\n#define SHADER_STORAGE_BONES\n";
		else
			vp_comb = "#version 430 core\n#define SHADER_STORAGE_LIGHTS\n#define SHADER_STORAGE_BONES\n";
	}

	if ((gl.flags & RFL_SHADER_STORAGE_BUFFER) && screen->allowSSBO())
	{
		vp_comb << "#define SUPPORTS_SHADOWMAPS\n";
	}

	FString fp_comb = vp_comb;
	vp_comb << defines << i_data.GetChars();
	fp_comb << "$placeholder$\n" << defines << i_data.GetChars();

	vp_comb << "#line 1\n";
	fp_comb << "#line 1\n";

	vp_comb << RemoveLayoutLocationDecl(vp_data.GetString(), "out").GetChars() << "\n";
	fp_comb << RemoveLayoutLocationDecl(fp_data.GetString(), "in").GetChars() << "\n";
	FString placeholder = "\n";

	if (proc_prog_lump != NULL)
	{
		fp_comb << "#line 1\n";

		if (*proc_prog_lump != '#')
		{
			int pp_lump = fileSystem.CheckNumForFullName(proc_prog_lump, 0);	// if it's a core shader, ignore overrides by user mods.
			if (pp_lump == -1) pp_lump = fileSystem.CheckNumForFullName(proc_prog_lump);
			if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump);
			FileData pp_data = fileSystem.ReadFile(pp_lump);

			if (pp_data.GetString().IndexOf("ProcessMaterial") < 0 && pp_data.GetString().IndexOf("SetupMaterial") < 0)
			{
				// this looks like an old custom hardware shader.

				if (pp_data.GetString().IndexOf("GetTexCoord") >= 0)
				{
					int pl_lump = fileSystem.CheckNumForFullName("shaders/glsl/func_defaultmat2.fp", 0);
					if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultmat2.fp");
					FileData pl_data = fileSystem.ReadFile(pl_lump);
					fp_comb << "\n" << pl_data.GetString().GetChars();
				}
				else
				{
					int pl_lump = fileSystem.CheckNumForFullName("shaders/glsl/func_defaultmat.fp", 0);
					if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultmat.fp");
					FileData pl_data = fileSystem.ReadFile(pl_lump);
					fp_comb << "\n" << pl_data.GetString().GetChars();

					if (pp_data.GetString().IndexOf("ProcessTexel") < 0)
					{
						// this looks like an even older custom hardware shader.
						// We need to replace the ProcessTexel call to make it work.

						fp_comb.Substitute("material.Base = ProcessTexel();", "material.Base = Process(vec4(1.0));");
					}
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
				int pl_lump = fileSystem.CheckNumForFullName("shaders/glsl/func_defaultlight.fp", 0);
				if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultlight.fp");
				FileData pl_data = fileSystem.ReadFile(pl_lump);
				fp_comb << "\n" << pl_data.GetString().GetChars();
			}

			// ProcessMaterial must be considered broken because it requires the user to fill in data they possibly cannot know all about.
			if (pp_data.GetString().IndexOf("ProcessMaterial") >= 0 && pp_data.GetString().IndexOf("SetupMaterial") < 0)
			{
				// This reactivates the old logic and disables all features that cannot be supported with that method.
				placeholder << "#define LEGACY_USER_SHADER\n";
			}
		}
		else
		{
			// Proc_prog_lump is not a lump name but the source itself (from generated shaders)
			fp_comb << proc_prog_lump + 1;
		}
	}
	fp_comb.Substitute("$placeholder$", placeholder);

	if (light_fragprog)
	{
		int pp_lump = fileSystem.CheckNumForFullName(light_fragprog, 0);
		if (pp_lump == -1) I_Error("Unable to load '%s'", light_fragprog);
		FileData pp_data = fileSystem.ReadFile(pp_lump);
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
	muBoneIndexBase.Init(hShader, "uBoneIndexBase");
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
	muDetailParms.Init(hShader, "uDetailParms");
#ifdef NPOT_EMULATION
	muNpotEmulation.Init(hShader, "uNpotEmulation");
#endif
	muInterpolationFactor.Init(hShader, "uInterpolationFactor");
	muAlphaThreshold.Init(hShader, "uAlphaThreshold");
	muSpecularMaterial.Init(hShader, "uSpecularMaterial");
	muAddColor.Init(hShader, "uAddColor");
	muTextureAddColor.Init(hShader, "uTextureAddColor");
	muTextureModulateColor.Init(hShader, "uTextureModulateColor");
	muTextureBlendColor.Init(hShader, "uTextureBlendColor");
	muTimer.Init(hShader, "timer");

	lights_index = glGetUniformLocation(hShader, "lights");
	modelmatrix_index = glGetUniformLocation(hShader, "ModelMatrix");
	texturematrix_index = glGetUniformLocation(hShader, "TextureMatrix");
	normalmodelmatrix_index = glGetUniformLocation(hShader, "NormalModelMatrix");

	if (!lightbuffertype)
	{
		int tempindex = glGetUniformBlockIndex(hShader, "LightBufferUBO");
		if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, LIGHTBUF_BINDINGPOINT);

		tempindex = glGetUniformBlockIndex(hShader, "BoneBufferUBO");
		if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, BONEBUF_BINDINGPOINT);
	}
	int tempindex = glGetUniformBlockIndex(hShader, "ViewpointUBO");
	if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, VIEWPOINT_BINDINGPOINT);

	glUseProgram(hShader);

	// set up other texture units (if needed by the shader)
	for (int i = 2; i<16; i++)
	{
		char stringbuf[20];
		mysnprintf(stringbuf, 20, "texture%d", i);
		tempindex = glGetUniformLocation(hShader, stringbuf);
		if (tempindex != -1) glUniform1i(tempindex, i - 1);
	}

	int shadowmapindex = glGetUniformLocation(hShader, "ShadowMap");
	if (shadowmapindex != -1) glUniform1i(shadowmapindex, 16);

	int lightmapindex = glGetUniformLocation(hShader, "LightMap");
	if (lightmapindex != -1) glUniform1i(lightmapindex, 17);

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
	if (shaderdefines) defines += shaderdefines;
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

bool FShaderManager::CompileNextShader()
{
	if (mPassShaders[mCompilePass]->CompileNextShader())
	{
		mCompilePass++;
		if (mCompilePass >= MAX_PASS_TYPES)
		{
			mCompilePass = -1;
			return true;
		}
	}
	return false;
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
	if (passType < mPassShaders.Size() && mCompilePass == -1)
		return mPassShaders[passType]->BindEffect(effect);
	else
		return nullptr;
}

FShader *FShaderManager::Get(unsigned int eff, bool alphateston, EPassType passType)
{
	if (mCompilePass > -1)
	{
		return mPassShaders[0]->Get(0, false);
	}
	if ((r_skipmats && eff >= 3 && eff <= 4))
		eff = 0;

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
	mPassType = passType;
	mMaterialShaders.Clear();
	mMaterialShadersNAT.Clear();
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i] = NULL;
	}
	CompileNextShader();
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

bool FShaderCollection::CompileNextShader()
{
	int i = mCompileIndex;
	if (mCompileState == 0)
	{
		FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, true, mPassType);
		mMaterialShaders.Push(shc);
		mCompileIndex++;
		if (defaultshaders[mCompileIndex].ShaderName == nullptr)
		{
			mCompileIndex = 0;
			mCompileState++;
			
		}
	}
	else if (mCompileState == 1)
	{
		FShader *shc1 = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, false, mPassType);
		mMaterialShadersNAT.Push(shc1);
		mCompileIndex++;
		if (mCompileIndex >= SHADER_NoTexture)
		{
			mCompileIndex = 0;
			mCompileState++;
			if (usershaders.Size() == 0) mCompileState++;
		}
	}
	else if (mCompileState == 2)
	{
		FString name = ExtractFileBase(usershaders[i].shader);
		FString defines = defaultshaders[usershaders[i].shaderType].Defines + usershaders[i].defines;
		FShader *shc = Compile(name, usershaders[i].shader, defaultshaders[usershaders[i].shaderType].lightfunc, defines, true, mPassType);
		mMaterialShaders.Push(shc);
		mCompileIndex++;
		if (mCompileIndex >= (int)usershaders.Size())
		{
			mCompileIndex = 0;
			mCompileState++;
		}
	}
	else if (mCompileState == 3)
	{
		FShader *eff = new FShader(effectshaders[i].ShaderName);
		if (!eff->Load(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
						effectshaders[i].fp2, effectshaders[i].fp3, effectshaders[i].defines))
		{
			delete eff;
		}
		else mEffectShaders[i] = eff;
		mCompileIndex++;
		if (mCompileIndex >= MAX_EFFECTS)
		{
			return true;
		}
	}
	return false;
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
