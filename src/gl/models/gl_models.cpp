/*
** gl_models.cpp
**
** General model handling code
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
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

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "c_console.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_state.h"
#include "d_player.h"
//#include "resources/voxels.h"
//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_convert.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"

static inline float GetTimeFloat()
{
	return (float)I_MSTime() * (float)TICRATE / 1000.0f;
}

CVAR(Bool, gl_interpolate_model_frames, true, CVAR_ARCHIVE)
CVAR(Bool, gl_light_models, true, CVAR_ARCHIVE)
EXTERN_CVAR(Int, gl_fogmode)

extern TDeletingArray<FVoxel *> Voxels;
extern TDeletingArray<FVoxelDef *> VoxelDefs;

DeletingModelArray Models;


void gl_LoadModels()
{
	/*
	for (int i = Models.Size() - 1; i >= 0; i--)
	{
		Models[i]->BuildVertexBuffer();
	}
	*/
}

void gl_FlushModels()
{
	for (int i = Models.Size() - 1; i >= 0; i--)
	{
		Models[i]->DestroyVertexBuffer();
	}
}

//===========================================================================
//
// Uses a hardware buffer if either single frame (i.e. no interpolation needed)
// or shading is available (interpolation is done by the vertex shader)
//
// If interpolation has to be done on the CPU side this will fall back
// to CPU-side arrays.
//
//===========================================================================

