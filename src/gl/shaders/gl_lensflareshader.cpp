
#include "gl/system/gl_system.h"
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_lensflareshader.h"

void FLensFlareShader::Bind() {
	if (!mShader)
	{
		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/lensflare.fp", "", 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/lensflare");
		mShader.SetAttribLocation(0, "PositionInProjection");
		InputTexture.Init(mShader, "InputTexture");
		//FlareTexture.Init(mShader, "FlareTexture");;
	}
	mShader.Bind();
}