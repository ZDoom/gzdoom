/*
 ** sdlglvideo.h
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2014 Alexey Lysiuk
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
 */


// IMPORTANT NOTE!
// This file was intentially named sdlglvideo.h but it has nothing with SDL
// The name was selected to avoid spreding of changes over the project
// The same applies to SDLGLFB class
// See gl/system/gl_framebuffer.h for details about its usage


#ifndef COCOA_SDLGLVIDEO_H_INCLUDED
#define COCOA_SDLGLVIDEO_H_INCLUDED

#include "v_video.h"

#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_hwtexture.h"


class SDLGLFB : public DFrameBuffer
{
public:
	// This must have the same parameters as the Windows version, even if they are not used!
	SDLGLFB(void *hMonitor, int width, int height, int, int, bool fullscreen);
	~SDLGLFB();

	virtual bool Lock(bool buffered = true);
	virtual void Unlock();
	virtual bool IsLocked();

	virtual bool IsFullscreen();
	virtual void SetVSync(bool vsync);

	int GetClientWidth();
	int GetClientHeight();

protected:
	int                 m_lock;
	bool                m_isUpdatePending;

	static const uint32_t GAMMA_CHANNEL_SIZE = 256;
	static const uint32_t GAMMA_CHANNEL_COUNT = 3;
	static const uint32_t GAMMA_TABLE_SIZE = GAMMA_CHANNEL_SIZE * GAMMA_CHANNEL_COUNT;

	bool				m_supportsGamma;
	WORD				m_originalGamma[GAMMA_TABLE_SIZE];

	SDLGLFB();

	void InitializeState();

	bool CanUpdate();
	void SwapBuffers();

	void SetGammaTable(WORD* table);
	void ResetGammaTable();
};

#endif // COCOA_SDLGLVIDEO_H_INCLUDED