FModelVertexBuffer::FModelVertexBuffer(bool needindex, bool singleframe)
	: FVertexBuffer(singleframe || gl.glslversion > 0)
{
	vbo_ptr = nullptr;
	ibo_id = 0;
	if (needindex)
	{
		glGenBuffers(1, &ibo_id);	// The index buffer can always be a real buffer.
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (gl.glslversion > 0)
	{
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glEnableVertexAttribArray(VATTR_VERTEX2);
		glDisableVertexAttribArray(VATTR_COLOR);
	}
	else
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertexBuffer::~FModelVertexBuffer()
{
	if (ibo_id != 0)
	{
		glDeleteBuffers(1, &ibo_id);
	}
	if (vbo_ptr != nullptr)
	{
		delete[] vbo_ptr;
	}
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertex *FModelVertexBuffer::LockVertexBuffer(unsigned int size)
{
	if (vbo_id > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferData(GL_ARRAY_BUFFER, size * sizeof(FModelVertex), nullptr, GL_STATIC_DRAW);
		if (gl.version >= 3.0)
			return (FModelVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, size * sizeof(FModelVertex), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		else
			return (FModelVertex*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	}
	else
	{
		if (vbo_ptr != nullptr) delete[] vbo_ptr;
		vbo_ptr = new FModelVertex[size];
		memset(vbo_ptr, 0, size * sizeof(FModelVertex));
		return vbo_ptr;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockVertexBuffer()
{
	if (vbo_id > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
}

//===========================================================================
//
//
//
//===========================================================================

unsigned int *FModelVertexBuffer::LockIndexBuffer(unsigned int size)
{
	if (ibo_id != 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * sizeof(unsigned int), NULL, GL_STATIC_DRAW);
		if (gl.version >= 3.0)
			return (unsigned int*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, size * sizeof(unsigned int), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		else
			return (unsigned int*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	}
	else
	{
		return nullptr;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockIndexBuffer()
{
	if (ibo_id > 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
}


//===========================================================================
//
// Sets up the buffer starts for frame interpolation
// This must be called after gl_RenderState.Apply!
//
//===========================================================================
static TArray<FModelVertex> iBuffer;

unsigned int FModelVertexBuffer::SetupFrame(unsigned int frame1, unsigned int frame2, unsigned int size)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (vbo_id > 0)
	{
		if (gl.glslversion > 0)
		{
			glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FModelVertex), &VMO[frame1].x);
			glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FModelVertex), &VMO[frame1].u);
			glVertexAttribPointer(VATTR_VERTEX2, 3, GL_FLOAT, false, sizeof(FModelVertex), &VMO[frame2].x);
		}
		else
		{
			// only used for single frame models so there is no vertex2 here, which has no use without a shader.
			glVertexPointer(3, GL_FLOAT, sizeof(FModelVertex), &VMO[frame1].x);
			glTexCoordPointer(2, GL_FLOAT, sizeof(FModelVertex), &VMO[frame1].u);
		}
	}
	else if (frame1 == frame2 || size == 0 || gl_RenderState.GetInterpolationFactor() == 0.f)
	{
		glVertexPointer(3, GL_FLOAT, sizeof(FModelVertex), &vbo_ptr[frame1].x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FModelVertex), &vbo_ptr[frame1].u);
	}
	else
	{
		// must interpolate
		iBuffer.Resize(size);
		glVertexPointer(3, GL_FLOAT, sizeof(FModelVertex), &iBuffer[0].x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FModelVertex), &vbo_ptr[frame1].u);
		float frac = gl_RenderState.GetInterpolationFactor();
		for (unsigned i = 0; i < size; i++)
		{
			iBuffer[i].x = vbo_ptr[frame1 + i].x * (1.f - frac) + vbo_ptr[frame2 + i].x * frac;
			iBuffer[i].y = vbo_ptr[frame1 + i].y * (1.f - frac) + vbo_ptr[frame2 + i].y * frac;
			iBuffer[i].z = vbo_ptr[frame1 + i].z * (1.f - frac) + vbo_ptr[frame2 + i].z * frac;
		}
	}
	return frame1;
}

//===========================================================================
//
// FModel::~FModel
//
//===========================================================================

FModel::~FModel()
{
	if (mVBuf != nullptr) delete mVBuf;
}




static TArray<FSpriteModelFrame> SpriteModelFrames;
static int * SpriteModelHash;
//TArray<FStateModelFrame> StateModelFrames;

static void DeleteModelHash()
{
	if (SpriteModelHash != nullptr) delete [] SpriteModelHash;
	SpriteModelHash = nullptr;
}

//===========================================================================
//
// FindGFXFile
//
//===========================================================================

static int FindGFXFile(FString & fn)
{
	int lump = Wads.CheckNumForFullName(fn);	// if we find something that matches the name plus the extension, return it and do not enter the substitution logic below.
	if (lump != -1) return lump;

	int best = -1;
	int dot = fn.LastIndexOf('.');
	int slash = fn.LastIndexOf('/');
	if (dot > slash) fn.Truncate(dot);

	static const char * extensions[] = { ".png", ".jpg", ".tga", ".pcx", nullptr };

	for (const char ** extp=extensions; *extp; extp++)
	{
		int lump = Wads.CheckNumForFullName(fn + *extp);
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
	if (texlump>=0)
	{
		return TexMan.CheckForTexture(Wads.GetLumpFullName(texlump), FTexture::TEX_Any, FTextureManager::TEXMAN_TryAny);
	}
	else 
	{
		return FNullTextureID();
	}
}

//===========================================================================
//
// ModelFrameHash
//
//===========================================================================

static int ModelFrameHash(FSpriteModelFrame * smf)
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

static unsigned FindModel(const char * path, const char * modelfile)
{
	FModel * model = nullptr;
	FString fullname;

	fullname.Format("%s%s", path, modelfile);
	int lump = Wads.CheckNumForFullName(fullname);

	if (lump<0)
	{
		Printf("FindModel: '%s' not found\n", fullname.GetChars());
		return -1;
	}

	for(unsigned i = 0; i< Models.Size(); i++)
	{
		if (!Models[i]->mFileName.CompareNoCase(fullname)) return i;
	}

	int len = Wads.LumpLength(lump);
	FMemLump lumpd = Wads.ReadLump(lump);
	char * buffer = (char*)lumpd.GetMem();

	if (!memcmp(buffer, "DMDM", 4))
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
			Printf("LoadModel: Unknown model format in '%s'\n", fullname.GetChars());
			return -1;
		}
	}
	// The vertex buffer cannot be initialized here because this gets called before OpenGL is initialized
	model->mFileName = fullname;
	return Models.Push(model);
}

//===========================================================================
//
// gl_InitModels
//
//===========================================================================

void gl_InitModels()
{
	int Lump, lastLump;
	FString path;
	int index, surface;
	int i;

	FSpriteModelFrame smf;

	lastLump = 0;

	for(unsigned i=0;i<Models.Size();i++)
	{
		delete Models[i];
	}
	Models.Clear();
	SpriteModelFrames.Clear();
	DeleteModelHash();

	// First, create models for each voxel
	for (unsigned i = 0; i < Voxels.Size(); i++)
	{
		FVoxelModel *md = new FVoxelModel(Voxels[i], false);
		Voxels[i]->VoxelIndex = Models.Push(md);
	}
	// now create GL model frames for the voxeldefs
	for (unsigned i = 0; i < VoxelDefs.Size(); i++)
	{
		FVoxelModel *md = (FVoxelModel*)Models[VoxelDefs[i]->Voxel->VoxelIndex];
		memset(&smf, 0, sizeof(smf));
		smf.modelIDs[1] = smf.modelIDs[2] = smf.modelIDs[3] = -1;
		smf.modelIDs[0] = VoxelDefs[i]->Voxel->VoxelIndex;
		smf.skinIDs[0] = md->GetPaletteTexture();
		smf.xscale = smf.yscale = smf.zscale = VoxelDefs[i]->Scale;
		smf.angleoffset = VoxelDefs[i]->AngleOffset.Degrees;
		if (VoxelDefs[i]->PlacedSpin != 0)
		{
			smf.yrotate = 1.f;
			smf.rotationSpeed = VoxelDefs[i]->PlacedSpin / 55.55f;
			smf.flags |= MDL_ROTATING;
		}
		VoxelDefs[i]->VoxeldefIndex = SpriteModelFrames.Push(smf);
		if (VoxelDefs[i]->PlacedSpin != VoxelDefs[i]->DroppedSpin)
		{
			if (VoxelDefs[i]->DroppedSpin != 0)
			{
				smf.yrotate = 1.f;
				smf.rotationSpeed = VoxelDefs[i]->DroppedSpin / 55.55f;
				smf.flags |= MDL_ROTATING;
			}
			else
			{
				smf.yrotate = 0;
				smf.rotationSpeed = 0;
				smf.flags &= ~MDL_ROTATING;
			}
			SpriteModelFrames.Push(smf);
		}
	}

	memset(&smf, 0, sizeof(smf));
	smf.modelIDs[0] = smf.modelIDs[1] = smf.modelIDs[2] = smf.modelIDs[3] = -1;
	while ((Lump = Wads.FindLump("MODELDEF", &lastLump)) != -1)
	{
		FScanner sc(Lump);
		while (sc.GetString())
		{
			if (sc.Compare("model"))
			{
				path = "";
				sc.MustGetString();
				memset(&smf, 0, sizeof(smf));
				smf.modelIDs[1] = smf.modelIDs[2] = smf.modelIDs[3] = -1;
				smf.xscale=smf.yscale=smf.zscale=1.f;

				smf.type = PClass::FindClass(sc.String);
				if (!smf.type || smf.type->Defaults == nullptr) 
				{
					sc.ScriptError("MODELDEF: Unknown actor type '%s'\n", sc.String);
				}
				sc.MustGetStringName("{");
				while (!sc.CheckString("}"))
				{
					sc.MustGetString();
					if (sc.Compare("path"))
					{
						sc.MustGetString();
						FixPathSeperator(sc.String);
						path = sc.String;
						if (path[(int)path.Len()-1]!='/') path+='/';
					}
					else if (sc.Compare("model"))
					{
						sc.MustGetNumber();
						index = sc.Number;
						if (index < 0 || index >= MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}
						sc.MustGetString();
						FixPathSeperator(sc.String);
						smf.modelIDs[index] = FindModel(path.GetChars(), sc.String);
						if (smf.modelIDs[index] == -1)
						{
							Printf("%s: model not found in %s\n", sc.String, path.GetChars());
						}
					}
					else if (sc.Compare("scale"))
					{
						sc.MustGetFloat();
						smf.xscale = sc.Float;
						sc.MustGetFloat();
						smf.yscale = sc.Float;
						sc.MustGetFloat();
						smf.zscale = sc.Float;
					}
					// [BB] Added zoffset reading. 
					// Now it must be considered deprecated.
					else if (sc.Compare("zoffset"))
					{
						sc.MustGetFloat();
						smf.zoffset=sc.Float;
					}
					// Offset reading.
					else if (sc.Compare("offset"))
					{
						sc.MustGetFloat();
						smf.xoffset = sc.Float;
						sc.MustGetFloat();
						smf.yoffset = sc.Float;
						sc.MustGetFloat();
						smf.zoffset = sc.Float;
					}
					// angleoffset, pitchoffset and rolloffset reading.
					else if (sc.Compare("angleoffset"))
					{
						sc.MustGetFloat();
						smf.angleoffset = sc.Float;
					}
					else if (sc.Compare("pitchoffset"))
					{
						sc.MustGetFloat();
						smf.pitchoffset = sc.Float;
					}
					else if (sc.Compare("rolloffset"))
					{
						sc.MustGetFloat();
						smf.rolloffset = sc.Float;
					}
					// [BB] Added model flags reading.
					else if (sc.Compare("ignoretranslation"))
					{
						smf.flags |= MDL_IGNORETRANSLATION;
					}
					else if (sc.Compare("pitchfrommomentum"))
					{
						smf.flags |= MDL_PITCHFROMMOMENTUM;
					}
					else if (sc.Compare("inheritactorpitch"))
					{
						smf.flags |= MDL_USEACTORPITCH | MDL_BADROTATION;
					}
					else if (sc.Compare("inheritactorroll"))
					{
						smf.flags |= MDL_USEACTORROLL;
					}
					else if (sc.Compare("useactorpitch"))
					{
						smf.flags |= MDL_USEACTORPITCH;
					}
					else if (sc.Compare("useactorroll"))
					{
						smf.flags |= MDL_USEACTORROLL;
					}
					else if (sc.Compare("rotating"))
					{
						smf.flags |= MDL_ROTATING;
						smf.xrotate = 0.;
						smf.yrotate = 1.;
						smf.zrotate = 0.;
						smf.rotationCenterX = 0.;
						smf.rotationCenterY = 0.;
						smf.rotationCenterZ = 0.;
						smf.rotationSpeed = 1.;
					}
					else if (sc.Compare("rotation-speed"))
					{
						sc.MustGetFloat();
						smf.rotationSpeed = sc.Float;
					}
					else if (sc.Compare("rotation-vector"))
					{
						sc.MustGetFloat();
						smf.xrotate = sc.Float;
						sc.MustGetFloat();
						smf.yrotate = sc.Float;
						sc.MustGetFloat();
						smf.zrotate = sc.Float;
					}
					else if (sc.Compare("rotation-center"))
					{
						sc.MustGetFloat();
						smf.rotationCenterX = sc.Float;
						sc.MustGetFloat();
						smf.rotationCenterY = sc.Float;
						sc.MustGetFloat();
						smf.rotationCenterZ = sc.Float;
					}
					else if (sc.Compare("interpolatedoubledframes"))
					{
						smf.flags |= MDL_INTERPOLATEDOUBLEDFRAMES;
					}
					else if (sc.Compare("nointerpolation"))
					{
						smf.flags |= MDL_NOINTERPOLATION;
					}
					else if (sc.Compare("skin"))
					{
						sc.MustGetNumber();
						index=sc.Number;
						if (index<0 || index>=MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}
						sc.MustGetString();
						FixPathSeperator(sc.String);
						if (sc.Compare(""))
						{
							smf.skinIDs[index]=FNullTextureID();
						}
						else
						{
							smf.skinIDs[index] = LoadSkin(path.GetChars(), sc.String);
							if (!smf.skinIDs[index].isValid())
							{
								Printf("Skin '%s' not found in '%s'\n",
									sc.String, smf.type->TypeName.GetChars());
							}
						}
					}
					else if (sc.Compare("surfaceskin"))
					{
						sc.MustGetNumber();
						index = sc.Number;
						sc.MustGetNumber();
						surface = sc.Number;

						if (index<0 || index >= MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}

						if (surface<0 || surface >= MD3_MAX_SURFACES)
						{
							sc.ScriptError("Invalid MD3 Surface %d in %s", MD3_MAX_SURFACES, smf.type->TypeName.GetChars());
						}

						sc.MustGetString();
						FixPathSeperator(sc.String);
						if (sc.Compare(""))
						{
							smf.surfaceskinIDs[index][surface] = FNullTextureID();
						}
						else
						{
							smf.surfaceskinIDs[index][surface] = LoadSkin(path.GetChars(), sc.String);
							if (!smf.surfaceskinIDs[index][surface].isValid())
							{
								Printf("Surface Skin '%s' not found in '%s'\n",
									sc.String, smf.type->TypeName.GetChars());
							}
						}
					}
					else if (sc.Compare("frameindex") || sc.Compare("frame"))
					{
						bool isframe=!!sc.Compare("frame");

						sc.MustGetString();
						smf.sprite = -1;
						for (i = 0; i < (int)sprites.Size (); ++i)
						{
							if (strnicmp (sprites[i].name, sc.String, 4) == 0)
							{
								if (sprites[i].numframes==0)
								{
									//sc.ScriptError("Sprite %s has no frames", sc.String);
								}
								smf.sprite = i;
								break;
							}
						}
						if (smf.sprite==-1)
						{
							sc.ScriptError("Unknown sprite %s in model definition for %s", sc.String, smf.type->TypeName.GetChars());
						}

						sc.MustGetString();
						FString framechars = sc.String;

						sc.MustGetNumber();
						index=sc.Number;
						if (index<0 || index>=MAX_MODELS_PER_FRAME)
						{
							sc.ScriptError("Too many models in %s", smf.type->TypeName.GetChars());
						}
						if (isframe)
						{
							sc.MustGetString();
							if (smf.modelIDs[index] != -1)
							{
								FModel *model = Models[smf.modelIDs[index]];
								smf.modelframes[index] = model->FindFrame(sc.String);
								if (smf.modelframes[index]==-1) sc.ScriptError("Unknown frame '%s' in %s", sc.String, smf.type->TypeName.GetChars());
							}
							else smf.modelframes[index] = -1;
						}
						else
						{
							sc.MustGetNumber();
							smf.modelframes[index] = sc.Number;
						}

						for(i=0; framechars[i]>0; i++)
						{
							char map[29]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
							int c = toupper(framechars[i])-'A';

							if (c<0 || c>=29)
							{
								sc.ScriptError("Invalid frame character %c found", c+'A');
							}
							if (map[c]) continue;
							smf.frame=c;
							SpriteModelFrames.Push(smf);
							GetDefaultByType(smf.type)->hasmodel = true;
							map[c]=1;
						}
					}
					else
					{
						sc.ScriptMessage("Unrecognized string \"%s\"", sc.String);
					}
				}
			}
		}
	}

	// create a hash table for quick access
	SpriteModelHash = new int[SpriteModelFrames.Size ()];
	atterm(DeleteModelHash);
	memset(SpriteModelHash, 0xff, SpriteModelFrames.Size () * sizeof(int));

	for (i = 0; i < (int)SpriteModelFrames.Size (); i++)
	{
		int j = ModelFrameHash(&SpriteModelFrames[i]) % SpriteModelFrames.Size ();

		SpriteModelFrames[i].hashnext = SpriteModelHash[j];
		SpriteModelHash[j]=i;
	}
}


//===========================================================================
//
// gl_FindModelFrame
//
//===========================================================================
EXTERN_CVAR (Bool, r_drawvoxels)

FSpriteModelFrame * gl_FindModelFrame(const PClass * ti, int sprite, int frame, bool dropped)
{
	if (GetDefaultByType(ti)->hasmodel)
	{
		FSpriteModelFrame smf;

		memset(&smf, 0, sizeof(smf));
		smf.type=ti;
		smf.sprite=sprite;
		smf.frame=frame;

		int hash = SpriteModelHash[ModelFrameHash(&smf) % SpriteModelFrames.Size()];

		while (hash>=0)
		{
			FSpriteModelFrame * smff = &SpriteModelFrames[hash];
			if (smff->type==ti && smff->sprite==sprite && smff->frame==frame) return smff;
			hash=smff->hashnext;
		}
	}

	// Check for voxel replacements
	if (r_drawvoxels)
	{
		spritedef_t *sprdef = &sprites[sprite];
		if (frame < sprdef->numframes)
		{
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + frame];
			if (sprframe->Voxel != nullptr)
			{
				int index = sprframe->Voxel->VoxeldefIndex;
				if (dropped && sprframe->Voxel->DroppedSpin !=sprframe->Voxel->PlacedSpin) index++;
				return &SpriteModelFrames[index];
			}
		}
	}
	return nullptr;
}


//===========================================================================
//
// gl_RenderModel
//
//===========================================================================

void gl_RenderFrameModels( const FSpriteModelFrame *smf,
						   const FState *curState,
						   const int curTics,
						   const PClass *ti,
						   Matrix3x4 *normaltransform,
						   int translation)
{
	// [BB] Frame interpolation: Find the FSpriteModelFrame smfNext which follows after smf in the animation
	// and the scalar value inter ( element of [0,1) ), both necessary to determine the interpolated frame.
	FSpriteModelFrame * smfNext = nullptr;
	double inter = 0.;
	if( gl_interpolate_model_frames && !(smf->flags & MDL_NOINTERPOLATION) )
	{
		FState *nextState = curState->GetNextState( );
		if( curState != nextState && nextState )
		{
			// [BB] To interpolate at more than 35 fps we take tic fractions into account.
			float ticFraction = 0.;
			// [BB] In case the tic counter is frozen we have to leave ticFraction at zero.
			if ( ConsoleState == c_up && menuactive != MENU_On && !(level.flags2 & LEVEL2_FROZEN) )
			{
				float time = GetTimeFloat();
				ticFraction = (time - static_cast<int>(time));
			}
			inter = static_cast<double>(curState->Tics - curTics - ticFraction)/static_cast<double>(curState->Tics);

			// [BB] For some actors (e.g. ZPoisonShroom) spr->actor->tics can be bigger than curState->Tics.
			// In this case inter is negative and we need to set it to zero.
			if ( inter < 0. )
				inter = 0.;
			else
			{
				// [BB] Workaround for actors that use the same frame twice in a row.
				// Most of the standard Doom monsters do this in their see state.
				if ( (smf->flags & MDL_INTERPOLATEDOUBLEDFRAMES) )
				{
					const FState *prevState = curState - 1;
					if ( (curState->sprite == prevState->sprite) && ( curState->Frame == prevState->Frame) )
					{
						inter /= 2.;
						inter += 0.5;
					}
					if ( (curState->sprite == nextState->sprite) && ( curState->Frame == nextState->Frame) )
					{
						inter /= 2.;
						nextState = nextState->GetNextState( );
					}
				}
				if ( inter != 0.0 )
					smfNext = gl_FindModelFrame(ti, nextState->sprite, nextState->Frame, false);
			}
		}
	}

	for(int i=0; i<MAX_MODELS_PER_FRAME; i++)
	{
		if (smf->modelIDs[i] != -1)
		{
			FModel * mdl = Models[smf->modelIDs[i]];
			FTexture *tex = smf->skinIDs[i].isValid()? TexMan(smf->skinIDs[i]) : nullptr;
			mdl->BuildVertexBuffer();
			gl_RenderState.SetVertexBuffer(mdl->mVBuf);

			mdl->PushSpriteMDLFrame(smf, i);

			if ( smfNext && smf->modelframes[i] != smfNext->modelframes[i] )
				mdl->RenderFrame(tex, smf->modelframes[i], smfNext->modelframes[i], inter, translation);
			else
				mdl->RenderFrame(tex, smf->modelframes[i], smf->modelframes[i], 0.f, translation);

			gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
		}
	}
}

void gl_RenderModel(GLSprite * spr)
{
	FSpriteModelFrame * smf = spr->modelframe;


	// Setup transformation.
	glDepthFunc(GL_LEQUAL);
	gl_RenderState.EnableTexture(true);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!( spr->actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	}

	int translation = 0;
	if ( !(smf->flags & MDL_IGNORETRANSLATION) )
		translation = spr->actor->Translation;


	// y scale for a sprite means height, i.e. z in the world!
	float scaleFactorX = spr->actor->Scale.X * smf->xscale;
	float scaleFactorY = spr->actor->Scale.X * smf->yscale;
	float scaleFactorZ = spr->actor->Scale.Y * smf->zscale;
	float pitch = 0;
	float roll = 0;
	float rotateOffset = 0;
	float angle = spr->actor->Angles.Yaw.Degrees;

	// [BB] Workaround for the missing pitch information.
	if ( (smf->flags & MDL_PITCHFROMMOMENTUM) )
	{
		const double x = spr->actor->Vel.X;
		const double y = spr->actor->Vel.Y;
		const double z = spr->actor->Vel.Z;

		if (spr->actor->Vel.LengthSquared() > EQUAL_EPSILON)
		{
			// [BB] Calculate the pitch using spherical coordinates.
			if (z || x || y) pitch = float(atan(z / sqrt(x*x + y*y)) / M_PI * 180);

			// Correcting pitch if model is moving backwards
			if (fabs(x) > EQUAL_EPSILON || fabs(y) > EQUAL_EPSILON)
			{
				if ((x * cos(angle * M_PI / 180) + y * sin(angle * M_PI / 180)) / sqrt(x * x + y * y) < 0) pitch *= -1;
			}
			else pitch = fabs(pitch);
		}
	}

	if( smf->flags & MDL_ROTATING )
	{
		const float time = smf->rotationSpeed*GetTimeFloat()/200.f;
		rotateOffset = float((time - xs_FloorToInt(time)) *360.f );
	}

	// Added MDL_USEACTORPITCH and MDL_USEACTORROLL flags processing.
	// If both flags MDL_USEACTORPITCH and MDL_PITCHFROMMOMENTUM are set, the pitch sums up the actor pitch and the momentum vector pitch.
	if (smf->flags & MDL_USEACTORPITCH)
	{
		double d = spr->actor->Angles.Pitch.Degrees;
		if (smf->flags & MDL_BADROTATION) pitch -= d;
		else pitch += d;
	}
	if(smf->flags & MDL_USEACTORROLL) roll += spr->actor->Angles.Roll.Degrees;

	gl_RenderState.mModelMatrix.loadIdentity();

	// Model space => World space
	gl_RenderState.mModelMatrix.translate(spr->x, spr->z, spr->y );	
	
	// Applying model transformations:
	// 1) Applying actor angle, pitch and roll to the model
	gl_RenderState.mModelMatrix.rotate(-angle, 0, 1, 0);
	gl_RenderState.mModelMatrix.rotate(pitch, 0, 0, 1);
	gl_RenderState.mModelMatrix.rotate(-roll, 1, 0, 0);
	
	// 2) Applying Doomsday like rotation of the weapon pickup models
	// The rotation angle is based on the elapsed time.
	
	if( smf->flags & MDL_ROTATING )
	{
		gl_RenderState.mModelMatrix.translate(smf->rotationCenterX, smf->rotationCenterY, smf->rotationCenterZ);
		gl_RenderState.mModelMatrix.rotate(rotateOffset, smf->xrotate, smf->yrotate, smf->zrotate);
		gl_RenderState.mModelMatrix.translate(-smf->rotationCenterX, -smf->rotationCenterY, -smf->rotationCenterZ);
	}

	// 3) Scaling model.
	gl_RenderState.mModelMatrix.scale(scaleFactorX, scaleFactorZ, scaleFactorY);

	// 4) Aplying model offsets (model offsets do not depend on model scalings).
	gl_RenderState.mModelMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);
	
	// 5) Applying model rotations.
	gl_RenderState.mModelMatrix.rotate(-smf->angleoffset, 0, 1, 0);
	gl_RenderState.mModelMatrix.rotate(smf->pitchoffset, 0, 0, 1);
	gl_RenderState.mModelMatrix.rotate(-smf->rolloffset, 1, 0, 0);

	// consider the pixel stretching. For non-voxels this must be factored out here
	float stretch = (smf->modelIDs[0] != -1 ? Models[smf->modelIDs[0]]->getAspectFactor() : 1.f) / glset.pixelstretch;
	gl_RenderState.mModelMatrix.scale(1, stretch, 1);


	gl_RenderState.EnableModelMatrix(true);
	gl_RenderFrameModels( smf, spr->actor->state, spr->actor->tics, spr->actor->GetClass(), nullptr, translation );
	gl_RenderState.EnableModelMatrix(false);

	glDepthFunc(GL_LESS);
	if (!( spr->actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
		glDisable(GL_CULL_FACE);
}


//===========================================================================
//
// gl_RenderHUDModel
//
//===========================================================================

void gl_RenderHUDModel(DPSprite *psp, float ofsX, float ofsY)
{
	AActor * playermo=players[consoleplayer].camera;
	FSpriteModelFrame *smf = gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false);

	// [BB] No model found for this sprite, so we can't render anything.
	if ( smf == nullptr )
		return;

	glDepthFunc(GL_LEQUAL);

	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!( playermo->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
	}

	// [BB] The model has to be drawn independently from the position of the player,
	// so we have to reset the view matrix.
	gl_RenderState.mViewMatrix.loadIdentity();

	// Scaling model (y scale for a sprite means height, i.e. z in the world!).
	gl_RenderState.mViewMatrix.scale(smf->xscale, smf->zscale, smf->yscale);
	
	// Aplying model offsets (model offsets do not depend on model scalings).
	gl_RenderState.mViewMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

	// [BB] Weapon bob, very similar to the normal Doom weapon bob.
	gl_RenderState.mViewMatrix.rotate(ofsX/4, 0, 1, 0);
	gl_RenderState.mViewMatrix.rotate((ofsY-WEAPONTOP)/-4., 1, 0, 0);

	// [BB] For some reason the jDoom models need to be rotated.
	gl_RenderState.mViewMatrix.rotate(90.f, 0, 1, 0);

	// Applying angleoffset, pitchoffset, rolloffset.
	gl_RenderState.mViewMatrix.rotate(-smf->angleoffset, 0, 1, 0);
	gl_RenderState.mViewMatrix.rotate(smf->pitchoffset, 0, 0, 1);
	gl_RenderState.mViewMatrix.rotate(-smf->rolloffset, 1, 0, 0);
	gl_RenderState.ApplyMatrices();

	gl_RenderFrameModels( smf, psp->GetState(), psp->GetTics(), playermo->player->ReadyWeapon->GetClass(), nullptr, 0 );

	glDepthFunc(GL_LESS);
	if (!( playermo->RenderStyle == LegacyRenderStyles[STYLE_Normal] ))
		glDisable(GL_CULL_FACE);
}

//===========================================================================
//
// gl_IsHUDModelForPlayerAvailable
//
//===========================================================================

bool gl_IsHUDModelForPlayerAvailable (player_t * player)
{
	if (player == nullptr || player->ReadyWeapon == nullptr)
		return false;

	DPSprite *psp = player->FindPSprite(PSP_WEAPON);

	if (psp == nullptr || psp->GetState() == nullptr)
		return false;

	FState* state = psp->GetState();
	FSpriteModelFrame *smf = gl_FindModelFrame(player->ReadyWeapon->GetClass(), state->sprite, state->GetFrame(), false);
	return ( smf != nullptr );
}

