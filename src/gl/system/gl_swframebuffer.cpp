/*
** gl_swframebuffer.cpp
** Code to let ZDoom use OpenGL as a simple framebuffer
**
**---------------------------------------------------------------------------
** Copyright 1998-2011 Randy Heit
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
** This file does _not_ implement hardware-acclerated 3D rendering. It is
** just a means of getting the pixel data to the screen in a more reliable
** method on modern hardware by copying the entire frame to a texture,
** drawing that to the screen, and presenting.
**
** That said, it does implement hardware-accelerated 2D rendering.
*/

#include "gl/system/gl_system.h"
#include "m_swap.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_png.h"
#include "m_crc32.h"
#include "vectors.h"
#include "v_palette.h"
#include "templates.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "r_data/r_translate.h"
#include "f_wipe.h"
#include "sbar.h"
#include "w_wad.h"
#include "r_data/colormaps.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_swframebuffer.h"
#include "gl/data/gl_data.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/gl_functions.h"
#include "gl_debug.h"
#include "r_videoscale.h"

#include "swrenderer/scene/r_light.h"

#ifndef NO_SSE
#include <immintrin.h>
#endif

CVAR(Int, gl_showpacks, 0, 0)
#ifndef WIN32 // Defined in fb_d3d9 for Windows
CVAR(Bool, vid_hwaalines, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Bool, vid_hw2d, true, CVAR_NOINITCALL)
{
	V_SetBorderNeedRefresh();
}
#else
EXTERN_CVAR(Bool, vid_hwaalines)
EXTERN_CVAR(Bool, vid_hw2d)
#endif

EXTERN_CVAR(Bool, fullscreen)
EXTERN_CVAR(Float, Gamma)
EXTERN_CVAR(Bool, vid_vsync)
EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, vid_refreshrate)

#ifdef WIN32
extern cycle_t BlitCycles;
#endif

void gl_LoadExtensions();
void gl_PrintStartupLog();

#ifndef WIN32
// This has to be in this file because system headers conflict Doom headers
DFrameBuffer *CreateGLSWFrameBuffer(int width, int height, bool bgra, bool fullscreen)
{
	return new OpenGLSWFrameBuffer(NULL, width, height, 32, 60, fullscreen, bgra);
}
#endif

const char *const OpenGLSWFrameBuffer::ShaderDefines[OpenGLSWFrameBuffer::NUM_SHADERS] =
{
	"#define ENORMALCOLOR", // NormalColor
	"#define ENORMALCOLOR\n#define PALTEX", // NormalColorPal
	"#define ENORMALCOLOR\n#define INVERT", // NormalColorInv
	"#define ENORMALCOLOR\n#define PALTEX\n#define INVERT", // NormalColorPalInv

	"#define EREDTOALPHA", // RedToAlpha
	"#define EREDTOALPHA\n#define INVERT", // RedToAlphaInv

	"#define EVERTEXCOLOR", // VertexColor

	"#define ESPECIALCOLORMAP\n", // SpecialColormap
	"#define ESPECIALCOLORMAP\n#define PALTEX", // SpecialColorMapPal

	"#define EINGAMECOLORMAP", // InGameColormap
	"#define EINGAMECOLORMAP\n#define DESAT", // InGameColormapDesat
	"#define EINGAMECOLORMAP\n#define INVERT", // InGameColormapInv
	"#define EINGAMECOLORMAP\n#define INVERT\n#define DESAT", // InGameColormapInvDesat
	"#define EINGAMECOLORMAP\n#define PALTEX\n", // InGameColormapPal
	"#define EINGAMECOLORMAP\n#define PALTEX\n#define DESAT", // InGameColormapPalDesat
	"#define EINGAMECOLORMAP\n#define PALTEX\n#define INVERT", // InGameColormapPalInv
	"#define EINGAMECOLORMAP\n#define PALTEX\n#define INVERT\n#define DESAT", // InGameColormapPalInvDesat

	"#define EBURNWIPE", // BurnWipe
	"#define EGAMMACORRECTION", // GammaCorrection
};

OpenGLSWFrameBuffer::OpenGLSWFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen, bool bgra) :
	Super(hMonitor, width, height, bits, refreshHz, fullscreen, bgra)
{
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
	FBTexture = nullptr;
	InitialWipeScreen = nullptr;
	ScreenshotTexture = nullptr;
	FinalWipeScreen = nullptr;
	PaletteTexture = nullptr;
	for (int i = 0; i < NUM_SHADERS; ++i)
	{
		Shaders[i] = nullptr;
	}

	BlendingRect.left = 0;
	BlendingRect.top = 0;
	BlendingRect.right = Width;
	BlendingRect.bottom = Height;
	In2D = 0;
	Palettes = nullptr;
	Textures = nullptr;
	Accel2D = true;
	GatheringWipeScreen = false;
	ScreenWipe = nullptr;
	InScene = false;
	QuadExtra = new BufferedTris[MAX_QUAD_BATCH];
	memset(QuadExtra, 0, sizeof(BufferedTris) * MAX_QUAD_BATCH);
	Atlases = nullptr;
	PixelDoubling = 0;

	Gamma = 1.0;
	FlashColor0 = 0;
	FlashColor1 = 0xFFFFFFFF;
	FlashColor = 0;
	FlashAmount = 0;

	NeedGammaUpdate = false;
	NeedPalUpdate = false;

	if (MemBuffer == nullptr)
	{
		return;
	}

	memcpy(SourcePalette, GPalette.BaseColors, sizeof(PalEntry) * 256);

	// To do: this needs to cooperate with the same static in OpenGLFrameBuffer::InitializeState
	static bool first = true;
	if (first)
	{
		if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
		{
			Printf("OpenGL load failed. No OpenGL acceleration will be used.\n");
			return;
		}
	}
	
	const char *glversion = (const char*)glGetString(GL_VERSION);
	bool isGLES = (glversion && strlen(glversion) > 10 && memcmp(glversion, "OpenGL ES ", 10) == 0);

	if (!isGLES && ogl_IsVersionGEQ(3, 0) == 0)
	{
		Printf("OpenGL acceleration requires at least OpenGL 3.0. No Acceleration will be used.\n");
		return;
	}
	gl_LoadExtensions();
	if (gl.legacyMode)
	{
		Printf("Legacy OpenGL path is active. No Acceleration will be used.\n");
		return;
	}
	InitializeState();
	if (first)
	{
		gl_PrintStartupLog();
		first = false;
	}

	if (!glGetString)
		return;

	// SetVSync needs to be at the very top to workaround a bug in Nvidia's OpenGL driver.
	// If wglSwapIntervalEXT is called after glBindFramebuffer in a frame the setting is not changed!
	Super::SetVSync(vid_vsync);

	Debug = std::make_shared<FGLDebug>();
	Debug->Update();

	//Windowed = !(static_cast<Win32Video *>(Video)->GoFullscreen(fullscreen));

	TrueHeight = height;

	Valid = CreateResources();
	if (Valid)
		SetInitialState();
}

OpenGLSWFrameBuffer::~OpenGLSWFrameBuffer()
{
	ReleaseResources();
	delete[] QuadExtra;
}

void *OpenGLSWFrameBuffer::MapBuffer(int target, int size)
{
	if (glMapBufferRange)
	{
		return (FBVERTEX*)glMapBufferRange(target, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
	}
	else
	{
		glBufferData(target, size, nullptr, GL_STREAM_DRAW);
		return glMapBuffer(target, GL_WRITE_ONLY);
	}
}

OpenGLSWFrameBuffer::HWFrameBuffer::~HWFrameBuffer()
{
	if (Framebuffer != 0) glDeleteFramebuffers(1, (GLuint*)&Framebuffer);
	Texture.reset();
}

OpenGLSWFrameBuffer::HWTexture::~HWTexture()
{
	if (Texture != 0) glDeleteTextures(1, (GLuint*)&Texture);
	if (Buffers[0] != 0) glDeleteBuffers(2, (GLuint*)Buffers);
}

OpenGLSWFrameBuffer::HWVertexBuffer::~HWVertexBuffer()
{
	if (VertexArray != 0) glDeleteVertexArrays(1, (GLuint*)&VertexArray);
	if (Buffer != 0) glDeleteBuffers(1, (GLuint*)&Buffer);
}

OpenGLSWFrameBuffer::FBVERTEX *OpenGLSWFrameBuffer::HWVertexBuffer::Lock()
{
	glBindBuffer(GL_ARRAY_BUFFER, Buffer);
	return (FBVERTEX*)MapBuffer(GL_ARRAY_BUFFER, Size);
}

void OpenGLSWFrameBuffer::HWVertexBuffer::Unlock()
{
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

OpenGLSWFrameBuffer::HWIndexBuffer::~HWIndexBuffer()
{
	if (Buffer != 0) glDeleteBuffers(1, (GLuint*)&Buffer);
}

uint16_t *OpenGLSWFrameBuffer::HWIndexBuffer::Lock()
{
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &LockedOldBinding);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Buffer);
	return (uint16_t*)MapBuffer(GL_ELEMENT_ARRAY_BUFFER, Size);
}

void OpenGLSWFrameBuffer::HWIndexBuffer::Unlock()
{
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, LockedOldBinding);
}

OpenGLSWFrameBuffer::HWPixelShader::~HWPixelShader()
{
	if (Program != 0) glDeleteProgram(Program);
	if (VertexShader != 0) glDeleteShader(VertexShader);
	if (FragmentShader != 0) glDeleteShader(FragmentShader);
}

std::unique_ptr<OpenGLSWFrameBuffer::HWFrameBuffer> OpenGLSWFrameBuffer::CreateFrameBuffer(const FString &name, int width, int height)
{
	std::unique_ptr<HWFrameBuffer> fb(new HWFrameBuffer());
	
	GLint format = GL_RGBA16F;
	if (gl.es) format = GL_RGB;

	fb->Texture = CreateTexture(name, width, height, 1, format);
	if (!fb->Texture)
	{
		return nullptr;
	}

	glGenFramebuffers(1, (GLuint*)&fb->Framebuffer);

	GLint oldFramebufferBinding = 0, oldTextureBinding = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebufferBinding);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTextureBinding);

	glBindFramebuffer(GL_FRAMEBUFFER, fb->Framebuffer);
	FGLDebug::LabelObject(GL_FRAMEBUFFER, fb->Framebuffer, name);

	glBindTexture(GL_TEXTURE_2D, fb->Texture->Texture);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->Texture->Texture, 0);

	GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glBindFramebuffer(GL_FRAMEBUFFER, oldFramebufferBinding);
	glBindTexture(GL_TEXTURE_2D, oldTextureBinding);

	if (result != GL_FRAMEBUFFER_COMPLETE)
	{
		Printf("Framebuffer is not complete\n");
		return nullptr;
	}

	return fb;
}

std::unique_ptr<OpenGLSWFrameBuffer::HWPixelShader> OpenGLSWFrameBuffer::CreatePixelShader(FString vertexsrc, FString fragmentsrc, const FString &defines)
{
	std::unique_ptr<HWPixelShader> shader(new HWPixelShader());

	shader->Program = glCreateProgram();
	if (shader->Program == 0) { Printf("glCreateProgram failed. Disabling OpenGL hardware acceleration.\n"); return nullptr; }
	shader->VertexShader = glCreateShader(GL_VERTEX_SHADER);
	if (shader->VertexShader == 0) { Printf("glCreateShader(GL_VERTEX_SHADER) failed. Disabling OpenGL hardware acceleration.\n"); return nullptr; }
	shader->FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	if (shader->FragmentShader == 0) { Printf("glCreateShader(GL_FRAGMENT_SHADER) failed. Disabling OpenGL hardware acceleration.\n"); return nullptr; }
	
	int maxGlslVersion = 330;
	int shaderVersion = MIN((int)round(gl.glslversion * 10) * 10, maxGlslVersion);
	
	FString prefix;
	prefix.AppendFormat("#version %d\n%s\n#line 0\n", shaderVersion, defines.GetChars());
	//Printf("Shader prefix: %s", prefix.GetChars());
	
	vertexsrc = prefix + vertexsrc;
	fragmentsrc = prefix + fragmentsrc;

	{
		int lengths[1] = { (int)vertexsrc.Len() };
		const char *sources[1] = { vertexsrc.GetChars() };
		glShaderSource(shader->VertexShader, 1, sources, lengths);
		glCompileShader(shader->VertexShader);
	}

	{
		int lengths[1] = { (int)fragmentsrc.Len() };
		const char *sources[1] = { fragmentsrc.GetChars() };
		glShaderSource(shader->FragmentShader, 1, sources, lengths);
		glCompileShader(shader->FragmentShader);
	}

	GLint status = 0;
	int errorShader = shader->VertexShader;
	glGetShaderiv(shader->VertexShader, GL_COMPILE_STATUS, &status);
	if (status != GL_FALSE) { errorShader = shader->FragmentShader; glGetShaderiv(shader->FragmentShader, GL_COMPILE_STATUS, &status); }
	if (status == GL_FALSE)
	{
		static char buffer[10000];
		GLsizei length = 0;
		buffer[0] = 0;
		glGetShaderInfoLog(errorShader, 10000, &length, buffer);
		//Printf("Shader compile failed: %s", buffer);

		return nullptr;
	}

	glAttachShader(shader->Program, shader->VertexShader);
	glAttachShader(shader->Program, shader->FragmentShader);
	glBindFragDataLocation(shader->Program, 0, "FragColor");
	glBindAttribLocation(shader->Program, 0, "AttrPosition");
	glBindAttribLocation(shader->Program, 1, "AttrColor0");
	glBindAttribLocation(shader->Program, 2, "AttrColor1");
	glBindAttribLocation(shader->Program, 3, "AttrTexCoord0");
	glLinkProgram(shader->Program);
	glGetProgramiv(shader->Program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		static char buffer[10000];
		GLsizei length = 0;
		buffer[0] = 0;
		glGetProgramInfoLog(shader->Program, 10000, &length, buffer);
		//Printf("Shader link failed: %s", buffer);
	
		return nullptr;
	}

	shader->ConstantLocations[PSCONST_Desaturation] = glGetUniformLocation(shader->Program, "Desaturation");
	shader->ConstantLocations[PSCONST_PaletteMod] = glGetUniformLocation(shader->Program, "PaletteMod");
	shader->ConstantLocations[PSCONST_Weights] = glGetUniformLocation(shader->Program, "Weights");
	shader->ConstantLocations[PSCONST_Gamma] = glGetUniformLocation(shader->Program, "Gamma");
	shader->ConstantLocations[PSCONST_ScreenSize] = glGetUniformLocation(shader->Program, "ScreenSize");
	shader->ImageLocation = glGetUniformLocation(shader->Program, "Image");
	shader->PaletteLocation = glGetUniformLocation(shader->Program, "Palette");
	shader->NewScreenLocation = glGetUniformLocation(shader->Program, "NewScreen");
	shader->BurnLocation = glGetUniformLocation(shader->Program, "Burn");

	return shader;
}

