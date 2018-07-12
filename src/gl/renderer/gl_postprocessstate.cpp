// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Magnus Norddahl
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
** gl_postprocessstate.cpp
** Render state maintenance
**
**/

#include "templates.h"
#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/renderer/gl_postprocessstate.h"

//-----------------------------------------------------------------------------
//
// Saves state modified by post processing shaders
//
//-----------------------------------------------------------------------------

FGLPostProcessState::FGLPostProcessState()
{
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
	glActiveTexture(GL_TEXTURE0);
	SaveTextureBindings(1);

	glGetBooleanv(GL_BLEND, &blendEnabled);
	glGetBooleanv(GL_SCISSOR_TEST, &scissorEnabled);
	glGetBooleanv(GL_DEPTH_TEST, &depthEnabled);
	glGetBooleanv(GL_MULTISAMPLE, &multisampleEnabled);
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
	glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
	glGetIntegerv(GL_BLEND_DST_RGB, &blendDestRgb);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestAlpha);

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
}

void FGLPostProcessState::SaveTextureBindings(unsigned int numUnits)
{
	while (textureBinding.Size() < numUnits)
	{
		unsigned int i = textureBinding.Size();

		GLint texture;
		glActiveTexture(GL_TEXTURE0 + i);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
		glBindTexture(GL_TEXTURE_2D, 0);
		textureBinding.Push(texture);

		if (gl.flags & RFL_SAMPLER_OBJECTS)
		{
			GLint sampler;
			glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
			glBindSampler(i, 0);
			samplerBinding.Push(sampler);
		}
	}
	glActiveTexture(GL_TEXTURE0);
}

//-----------------------------------------------------------------------------
//
// Restores state at the end of post processing
//
//-----------------------------------------------------------------------------

FGLPostProcessState::~FGLPostProcessState()
{
	if (blendEnabled)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	if (scissorEnabled)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);

	if (depthEnabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	if (multisampleEnabled)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);

	glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
	glBlendFuncSeparate(blendSrcRgb, blendDestRgb, blendSrcAlpha, blendDestAlpha);

	glUseProgram(currentProgram);

	// Fully unbind to avoid incomplete texture warnings from Nvidia's driver when gl_debug_level 4 is active
	for (unsigned int i = 0; i < textureBinding.Size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	for (unsigned int i = 0; i < samplerBinding.Size(); i++)
	{
		glBindSampler(i, samplerBinding[i]);
	}

	for (unsigned int i = 0; i < textureBinding.Size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, textureBinding[i]);
	}

	glActiveTexture(activeTex);
}
