//
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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
** gl_models.cpp
**
** General model handling code
**
**/

#include "filesystem.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "printf.h"
#include "model_ue1.h"
#include "model_obj.h"
#include "model_md2.h"
#include "model_md3.h"
#include "model_kvx.h"
#include "model_iqm.h"
#include "i_time.h"
#include "voxels.h"
#include "texturemanager.h"
#include "modelrenderer.h"


TArray<FString> savedModelFiles;
TDeletingArray<FModel*> Models;
TArray<FSpriteModelFrame> SpriteModelFrames;


/////////////////////////////////////////////////////////////////////////////

void FlushModels()
{
	for (int i = Models.Size() - 1; i >= 0; i--)
	{
		Models[i]->DestroyVertexBuffer();
	}
}

/////////////////////////////////////////////////////////////////////////////

FModel::FModel()
{
	for (int i = 0; i < NumModelRendererTypes; i++)
		mVBuf[i] = nullptr;
}

FModel::~FModel()
{
	DestroyVertexBuffer();
}

void FModel::DestroyVertexBuffer()
{
	for (int i = 0; i < NumModelRendererTypes; i++)
	{
		delete mVBuf[i];
		mVBuf[i] = nullptr;
	}
}

//===========================================================================
//
// FindGFXFile
//
//===========================================================================

static int FindGFXFile(FString & fn)
{
	int lump = fileSystem.CheckNumForFullName(fn);	// if we find something that matches the name plus the extension, return it and do not enter the substitution logic below.
	if (lump != -1) return lump;

	int best = -1;
	auto dot = fn.LastIndexOf('.');
	auto slash = fn.LastIndexOf('/');
	if (dot > slash) fn.Truncate(dot);

	static const char * extensions[] = { ".png", ".jpg", ".tga", ".pcx", nullptr };

	for (const char ** extp=extensions; *extp; extp++)
	{
		lump = fileSystem.CheckNumForFullName(fn + *extp);
		if (lump >= best)  best = lump;
	}
	return best;
}


//===========================================================================
//
// LoadSkin
//
//===========================================================================

FTextureID LoadSkin(const char * path, const char * fn)
{
	FString buffer;

	buffer.Format("%s%s", path, fn);

	int texlump = FindGFXFile(buffer);
	const char * const texname = texlump < 0 ? fn : fileSystem.GetFileFullName(texlump);
	return TexMan.CheckForTexture(texname, ETextureType::Any, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ForceLookup);
}

//===========================================================================
//
// ModelFrameHash
//
//===========================================================================

int ModelFrameHash(FSpriteModelFrame * smf)
{
	const uint32_t *table = GetCRCTable ();
	uint32_t hash = 0xffffffff;

	const char * s = (const char *)(&smf->type);	// this uses type, sprite and frame for hashing
	const char * se= (const char *)(&smf->hashnext);

	for (; s<se; s++)
	{
		hash = CRC1 (hash, *s, table);
	}
	return hash ^ 0xffffffff;
}

//===========================================================================
//
// FindModel
//
//===========================================================================

unsigned FindModel(const char * path, const char * modelfile, bool silent)
{
	FModel * model = nullptr;
	FString fullname;

	if (path) fullname.Format("%s%s", path, modelfile);
	else fullname = modelfile;
	int lump = fileSystem.CheckNumForFullName(fullname);

	if (lump<0)
	{
		Printf(PRINT_HIGH, "FindModel: '%s' not found\n", fullname.GetChars());
		return -1;
	}

	for(unsigned i = 0; i< Models.Size(); i++)
	{
		if (!Models[i]->mFileName.CompareNoCase(fullname)) return i;
	}

	int len = fileSystem.FileLength(lump);
	FileData lumpd = fileSystem.ReadFile(lump);
	char * buffer = (char*)lumpd.GetMem();

	if ( (size_t)fullname.LastIndexOf("_d.3d") == fullname.Len()-5 )
	{
		FString anivfile = fullname.GetChars();
		anivfile.Substitute("_d.3d","_a.3d");
		if ( fileSystem.CheckNumForFullName(anivfile) > 0 )
		{
			model = new FUE1Model;
		}
	}
	else if ( (size_t)fullname.LastIndexOf("_a.3d") == fullname.Len()-5 )
	{
		FString datafile = fullname.GetChars();
		datafile.Substitute("_a.3d","_d.3d");
		if ( fileSystem.CheckNumForFullName(datafile) > 0 )
		{
			model = new FUE1Model;
		}
	}
	else if ( (size_t)fullname.LastIndexOf(".obj") == fullname.Len() - 4 )
	{
		model = new FOBJModel;
	}
	else if (!memcmp(buffer, "DMDM", 4))
	{
		model = new FDMDModel;
	}
	else if (!memcmp(buffer, "IDP2", 4))
	{
		model = new FMD2Model;
	}
	else if (!memcmp(buffer, "IDP3", 4))
	{
		model = new FMD3Model;
	}
	else if (!memcmp(buffer, "INTERQUAKEMODEL\0", 16))
	{
		model = new IQMModel;
	}

	if (model != nullptr)
	{
		if (!model->Load(path, lump, buffer, len))
		{
			delete model;
			return -1;
		}
	}
	else
	{
		// try loading as a voxel
		FVoxel *voxel = R_LoadKVX(lump);
		if (voxel != nullptr)
		{
			model = new FVoxelModel(voxel, true);
		}
		else
		{
			Printf(PRINT_HIGH, "LoadModel: Unknown model format in '%s'\n", fullname.GetChars());
			return -1;
		}
	}
	// The vertex buffer cannot be initialized here because this gets called before OpenGL is initialized
	model->mFileName = fullname;
	return Models.Push(model);
}

