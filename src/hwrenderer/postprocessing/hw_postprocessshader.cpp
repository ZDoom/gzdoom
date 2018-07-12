// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2017 Magnus Norddahl
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

#include "vm.h"
#include "d_player.h"
#include "hw_postprocessshader.h"

TArray<PostProcessShader> PostProcessShaders;


static bool IsConsolePlayer(player_t *player)
{
	AActor *activator = player ? player->mo : nullptr;
	if (activator == nullptr || activator->player == nullptr)
		return false;
	return int(activator->player - players) == consoleplayer;
}

DEFINE_ACTION_FUNCTION(_Shader, SetEnabled)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_DEF(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_BOOL_DEF(value);

	if (IsConsolePlayer(player))
	{
		for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
		{
			PostProcessShader &shader = PostProcessShaders[i];
			if (shader.Name == shaderName)
				shader.Enabled = value;
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform1f)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_DEF(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT_DEF(value);

	if (IsConsolePlayer(player))
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
	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform2f)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_DEF(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT_DEF(x);
	PARAM_FLOAT_DEF(y);

	if (IsConsolePlayer(player))
	{
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
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform3f)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_DEF(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT_DEF(x);
	PARAM_FLOAT_DEF(y);
	PARAM_FLOAT_DEF(z);

	if (IsConsolePlayer(player))
	{
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
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform1i)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_DEF(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_INT_DEF(value);

	if (IsConsolePlayer(player))
	{
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
	}
	return 0;
}