std::unique_ptr<OpenGLSWFrameBuffer::HWVertexBuffer> OpenGLSWFrameBuffer::CreateVertexBuffer(int size)
{
	std::unique_ptr<HWVertexBuffer> obj(new HWVertexBuffer());

	obj->Size = size;

	GLint oldBinding = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldBinding);

	glGenVertexArrays(1, (GLuint*)&obj->VertexArray);
	glGenBuffers(1, (GLuint*)&obj->Buffer);
	glBindVertexArray(obj->VertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, obj->Buffer);
	FGLDebug::LabelObject(GL_BUFFER, obj->Buffer, "VertexBuffer");
	glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, x));
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, color0));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, color1));
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, tu));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(oldBinding);

	return obj;
}

std::unique_ptr<OpenGLSWFrameBuffer::HWIndexBuffer> OpenGLSWFrameBuffer::CreateIndexBuffer(int size)
{
	std::unique_ptr<HWIndexBuffer> obj(new HWIndexBuffer());

	obj->Size = size;

	GLint oldBinding = 0;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldBinding);

	glGenBuffers(1, (GLuint*)&obj->Buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->Buffer);
	FGLDebug::LabelObject(GL_BUFFER, obj->Buffer, "IndexBuffer");
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldBinding);

	return obj;
}

std::unique_ptr<OpenGLSWFrameBuffer::HWTexture> OpenGLSWFrameBuffer::CreateTexture(const FString &name, int width, int height, int levels, int format)
{
	std::unique_ptr<HWTexture> obj(new HWTexture());

	obj->Format = format;

	GLint oldBinding = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);

	glGenTextures(1, (GLuint*)&obj->Texture);
	glBindTexture(GL_TEXTURE_2D, obj->Texture);
	GLenum srcformat;
	switch (format)
	{
	case GL_RGB: srcformat = GL_RGB; break;
	case GL_R8: srcformat = GL_RED; break;
	case GL_RGBA8: srcformat = gl.es ? GL_RGBA : GL_BGRA; break;
	case GL_RGBA16F: srcformat = GL_RGBA; break;
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: srcformat = GL_RGB; break;
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: srcformat = GL_RGBA; break;
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: srcformat = GL_RGBA; break;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: srcformat = GL_RGBA; break;
	default:
		I_FatalError("Unknown format passed to CreateTexture");
		return nullptr;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, srcformat, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	FGLDebug::LabelObject(GL_TEXTURE, obj->Texture, name);

	glBindTexture(GL_TEXTURE_2D, oldBinding);

	return obj;
}

std::unique_ptr<OpenGLSWFrameBuffer::HWTexture> OpenGLSWFrameBuffer::CopyCurrentScreen()
{
	std::unique_ptr<HWTexture> obj(new HWTexture());
	obj->Format = GL_RGBA16F;

	GLint oldBinding = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);

	glGenTextures(1, (GLuint*)&obj->Texture);
	glBindTexture(GL_TEXTURE_2D, obj->Texture);

	glCopyTexImage2D(GL_TEXTURE_2D, 0, obj->Format, 0, 0, Width, Height, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	FGLDebug::LabelObject(GL_TEXTURE, obj->Texture, "CopyCurrentScreen");

	glBindTexture(GL_TEXTURE_2D, oldBinding);

	return obj;
}

void OpenGLSWFrameBuffer::SetGammaRamp(const GammaRamp *ramp)
{
}

void OpenGLSWFrameBuffer::SetPixelShaderConstantF(int uniformIndex, const float *data, int vec4fcount)
{
	assert(uniformIndex < NumPSCONST && vec4fcount == 1); // This emulation of d3d9 only works for very simple stuff
	for (int i = 0; i < 4; i++)
		ShaderConstants[uniformIndex * 4 + i] = data[i];
	if (CurrentShader && CurrentShader->ConstantLocations[uniformIndex] != -1)
		glUniform4fv(CurrentShader->ConstantLocations[uniformIndex], vec4fcount, data);
}

void OpenGLSWFrameBuffer::SetHWPixelShader(HWPixelShader *shader)
{
	if (shader != CurrentShader)
	{
		if (shader)
		{
			glUseProgram(shader->Program);
			for (int i = 0; i < NumPSCONST; i++)
			{
				if (shader->ConstantLocations[i] != -1)
					glUniform4fv(shader->ConstantLocations[i], 1, &ShaderConstants[i * 4]);
			}
		}
		else
		{
			glUseProgram(0);
		}
	}
	CurrentShader = shader;
}

void OpenGLSWFrameBuffer::SetStreamSource(HWVertexBuffer *vertexBuffer)
{
	if (vertexBuffer)
		glBindVertexArray(vertexBuffer->VertexArray);
	else
		glBindVertexArray(0);
}

void OpenGLSWFrameBuffer::SetIndices(HWIndexBuffer *indexBuffer)
{
	if (indexBuffer)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->Buffer);
	else
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGLSWFrameBuffer::DrawTriangleFans(int count, const FBVERTEX *vertices)
{
	count = 2 + count;

	GLint oldBinding = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldBinding);

	if (!StreamVertexBuffer)
	{
		StreamVertexBuffer.reset(new HWVertexBuffer());
		glGenVertexArrays(1, (GLuint*)&StreamVertexBuffer->VertexArray);
		glGenBuffers(1, (GLuint*)&StreamVertexBuffer->Buffer);
		glBindVertexArray(StreamVertexBuffer->VertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, StreamVertexBuffer->Buffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(FBVERTEX), vertices, GL_STREAM_DRAW);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, x));
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, color0));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, color1));
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, tu));
	}
	else
	{
		glBindVertexArray(StreamVertexBuffer->VertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, StreamVertexBuffer->Buffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(FBVERTEX), vertices, GL_STREAM_DRAW);
	}

	glDrawArrays(GL_TRIANGLE_FAN, 0, count);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(oldBinding);
}

void OpenGLSWFrameBuffer::DrawTriangleFans(int count, const BURNVERTEX *vertices)
{
	count = 2 + count;

	GLint oldBinding = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldBinding);

	if (!StreamVertexBufferBurn)
	{
		StreamVertexBufferBurn.reset(new HWVertexBuffer());
		glGenVertexArrays(1, (GLuint*)&StreamVertexBufferBurn->VertexArray);
		glGenBuffers(1, (GLuint*)&StreamVertexBufferBurn->Buffer);
		glBindVertexArray(StreamVertexBufferBurn->VertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, StreamVertexBufferBurn->Buffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(BURNVERTEX), vertices, GL_STREAM_DRAW);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BURNVERTEX), (const GLvoid*)offsetof(BURNVERTEX, x));
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BURNVERTEX), (const GLvoid*)offsetof(BURNVERTEX, tu0));
	}
	else
	{
		glBindVertexArray(StreamVertexBufferBurn->VertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, StreamVertexBufferBurn->Buffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(BURNVERTEX), vertices, GL_STREAM_DRAW);
	}

	glDrawArrays(GL_TRIANGLE_FAN, 0, count);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(oldBinding);
}

void OpenGLSWFrameBuffer::DrawPoints(int count, const FBVERTEX *vertices)
{
	GLint oldBinding = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldBinding);

	if (!StreamVertexBuffer)
	{
		StreamVertexBuffer.reset(new HWVertexBuffer());
		glGenVertexArrays(1, (GLuint*)&StreamVertexBuffer->VertexArray);
		glGenBuffers(1, (GLuint*)&StreamVertexBuffer->Buffer);
		glBindVertexArray(StreamVertexBuffer->VertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, StreamVertexBuffer->Buffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(FBVERTEX), vertices, GL_STREAM_DRAW);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, x));
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, color0));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, color1));
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(FBVERTEX), (const GLvoid*)offsetof(FBVERTEX, tu));
	}
	else
	{
		glBindVertexArray(StreamVertexBuffer->VertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, StreamVertexBuffer->Buffer);
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(FBVERTEX), vertices, GL_STREAM_DRAW);
	}

	glDrawArrays(GL_POINTS, 0, count);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(oldBinding);
}

void OpenGLSWFrameBuffer::DrawLineList(int count)
{
	glDrawArrays(GL_LINES, 0, count * 2);
}

void OpenGLSWFrameBuffer::DrawTriangleList(int minIndex, int numVertices, int startIndex, int primitiveCount)
{
	glDrawRangeElements(GL_TRIANGLES, minIndex, minIndex + numVertices - 1, primitiveCount * 3, GL_UNSIGNED_SHORT, (const void*)(startIndex * sizeof(uint16_t)));
}

void OpenGLSWFrameBuffer::GetLetterboxFrame(int &letterboxX, int &letterboxY, int &letterboxWidth, int &letterboxHeight)
{
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();

	float scaleX, scaleY;
	if (ViewportIsScaled43())
	{
		scaleX = MIN(clientWidth / (float)Width, clientHeight / (Height * 1.2f));
		scaleY = scaleX * 1.2f;
	}
	else
	{
		scaleX = MIN(clientWidth / (float)Width, clientHeight / (float)Height);
		scaleY = scaleX;
	}

	letterboxWidth = (int)round(Width * scaleX);
	letterboxHeight = (int)round(Height * scaleY);
	letterboxX = (clientWidth - letterboxWidth) / 2;
	letterboxY = (clientHeight - letterboxHeight) / 2;
}

void OpenGLSWFrameBuffer::Present()
{
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();
	if (clientWidth > 0 && clientHeight > 0)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, clientWidth, clientHeight);

		int letterboxX, letterboxY, letterboxWidth, letterboxHeight;
		GetLetterboxFrame(letterboxX, letterboxY, letterboxWidth, letterboxHeight);
		DrawLetterbox(letterboxX, letterboxY, letterboxWidth, letterboxHeight);
		glViewport(letterboxX, letterboxY, letterboxWidth, letterboxHeight);

		FBVERTEX verts[4];
		CalcFullscreenCoords(verts, false, 0, 0xFFFFFFFF);
		SetTexture(0, OutputFB->Texture.get());

		if (ViewportLinearScale())
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}

		SetPixelShader(Shaders[SHADER_GammaCorrection].get());
		SetAlphaBlend(0);
		EnableAlphaTest(false);
		DrawTriangleFans(2, verts);
	}

	SwapBuffers();
	Debug->Update();

	float screensize[4] = { (float)Width, (float)Height, 1.0f, 1.0f };
	SetPixelShaderConstantF(PSCONST_ScreenSize, screensize, 1);

	glBindFramebuffer(GL_FRAMEBUFFER, OutputFB->Framebuffer);
	glViewport(0, 0, Width, Height);
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: SetInitialState
//
// Called after initial device creation and reset, when everything is set
// to OpenGL's defaults.
//
//==========================================================================

