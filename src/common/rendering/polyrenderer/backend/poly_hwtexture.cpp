/*
**  Softpoly backend
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
**
*/


#include "c_cvars.h"
#include "hw_material.h"
#include "hw_cvars.h"
#include "hw_renderstate.h"
#include "poly_framebuffer.h"
#include "poly_hwtexture.h"

PolyHardwareTexture *PolyHardwareTexture::First = nullptr;

PolyHardwareTexture::PolyHardwareTexture()
{
	Next = First;
	First = this;
	if (Next) Next->Prev = this;
}

PolyHardwareTexture::~PolyHardwareTexture()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	else First = Next;

	Reset();
}

void PolyHardwareTexture::ResetAll()
{
	for (PolyHardwareTexture *cur = PolyHardwareTexture::First; cur; cur = cur->Next)
		cur->Reset();
}

void PolyHardwareTexture::Reset()
{
	if (auto fb = GetPolyFrameBuffer())
	{
		auto &deleteList = fb->FrameDeleteList;
		if (mCanvas) deleteList.Images.push_back(std::move(mCanvas));
	}
}

DCanvas *PolyHardwareTexture::GetImage(FTexture *tex, int translation, int flags)
{
	if (!mCanvas)
		CreateImage(tex, translation, flags);
	return mCanvas.get();
}

PolyDepthStencil *PolyHardwareTexture::GetDepthStencil(FTexture *tex)
{
	if (!mDepthStencil)
	{
		int w = tex->GetWidth();
		int h = tex->GetHeight();
		mDepthStencil.reset(new PolyDepthStencil(w, h));
	}
	return mDepthStencil.get();
}

void PolyHardwareTexture::AllocateBuffer(int w, int h, int texelsize)
{
	if (!mCanvas || mCanvas->GetWidth() != w || mCanvas->GetHeight() != h)
	{
		mCanvas.reset(new DCanvas(0, 0, texelsize == 4));
		mCanvas->Resize(w, h, false);
		bufferpitch = mCanvas->GetPitch();
	}
}

uint8_t *PolyHardwareTexture::MapBuffer()
{
	return mCanvas->GetPixels();
}

unsigned int PolyHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name)
{
	return 0;
}

void PolyHardwareTexture::CreateWipeTexture(int w, int h, const char *name)
{
	if (!mCanvas || mCanvas->GetWidth() != w || mCanvas->GetHeight() != h)
	{
		mCanvas.reset(new DCanvas(0, 0, true));
		mCanvas->Resize(w, h, false);
	}

	auto fb = static_cast<PolyFrameBuffer*>(screen);

	fb->FlushDrawCommands();
	DrawerThreads::WaitForWorkers();

	uint32_t* dest = (uint32_t*)mCanvas->GetPixels();
	uint32_t* src = (uint32_t*)fb->GetCanvas()->GetPixels();
	int dpitch = mCanvas->GetPitch();
	int spitch = fb->GetCanvas()->GetPitch();
	int pixelsize = 4;

	for (int y = 0; y < h; y++)
	{
		memcpy(dest + dpitch * (h - 1 - y), src + spitch * y, w * pixelsize);
	}
}

void PolyHardwareTexture::CreateImage(FTexture *tex, int translation, int flags)
{
	mCanvas.reset(new DCanvas(0, 0, true));

	if (!tex->isHardwareCanvas())
	{
		FTextureBuffer texbuffer = tex->CreateTexBuffer(translation, flags | CTF_ProcessData);
		mCanvas->Resize(texbuffer.mWidth, texbuffer.mHeight, false);
		memcpy(mCanvas->GetPixels(), texbuffer.mBuffer, texbuffer.mWidth * texbuffer.mHeight * 4);
	}
	else
	{
		int w = tex->GetWidth();
		int h = tex->GetHeight();
		mCanvas->Resize(w, h, false);
	}
}
