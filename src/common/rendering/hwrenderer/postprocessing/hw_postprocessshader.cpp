/*
**  Postprocessing framework
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
*/

#include "vm.h"
#include "hwrenderer/postprocessing/hw_postprocessshader.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

static void ShaderSetEnabled(const FString &shaderName, bool value)
{
	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		PostProcessShader &shader = PostProcessShaders[i];
		if (shader.Name == shaderName)
			shader.Enabled = value;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_PPShader, SetEnabled, ShaderSetEnabled)
{
	PARAM_PROLOGUE;
	PARAM_STRING(shaderName);
	PARAM_BOOL(value);
	ShaderSetEnabled(shaderName, value);

	return 0;
}

static void ShaderSetUniform1f(const FString &shaderName, const FString &uniformName, double value)
{
	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		PostProcessShader &shader = PostProcessShaders[i];
		if (shader.Name == shaderName)
		{
			double *vec4 = shader.Uniforms[uniformName].Values;
			vec4[0] = value;
			vec4[1] = 0.0;
			vec4[2] = 0.0;
			vec4[3] = 1.0;
		}
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_PPShader, SetUniform1f, ShaderSetUniform1f)
{
	PARAM_PROLOGUE;
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT(value);
	ShaderSetUniform1f(shaderName, uniformName, value);
	return 0;
}

DEFINE_ACTION_FUNCTION(_PPShader, SetUniform2f)
{
	PARAM_PROLOGUE;
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		PostProcessShader &shader = PostProcessShaders[i];
		if (shader.Name == shaderName)
		{
			double *vec4 = shader.Uniforms[uniformName].Values;
			vec4[0] = x;
			vec4[1] = y;
			vec4[2] = 0.0;
			vec4[3] = 1.0;
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_PPShader, SetUniform3f)
{
	PARAM_PROLOGUE;
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);

	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		PostProcessShader &shader = PostProcessShaders[i];
		if (shader.Name == shaderName)
		{
			double *vec4 = shader.Uniforms[uniformName].Values;
			vec4[0] = x;
			vec4[1] = y;
			vec4[2] = z;
			vec4[3] = 1.0;
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_PPShader, SetUniform1i)
{
	PARAM_PROLOGUE;
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_INT(value);

	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		PostProcessShader &shader = PostProcessShaders[i];
		if (shader.Name == shaderName)
		{
			double *vec4 = shader.Uniforms[uniformName].Values;
			vec4[0] = (double)value;
			vec4[1] = 0.0;
			vec4[2] = 0.0;
			vec4[3] = 1.0;
		}
	}
	return 0;
}
