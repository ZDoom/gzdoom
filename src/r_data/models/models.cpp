// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl_models.cpp
**
** General model handling code
**
**/

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
#include "g_levellocals.h"
#include "r_utility.h"
#include "r_data/models/models.h"

#ifdef _MSC_VER
#pragma warning(disable:4244) // warning C4244: conversion from 'double' to 'float', possible loss of data
#endif

CVAR(Bool, gl_interpolate_model_frames, true, CVAR_ARCHIVE)
EXTERN_CVAR(Bool, r_drawvoxels)

extern TDeletingArray<FVoxel *> Voxels;
extern TDeletingArray<FVoxelDef *> VoxelDefs;

DeletingModelArray Models;

void FModelRenderer::RenderModel(float x, float y, float z, FSpriteModelFrame *smf, AActor *actor)
{
	// Setup transformation.

	int translation = 0;
	if (!(smf->flags & MDL_IGNORETRANSLATION))
		translation = actor->Translation;

	// y scale for a sprite means height, i.e. z in the world!
	float scaleFactorX = actor->Scale.X * smf->xscale;
	float scaleFactorY = actor->Scale.X * smf->yscale;
	float scaleFactorZ = actor->Scale.Y * smf->zscale;
	float pitch = 0;
	float roll = 0;
	double rotateOffset = 0;
	float angle = actor->Angles.Yaw.Degrees;

	// [BB] Workaround for the missing pitch information.
	if ((smf->flags & MDL_PITCHFROMMOMENTUM))
	{
		const double x = actor->Vel.X;
		const double y = actor->Vel.Y;
		const double z = actor->Vel.Z;

		if (actor->Vel.LengthSquared() > EQUAL_EPSILON)
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

	if (smf->flags & MDL_ROTATING)
	{
		const double time = smf->rotationSpeed*GetTimeFloat() / 200.;
		rotateOffset = double((time - xs_FloorToInt(time)) *360.);
	}

	// Added MDL_USEACTORPITCH and MDL_USEACTORROLL flags processing.
	// If both flags MDL_USEACTORPITCH and MDL_PITCHFROMMOMENTUM are set, the pitch sums up the actor pitch and the velocity vector pitch.
	if (smf->flags & MDL_USEACTORPITCH)
	{
		double d = actor->Angles.Pitch.Degrees;
		if (smf->flags & MDL_BADROTATION) pitch += d;
		else pitch -= d;
	}
	if (smf->flags & MDL_USEACTORROLL) roll += actor->Angles.Roll.Degrees;

	VSMatrix objectToWorldMatrix;
	objectToWorldMatrix.loadIdentity();

	// Model space => World space
	objectToWorldMatrix.translate(x, z, y);

	// [Nash] take SpriteRotation into account
	angle += actor->SpriteRotation.Degrees;

	if (actor->renderflags & RF_INTERPOLATEANGLES)
	{
		// [Nash] use interpolated angles
		DRotator Angles = actor->InterpolatedAngles(r_viewpoint.TicFrac);
		angle = Angles.Yaw.Degrees;
	}

	// Applying model transformations:
	// 1) Applying actor angle, pitch and roll to the model
	if (smf->flags & MDL_USEROTATIONCENTER)
	{
		objectToWorldMatrix.translate(smf->rotationCenterX, smf->rotationCenterZ, smf->rotationCenterY);
	}
	objectToWorldMatrix.rotate(-angle, 0, 1, 0);
	objectToWorldMatrix.rotate(pitch, 0, 0, 1);
	objectToWorldMatrix.rotate(-roll, 1, 0, 0);
	if (smf->flags & MDL_USEROTATIONCENTER)
	{
		objectToWorldMatrix.translate(-smf->rotationCenterX, -smf->rotationCenterZ, -smf->rotationCenterY);
	}

	// 2) Applying Doomsday like rotation of the weapon pickup models
	// The rotation angle is based on the elapsed time.

	if (smf->flags & MDL_ROTATING)
	{
		objectToWorldMatrix.translate(smf->rotationCenterX, smf->rotationCenterY, smf->rotationCenterZ);
		objectToWorldMatrix.rotate(rotateOffset, smf->xrotate, smf->yrotate, smf->zrotate);
		objectToWorldMatrix.translate(-smf->rotationCenterX, -smf->rotationCenterY, -smf->rotationCenterZ);
	}

	// 3) Scaling model.
	objectToWorldMatrix.scale(scaleFactorX, scaleFactorZ, scaleFactorY);

	// 4) Aplying model offsets (model offsets do not depend on model scalings).
	objectToWorldMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

	// 5) Applying model rotations.
	objectToWorldMatrix.rotate(-smf->angleoffset, 0, 1, 0);
	objectToWorldMatrix.rotate(smf->pitchoffset, 0, 0, 1);
	objectToWorldMatrix.rotate(-smf->rolloffset, 1, 0, 0);

	// consider the pixel stretching. For non-voxels this must be factored out here
	float stretch = (smf->modelIDs[0] != -1 ? Models[smf->modelIDs[0]]->getAspectFactor() : 1.f) / level.info->pixelstretch;
	objectToWorldMatrix.scale(1, stretch, 1);

	BeginDrawModel(actor, smf, objectToWorldMatrix);
	RenderFrameModels(smf, actor->state, actor->tics, actor->GetClass(), nullptr, translation);
	EndDrawModel(actor, smf);
}

void FModelRenderer::RenderHUDModel(DPSprite *psp, float ofsX, float ofsY)
{
	AActor * playermo = players[consoleplayer].camera;
	FSpriteModelFrame *smf = gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false);

	// [BB] No model found for this sprite, so we can't render anything.
	if (smf == nullptr)
		return;

	// The model position and orientation has to be drawn independently from the position of the player,
	// but we need to position it correctly in the world for light to work properly.
	VSMatrix objectToWorldMatrix = GetViewToWorldMatrix();

	// Scaling model (y scale for a sprite means height, i.e. z in the world!).
	objectToWorldMatrix.scale(smf->xscale, smf->zscale, smf->yscale);

	// Aplying model offsets (model offsets do not depend on model scalings).
	objectToWorldMatrix.translate(smf->xoffset / smf->xscale, smf->zoffset / smf->zscale, smf->yoffset / smf->yscale);

	// [BB] Weapon bob, very similar to the normal Doom weapon bob.
	objectToWorldMatrix.rotate(ofsX / 4, 0, 1, 0);
	objectToWorldMatrix.rotate((ofsY - WEAPONTOP) / -4., 1, 0, 0);

	// [BB] For some reason the jDoom models need to be rotated.
	objectToWorldMatrix.rotate(90.f, 0, 1, 0);

	// Applying angleoffset, pitchoffset, rolloffset.
	objectToWorldMatrix.rotate(-smf->angleoffset, 0, 1, 0);
	objectToWorldMatrix.rotate(smf->pitchoffset, 0, 0, 1);
	objectToWorldMatrix.rotate(-smf->rolloffset, 1, 0, 0);

	BeginDrawHUDModel(playermo, objectToWorldMatrix);
	RenderFrameModels(smf, psp->GetState(), psp->GetTics(), playermo->player->ReadyWeapon->GetClass(), nullptr, 0);
	EndDrawHUDModel(playermo);
}

