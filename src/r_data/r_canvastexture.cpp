/*
** r_canvastexture.cpp
** Maintenance data for camera textures
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
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
**
*/

#include "actor.h"
#include "r_canvastexture.h"
#include "g_levellocals.h"
#include "serializer.h"
#include "serialize_obj.h"
#include "texturemanager.h"

//==========================================================================
//
// FCanvasTextureInfo :: Add
//
// Assigns a camera to a canvas texture.
//
//==========================================================================

void FCanvasTextureInfo::Add (AActor *viewpoint, FTextureID picnum, double fov)
{
	FCanvasTexture *texture;

	if (!picnum.isValid())
	{
		return;
	}
	auto gt = TexMan.GetGameTexture(picnum);
	texture = static_cast<FCanvasTexture *>(gt->GetTexture());
	if (!texture->bHasCanvas)
	{
		Printf ("%s is not a valid target for a camera\n", gt->GetName().GetChars());
		return;
	}

	// Is this texture already assigned to a camera?
	unsigned index = List.FindEx([=](auto &entry) { return entry.Texture == texture; });
	if (index < List.Size())
	{
		auto probe = &List[index];
		// Yes, change its assignment to this new camera
		if (probe->Viewpoint != viewpoint || probe->FOV != fov)
		{
			texture->bFirstUpdate = true;
		}
		probe->Viewpoint = viewpoint;
		probe->FOV = fov;
		return;
	}
	// No, create a new assignment
	auto probe = &List[List.Reserve(1)];
	probe->Viewpoint = viewpoint;
	probe->Texture = texture;
	probe->PicNum = picnum;
	probe->FOV = fov;
	texture->bFirstUpdate = true;
}

//==========================================================================
//
// SetCameraToTexture
//
// [ZZ] expose this to ZScript
//
//==========================================================================

void SetCameraToTexture(AActor *viewpoint, const FString &texturename, double fov)
{
	FTextureID textureid = TexMan.CheckForTexture(texturename, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
	if (textureid.isValid())
	{
		// Only proceed if the texture actually has a canvas.
		auto tex = TexMan.GetGameTexture(textureid);
		if (tex && tex->isHardwareCanvas()) // Q: how to deal with the software renderer here?
		{
			viewpoint->Level->canvasTextureInfo.Add(viewpoint, textureid, fov);
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: UpdateAll
//
// Updates all canvas textures that were visible in the last frame.
//
//==========================================================================

void FCanvasTextureInfo::UpdateAll(std::function<void(AActor *, FCanvasTexture *, double fov)> callback)
{
	for (auto &probe : List)
	{
		if (probe.Viewpoint != nullptr && probe.Texture->bNeedsUpdate)
		{
			callback(probe.Viewpoint, probe.Texture, probe.FOV);
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: EmptyList
//
// Removes all camera->texture assignments.
//
//==========================================================================

void FCanvasTextureInfo::EmptyList ()
{
	List.Clear();
}

//==========================================================================
//
// FCanvasTextureInfo :: Serialize
//
// Reads or writes the current set of mappings in an archive.
//
//==========================================================================

void FCanvasTextureInfo::Serialize(FSerializer &arc)
{
	if (arc.isWriting())
	{
		if (List.Size() > 0)
		{
			if (arc.BeginArray("canvastextures"))
			{
				for (auto &probe : List)
				{
					if (probe.Texture != nullptr && probe.Viewpoint != nullptr)
					{
						if (arc.BeginObject(nullptr))
						{
							arc("viewpoint", probe.Viewpoint)
								("fov", probe.FOV)
								("texture", probe.PicNum)
								.EndObject();
						}
					}
				}
				arc.EndArray();
			}
		}
	}
	else
	{
		if (arc.BeginArray("canvastextures"))
		{
			AActor *viewpoint = nullptr;
			double fov;
			FTextureID picnum;
			while (arc.BeginObject(nullptr))
			{
				arc("viewpoint", viewpoint)
					("fov", fov)
					("texture", picnum)
					.EndObject();
				Add(viewpoint, picnum, fov);
			}
			arc.EndArray();
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: Mark
//
// Marks all viewpoints in the list for the collector.
//
//==========================================================================

void FCanvasTextureInfo::Mark()
{
	for (auto & info : List)
	{
		GC::Mark(info.Viewpoint);
	}
}

