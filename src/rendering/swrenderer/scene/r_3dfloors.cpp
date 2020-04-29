/*
**	r_3dfloors.cpp
**	software 3D floors addon
**
**---------------------------------------------------------------------------
** Copyright 2010 kgsws
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

#include "templates.h"
#include "doomdef.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "r_opaque_pass.h"
#include "c_cvars.h"
#include "r_3dfloors.h"
#include "r_utility.h"
#include "swrenderer/r_renderthread.h"
#include "r_memory.h"

CVAR(Int, r_3dfloors, true, 0);

namespace swrenderer
{
	Clip3DFloors::Clip3DFloors(RenderThread *thread)
	{
		Thread = thread;
	}

	void Clip3DFloors::SetFakeFloor(F3DFloor *newFakeFloor)
	{
		auto &clip = FakeFloors[newFakeFloor];
		if (clip == nullptr)
		{
			clip = Thread->FrameMemory->NewObject<FakeFloorClip>(newFakeFloor);
		}
		fakeFloor = clip;
	}

	void Clip3DFloors::Cleanup()
	{
		FakeFloors.clear();

		fakeActive = false;
		while (CurrentSkybox)
		{
			DeleteHeights();
			LeaveSkybox();
		}
		ResetClip();
		DeleteHeights();
	}

	void Clip3DFloors::DeleteHeights()
	{
		height_cur = height_top;
		while (height_cur)
		{
			height_top = height_cur;
			height_cur = height_cur->next;
			M_Free(height_top);
		}
		height_max = -1;
		height_top = height_cur = nullptr;
	}

	void Clip3DFloors::AddHeight(secplane_t *add, sector_t *sec)
	{
		HeightLevel *near;
		HeightLevel *curr;

		double height = add->ZatPoint(Thread->Viewport->viewpoint.Pos);
		if (height >= sec->CenterCeiling()) return;
		if (height <= sec->CenterFloor()) return;

		fakeActive = true;

		if (height_max >= 0)
		{
			near = height_top;
			while (near && near->height < height) near = near->next;
			if (near)
			{
				if (near->height == height) return;
				curr = (HeightLevel*)M_Malloc(sizeof(HeightLevel));
				curr->height = height;
				curr->prev = near->prev;
				curr->next = near;
				if (near->prev) near->prev->next = curr;
				else height_top = curr;
				near->prev = curr;
			}
			else
			{
				curr = (HeightLevel*)M_Malloc(sizeof(HeightLevel));
				curr->height = height;
				curr->prev = height_cur;
				curr->next = nullptr;
				height_cur->next = curr;
				height_cur = curr;
			}
		}
		else
		{
			height_top = height_cur = (HeightLevel*)M_Malloc(sizeof(HeightLevel));
			height_top->height = height;
			height_top->prev = nullptr;
			height_top->next = nullptr;
		}
		height_max++;
	}

	void Clip3DFloors::NewClip()
	{
		ClipStack *curr;

		curr = (ClipStack*)M_Malloc(sizeof(ClipStack));
		curr->next = 0;
		memcpy(curr->floorclip, Thread->OpaquePass->floorclip, sizeof(short) * Thread->Viewport->RenderTarget->GetWidth());
		memcpy(curr->ceilingclip, Thread->OpaquePass->ceilingclip, sizeof(short) * Thread->Viewport->RenderTarget->GetWidth());
		curr->ffloor = fakeFloor;
		assert(fakeFloor->floorclip == nullptr);
		assert(fakeFloor->ceilingclip == nullptr);
		fakeFloor->floorclip = curr->floorclip;
		fakeFloor->ceilingclip = curr->ceilingclip;
		if (clip_top)
		{
			clip_cur->next = curr;
			clip_cur = curr;
		}
		else
		{
			clip_top = clip_cur = curr;
		}
	}

	void Clip3DFloors::ResetClip()
	{
		clip_cur = clip_top;
		while (clip_cur)
		{
			assert(clip_cur->ffloor->floorclip != nullptr);
			assert(clip_cur->ffloor->ceilingclip != nullptr);
			clip_cur->ffloor->ceilingclip = clip_cur->ffloor->floorclip = nullptr;
			clip_top = clip_cur;
			clip_cur = clip_cur->next;
			M_Free(clip_top);
		}
		clip_cur = clip_top = nullptr;
	}

	void Clip3DFloors::EnterSkybox()
	{
		HeightStack current;

		current.height_top = height_top;
		current.height_cur = height_cur;
		current.height_max = height_max;

		toplist.Push(current);

		height_top = nullptr;
		height_cur = nullptr;
		height_max = -1;

		CurrentSkybox++;
	}

	void Clip3DFloors::LeaveSkybox()
	{
		HeightStack current;

		current.height_top = nullptr;
		current.height_cur = nullptr;
		current.height_max = -1;

		toplist.Pop(current);

		height_top = current.height_top;
		height_cur = current.height_cur;
		height_max = current.height_max;

		CurrentSkybox--;
	}
}