void OpenGLSWFrameBuffer::SetInitialState()
{
	if (gl.es) UseMappedMemBuffer = false;

	AlphaBlendEnabled = false;
	AlphaBlendOp = GL_FUNC_ADD;
	AlphaSrcBlend = 0;
	AlphaDestBlend = 0;

	CurPixelShader = nullptr;
	memset(Constant, 0, sizeof(Constant));

	for (unsigned i = 0; i < countof(Texture); ++i)
	{
		Texture[i] = nullptr;
		SamplerWrapS[i] = GL_CLAMP_TO_EDGE;
		SamplerWrapT[i] = GL_CLAMP_TO_EDGE;
	}

	NeedGammaUpdate = true;
	NeedPalUpdate = true;

	// This constant is used for grayscaling weights (.xyz) and color inversion (.w)
	float weights[4] = { 77 / 256.f, 143 / 256.f, 37 / 256.f, 1 };
	SetPixelShaderConstantF(PSCONST_Weights, weights, 1);

	float screensize[4] = { (float)Width, (float)Height, 1.0f, 1.0f };
	SetPixelShaderConstantF(PSCONST_ScreenSize, screensize, 1);

	AlphaTestEnabled = false;

	CurBorderColor = 0;

	// Clear to black, just in case it wasn't done already.
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CreateResources
//
//==========================================================================

bool OpenGLSWFrameBuffer::CreateResources()
{
	Atlases = nullptr;
	if (!LoadShaders())
		return false;

	OutputFB = CreateFrameBuffer("OutputFB", Width, Height);
	if (!OutputFB)
		return false;
		
	glBindFramebuffer(GL_FRAMEBUFFER, OutputFB->Framebuffer);

	if (!CreateFBTexture() ||
		!CreatePaletteTexture())
	{
		return false;
	}
	if (!CreateVertexes())
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: LoadShaders
//
// Returns true if all required shaders were loaded. (Gamma and burn wipe
// are the only ones not considered "required".)
//
//==========================================================================

bool OpenGLSWFrameBuffer::LoadShaders()
{
	int lumpvert = Wads.CheckNumForFullName("shaders/glsl/swshader.vp");
	int lumpfrag = Wads.CheckNumForFullName("shaders/glsl/swshader.fp");
	if (lumpvert < 0 || lumpfrag < 0)
		return false;

	FString vertsource = Wads.ReadLump(lumpvert).GetString();
	FString fragsource = Wads.ReadLump(lumpfrag).GetString();

	FString shaderdir, shaderpath;
	unsigned int i;

	for (i = 0; i < NUM_SHADERS; ++i)
	{
		shaderpath = shaderdir;
		Shaders[i] = CreatePixelShader(vertsource, fragsource, ShaderDefines[i]);
		if (!Shaders[i] && i < SHADER_BurnWipe)
		{
			break;
		}

		glUseProgram(Shaders[i]->Program);
		if (Shaders[i]->ImageLocation != -1) glUniform1i(Shaders[i]->ImageLocation, 0);
		if (Shaders[i]->PaletteLocation != -1) glUniform1i(Shaders[i]->PaletteLocation, 1);
		if (Shaders[i]->NewScreenLocation != -1) glUniform1i(Shaders[i]->NewScreenLocation, 0);
		if (Shaders[i]->BurnLocation != -1) glUniform1i(Shaders[i]->BurnLocation, 1);
		glUseProgram(0);
	}
	if (i == NUM_SHADERS)
	{ // Success!
		return true;
	}
	// Failure. Release whatever managed to load (which is probably nothing.)
	for (i = 0; i < NUM_SHADERS; ++i)
	{
		Shaders[i].reset();
	}
	return false;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: ReleaseResources
//
//==========================================================================

void OpenGLSWFrameBuffer::ReleaseResources()
{
#ifdef WIN32
	I_SaveWindowedPos();
#endif
	KillNativeTexs();
	KillNativePals();
	ReleaseDefaultPoolItems();
	ScreenshotTexture.reset();
	PaletteTexture.reset();
	for (int i = 0; i < NUM_SHADERS; ++i)
	{
		Shaders[i].reset();
	}
	if (ScreenWipe != nullptr)
	{
		delete ScreenWipe;
		ScreenWipe = nullptr;
	}
	Atlas *pack, *next;
	for (pack = Atlases; pack != nullptr; pack = next)
	{
		next = pack->Next;
		delete pack;
	}
	GatheringWipeScreen = false;
}

void OpenGLSWFrameBuffer::ReleaseDefaultPoolItems()
{
	FBTexture.reset();
	FinalWipeScreen.reset();
	InitialWipeScreen.reset();
	VertexBuffer.reset();
	IndexBuffer.reset();
	OutputFB.reset();
}

bool OpenGLSWFrameBuffer::Reset()
{
	ReleaseDefaultPoolItems();

	OutputFB = CreateFrameBuffer("OutputFB", Width, Height);
	if (!OutputFB || !CreateFBTexture() || !CreateVertexes())
	{
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, OutputFB->Framebuffer);
	glViewport(0, 0, Width, Height);

	SetInitialState();
	return true;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: KillNativePals
//
// Frees all native palettes.
//
//==========================================================================

void OpenGLSWFrameBuffer::KillNativePals()
{
	while (Palettes != nullptr)
	{
		delete Palettes;
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: KillNativeTexs
//
// Frees all native textures.
//
//==========================================================================

void OpenGLSWFrameBuffer::KillNativeTexs()
{
	while (Textures != nullptr)
	{
		delete Textures;
	}
}

bool OpenGLSWFrameBuffer::CreateFBTexture()
{
	FBTexture = CreateTexture("FBTexture", Width, Height, 1, IsBgra() ? GL_RGBA8 : GL_R8);
	return FBTexture != nullptr;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CreatePaletteTexture
//
//==========================================================================

bool OpenGLSWFrameBuffer::CreatePaletteTexture()
{
	PaletteTexture = CreateTexture("PaletteTexture", 256, 1, 1, GL_RGBA8);
	return PaletteTexture != nullptr;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CreateVertexes
//
//==========================================================================

bool OpenGLSWFrameBuffer::CreateVertexes()
{
	VertexPos = -1;
	IndexPos = -1;
	QuadBatchPos = -1;
	BatchType = BATCH_None;
	VertexBuffer = CreateVertexBuffer(sizeof(FBVERTEX)*NUM_VERTS);
	if (!VertexBuffer)
	{
		return false;
	}
	IndexBuffer = CreateIndexBuffer(sizeof(uint16_t)*NUM_INDEXES);
	if (!IndexBuffer)
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CalcFullscreenCoords
//
//==========================================================================

void OpenGLSWFrameBuffer::CalcFullscreenCoords(FBVERTEX verts[4], bool viewarea_only, uint32_t color0, uint32_t color1) const
{
	float mxl, mxr, myt, myb, tmxl, tmxr, tmyt, tmyb;

	if (viewarea_only)
	{ // Just calculate vertices for the viewarea/BlendingRect
		mxl = float(BlendingRect.left);
		mxr = float(BlendingRect.right);
		myt = float(BlendingRect.top);
		myb = float(BlendingRect.bottom);
		tmxl = float(BlendingRect.left) / float(Width);
		tmxr = float(BlendingRect.right) / float(Width);
		tmyt = float(BlendingRect.top) / float(Height);
		tmyb = float(BlendingRect.bottom) / float(Height);
	}
	else
	{ // Calculate vertices for the whole screen
		mxl = 0.0f;
		mxr = float(Width);
		myt = 0.0f;
		myb = float(Height);
		tmxl = 0;
		tmxr = 1.0f;
		tmyt = 0;
		tmyb = 1.0f;
	}

	//{   mxl, myt, 0, 1, 0, 0xFFFFFFFF,    tmxl,    tmyt },
	//{   mxr, myt, 0, 1, 0, 0xFFFFFFFF,    tmxr,    tmyt },
	//{   mxr, myb, 0, 1, 0, 0xFFFFFFFF,    tmxr,    tmyb },
	//{   mxl, myb, 0, 1, 0, 0xFFFFFFFF,    tmxl,    tmyb },

	verts[0].x = mxl;
	verts[0].y = myt;
	verts[0].z = 0;
	verts[0].rhw = 1;
	verts[0].color0 = color0;
	verts[0].color1 = color1;
	verts[0].tu = tmxl;
	verts[0].tv = tmyt;

	verts[1].x = mxr;
	verts[1].y = myt;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color0;
	verts[1].color1 = color1;
	verts[1].tu = tmxr;
	verts[1].tv = tmyt;

	verts[2].x = mxr;
	verts[2].y = myb;
	verts[2].z = 0;
	verts[2].rhw = 1;
	verts[2].color0 = color0;
	verts[2].color1 = color1;
	verts[2].tu = tmxr;
	verts[2].tv = tmyb;

	verts[3].x = mxl;
	verts[3].y = myb;
	verts[3].z = 0;
	verts[3].rhw = 1;
	verts[3].color0 = color0;
	verts[3].color1 = color1;
	verts[3].tu = tmxl;
	verts[3].tv = tmyb;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: GetPageCount
//
//==========================================================================

int OpenGLSWFrameBuffer::GetPageCount()
{
	return 2;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: IsValid
//
//==========================================================================

bool OpenGLSWFrameBuffer::IsValid()
{
	return Valid;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Lock
//
//==========================================================================

bool OpenGLSWFrameBuffer::Lock(bool buffered)
{
	if (m_Lock++ > 0)
	{
		return false;
	}
	assert(!In2D);
	Accel2D = vid_hw2d;
	if (UseMappedMemBuffer)
	{
		if (!MappedMemBuffer)
		{
			BindFBBuffer();

			MappedMemBuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
			Pitch = Width;
			if (MappedMemBuffer == nullptr)
				return true;
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
		Buffer = (uint8_t*)MappedMemBuffer;
	}
	else
	{
		Buffer = MemBuffer;
	}
	return false;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Unlock
//
//==========================================================================

void OpenGLSWFrameBuffer::Unlock()
{
	if (m_Lock == 0)
	{
		return;
	}

	if (UpdatePending && m_Lock == 1)
	{
		Update();
	}
	else if (--m_Lock == 0)
	{
		Buffer = nullptr;

		if (MappedMemBuffer)
		{
			BindFBBuffer();
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			MappedMemBuffer = nullptr;
		}
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Update
//
// When In2D == 0: Copy buffer to screen and present
// When In2D == 1: Copy buffer to screen but do not present
// When In2D == 2: Set up for 2D drawing but do not draw anything
// When In2D == 3: Present and set In2D to 0
//
//==========================================================================

void OpenGLSWFrameBuffer::Update()
{
	if (In2D == 3)
	{
		if (InScene)
		{
			DrawRateStuff();
			DrawPackedTextures(gl_showpacks);
			EndBatch();		// Make sure all batched primitives are drawn.
			Flip();
		}
		In2D = 0;
		return;
	}

	if (m_Lock != 1)
	{
		I_FatalError("Framebuffer must have exactly 1 lock to be updated");
		if (m_Lock > 0)
		{
			UpdatePending = true;
			--m_Lock;
		}
		return;
	}

	if (In2D == 0)
	{
		DrawRateStuff();
	}

	if (NeedGammaUpdate)
	{
		float psgamma[4];
		float igamma;

		NeedGammaUpdate = false;
		igamma = 1 / Gamma;
		if (IsFullscreen())
		{
			GammaRamp ramp;

			for (int i = 0; i < 256; ++i)
			{
				ramp.blue[i] = ramp.green[i] = ramp.red[i] = uint16_t(65535.f * powf(i / 255.f, igamma));
			}
			SetGammaRamp(&ramp);
		}
		psgamma[2] = psgamma[1] = psgamma[0] = igamma;
		psgamma[3] = 0.5;		// For SM14 version
		SetPixelShaderConstantF(PSCONST_Gamma, psgamma, 1);
	}

	if (NeedPalUpdate)
	{
		UploadPalette();
		NeedPalUpdate = false;
	}

#ifdef WIN32
	BlitCycles.Reset();
	BlitCycles.Clock();
#endif

	m_Lock = 0;
	Draw3DPart(In2D <= 1);
	if (In2D == 0)
	{
		Flip();
	}

#ifdef WIN32
	BlitCycles.Unclock();
	//LOG1 ("cycles = %d\n", BlitCycles);
#endif

	Buffer = nullptr;
	UpdatePending = false;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Flip
//
//==========================================================================

void OpenGLSWFrameBuffer::Flip()
{
	assert(InScene);

	Present();
	InScene = false;

	if (!IsFullscreen())
	{
		int clientWidth = ViewportScaledWidth(GetClientWidth(), GetClientHeight());
		int clientHeight = ViewportScaledHeight(GetClientWidth(), GetClientHeight());
		if (clientWidth > 0 && clientHeight > 0 && (Width != clientWidth || Height != clientHeight))
		{
			Resize(clientWidth, clientHeight);

			TrueHeight = Height;
			PixelDoubling = 0;
			Reset();

			V_OutputResized(Width, Height);
		}
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: PaintToWindow
//
//==========================================================================

#ifdef WIN32

bool OpenGLSWFrameBuffer::PaintToWindow()
{
	if (m_Lock != 0)
	{
		return false;
	}
	Draw3DPart(true);
	return true;
}

#endif

//==========================================================================
//
// OpenGLSWFrameBuffer :: Draw3DPart
//
// The software 3D part, to be exact.
//
//==========================================================================

void OpenGLSWFrameBuffer::BindFBBuffer()
{
	int usage = UseMappedMemBuffer ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW;

	int pixelsize = IsBgra() ? 4 : 1;
	int size = Width * Height * pixelsize;

	if (FBTexture->Buffers[0] == 0)
	{
		glGenBuffers(2, (GLuint*)FBTexture->Buffers);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, FBTexture->Buffers[1]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, size, nullptr, usage);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, FBTexture->Buffers[0]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, size, nullptr, usage);
	}
	else
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, FBTexture->Buffers[FBTexture->CurrentBuffer]);
	}
}

void OpenGLSWFrameBuffer::BgraToRgba(uint32_t *dest, const uint32_t *src, int width, int height, int srcpitch)
{
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			uint32_t r = RPART(src[x]);
			uint32_t g = GPART(src[x]);
			uint32_t b = BPART(src[x]);
			uint32_t a = APART(src[x]);
			dest[x] = r | (g << 8) | (b << 16) | (a << 24);
		}
		dest += width;
		src += srcpitch;
	}
}

void OpenGLSWFrameBuffer::Draw3DPart(bool copy3d)
{
	if (copy3d)
	{
		BindFBBuffer();
		FBTexture->CurrentBuffer = (FBTexture->CurrentBuffer + 1) & 1;

		if (!UseMappedMemBuffer)
		{
			int pixelsize = IsBgra() ? 4 : 1;
			int size = Width * Height * pixelsize;

			uint8_t *dest = (uint8_t*)MapBuffer(GL_PIXEL_UNPACK_BUFFER, size);
			if (dest)
			{
				if (gl.es && pixelsize == 4)
				{
					BgraToRgba((uint32_t*)dest, (const uint32_t *)MemBuffer, Width, Height, Pitch);
				}
				else if (Pitch == Width)
				{
					memcpy(dest, MemBuffer, Width * Height * pixelsize);
				}
				else
				{
					uint8_t *src = MemBuffer;
					for (int y = 0; y < Height; y++)
					{
						memcpy(dest, src, Width * pixelsize);
						dest += Width * pixelsize;
						src += Pitch * pixelsize;
					}
				}
				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			}
		}
		else if (MappedMemBuffer)
		{
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			MappedMemBuffer = nullptr;
		}

		GLint oldBinding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);
		glBindTexture(GL_TEXTURE_2D, FBTexture->Texture);
		if (IsBgra())
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Width, Height, gl.es ? GL_RGBA : GL_BGRA, GL_UNSIGNED_BYTE, 0);
		else
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Width, Height, GL_RED, GL_UNSIGNED_BYTE, 0);
		glBindTexture(GL_TEXTURE_2D, oldBinding);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	InScene = true;
	if (vid_hwaalines)
		glEnable(GL_LINE_SMOOTH);
	else
		glDisable(GL_LINE_SMOOTH);

	SetTexture(0, FBTexture.get());
	SetPaletteTexture(PaletteTexture.get(), 256, BorderColor);
	memset(Constant, 0, sizeof(Constant));
	SetAlphaBlend(0);
	EnableAlphaTest(false);
	if (IsBgra())
		SetPixelShader(Shaders[SHADER_NormalColor].get());
	else
		SetPixelShader(Shaders[SHADER_NormalColorPal].get());
	if (copy3d)
	{
		FBVERTEX verts[4];
		uint32_t color0, color1;
		if (Accel2D)
		{
			auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
			if (map == nullptr)
			{
				color0 = 0;
				color1 = 0xFFFFFFF;
			}
			else
			{
				color0 = ColorValue(map->ColorizeStart[0] / 2, map->ColorizeStart[1] / 2, map->ColorizeStart[2] / 2, 0);
				color1 = ColorValue(map->ColorizeEnd[0] / 2, map->ColorizeEnd[1] / 2, map->ColorizeEnd[2] / 2, 1);
				if (IsBgra())
					SetPixelShader(Shaders[SHADER_SpecialColormap].get());
				else
					SetPixelShader(Shaders[SHADER_SpecialColormapPal].get());
			}
		}
		else
		{
			color0 = FlashColor0;
			color1 = FlashColor1;
		}
		CalcFullscreenCoords(verts, Accel2D, color0, color1);
		DrawTriangleFans(2, verts);
	}
	if (IsBgra())
		SetPixelShader(Shaders[SHADER_NormalColor].get());
	else
		SetPixelShader(Shaders[SHADER_NormalColorPal].get());
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: DrawLetterbox
//
// Draws the black bars at the top and bottom of the screen for letterboxed
// modes.
//
//==========================================================================

void OpenGLSWFrameBuffer::DrawLetterbox(int x, int y, int width, int height)
{
	int clientWidth = GetClientWidth();
	int clientHeight = GetClientHeight();
	if (clientWidth == 0 || clientHeight == 0)
		return;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_SCISSOR_TEST);
	if (y > 0)
	{
		glScissor(0, 0, clientWidth, y);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (clientHeight - y - height > 0)
	{
		glScissor(0, y + height, clientWidth, clientHeight - y - height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (x > 0)
	{
		glScissor(0, y, x, height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	if (clientWidth - x - width > 0)
	{
		glScissor(x + width, y, clientWidth - x - width, height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_SCISSOR_TEST);
}

void OpenGLSWFrameBuffer::UploadPalette()
{
	if (PaletteTexture->Buffers[0] == 0)
	{
		glGenBuffers(2, (GLuint*)PaletteTexture->Buffers);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PaletteTexture->Buffers[0]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, 256 * 4, nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PaletteTexture->Buffers[1]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, 256 * 4, nullptr, GL_STREAM_DRAW);
		
		if (gl.es) PaletteTexture->MapBuffer.resize(256 * 4);
	}
	else
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PaletteTexture->Buffers[PaletteTexture->CurrentBuffer]);
		PaletteTexture->CurrentBuffer = (PaletteTexture->CurrentBuffer + 1) & 1;
	}

	uint8_t *pix = gl.es ? PaletteTexture->MapBuffer.data() : (uint8_t*)MapBuffer(GL_PIXEL_UNPACK_BUFFER, 256 * 4);
	if (pix)
	{
		int i;

		for (i = 0; i < 256; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = (i == 0 ? 0 : 255);
			// To let masked textures work, the first palette entry's alpha is 0.
		}
		pix += 4;
		for (; i < 255; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = 255;
		}
		if (gl.es)
		{
			uint8_t *tempbuffer = PaletteTexture->MapBuffer.data();
			BgraToRgba((uint32_t*)tempbuffer, (const uint32_t *)tempbuffer, 256, 1, 256);
			glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, 256 * 4, tempbuffer);
		}
		else
		{
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		}
		
		GLint oldBinding = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);
		glBindTexture(GL_TEXTURE_2D, PaletteTexture->Texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, gl.es ? GL_RGBA : GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindTexture(GL_TEXTURE_2D, oldBinding);
		BorderColor = ColorXRGB(SourcePalette[255].r, SourcePalette[255].g, SourcePalette[255].b);
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

PalEntry *OpenGLSWFrameBuffer::GetPalette()
{
	return SourcePalette;
}

void OpenGLSWFrameBuffer::UpdatePalette()
{
	NeedPalUpdate = true;
}

bool OpenGLSWFrameBuffer::SetGamma(float gamma)
{
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool OpenGLSWFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	FlashColor = rgb;
	FlashAmount = amount;

	// Fill in the constants for the pixel shader to do linear interpolation between the palette and the flash:
	float r = rgb.r / 255.f, g = rgb.g / 255.f, b = rgb.b / 255.f, a = amount / 256.f;
	FlashColor0 = ColorValue(r * a, g * a, b * a, 0);
	a = 1 - a;
	FlashColor1 = ColorValue(a, a, a, 1);
	return true;
}

void OpenGLSWFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = FlashColor;
	amount = FlashAmount;
}

void OpenGLSWFrameBuffer::GetFlashedPalette(PalEntry pal[256])
{
	memcpy(pal, SourcePalette, 256 * sizeof(PalEntry));
	if (FlashAmount)
	{
		DoBlending(pal, pal, 256, FlashColor.r, FlashColor.g, FlashColor.b, FlashAmount);
	}
}

void OpenGLSWFrameBuffer::SetVSync(bool vsync)
{
	// Switch to the default frame buffer because Nvidia's driver associates the vsync state with the bound FB object.
	GLint oldDrawFramebufferBinding = 0, oldReadFramebufferBinding = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebufferBinding);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	Super::SetVSync(vsync);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
}

void OpenGLSWFrameBuffer::NewRefreshRate()
{
}

void OpenGLSWFrameBuffer::SetBlendingRect(int x1, int y1, int x2, int y2)
{
	BlendingRect.left = x1;
	BlendingRect.top = y1;
	BlendingRect.right = x2;
	BlendingRect.bottom = y2;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: GetScreenshotBuffer
//
// Returns a pointer into a surface holding the current screen data.
//
//==========================================================================

void OpenGLSWFrameBuffer::GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma)
{
	Super::GetScreenshotBuffer(buffer, pitch, color_type, gamma);
	/*
	LockedRect lrect;

	if (!Accel2D)
	{
		Super::GetScreenshotBuffer(buffer, pitch, color_type, gamma);
		return;
	}
	buffer = nullptr;
	if ((ScreenshotTexture = GetCurrentScreen()) != nullptr)
	{
		if (!ScreenshotTexture->GetSurfaceLevel(0, &ScreenshotSurface))
		{
			delete ScreenshotTexture;
			ScreenshotTexture = nullptr;
		}
		else if (!ScreenshotSurface->LockRect(&lrect, nullptr, false))
		{
			delete ScreenshotSurface;
			ScreenshotSurface = nullptr;
			delete ScreenshotTexture;
			ScreenshotTexture = nullptr;
		}
		else
		{
			buffer = (const uint8_t *)lrect.pBits;
			pitch = lrect.Pitch;
			color_type = SS_BGRA;
			gamma = Gamma;
		}
	}
	*/
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: ReleaseScreenshotBuffer
//
//==========================================================================

void OpenGLSWFrameBuffer::ReleaseScreenshotBuffer()
{
	if (m_Lock > 0)
	{
		Super::ReleaseScreenshotBuffer();
	}
	ScreenshotTexture.reset();
}

/**************************************************************************/
/*                                  2D Stuff                              */
/**************************************************************************/

//==========================================================================
//
// OpenGLSWFrameBuffer :: DrawPackedTextures
//
// DEBUG: Draws the texture atlases to the screen, starting with the
// 1-based packnum. Ignores atlases that are flagged for use by one
// texture only.
//
//==========================================================================

void OpenGLSWFrameBuffer::DrawPackedTextures(int packnum)
{
	uint32_t empty_colors[8] =
	{
		0x50FF0000, 0x5000FF00, 0x500000FF, 0x50FFFF00,
		0x50FF00FF, 0x5000FFFF, 0x50FF8000, 0x500080FF
	};
	Atlas *pack;
	int x = 8, y = 8;

	if (packnum <= 0)
	{
		return;
	}
	pack = Atlases;
	// Find the first texture atlas that is an actual atlas.
	while (pack != nullptr && pack->OneUse)
	{ // Skip textures that aren't used as atlases
		pack = pack->Next;
	}
	// Skip however many atlases we would have otherwise drawn
	// until we've skipped <packnum> of them.
	while (pack != nullptr && packnum != 1)
	{
		if (!pack->OneUse)
		{ // Skip textures that aren't used as atlases
			packnum--;
		}
		pack = pack->Next;
	}
	// Draw atlases until we run out of room on the screen.
	while (pack != nullptr)
	{
		if (pack->OneUse)
		{ // Skip textures that aren't used as atlases
			pack = pack->Next;
			continue;
		}

		AddColorOnlyRect(x - 1, y - 1, 258, 258, ColorXRGB(255, 255, 0));
		int back = 0;
		for (PackedTexture *box = pack->UsedList; box != nullptr; box = box->Next)
		{
			AddColorOnlyQuad(
				x + box->Area.left * 256 / pack->Width,
				y + box->Area.top * 256 / pack->Height,
				(box->Area.right - box->Area.left) * 256 / pack->Width,
				(box->Area.bottom - box->Area.top) * 256 / pack->Height, empty_colors[back]);
			back = (back + 1) & 7;
		}
		//		AddColorOnlyQuad(x, y-LBOffsetI, 256, 256, ColorARGB(180,0,0,0));

		CheckQuadBatch();

		BufferedTris *quad = &QuadExtra[QuadBatchPos];
		FBVERTEX *vert = &VertexData[VertexPos];

		quad->ClearSetup();
		if (pack->Format == GL_R8/* && !tex->IsGray*/)
		{
			quad->Flags = BQF_WrapUV | BQF_GamePalette/* | BQF_DisableAlphaTest*/;
			quad->ShaderNum = BQS_PalTex;
		}
		else
		{
			quad->Flags = BQF_WrapUV/* | BQF_DisableAlphaTest*/;
			quad->ShaderNum = BQS_Plain;
		}
		quad->Palette = nullptr;
		quad->Texture = pack->Tex.get();
		quad->NumVerts = 4;
		quad->NumTris = 2;

		float x0 = float(x);
		float y0 = float(y);
		float x1 = x0 + 256.f;
		float y1 = y0 + 256.f;

		vert[0].x = x0;
		vert[0].y = y0;
		vert[0].z = 0;
		vert[0].rhw = 1;
		vert[0].color0 = 0;
		vert[0].color1 = 0xFFFFFFFF;
		vert[0].tu = 0;
		vert[0].tv = 0;

		vert[1].x = x1;
		vert[1].y = y0;
		vert[1].z = 0;
		vert[1].rhw = 1;
		vert[1].color0 = 0;
		vert[1].color1 = 0xFFFFFFFF;
		vert[1].tu = 1;
		vert[1].tv = 0;

		vert[2].x = x1;
		vert[2].y = y1;
		vert[2].z = 0;
		vert[2].rhw = 1;
		vert[2].color0 = 0;
		vert[2].color1 = 0xFFFFFFFF;
		vert[2].tu = 1;
		vert[2].tv = 1;

		vert[3].x = x0;
		vert[3].y = y1;
		vert[3].z = 0;
		vert[3].rhw = 1;
		vert[3].color0 = 0;
		vert[3].color1 = 0xFFFFFFFF;
		vert[3].tu = 0;
		vert[3].tv = 1;

		IndexData[IndexPos] = VertexPos;
		IndexData[IndexPos + 1] = VertexPos + 1;
		IndexData[IndexPos + 2] = VertexPos + 2;
		IndexData[IndexPos + 3] = VertexPos;
		IndexData[IndexPos + 4] = VertexPos + 2;
		IndexData[IndexPos + 5] = VertexPos + 3;

		QuadBatchPos++;
		VertexPos += 4;
		IndexPos += 6;

		x += 256 + 8;
		if (x > Width - 256)
		{
			x = 8;
			y += 256 + 8;
			if (y > Height - 256)
			{
				return;
			}
		}
		pack = pack->Next;
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: AllocPackedTexture
//
// Finds space to pack an image inside a texture atlas and returns it.
// Large images and those that need to wrap always get their own textures.
//
//==========================================================================

OpenGLSWFrameBuffer::PackedTexture *OpenGLSWFrameBuffer::AllocPackedTexture(int w, int h, bool wrapping, int format)
{
	Atlas *pack;
	Rect box;
	bool padded;

	// The - 2 to account for padding
	if (w > 256 - 2 || h > 256 - 2 || wrapping)
	{ // Create a new texture atlas.
		pack = new Atlas(this, w, h, format);
		pack->OneUse = true;
		box = pack->Packer.Insert(w, h);
		padded = false;
	}
	else
	{ // Try to find space in an existing texture atlas.
		w += 2; // Add padding
		h += 2;
		for (pack = Atlases; pack != nullptr; pack = pack->Next)
		{
			// Use the first atlas it fits in.
			if (pack->Format == format)
			{
				box = pack->Packer.Insert(w, h);
				if (box.width != 0)
				{
					break;
				}
			}
		}
		if (pack == nullptr)
		{ // Create a new texture atlas.
			pack = new Atlas(this, DEF_ATLAS_WIDTH, DEF_ATLAS_HEIGHT, format);
			box = pack->Packer.Insert(w, h);
		}
		padded = true;
	}
	assert(box.width != 0 && box.height != 0);
	return pack->AllocateImage(box, padded);
}

//==========================================================================
//
// Atlas Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::Atlas::Atlas(OpenGLSWFrameBuffer *fb, int w, int h, int format)
	: Packer(w, h, true)
{
	Format = format;
	UsedList = nullptr;
	OneUse = false;
	Width = 0;
	Height = 0;
	Next = nullptr;

	// Attach to the end of the atlas list
	Atlas **prev = &fb->Atlases;
	while (*prev != nullptr)
	{
		prev = &((*prev)->Next);
	}
	*prev = this;

	Tex = fb->CreateTexture("Atlas", w, h, 1, format);
	Width = w;
	Height = h;
}

//==========================================================================
//
// Atlas Destructor
//
//==========================================================================

OpenGLSWFrameBuffer::Atlas::~Atlas()
{
	PackedTexture *box, *next;

	Tex.reset();
	for (box = UsedList; box != nullptr; box = next)
	{
		next = box->Next;
		delete box;
	}
}

//==========================================================================
//
// Atlas :: AllocateImage
//
// Moves the box from the empty list to the used list, sizing it to the
// requested dimensions and adding additional boxes to the empty list if
// needed.
//
// The passed box *MUST* be in this texture atlas's empty list.
//
//==========================================================================

OpenGLSWFrameBuffer::PackedTexture *OpenGLSWFrameBuffer::Atlas::AllocateImage(const Rect &rect, bool padded)
{
	PackedTexture *box = new PackedTexture;

	box->Owner = this;
	box->Area.left = rect.x;
	box->Area.top = rect.y;
	box->Area.right = rect.x + rect.width;
	box->Area.bottom = rect.y + rect.height;

	box->Left = float(box->Area.left + padded) / Width;
	box->Right = float(box->Area.right - padded) / Width;
	box->Top = float(box->Area.top + padded) / Height;
	box->Bottom = float(box->Area.bottom - padded) / Height;

	box->Padded = padded;

	// Add it to the used list.
	box->Next = UsedList;
	if (box->Next != nullptr)
	{
		box->Next->Prev = &box->Next;
	}
	UsedList = box;
	box->Prev = &UsedList;

	return box;
}

//==========================================================================
//
// Atlas :: FreeBox
//
// Removes a box from the used list and deletes it. Space is returned to the
// waste list. Once all boxes for this atlas are freed, the entire bin
// packer is reinitialized for maximum efficiency.
//
//==========================================================================

void OpenGLSWFrameBuffer::Atlas::FreeBox(OpenGLSWFrameBuffer::PackedTexture *box)
{
	*(box->Prev) = box->Next;
	if (box->Next != nullptr)
	{
		box->Next->Prev = box->Prev;
	}
	Rect waste;
	waste.x = box->Area.left;
	waste.y = box->Area.top;
	waste.width = box->Area.right - box->Area.left;
	waste.height = box->Area.bottom - box->Area.top;
	box->Owner->Packer.AddWaste(waste);
	delete box;
	if (UsedList == nullptr)
	{
		Packer.Init(Width, Height, true);
	}
}

//==========================================================================
//
// OpenGLTex Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::OpenGLTex::OpenGLTex(FTexture *tex, FTextureFormat fmt, OpenGLSWFrameBuffer *fb, bool wrapping)
	: FNativeTexture(tex, fmt)
{
	// Attach to the texture list for the OpenGLSWFrameBuffer
	Next = fb->Textures;
	if (Next != nullptr)
	{
		Next->Prev = &Next;
	}
	Prev = &fb->Textures;
	fb->Textures = this;

	GameTex = tex;
	Box = nullptr;
	IsGray = false;

	Create(fb, wrapping);
}

//==========================================================================
//
// OpenGLTex Destructor
//
//==========================================================================

OpenGLSWFrameBuffer::OpenGLTex::~OpenGLTex()
{
	if (Box != nullptr)
	{
		Box->Owner->FreeBox(Box);
		Box = nullptr;
	}
	// Detach from the texture list
	*Prev = Next;
	if (Next != nullptr)
	{
		Next->Prev = Prev;
	}
	// Remove link from the game texture
	if (GameTex != nullptr)
	{
		mGameTex->Native[mFormat] = nullptr;
	}
}

//==========================================================================
//
// OpenGLTex :: CheckWrapping
//
// Returns true if the texture is compatible with the specified wrapping
// mode.
//
//==========================================================================

bool OpenGLSWFrameBuffer::OpenGLTex::CheckWrapping(bool wrapping)
{
	// If it doesn't need to wrap, then it works.
	if (!wrapping)
	{
		return true;
	}
	// If it needs to wrap, then it can't be packed inside another texture.
	return Box->Owner->OneUse;
}

//==========================================================================
//
// OpenGLTex :: Create
//
// Creates an HWTexture for the texture and copies the image data
// to it. Note that unlike FTexture, this image is row-major.
//
//==========================================================================

bool OpenGLSWFrameBuffer::OpenGLTex::Create(OpenGLSWFrameBuffer *fb, bool wrapping)
{
	assert(Box == nullptr);
	if (Box != nullptr)
	{
		Box->Owner->FreeBox(Box);
	}

	Box = fb->AllocPackedTexture(GameTex->GetWidth(), GameTex->GetHeight(), wrapping, GetTexFormat());

	if (Box == nullptr)
	{
		return false;
	}
	if (!Update())
	{
		Box->Owner->FreeBox(Box);
		Box = nullptr;
		return false;
	}
	return true;
}

//==========================================================================
//
// OpenGLTex :: Update
//
// Copies image data from the underlying FTexture to the OpenGL texture.
//
//==========================================================================

bool OpenGLSWFrameBuffer::OpenGLTex::Update()
{
	LTRBRect rect;
	uint8_t *dest;

	assert(Box != nullptr);
	assert(Box->Owner != nullptr);
	assert(Box->Owner->Tex != nullptr);
	assert(GameTex != nullptr);

	int format = Box->Owner->Tex->Format;

	rect = Box->Area;

	if (Box->Owner->Tex->Buffers[0] == 0)
		glGenBuffers(2, (GLuint*)Box->Owner->Tex->Buffers);

	int bytesPerPixel = 4;
	switch (format)
	{
	case GL_R8: bytesPerPixel = 1; break;
	case GL_RGBA8: bytesPerPixel = 4; break;
	default: return false;
	}

	int buffersize = (rect.right - rect.left) * (rect.bottom - rect.top) * bytesPerPixel;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Box->Owner->Tex->Buffers[Box->Owner->Tex->CurrentBuffer]);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, buffersize, nullptr, GL_STREAM_DRAW);
	Box->Owner->Tex->CurrentBuffer = (Box->Owner->Tex->CurrentBuffer + 1) & 1;
	
	static std::vector<uint8_t> tempbuffer;
	if (gl.es)
		tempbuffer.resize(buffersize);

	int pitch = (rect.right - rect.left) * bytesPerPixel;
	uint8_t *bits = gl.es ? tempbuffer.data() : (uint8_t *)MapBuffer(GL_PIXEL_UNPACK_BUFFER, buffersize);
	dest = bits;
	if (!dest)
	{
		return false;
	}
	if (Box->Padded)
	{
		dest += pitch + (format == GL_R8 ? 1 : 4);
	}
	GameTex->FillBuffer(dest, pitch, GameTex->GetHeight(), mFormat);
	if (Box->Padded)
	{
		// Clear top padding row.
		dest = bits;
		int numbytes = GameTex->GetWidth() + 2;
		if (format != GL_R8)
		{
			numbytes <<= 2;
		}
		memset(dest, 0, numbytes);
		dest += pitch;
		// Clear left and right padding columns.
		if (format == GL_R8)
		{
			for (int y = Box->Area.bottom - Box->Area.top - 2; y > 0; --y)
			{
				dest[0] = 0;
				dest[numbytes - 1] = 0;
				dest += pitch;
			}
		}
		else
		{
			for (int y = Box->Area.bottom - Box->Area.top - 2; y > 0; --y)
			{
				*(uint32_t *)dest = 0;
				*(uint32_t *)(dest + numbytes - 4) = 0;
				dest += pitch;
			}
		}
		// Clear bottom padding row.
		memset(dest, 0, numbytes);
	}
	
	if (gl.es && format == GL_RGBA8)
	{
		BgraToRgba((uint32_t*)bits, (const uint32_t *)bits, rect.right - rect.left, rect.bottom - rect.top, rect.right - rect.left);
	}

	if (gl.es)
		glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, buffersize, bits);
	else
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	GLint oldBinding = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);
	glBindTexture(GL_TEXTURE_2D, Box->Owner->Tex->Texture);
	if (format == GL_RGBA8)
		glTexSubImage2D(GL_TEXTURE_2D, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, gl.es ? GL_RGBA : GL_BGRA, GL_UNSIGNED_BYTE, 0);
	else
		glTexSubImage2D(GL_TEXTURE_2D, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, GL_RED, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, oldBinding);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	return true;
}

//==========================================================================
//
// OpenGLTex :: GetTexFormat
//
// Returns the texture format that would best fit this texture.
//
//==========================================================================

int OpenGLSWFrameBuffer::OpenGLTex::GetTexFormat()
{
	IsGray = false;

	switch (mFormat)
	{
	case TEX_Pal:	return GL_R8;
	case TEX_Gray:	IsGray = true; return GL_R8;
	case TEX_RGB:	return GL_RGBA8;
#if 0
	case TEX_DXT1:	return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
	case TEX_DXT2:	return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	case TEX_DXT3:	return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	case TEX_DXT4:	return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // Doesn't exist in OpenGL. Closest match is DXT5.
	case TEX_DXT5:	return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#endif
	default:		I_FatalError("GameTex->GetFormat() returned invalid format.");
	}
	return GL_R8;
}

//==========================================================================
//
// OpenGLPal Constructor
//
//==========================================================================

OpenGLSWFrameBuffer::OpenGLPal::OpenGLPal(FRemapTable *remap, OpenGLSWFrameBuffer *fb)
	: Remap(remap)
{
	int count;

	// Attach to the palette list for the OpenGLSWFrameBuffer
	Next = fb->Palettes;
	if (Next != nullptr)
	{
		Next->Prev = &Next;
	}
	Prev = &fb->Palettes;
	fb->Palettes = this;

	int pow2count;

	// Round up to the nearest power of 2.
	for (pow2count = 1; pow2count < remap->NumEntries; pow2count <<= 1)
	{
	}
	count = pow2count;
	DoColorSkip = false;

	BorderColor = 0;
	RoundedPaletteSize = count;
	Tex = fb->CreateTexture("Pal", count, 1, 1, GL_RGBA8);
	if (Tex)
	{
		if (!Update())
		{
			Tex.reset();
		}
	}
}

//==========================================================================
//
// OpenGLPal Destructor
//
//==========================================================================

OpenGLSWFrameBuffer::OpenGLPal::~OpenGLPal()
{
	Tex.reset();
	// Detach from the palette list
	*Prev = Next;
	if (Next != nullptr)
	{
		Next->Prev = Prev;
	}
	// Remove link from the remap table
	if (Remap != nullptr)
	{
		Remap->Native = nullptr;
	}
}

//==========================================================================
//
// OpenGLPal :: Update
//
// Copies the palette to the texture.
//
//==========================================================================

bool OpenGLSWFrameBuffer::OpenGLPal::Update()
{
	uint32_t *buff;
	const PalEntry *pal;
	int skipat, i;

	assert(Tex != nullptr);

	if (Tex->Buffers[0] == 0)
	{
		glGenBuffers(2, (GLuint*)Tex->Buffers);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Tex->Buffers[0]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, RoundedPaletteSize * 4, nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Tex->Buffers[1]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, RoundedPaletteSize * 4, nullptr, GL_STREAM_DRAW);
	}
	else
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Tex->Buffers[Tex->CurrentBuffer]);
		Tex->CurrentBuffer = (Tex->CurrentBuffer + 1) & 1;
	}

	int numEntries = MIN(Remap->NumEntries, RoundedPaletteSize);
	
	std::vector<uint8_t> &tempbuffer = Tex->MapBuffer;
	if (gl.es)
		tempbuffer.resize(numEntries * 4);

	buff = gl.es ? (uint32_t*)tempbuffer.data() : (uint32_t *)MapBuffer(GL_PIXEL_UNPACK_BUFFER, numEntries * 4);
	if (buff == nullptr)
	{
		return false;
	}
	pal = Remap->Palette;

	// See explanation in UploadPalette() for skipat rationale.
	skipat = MIN(numEntries, DoColorSkip ? 256 - 8 : 256);

#ifndef NO_SSE
	// Manual SSE vectorized version here to workaround a bug in GCC's auto-vectorizer

	int sse_count = skipat / 4 * 4;
	for (i = 0; i < sse_count; i += 4)
	{
		_mm_storeu_si128((__m128i*)(&buff[i]), _mm_loadu_si128((__m128i*)(&pal[i])));
	}
	switch (skipat - i)
	{
	// fall through is intentional
	case 3: buff[i] = pal[i].d; i++;
	case 2: buff[i] = pal[i].d; i++;
	case 1: buff[i] = pal[i].d; i++;
	default: i++;
	}
	sse_count = numEntries / 4 * 4;
	__m128i alphamask = _mm_set1_epi32(0xff000000);
	while (i < sse_count)
	{
		__m128i lastcolor = _mm_loadu_si128((__m128i*)(&pal[i - 1]));
		__m128i color = _mm_loadu_si128((__m128i*)(&pal[i]));
		_mm_storeu_si128((__m128i*)(&buff[i]), _mm_or_si128(_mm_and_si128(alphamask, color), _mm_andnot_si128(alphamask, lastcolor)));
		i += 4;
	}
	switch (numEntries - i)
	{
	// fall through is intentional
	case 3: buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b); i++;
	case 2: buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b); i++;
	case 1: buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b); i++;
	default: break;
	}

#else
	for (i = 0; i < skipat; ++i)
	{
		buff[i] = ColorARGB(pal[i].a, pal[i].r, pal[i].g, pal[i].b);
	}
	for (++i; i < numEntries; ++i)
	{
		buff[i] = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b);
	}
#endif
	if (numEntries > 1)
	{
		i = numEntries - 1;
		BorderColor = ColorARGB(pal[i].a, pal[i - 1].r, pal[i - 1].g, pal[i - 1].b);
	}
	
	if (gl.es)
	{
		BgraToRgba((uint32_t*)buff, (const uint32_t *)buff, numEntries, 1, numEntries);
		glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, numEntries * 4, buff);
	}
	else
	{
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	}
	
	GLint oldBinding = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldBinding);
	glBindTexture(GL_TEXTURE_2D, Tex->Texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, numEntries, 1, gl.es ? GL_RGBA : GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, oldBinding);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	return true;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Begin2D
//
// Begins 2D mode drawing operations. In particular, DrawTexture is
// rerouted to use Direct3D instead of the software renderer.
//
//==========================================================================

bool OpenGLSWFrameBuffer::Begin2D(bool copy3d)
{
	Super::Begin2D(copy3d);
	if (!Accel2D)
	{
		return false;
	}
	if (In2D)
	{
		return true;
	}
	In2D = 2 - copy3d;
	Update();
	In2D = 3;

	return true;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: DrawBlendingRect
//
// Call after Begin2D to blend the 3D view.
//
//==========================================================================

void OpenGLSWFrameBuffer::DrawBlendingRect()
{
	if (!In2D || !Accel2D)
	{
		return;
	}
	Dim(FlashColor, FlashAmount / 256.f, viewwindowx, viewwindowy, viewwidth, viewheight);
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CreateTexture
//
// Returns a native texture that wraps a FTexture.
//
//==========================================================================

FNativeTexture *OpenGLSWFrameBuffer::CreateTexture(FTexture *gametex, FTextureFormat fmt, bool wrapping)
{
	OpenGLTex *tex = new OpenGLTex(gametex, fmt, this, wrapping);
	if (tex->Box == nullptr)
	{
		delete tex;
		return nullptr;
	}
	return tex;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CreatePalette
//
// Returns a native texture that contains a palette.
//
//==========================================================================

FNativePalette *OpenGLSWFrameBuffer::CreatePalette(FRemapTable *remap)
{
	OpenGLPal *tex = new OpenGLPal(remap, this);
	if (tex->Tex == nullptr)
	{
		delete tex;
		return nullptr;
	}
	return tex;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Clear
//
// Fills the specified region with a color.
//
//==========================================================================

void OpenGLSWFrameBuffer::DoClear(int left, int top, int right, int bottom, int palcolor, uint32_t color)
{
	if (In2D < 2)
	{
		Super::DoClear(left, top, right, bottom, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}
	if (palcolor >= 0 && color == 0)
	{
		color = GPalette.BaseColors[palcolor];
	}
	else if (APART(color) < 255)
	{
		Dim(color, APART(color) / 255.f, left, top, right - left, bottom - top);
		return;
	}
	AddColorOnlyQuad(left, top, right - left, bottom - top, color | 0xFF000000);
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: Dim
//
//==========================================================================

void OpenGLSWFrameBuffer::DoDim(PalEntry color, float amount, int x1, int y1, int w, int h)
{
	if (amount <= 0)
	{
		return;
	}
	if (In2D < 2)
	{
		Super::DoDim(color, amount, x1, y1, w, h);
		return;
	}
	if (!InScene)
	{
		return;
	}
	if (amount > 1)
	{
		amount = 1;
	}
	AddColorOnlyQuad(x1, y1, w, h, color | (int(amount * 255) << 24));
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: BeginLineBatch
//
//==========================================================================

void OpenGLSWFrameBuffer::BeginLineBatch()
{
	if (In2D < 2 || !InScene || BatchType == BATCH_Lines)
	{
		return;
	}
	EndQuadBatch();		// Make sure all quads have been drawn first.
	VertexData = VertexBuffer->Lock();
	VertexPos = 0;
	BatchType = BATCH_Lines;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: EndLineBatch
//
//==========================================================================

void OpenGLSWFrameBuffer::EndLineBatch()
{
	if (In2D < 2 || !InScene || BatchType != BATCH_Lines)
	{
		return;
	}
	VertexBuffer->Unlock();
	if (VertexPos > 0)
	{
		SetPixelShader(Shaders[SHADER_VertexColor].get());
		SetAlphaBlend(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		SetStreamSource(VertexBuffer.get());
		DrawLineList(VertexPos / 2);
	}
	VertexPos = -1;
	BatchType = BATCH_None;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: DrawLine
//
//==========================================================================

void OpenGLSWFrameBuffer::DrawLine(int x0, int y0, int x1, int y1, int palcolor, uint32_t color)
{
	if (In2D < 2)
	{
		Super::DrawLine(x0, y0, x1, y1, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}
	if (BatchType != BATCH_Lines)
	{
		BeginLineBatch();
	}
	if (VertexPos == NUM_VERTS)
	{ // Flush the buffer and refill it.
		EndLineBatch();
		BeginLineBatch();
	}
	// Add the endpoints to the vertex buffer.
	VertexData[VertexPos].x = float(x0);
	VertexData[VertexPos].y = float(y0);
	VertexData[VertexPos].z = 0;
	VertexData[VertexPos].rhw = 1;
	VertexData[VertexPos].color0 = color;
	VertexData[VertexPos].color1 = 0;
	VertexData[VertexPos].tu = 0;
	VertexData[VertexPos].tv = 0;

	VertexData[VertexPos + 1].x = float(x1);
	VertexData[VertexPos + 1].y = float(y1);
	VertexData[VertexPos + 1].z = 0;
	VertexData[VertexPos + 1].rhw = 1;
	VertexData[VertexPos + 1].color0 = color;
	VertexData[VertexPos + 1].color1 = 0;
	VertexData[VertexPos + 1].tu = 0;
	VertexData[VertexPos + 1].tv = 0;

	VertexPos += 2;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: DrawPixel
//
//==========================================================================

void OpenGLSWFrameBuffer::DrawPixel(int x, int y, int palcolor, uint32_t color)
{
	if (In2D < 2)
	{
		Super::DrawPixel(x, y, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}
	FBVERTEX pt =
	{
		float(x), float(y), 0, 1, color
	};
	EndBatch();		// Draw out any batched operations.
	SetPixelShader(Shaders[SHADER_VertexColor].get());
	SetAlphaBlend(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	DrawPoints(1, &pt);
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: DrawTextureV
//
// If not in 2D mode, just call the normal software version.
// If in 2D mode, then use Direct3D calls to perform the drawing.
//
//==========================================================================

void OpenGLSWFrameBuffer::DrawTextureParms(FTexture *img, DrawParms &parms)
{
	if (In2D < 2)
	{
		Super::DrawTextureParms(img, parms);
		return;
	}
	if (!InScene)
	{
		return;
	}

	FTextureFormat fmt;
	if (parms.style.Flags & STYLEF_RedIsAlpha) fmt = TEX_Gray;
	else if (parms.remap != nullptr) fmt = TEX_Pal;
	else fmt = img->GetFormat();

	OpenGLTex *tex = static_cast<OpenGLTex *>(img->GetNative(fmt, false));

	if (tex == nullptr)
	{
		assert(tex != nullptr);
		return;
	}

	CheckQuadBatch();

	double xscale = parms.destwidth / parms.texwidth;
	double yscale = parms.destheight / parms.texheight;
	double x0 = parms.x - parms.left * xscale;
	double y0 = parms.y - parms.top * yscale;
	double x1 = x0 + parms.destwidth;
	double y1 = y0 + parms.destheight;
	float u0 = tex->Box->Left;
	float v0 = tex->Box->Top;
	float u1 = tex->Box->Right;
	float v1 = tex->Box->Bottom;
	double uscale = 1.f / tex->Box->Owner->Width;
	bool scissoring = false;
	FBVERTEX *vert;

	if (parms.flipX)
	{
		swapvalues(u0, u1);
	}
	if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
	{
		double wi = MIN(parms.windowright, parms.texwidth);
		x0 += parms.windowleft * xscale;
		u0 = float(u0 + parms.windowleft * uscale);
		x1 -= (parms.texwidth - wi) * xscale;
		u1 = float(u1 - (parms.texwidth - wi) * uscale);
	}

#if 0
	float vscale = 1.f / tex->Box->Owner->Height / yscale;
	if (y0 < parms.uclip)
	{
		v0 += (float(parms.uclip) - y0) * vscale;
		y0 = float(parms.uclip);
	}
	if (y1 > parms.dclip)
	{
		v1 -= (y1 - float(parms.dclip)) * vscale;
		y1 = float(parms.dclip);
	}
	if (x0 < parms.lclip)
	{
		u0 += float(parms.lclip - x0) * uscale / xscale * 2;
		x0 = float(parms.lclip);
	}
	if (x1 > parms.rclip)
	{
		u1 -= (x1 - parms.rclip) * uscale / xscale * 2;
		x1 = float(parms.rclip);
	}
#else
	// Use a scissor test because the math above introduces some jitter
	// that is noticeable at low resolutions. Unfortunately, this means this
	// quad has to be in a batch by itself.
	if (y0 < parms.uclip || y1 > parms.dclip || x0 < parms.lclip || x1 > parms.rclip)
	{
		scissoring = true;
		if (QuadBatchPos > 0)
		{
			EndQuadBatch();
			BeginQuadBatch();
		}
		glEnable(GL_SCISSOR_TEST);
		glScissor(parms.lclip, parms.uclip, parms.rclip - parms.lclip, parms.dclip - parms.uclip);
	}
#endif
	parms.bilinear = false;

	uint32_t color0, color1;
	BufferedTris *quad = &QuadExtra[QuadBatchPos];

	if (!SetStyle(tex, parms, color0, color1, *quad))
	{
		goto done;
	}

	quad->Texture = tex->Box->Owner->Tex.get();
	if (parms.bilinear)
	{
		quad->Flags |= BQF_Bilinear;
	}
	quad->NumTris = 2;
	quad->NumVerts = 4;

	vert = &VertexData[VertexPos];

	{
		PalEntry color = color1;
		color = PalEntry((color.a * parms.color.a) / 255, (color.r * parms.color.r) / 255, (color.g * parms.color.g) / 255, (color.b * parms.color.b) / 255);
		color1 = color; 
	}

	// Fill the vertex buffer.
	vert[0].x = float(x0);
	vert[0].y = float(y0);
	vert[0].z = 0;
	vert[0].rhw = 1;
	vert[0].color0 = color0;
	vert[0].color1 = color1;
	vert[0].tu = u0;
	vert[0].tv = v0;

	vert[1].x = float(x1);
	vert[1].y = float(y0);
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = color0;
	vert[1].color1 = color1;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[2].x = float(x1);
	vert[2].y = float(y1);
	vert[2].z = 0;
	vert[2].rhw = 1;
	vert[2].color0 = color0;
	vert[2].color1 = color1;
	vert[2].tu = u1;
	vert[2].tv = v1;

	vert[3].x = float(x0);
	vert[3].y = float(y1);
	vert[3].z = 0;
	vert[3].rhw = 1;
	vert[3].color0 = color0;
	vert[3].color1 = color1;
	vert[3].tu = u0;
	vert[3].tv = v1;

	// Fill the vertex index buffer.
	IndexData[IndexPos] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	// Batch the quad.
	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
done:
	if (scissoring)
	{
		EndQuadBatch();
		glDisable(GL_SCISSOR_TEST);
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: FlatFill
//
// Fills an area with a repeating copy of the texture.
//
//==========================================================================

void OpenGLSWFrameBuffer::FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	if (In2D < 2)
	{
		Super::FlatFill(left, top, right, bottom, src, local_origin);
		return;
	}
	if (!InScene)
	{
		return;
	}
	OpenGLTex *tex = static_cast<OpenGLTex *>(src->GetNative(src->GetFormat(), true));
	if (tex == nullptr)
	{
		return;
	}
	float x0 = float(left);
	float y0 = float(top);
	float x1 = float(right);
	float y1 = float(bottom);
	float itw = 1.f / float(src->GetWidth());
	float ith = 1.f / float(src->GetHeight());
	float xo = local_origin ? x0 : 0;
	float yo = local_origin ? y0 : 0;
	float u0 = (x0 - xo) * itw;
	float v0 = (y0 - yo) * ith;
	float u1 = (x1 - xo) * itw;
	float v1 = (y1 - yo) * ith;

	CheckQuadBatch();

	BufferedTris *quad = &QuadExtra[QuadBatchPos];
	FBVERTEX *vert = &VertexData[VertexPos];

	quad->ClearSetup();
	if (tex->GetTexFormat() == GL_R8 && !tex->IsGray)
	{
		quad->Flags = BQF_WrapUV | BQF_GamePalette; // | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_PalTex;
	}
	else
	{
		quad->Flags = BQF_WrapUV; // | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_Plain;
	}
	quad->Palette = nullptr;
	quad->Texture = tex->Box->Owner->Tex.get();
	quad->NumVerts = 4;
	quad->NumTris = 2;

	vert[0].x = x0;
	vert[0].y = y0;
	vert[0].z = 0;
	vert[0].rhw = 1;
	vert[0].color0 = 0;
	vert[0].color1 = 0xFFFFFFFF;
	vert[0].tu = u0;
	vert[0].tv = v0;

	vert[1].x = x1;
	vert[1].y = y0;
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = 0;
	vert[1].color1 = 0xFFFFFFFF;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[2].x = x1;
	vert[2].y = y1;
	vert[2].z = 0;
	vert[2].rhw = 1;
	vert[2].color0 = 0;
	vert[2].color1 = 0xFFFFFFFF;
	vert[2].tu = u1;
	vert[2].tv = v1;

	vert[3].x = x0;
	vert[3].y = y1;
	vert[3].z = 0;
	vert[3].rhw = 1;
	vert[3].color0 = 0;
	vert[3].color1 = 0xFFFFFFFF;
	vert[3].tu = u0;
	vert[3].tv = v1;

	IndexData[IndexPos] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: FillSimplePoly
//
// Here, "simple" means that a simple triangle fan can draw it.
//
//==========================================================================

void OpenGLSWFrameBuffer::FillSimplePoly(FTexture *texture, FVector2 *points, int npoints,
	double originx, double originy, double scalex, double scaley,
	DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip)
{
	// Use an equation similar to player sprites to determine shade
	double fadelevel = clamp((swrenderer::LightVisibility::LightLevelToShade(lightlevel, true) / 65536. - 12) / NUMCOLORMAPS, 0.0, 1.0);

	BufferedTris *quad;
	FBVERTEX *verts;
	OpenGLTex *tex;
	float uscale, vscale;
	int i, ipos;
	uint32_t color0, color1;
	float ox, oy;
	float cosrot, sinrot;
	bool dorotate = rotation != 0;

	if (npoints < 3)
	{ // This is no polygon.
		return;
	}
	if (In2D < 2)
	{
		Super::FillSimplePoly(texture, points, npoints, originx, originy, scalex, scaley, rotation, colormap, lightlevel, flatcolor, bottomclip);
		return;
	}
	if (!InScene)
	{
		return;
	}
	tex = static_cast<OpenGLTex *>(texture->GetNative(texture->GetFormat(), true));
	if (tex == nullptr)
	{
		return;
	}

	cosrot = (float)cos(rotation.Radians());
	sinrot = (float)sin(rotation.Radians());

	CheckQuadBatch(npoints - 2, npoints);
	quad = &QuadExtra[QuadBatchPos];
	verts = &VertexData[VertexPos];

	color0 = 0;
	color1 = 0xFFFFFFFF;

	quad->ClearSetup();
	if (tex->GetTexFormat() == GL_R8 && !tex->IsGray)
	{
		quad->Flags = BQF_WrapUV | BQF_GamePalette | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_PalTex;
		if (colormap.Desaturation != 0)
		{
			quad->Flags |= BQF_Desaturated;
		}
		quad->ShaderNum = BQS_InGameColormap;
		quad->Desat = colormap.Desaturation;
		color0 = ColorARGB(255, colormap.LightColor.r, colormap.LightColor.g, colormap.LightColor.b);
		color1 = ColorARGB(uint32_t((1 - fadelevel) * 255),
			uint32_t(colormap.FadeColor.r * fadelevel),
			uint32_t(colormap.FadeColor.g * fadelevel),
			uint32_t(colormap.FadeColor.b * fadelevel));
	}
	else
	{
		quad->Flags = BQF_WrapUV | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_Plain;
	}
	quad->Palette = nullptr;
	quad->Texture = tex->Box->Owner->Tex.get();
	quad->NumVerts = npoints;
	quad->NumTris = npoints - 2;

	uscale = float(1.f / (texture->GetScaledWidth() * scalex));
	vscale = float(1.f / (texture->GetScaledHeight() * scaley));
	ox = float(originx);
	oy = float(originy);

	for (i = 0; i < npoints; ++i)
	{
		verts[i].x = points[i].X;
		verts[i].y = points[i].Y;
		verts[i].z = 0;
		verts[i].rhw = 1;
		verts[i].color0 = color0;
		verts[i].color1 = color1;
		float u = points[i].X - ox;
		float v = points[i].Y - oy;
		if (dorotate)
		{
			float t = u;
			u = t * cosrot - v * sinrot;
			v = v * cosrot + t * sinrot;
		}
		verts[i].tu = u * uscale;
		verts[i].tv = v * vscale;
	}
	for (ipos = IndexPos, i = 2; i < npoints; ++i, ipos += 3)
	{
		IndexData[ipos] = VertexPos;
		IndexData[ipos + 1] = VertexPos + i - 1;
		IndexData[ipos + 2] = VertexPos + i;
	}

	QuadBatchPos++;
	VertexPos += npoints;
	IndexPos = ipos;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: AddColorOnlyQuad
//
// Adds a single-color, untextured quad to the batch.
//
//==========================================================================

void OpenGLSWFrameBuffer::AddColorOnlyQuad(int left, int top, int width, int height, uint32_t color)
{
	BufferedTris *quad;
	FBVERTEX *verts;

	CheckQuadBatch();
	quad = &QuadExtra[QuadBatchPos];
	verts = &VertexData[VertexPos];

	float x = float(left);
	float y = float(top);

	quad->ClearSetup();
	quad->ShaderNum = BQS_ColorOnly;
	if ((color & 0xFF000000) != 0xFF000000)
	{
		quad->BlendOp = GL_FUNC_ADD;
		quad->SrcBlend = GL_SRC_ALPHA;
		quad->DestBlend = GL_ONE_MINUS_SRC_ALPHA;
	}
	quad->Palette = nullptr;
	quad->Texture = nullptr;
	quad->NumVerts = 4;
	quad->NumTris = 2;

	verts[0].x = x;
	verts[0].y = y;
	verts[0].z = 0;
	verts[0].rhw = 1;
	verts[0].color0 = color;
	verts[0].color1 = 0;
	verts[0].tu = 0;
	verts[0].tv = 0;

	verts[1].x = x + width;
	verts[1].y = y;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color;
	verts[1].color1 = 0;
	verts[1].tu = 0;
	verts[1].tv = 0;

	verts[2].x = x + width;
	verts[2].y = y + height;
	verts[2].z = 0;
	verts[2].rhw = 1;
	verts[2].color0 = color;
	verts[2].color1 = 0;
	verts[2].tu = 0;
	verts[2].tv = 0;

	verts[3].x = x;
	verts[3].y = y + height;
	verts[3].z = 0;
	verts[3].rhw = 1;
	verts[3].color0 = color;
	verts[3].color1 = 0;
	verts[3].tu = 0;
	verts[3].tv = 0;

	IndexData[IndexPos] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: AddColorOnlyRect
//
// Like AddColorOnlyQuad, except it's hollow.
//
//==========================================================================

void OpenGLSWFrameBuffer::AddColorOnlyRect(int left, int top, int width, int height, uint32_t color)
{
	AddColorOnlyQuad(left, top, width - 1, 1, color);					// top
	AddColorOnlyQuad(left + width - 1, top, 1, height - 1, color);		// right
	AddColorOnlyQuad(left + 1, top + height - 1, width - 1, 1, color);	// bottom
	AddColorOnlyQuad(left, top + 1, 1, height - 1, color);				// left
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: CheckQuadBatch
//
// Make sure there's enough room in the batch for one more set of triangles.
//
//==========================================================================

void OpenGLSWFrameBuffer::CheckQuadBatch(int numtris, int numverts)
{
	if (BatchType == BATCH_Lines)
	{
		EndLineBatch();
	}
	else if (QuadBatchPos == MAX_QUAD_BATCH ||
		VertexPos + numverts > NUM_VERTS ||
		IndexPos + numtris * 3 > NUM_INDEXES)
	{
		EndQuadBatch();
	}
	if (QuadBatchPos < 0)
	{
		BeginQuadBatch();
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: BeginQuadBatch
//
// Locks the vertex buffer for quads and sets the cursor to 0.
//
//==========================================================================

void OpenGLSWFrameBuffer::BeginQuadBatch()
{
	if (In2D < 2 || !InScene || QuadBatchPos >= 0)
	{
		return;
	}
	EndLineBatch();		// Make sure all lines have been drawn first.
	VertexData = VertexBuffer->Lock();
	IndexData = IndexBuffer->Lock();
	VertexPos = 0;
	IndexPos = 0;
	QuadBatchPos = 0;
	BatchType = BATCH_Quads;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: EndQuadBatch
//
// Draws all the quads that have been batched up.
//
//==========================================================================

void OpenGLSWFrameBuffer::EndQuadBatch()
{
	if (In2D < 2 || !InScene || BatchType != BATCH_Quads)
	{
		return;
	}
	BatchType = BATCH_None;
	VertexBuffer->Unlock();
	IndexBuffer->Unlock();
	if (QuadBatchPos == 0)
	{
		QuadBatchPos = -1;
		VertexPos = -1;
		IndexPos = -1;
		return;
	}
	SetStreamSource(VertexBuffer.get());
	SetIndices(IndexBuffer.get());
	bool uv_wrapped = false;
	bool uv_should_wrap;
	int indexpos, vertpos;

	indexpos = vertpos = 0;
	for (int i = 0; i < QuadBatchPos; )
	{
		const BufferedTris *quad = &QuadExtra[i];
		int j;

		int startindex = indexpos;
		int startvertex = vertpos;

		indexpos += quad->NumTris * 3;
		vertpos += quad->NumVerts;

		// Quads with matching parameters should be done with a single
		// DrawPrimitive call.
		for (j = i + 1; j < QuadBatchPos; ++j)
		{
			const BufferedTris *q2 = &QuadExtra[j];
			if (quad->Texture != q2->Texture ||
				!quad->IsSameSetup(*q2) ||
				quad->Palette != q2->Palette)
			{
				break;
			}
			if (quad->ShaderNum == BQS_InGameColormap && (quad->Flags & BQF_Desaturated) && quad->Desat != q2->Desat)
			{
				break;
			}
			indexpos += q2->NumTris * 3;
			vertpos += q2->NumVerts;
		}

		// Set the palette (if one)
		if ((quad->Flags & BQF_Paletted) == BQF_GamePalette)
		{
			SetPaletteTexture(PaletteTexture.get(), 256, BorderColor);
		}
		else if ((quad->Flags & BQF_Paletted) == BQF_CustomPalette)
		{
			assert(quad->Palette != nullptr);
			SetPaletteTexture(quad->Palette->Tex.get(), quad->Palette->RoundedPaletteSize, quad->Palette->BorderColor);
		}

		// Set the alpha blending
		SetAlphaBlend(quad->BlendOp, quad->SrcBlend, quad->DestBlend);

		// Set the alpha test
		EnableAlphaTest(!(quad->Flags & BQF_DisableAlphaTest));

		// Set the pixel shader
		if (quad->ShaderNum == BQS_PalTex)
		{
			SetPixelShader(Shaders[(quad->Flags & BQF_InvertSource) ? SHADER_NormalColorPalInv : SHADER_NormalColorPal].get());
		}
		else if (quad->ShaderNum == BQS_Plain)
		{
			SetPixelShader(Shaders[(quad->Flags & BQF_InvertSource) ? SHADER_NormalColorInv : SHADER_NormalColor].get());
		}
		else if (quad->ShaderNum == BQS_RedToAlpha)
		{
			SetPixelShader(Shaders[(quad->Flags & BQF_InvertSource) ? SHADER_RedToAlphaInv : SHADER_RedToAlpha].get());
		}
		else if (quad->ShaderNum == BQS_ColorOnly)
		{
			SetPixelShader(Shaders[SHADER_VertexColor].get());
		}
		else if (quad->ShaderNum == BQS_SpecialColormap)
		{
			int select;

			select = !!(quad->Flags & BQF_Paletted);
			SetPixelShader(Shaders[SHADER_SpecialColormap + select].get());
		}
		else if (quad->ShaderNum == BQS_InGameColormap)
		{
			int select;

			select = !!(quad->Flags & BQF_Desaturated);
			select |= !!(quad->Flags & BQF_InvertSource) << 1;
			select |= !!(quad->Flags & BQF_Paletted) << 2;
			if (quad->Flags & BQF_Desaturated)
			{
				SetConstant(PSCONST_Desaturation, quad->Desat / 255.f, (255 - quad->Desat) / 255.f, 0, 0);
			}
			SetPixelShader(Shaders[SHADER_InGameColormap + select].get());
		}

		// Set the texture clamp addressing mode
		uv_should_wrap = !!(quad->Flags & BQF_WrapUV);
		if (uv_wrapped != uv_should_wrap)
		{
			uint32_t mode = uv_should_wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
			uv_wrapped = uv_should_wrap;
			SetSamplerWrapS(0, mode);
			SetSamplerWrapT(0, mode);
		}

		// Set the texture
		if (quad->Texture != nullptr)
		{
			SetTexture(0, quad->Texture);
		}

		// Draw the quad
		DrawTriangleList(
			startvertex,					// MinIndex
			vertpos - startvertex,			// NumVertices
			startindex,						// StartIndex
			(indexpos - startindex) / 3		// PrimitiveCount
		/*4 * i, 4 * (j - i), 6 * i, 2 * (j - i)*/);
		i = j;
	}
	if (uv_wrapped)
	{
		SetSamplerWrapS(0, GL_CLAMP_TO_EDGE);
		SetSamplerWrapT(0, GL_CLAMP_TO_EDGE);
	}
	QuadBatchPos = -1;
	VertexPos = -1;
	IndexPos = -1;
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: EndBatch
//
// Draws whichever type of primitive is currently being batched.
//
//==========================================================================

void OpenGLSWFrameBuffer::EndBatch()
{
	if (BatchType == BATCH_Quads)
	{
		EndQuadBatch();
	}
	else if (BatchType == BATCH_Lines)
	{
		EndLineBatch();
	}
}

//==========================================================================
//
// OpenGLSWFrameBuffer :: SetStyle
//
// Patterned after R_SetPatchStyle.
//
//==========================================================================

bool OpenGLSWFrameBuffer::SetStyle(OpenGLTex *tex, DrawParms &parms, uint32_t &color0, uint32_t &color1, BufferedTris &quad)
{
	int fmt = tex->GetTexFormat();
	FRenderStyle style = parms.style;
	float alpha;
	bool stencilling;

	if (style.Flags & STYLEF_TransSoulsAlpha)
	{
		alpha = transsouls;
	}
	else if (style.Flags & STYLEF_Alpha1)
	{
		alpha = 1;
	}
	else
	{
		alpha = clamp(parms.Alpha, 0.f, 1.f);
	}

	style.CheckFuzz();
	if (style.BlendOp == STYLEOP_Shadow)
	{
		style = LegacyRenderStyles[STYLE_TranslucentStencil];
		alpha = 0.3f;
		parms.fillcolor = 0;
	}

	// FIXME: Fuzz effect is not written
	if (style.BlendOp == STYLEOP_FuzzOrAdd || style.BlendOp == STYLEOP_Fuzz)
	{
		style.BlendOp = STYLEOP_Add;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrSub)
	{
		style.BlendOp = STYLEOP_Sub;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrRevSub)
	{
		style.BlendOp = STYLEOP_RevSub;
	}

	stencilling = false;
	quad.Palette = nullptr;
	quad.Flags = 0;
	quad.Desat = 0;

	switch (style.BlendOp)
	{
	default:
	case STYLEOP_Add:		quad.BlendOp = GL_FUNC_ADD;			break;
	case STYLEOP_Sub:		quad.BlendOp = GL_FUNC_SUBTRACT;		break;
	case STYLEOP_RevSub:	quad.BlendOp = GL_FUNC_REVERSE_SUBTRACT;	break;
	case STYLEOP_None:		return false;
	}
	quad.SrcBlend = GetStyleAlpha(style.SrcAlpha);
	quad.DestBlend = GetStyleAlpha(style.DestAlpha);

	if (style.Flags & STYLEF_InvertOverlay)
	{
		// Only the overlay color is inverted, not the overlay alpha.
		parms.colorOverlay = ColorARGB(APART(parms.colorOverlay),
			255 - RPART(parms.colorOverlay), 255 - GPART(parms.colorOverlay),
			255 - BPART(parms.colorOverlay));
	}

	SetColorOverlay(parms.colorOverlay, alpha, color0, color1);

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{ // Since the source color is a constant, we can invert it now
		  // without spending time doing it in the shader.
			parms.fillcolor = ColorXRGB(255 - RPART(parms.fillcolor),
				255 - GPART(parms.fillcolor), 255 - BPART(parms.fillcolor));
		}
		// Set up the color mod to replace the color from the image data.
		color0 = (color0 & ColorRGBA(0, 0, 0, 255)) | (parms.fillcolor & ColorRGBA(255, 255, 255, 0));
		color1 &= ColorRGBA(0, 0, 0, 255);

		if (style.Flags & STYLEF_RedIsAlpha)
		{
			// Note that if the source texture is paletted, the palette is ignored.
			quad.Flags = 0;
			quad.ShaderNum = BQS_RedToAlpha;
		}
		else if (fmt == GL_R8)
		{
			quad.Flags = BQF_GamePalette;
			quad.ShaderNum = BQS_PalTex;
		}
		else
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_Plain;
		}
	}
	else
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_RedToAlpha;
		}
		else if (fmt == GL_R8)
		{
			if (parms.remap != nullptr)
			{
				quad.Flags = BQF_CustomPalette;
				quad.Palette = reinterpret_cast<OpenGLPal *>(parms.remap->GetNative());
				quad.ShaderNum = BQS_PalTex;
			}
			else if (tex->IsGray)
			{
				quad.Flags = 0;
				quad.ShaderNum = BQS_Plain;
			}
			else
			{
				quad.Flags = BQF_GamePalette;
				quad.ShaderNum = BQS_PalTex;
			}
		}
		else
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_Plain;
		}
		if (style.Flags & STYLEF_InvertSource)
		{
			quad.Flags |= BQF_InvertSource;
		}

		if (parms.specialcolormap != nullptr)
		{ // Emulate an invulnerability or similar colormap.
			float *start, *end;
			start = parms.specialcolormap->ColorizeStart;
			end = parms.specialcolormap->ColorizeEnd;
			if (quad.Flags & BQF_InvertSource)
			{
				quad.Flags &= ~BQF_InvertSource;
				swapvalues(start, end);
			}
			quad.ShaderNum = BQS_SpecialColormap;
			color0 = ColorRGBA(uint32_t(start[0] / 2 * 255), uint32_t(start[1] / 2 * 255), uint32_t(start[2] / 2 * 255), color0 >> 24);
			color1 = ColorRGBA(uint32_t(end[0] / 2 * 255), uint32_t(end[1] / 2 * 255), uint32_t(end[2] / 2 * 255), color1 >> 24);
		}
		else if (parms.colormapstyle != nullptr)
		{ // Emulate the fading from an in-game colormap (colorized, faded, and desaturated)
			if (parms.colormapstyle->Desaturate != 0)
			{
				quad.Flags |= BQF_Desaturated;
			}
			quad.ShaderNum = BQS_InGameColormap;
			quad.Desat = parms.colormapstyle->Desaturate;
			color0 = ColorARGB(color1 >> 24,
				parms.colormapstyle->Color.r,
				parms.colormapstyle->Color.g,
				parms.colormapstyle->Color.b);
			double fadelevel = parms.colormapstyle->FadeLevel;
			color1 = ColorARGB(uint32_t((1 - fadelevel) * 255),
				uint32_t(parms.colormapstyle->Fade.r * fadelevel),
				uint32_t(parms.colormapstyle->Fade.g * fadelevel),
				uint32_t(parms.colormapstyle->Fade.b * fadelevel));
		}
	}

	// For unmasked images, force the alpha from the image data to be ignored.
	if (!parms.masked && quad.ShaderNum != BQS_InGameColormap)
	{
		color0 = (color0 & ColorRGBA(255, 255, 255, 0)) | ColorValue(0, 0, 0, alpha);
		color1 &= ColorRGBA(255, 255, 255, 0);

		// If our alpha is one and we are doing normal adding, then we can turn the blend off completely.
		if (quad.BlendOp == GL_FUNC_ADD &&
			((alpha == 1 && quad.SrcBlend == GL_SRC_ALPHA) || quad.SrcBlend == GL_ONE) &&
			((alpha == 1 && quad.DestBlend == GL_ONE_MINUS_SRC_ALPHA) || quad.DestBlend == GL_ZERO))
		{
			quad.BlendOp = 0;
		}
		quad.Flags |= BQF_DisableAlphaTest;
	}
	return true;
}

int OpenGLSWFrameBuffer::GetStyleAlpha(int type)
{
	switch (type)
	{
	case STYLEALPHA_Zero:		return GL_ZERO;
	case STYLEALPHA_One:		return GL_ONE;
	case STYLEALPHA_Src:		return GL_SRC_ALPHA;
	case STYLEALPHA_InvSrc:		return GL_ONE_MINUS_SRC_ALPHA;
	default:					return GL_ZERO;
	}
}


void OpenGLSWFrameBuffer::SetColorOverlay(uint32_t color, float alpha, uint32_t &color0, uint32_t &color1)
{
	if (APART(color) != 0)
	{
		int a = APART(color) * 256 / 255;
		color0 = ColorRGBA(
			(RPART(color) * a) >> 8,
			(GPART(color) * a) >> 8,
			(BPART(color) * a) >> 8,
			0);
		a = 256 - a;
		color1 = ColorRGBA(a, a, a, int(alpha * 255));
	}
	else
	{
		color0 = 0;
		color1 = ColorValue(1, 1, 1, alpha);
	}
}

void OpenGLSWFrameBuffer::EnableAlphaTest(bool enabled)
{
	if (enabled != AlphaTestEnabled)
	{
		AlphaTestEnabled = enabled;
		//glEnable(GL_ALPHA_TEST); // To do: move to shader as this is only in the compatibility profile
	}
}

void OpenGLSWFrameBuffer::SetAlphaBlend(int op, int srcblend, int destblend)
{
	if (op == 0)
	{ // Disable alpha blend
		if (AlphaBlendEnabled)
		{
			AlphaBlendEnabled = false;
			glDisable(GL_BLEND);
		}
	}
	else
	{ // Enable alpha blend
		assert(srcblend != 0);
		assert(destblend != 0);

		if (!AlphaBlendEnabled)
		{
			AlphaBlendEnabled = true;
			glEnable(GL_BLEND);
		}
		if (AlphaBlendOp != op)
		{
			AlphaBlendOp = op;
			glBlendEquation(op);
		}
		if (AlphaSrcBlend != srcblend || AlphaDestBlend != destblend)
		{
			AlphaSrcBlend = srcblend;
			AlphaDestBlend = destblend;
			glBlendFunc(srcblend, destblend);
		}
	}
}

void OpenGLSWFrameBuffer::SetConstant(int cnum, float r, float g, float b, float a)
{
	if (Constant[cnum][0] != r ||
		Constant[cnum][1] != g ||
		Constant[cnum][2] != b ||
		Constant[cnum][3] != a)
	{
		Constant[cnum][0] = r;
		Constant[cnum][1] = g;
		Constant[cnum][2] = b;
		Constant[cnum][3] = a;
		SetPixelShaderConstantF(cnum, Constant[cnum], 1);
	}
}

void OpenGLSWFrameBuffer::SetPixelShader(HWPixelShader *shader)
{
	if (CurPixelShader != shader)
	{
		CurPixelShader = shader;
		SetHWPixelShader(shader);
	}
}

void OpenGLSWFrameBuffer::SetTexture(int tnum, HWTexture *texture)
{
	assert(unsigned(tnum) < countof(Texture));
	if (texture)
	{
		if (Texture[tnum] != texture || SamplerWrapS[tnum] != texture->WrapS || SamplerWrapT[tnum] != texture->WrapT)
		{
			Texture[tnum] = texture;
			glActiveTexture(GL_TEXTURE0 + tnum);
			glBindTexture(GL_TEXTURE_2D, texture->Texture);
			if (Texture[tnum]->WrapS != SamplerWrapS[tnum])
			{
				Texture[tnum]->WrapS = SamplerWrapS[tnum];
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, SamplerWrapS[tnum]);
			}
			if (Texture[tnum]->WrapT != SamplerWrapT[tnum])
			{
				Texture[tnum]->WrapT = SamplerWrapT[tnum];
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, SamplerWrapT[tnum]);
			}
		}
	}
	else if (Texture[tnum] != texture)
	{
		Texture[tnum] = texture;
		glActiveTexture(GL_TEXTURE0 + tnum);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void OpenGLSWFrameBuffer::SetSamplerWrapS(int tnum, int mode)
{
	assert(unsigned(tnum) < countof(Texture));
	if (Texture[tnum] && SamplerWrapS[tnum] != mode)
	{
		SamplerWrapS[tnum] = mode;
		Texture[tnum]->WrapS = mode;
		glActiveTexture(GL_TEXTURE0 + tnum);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, SamplerWrapS[tnum]);
	}
}

void OpenGLSWFrameBuffer::SetSamplerWrapT(int tnum, int mode)
{
	assert(unsigned(tnum) < countof(Texture));
	if (Texture[tnum] && SamplerWrapT[tnum] != mode)
	{
		SamplerWrapT[tnum] = mode;
		Texture[tnum]->WrapT = mode;
		glActiveTexture(GL_TEXTURE0 + tnum);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, SamplerWrapT[tnum]);
	}
}

void OpenGLSWFrameBuffer::SetPaletteTexture(HWTexture *texture, int count, uint32_t border_color)
{
	// The pixel shader receives color indexes in the range [0.0,1.0].
	// The palette texture is also addressed in the range [0.0,1.0],
	// HOWEVER the coordinate 1.0 is the right edge of the texture and
	// not actually the texture itself. We need to scale and shift
	// the palette indexes so they lie exactly in the center of each
	// texel. For a normal palette with 256 entries, that means the
	// range we use should be [0.5,255.5], adjusted so the coordinate
	// is still within [0.0,1.0].
	//
	// The constant register c2 is used to hold the multiplier in the
	// x part and the adder in the y part.
	float fcount = 1 / float(count);
	SetConstant(PSCONST_PaletteMod, 255 * fcount, 0.5f * fcount, 0, 0);
	SetTexture(1, texture);
}

void OpenGLSWFrameBuffer::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	int letterboxX, letterboxY, letterboxWidth, letterboxHeight;
	GetLetterboxFrame(letterboxX, letterboxY, letterboxWidth, letterboxHeight);

	// Subtract the LB video mode letterboxing
	if (IsFullscreen())
		y -= (GetTrueHeight() - VideoHeight) / 2;

	x = int16_t((x - letterboxX) * Width / letterboxWidth);
	y = int16_t((y - letterboxY) * Height / letterboxHeight);
}
