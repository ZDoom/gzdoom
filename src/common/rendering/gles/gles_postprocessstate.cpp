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

#include "templates.h"
#include "gles_system.h"
#include "gles_postprocessstate.h"

namespace OpenGLESRenderer
{

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
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
	glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
	glGetIntegerv(GL_BLEND_DST_RGB, &blendDestRgb);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestAlpha);

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


	glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
	glBlendFuncSeparate(blendSrcRgb, blendDestRgb, blendSrcAlpha, blendDestAlpha);

	glUseProgram(currentProgram);

	// Fully unbind to avoid incomplete texture warnings from Nvidia's driver when gl_debug_level 4 is active
	for (unsigned int i = 0; i < textureBinding.Size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}


	for (unsigned int i = 0; i < textureBinding.Size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, textureBinding[i]);
	}

	glActiveTexture(activeTex);
}

}