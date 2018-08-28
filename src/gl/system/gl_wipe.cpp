// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2008-2016 Christoph Oelckers
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
** Screen wipe stuff
**
*/

#include "gl_load/gl_system.h"
#include "f_wipe.h"
#include "m_random.h"
#include "w_wad.h"
#include "v_palette.h"
#include "templates.h"

#include "gl_load/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/textures/gl_samplers.h"
#include "gl/data/gl_vertexbuffer.h"


class FGLWipeTexture : public FTexture
{
public:
    
    FGLWipeTexture(int w, int h)
    {
        Width = w;
        Height = h;
        WidthBits = 4;
        UseType = ETextureType::SWCanvas;
        bNoCompress = true;
        SystemTexture[0] = screen->CreateHardwareTexture(this);
    }
    
    // This is just a wrapper around the hardware texture being extracted below so that it can be passed to the 2D code.
};

//==========================================================================
//
// OpenGLFrameBuffer :: WipeStartScreen
//
// Called before the current screen has started rendering. This needs to
// save what was drawn the previous frame so that it can be animated into
// what gets drawn this frame.
//
//==========================================================================

FTexture *OpenGLFrameBuffer::WipeStartScreen()
{
	const auto &viewport = screen->mScreenViewport;
	
	FGLWipeTexture *tex = new FGLWipeTexture(viewport.width, viewport.height);
	tex->SystemTexture[0]->CreateTexture(nullptr, viewport.width, viewport.height, 0, false, 0, "WipeStartScreen");
	glFinish();
	static_cast<FHardwareTexture*>(tex->SystemTexture[0])->Bind(0, false, false);

	GLRenderer->mBuffers->BindCurrentFB();
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport.left, viewport.top, viewport.width, viewport.height);
	return tex;
}

//==========================================================================
//
// OpenGLFrameBuffer :: WipeEndScreen
//
// The screen we want to animate to has just been drawn.
//
//==========================================================================

FTexture *OpenGLFrameBuffer::WipeEndScreen()
{
	GLRenderer->Flush();
	const auto &viewport = screen->mScreenViewport;
	FGLWipeTexture *tex = new FGLWipeTexture(viewport.width, viewport.height);
	tex->SystemTexture[0]->CreateTexture(NULL, viewport.width, viewport.height, 0, false, 0, "WipeEndScreen");
	glFinish();
	static_cast<FHardwareTexture*>(tex->SystemTexture[0])->Bind(0, false, false);
	GLRenderer->mBuffers->BindCurrentFB();
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport.left, viewport.top, viewport.width, viewport.height);
	return tex;
}