void FModelRenderer::RenderFrameModels(const FSpriteModelFrame *smf,
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
	if (gl_interpolate_model_frames && !(smf->flags & MDL_NOINTERPOLATION))
	{
		FState *nextState = curState->GetNextState();
		if (curState != nextState && nextState)
		{
			// [BB] To interpolate at more than 35 fps we take tic fractions into account.
			float ticFraction = 0.;
			// [BB] In case the tic counter is frozen we have to leave ticFraction at zero.
			if (ConsoleState == c_up && menuactive != MENU_On && !(level.flags2 & LEVEL2_FROZEN))
			{
				double time = GetTimeFloat();
				ticFraction = (time - static_cast<int>(time));
			}
			inter = static_cast<double>(curState->Tics - curTics - ticFraction) / static_cast<double>(curState->Tics);

			// [BB] For some actors (e.g. ZPoisonShroom) spr->actor->tics can be bigger than curState->Tics.
			// In this case inter is negative and we need to set it to zero.
			if (inter < 0.)
				inter = 0.;
			else
			{
				// [BB] Workaround for actors that use the same frame twice in a row.
				// Most of the standard Doom monsters do this in their see state.
				if ((smf->flags & MDL_INTERPOLATEDOUBLEDFRAMES))
				{
					const FState *prevState = curState - 1;
					if ((curState->sprite == prevState->sprite) && (curState->Frame == prevState->Frame))
					{
						inter /= 2.;
						inter += 0.5;
					}
					if ((curState->sprite == nextState->sprite) && (curState->Frame == nextState->Frame))
					{
						inter /= 2.;
						nextState = nextState->GetNextState();
					}
				}
				if (inter != 0.0)
					smfNext = gl_FindModelFrame(ti, nextState->sprite, nextState->Frame, false);
			}
		}
	}

	for (int i = 0; i<MAX_MODELS_PER_FRAME; i++)
	{
		if (smf->modelIDs[i] != -1)
		{
			FModel * mdl = Models[smf->modelIDs[i]];
			FTexture *tex = smf->skinIDs[i].isValid() ? TexMan(smf->skinIDs[i]) : nullptr;
			mdl->BuildVertexBuffer(this);
			SetVertexBuffer(mdl->mVBuf);

			mdl->PushSpriteMDLFrame(smf, i);

			if (smfNext && smf->modelframes[i] != smfNext->modelframes[i])
				mdl->RenderFrame(this, tex, smf->modelframes[i], smfNext->modelframes[i], inter, translation);
			else
				mdl->RenderFrame(this, tex, smf->modelframes[i], smf->modelframes[i], 0.f, translation);

			ResetVertexBuffer();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////

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
	const char * const texname = texlump < 0 ? fn : Wads.GetLumpFullName(texlump);
	return TexMan.CheckForTexture(texname, ETextureType::Any, FTextureManager::TEXMAN_TryAny);
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
				smf.modelIDs[0] = smf.modelIDs[1] = smf.modelIDs[2] = smf.modelIDs[3] = -1;
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
					else if (sc.Compare("dontcullbackfaces"))
					{
						smf.flags |= MDL_DONTCULLBACKFACES;
					}
					else if (sc.Compare("userotationcenter"))
					{
						smf.flags |= MDL_USEROTATIONCENTER;
						smf.rotationCenterX = 0.;
						smf.rotationCenterY = 0.;
						smf.rotationCenterZ = 0.;
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

