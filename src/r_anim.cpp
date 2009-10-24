/*
** r_anim.cpp
** Routines for handling texture animation.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "doomtype.h"
#include "cmdlib.h"
#include "i_system.h"
#include "r_local.h"
#include "r_sky.h"
#include "m_random.h"
#include "d_player.h"
#include "p_spec.h"
#include "sc_man.h"
#include "templates.h"
#include "w_wad.h"
#include "g_level.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

//
// Animating textures and planes
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//

struct FAnimDef
{
	FTextureID 	BasePic;
	WORD	NumFrames;
	WORD	CurFrame;
	BYTE	AnimType;
	DWORD	SwitchTime;			// Time to advance to next frame
	struct FAnimFrame
	{
		DWORD	SpeedMin;		// Speeds are in ms, not tics
		DWORD	SpeedRange;
		FTextureID	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
		ANIM_DiscreteFrames
	};

	void SetSwitchTime (DWORD mstime);
};

// This is an array of pointers to animation definitions.
// When it is destroyed, it deletes any animations it points to as well.
class AnimArray : public TArray<FAnimDef *>
{
public:
	~AnimArray();
	void AddAnim (FAnimDef *anim);
	void FixAnimations ();
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void R_InitAnimDefs ();
static void R_AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames);
static void ParseAnim (FScanner &sc, bool istex);
static void ParseRangeAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing);
static void ParsePicAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames);
static FTextureID  ParseFramenum (FScanner &sc, FTextureID basepicnum, int usetype, bool allowMissing);
static void ParseTime (FScanner &sc, DWORD &min, DWORD &max);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static AnimArray Anims;
static FRandom pr_animatepictures ("AnimatePics");

// CODE --------------------------------------------------------------------

//==========================================================================
//
// R_InitPicAnims
//
// [description copied from BOOM]
// Load the table of animation definitions, checking for existence of
// the start and end of each frame. If the start doesn't exist the sequence
// is skipped, if the last doesn't exist, BOOM exits.
//
// Wall/Flat animation sequences, defined by name of first and last frame,
// The full animation sequence is given using all lumps between the start
// and end entry, in the order found in the WAD file.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called ANIMATED rather than a static table in this module to
// allow wad designers to insert or modify animation sequences.
//
// Lump format is an array of byte packed animdef_t structures, terminated
// by a structure with istexture == -1. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// [RH] Rewritten to support BOOM ANIMATED lump but also make absolutely
//		no assumptions about how the compiler packs the animdefs array.
//
//==========================================================================

CVAR(Bool, debuganimated, false, 0)

void R_InitPicAnims (void)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable;
		// I think better not! This is only for old ANIMATED definition that
		// don't know about ZDoom's more flexible texture system.
		// | FTextureManager::TEXMAN_TryAny;

	if (Wads.CheckNumForName ("ANIMATED") != -1)
	{
		FMemLump animatedlump = Wads.ReadLump ("ANIMATED");
		const char *animdefs = (const char *)animatedlump.GetMem();
		const char *anim_p;
		FTextureID pic1, pic2;
		int animtype;
		DWORD animspeed;

		// Init animation
		animtype = FAnimDef::ANIM_Forward;

		for (anim_p = animdefs; *anim_p != -1; anim_p += 23)
		{
			if (*anim_p /* .istexture */ & 1)
			{
				// different episode ?
				if (!(pic1 = TexMan.CheckForTexture (anim_p + 10 /* .startname */, FTexture::TEX_Wall, texflags)).Exists() ||
					!(pic2 = TexMan.CheckForTexture (anim_p + 1 /* .endname */, FTexture::TEX_Wall, texflags)).Exists())
					continue;		

				// [RH] Bit 1 set means allow decals on walls with this texture
				TexMan[pic2]->bNoDecals = TexMan[pic1]->bNoDecals = !(*anim_p & 2);
			}
			else
			{
				if (!(pic1 = TexMan.CheckForTexture (anim_p + 10 /* .startname */, FTexture::TEX_Flat, texflags)).Exists() ||
					!(pic2 = TexMan.CheckForTexture (anim_p + 1 /* .startname */, FTexture::TEX_Flat, texflags)).Exists())
					continue;
			}
			if (pic1 == pic2)
			{
				// This animation only has one frame. Skip it. (Doom aborted instead.)
				Printf ("Animation %s in ANIMATED has only one frame", anim_p + 10);
				continue;
			}

			FTexture *tex1 = TexMan[pic1];
			FTexture *tex2 = TexMan[pic2];

			if (tex1->UseType != tex2->UseType)
			{
				// not the same type - 
				continue;
			}

			if (debuganimated)
			{
				Printf("Defining animation '%s' (texture %d, lump %d, file %d) to '%s' (texture %d, lump %d, file %d)\n",
					tex1->Name, pic1.GetIndex(), tex1->GetSourceLump(), Wads.GetLumpFile(tex1->GetSourceLump()),
					tex2->Name, pic2.GetIndex(), tex2->GetSourceLump(), Wads.GetLumpFile(tex2->GetSourceLump()));
			}

			// [RH] Allow for backward animations as well as forward.
			if (pic1 > pic2)
			{
				swap (pic1, pic2);
				animtype = FAnimDef::ANIM_Backward;
			}

			// Speed is stored as tics, but we want ms so scale accordingly.
			animspeed = /* .speed */
				Scale ((BYTE(anim_p[19]) << 0) |
					   (BYTE(anim_p[20]) << 8) |
					   (BYTE(anim_p[21]) << 16) |
					   (BYTE(anim_p[22]) << 24), 1000, 35);

			R_AddSimpleAnim (pic1, pic2 - pic1 + 1, animtype, animspeed);
		}
	}
	// [RH] Load any ANIMDEFS lumps
	R_InitAnimDefs ();
	Anims.FixAnimations ();
}

//==========================================================================
//
// R_AddSimpleAnim
//
// Creates an animation with simple characteristics. This is used for
// original Doom (non-ANIMDEFS-style) animations and Build animations.
//
//==========================================================================

void R_AddSimpleAnim (FTextureID picnum, int animcount, int animtype, DWORD speedmin, DWORD speedrange)
{
	if (TexMan.AreTexturesCompatible(picnum, picnum + (animcount - 1)))
	{
		FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef));
		anim->CurFrame = 0;
		anim->BasePic = picnum;
		anim->NumFrames = animcount;
		anim->AnimType = animtype;
		anim->SwitchTime = 0;
		anim->Frames[0].SpeedMin = speedmin;
		anim->Frames[0].SpeedRange = speedrange;
		anim->Frames[0].FramePic = anim->BasePic;
		Anims.AddAnim (anim);
	}
}

//==========================================================================
//
// R_AddComplexAnim
//
// Creates an animation with individually defined frames.
//
//==========================================================================

static void R_AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames)
{
	FAnimDef *anim = (FAnimDef *)M_Malloc (sizeof(FAnimDef) + (frames.Size()-1) * sizeof(frames[0]));
	anim->BasePic = picnum;
	anim->NumFrames = frames.Size();
	anim->CurFrame = 0;
	anim->AnimType = FAnimDef::ANIM_DiscreteFrames;
	anim->SwitchTime = 0;
	memcpy (&anim->Frames[0], &frames[0], frames.Size() * sizeof(frames[0]));
	Anims.AddAnim (anim);
}

//==========================================================================
//
// R_InitAnimDefs
//
// This uses a Hexen ANIMDEFS lump to define the animation sequences
//
//==========================================================================

static void R_InitAnimDefs ()
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	int lump, lastlump = 0;
	
	while ((lump = Wads.FindLump ("ANIMDEFS", &lastlump)) != -1)
	{
		FScanner sc(lump);

		while (sc.GetString ())
		{
			if (sc.Compare ("flat"))
			{
				ParseAnim (sc, false);
			}
			else if (sc.Compare ("texture"))
			{
				ParseAnim (sc, true);
			}
			else if (sc.Compare ("switch"))
			{
				P_ProcessSwitchDef (sc);
			}
			// [GRB] Added warping type 2
			else if (sc.Compare ("warp") || sc.Compare ("warp2"))
			{
				bool isflat = false;
				bool type2 = sc.Compare ("warp2");	// [GRB]
				sc.MustGetString ();
				if (sc.Compare ("flat"))
				{
					isflat = true;
					sc.MustGetString ();
				}
				else if (sc.Compare ("texture"))
				{
					isflat = false;
					sc.MustGetString ();
				}
				else
				{
					sc.ScriptError (NULL);
				}
				FTextureID picnum = TexMan.CheckForTexture (sc.String, isflat ? FTexture::TEX_Flat : FTexture::TEX_Wall, texflags);
				if (picnum.isValid())
				{
					FTexture * warper = TexMan[picnum];

					// don't warp a texture more than once
					if (!warper->bWarped)
					{
						if (type2)	// [GRB]
							warper = new FWarp2Texture (warper);
						else
							warper = new FWarpTexture (warper);
						TexMan.ReplaceTexture (picnum, warper, false);
					}

					if (sc.CheckFloat())
					{
						static_cast<FWarpTexture*>(warper)->SetSpeed(float(sc.Float));
					}

					// No decals on warping textures, by default.
					// Warping information is taken from the last warp 
					// definition for this texture.
					warper->bNoDecals = true;
					if (sc.GetString ())
					{
						if (sc.Compare ("allowdecals"))
						{
							warper->bNoDecals = false;
						}
						else
						{
							sc.UnGet ();
						}
					}
				}
			}
			else if (sc.Compare ("cameratexture"))
			{
				int width, height;
				int fitwidth, fitheight;
				FString picname;

				sc.MustGetString ();
				picname = sc.String;
				sc.MustGetNumber ();
				width = sc.Number;
				sc.MustGetNumber ();
				height = sc.Number;
				FTextureID picnum = TexMan.CheckForTexture (picname, FTexture::TEX_Flat, texflags);
				FTexture *viewer = new FCanvasTexture (picname, width, height);
				if (picnum.Exists())
				{
					FTexture *oldtex = TexMan[picnum];
					fitwidth = oldtex->GetScaledWidth ();
					fitheight = oldtex->GetScaledHeight ();
					viewer->UseType = oldtex->UseType;
					TexMan.ReplaceTexture (picnum, viewer, true);
				}
				else
				{
					fitwidth = width;
					fitheight = height;
					// [GRB] No need for oldtex
					viewer->UseType = FTexture::TEX_Wall;
					TexMan.AddTexture (viewer);
				}
				if (sc.GetString())
				{
					if (sc.Compare ("fit"))
					{
						sc.MustGetNumber ();
						fitwidth = sc.Number;
						sc.MustGetNumber ();
						fitheight = sc.Number;
					}
					else
					{
						sc.UnGet ();
					}
				}
				viewer->SetScaledSize(fitwidth, fitheight);
			}
			else if (sc.Compare ("animatedDoor"))
			{
				P_ParseAnimatedDoor (sc);
			}
			else
			{
				sc.ScriptError (NULL);
			}
		}
	}
}

//==========================================================================
//
// ParseAnim
//
// Parse a single animation definition out of an ANIMDEFS lump and
// create the corresponding animation structure.
//
//==========================================================================

static void ParseAnim (FScanner &sc, bool istex)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	TArray<FAnimDef::FAnimFrame> frames (32);
	FTextureID picnum;
	int usetype;
	int defined = 0;
	bool optional = false, missing = false;

	usetype = istex ? FTexture::TEX_Wall : FTexture::TEX_Flat;

	sc.MustGetString ();
	if (sc.Compare ("optional"))
	{
		optional = true;
		sc.MustGetString ();
	}
	picnum = TexMan.CheckForTexture (sc.String, usetype, texflags);

	if (!picnum.Exists())
	{
		if (optional)
		{
			missing = true;
		}
		else
		{
			Printf (PRINT_BOLD, "ANIMDEFS: Can't find %s\n", sc.String);
		}
	}

	// no decals on animating textures, by default
	if (picnum.isValid())
	{
		TexMan[picnum]->bNoDecals = true;
	}

	while (sc.GetString ())
	{
		if (sc.Compare ("allowdecals"))
		{
			if (picnum.isValid())
			{
				TexMan[picnum]->bNoDecals = false;
			}
			continue;
		}
		else if (sc.Compare ("range"))
		{
			if (defined == 2)
			{
				sc.ScriptError ("You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			if (defined == 1)
			{
				sc.ScriptError ("You can only use one \"range\" per animation.");
			}
			defined = 1;
			ParseRangeAnim (sc, picnum, usetype, missing);
		}
		else if (sc.Compare ("pic"))
		{
			if (defined == 1)
			{
				sc.ScriptError ("You cannot use \"pic\" and \"range\" together in a single animation.");
			}
			defined = 2;
			ParsePicAnim (sc, picnum, usetype, missing, frames);
		}
		else
		{
			sc.UnGet ();
			break;
		}
	}

	// If base pic is not present, don't add this anim
	// ParseRangeAnim adds the anim itself, but ParsePicAnim does not.
	if (picnum.isValid() && defined == 2)
	{
		if (frames.Size() < 2)
		{
			sc.ScriptError ("Animation needs at least 2 frames");
		}
		R_AddComplexAnim (picnum, frames);
	}
}

//==========================================================================
//
// ParseRangeAnim
//
// Parse an animation defined using "range". Not that one range entry is
// enough to define a complete animation, unlike "pic".
//
//==========================================================================

static void ParseRangeAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing)
{
	int type;
	FTextureID framenum;
	DWORD min, max;

	type = FAnimDef::ANIM_Forward;
	framenum = ParseFramenum (sc, picnum, usetype, missing);
	ParseTime (sc, min, max);

	if (framenum == picnum || !picnum.Exists())
	{
		return;		// Animation is only one frame or does not exist
	}
	if (framenum < picnum)
	{
		type = FAnimDef::ANIM_Backward;
		TexMan[framenum]->bNoDecals = TexMan[picnum]->bNoDecals;
		swap (framenum, picnum);
	}
	if (sc.GetString())
	{
		if (sc.Compare ("Oscillate"))
		{
			type = type == FAnimDef::ANIM_Forward ? FAnimDef::ANIM_OscillateUp : FAnimDef::ANIM_OscillateDown;
		}
		else
		{
			sc.UnGet ();
		}
	}
	R_AddSimpleAnim (picnum, framenum - picnum + 1, type, min, max - min);
}

//==========================================================================
//
// ParsePicAnim
//
// Parse a single frame from ANIMDEFS defined using "pic".
//
//==========================================================================

static void ParsePicAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames)
{
	FTextureID framenum;
	DWORD min, max;

	framenum = ParseFramenum (sc, picnum, usetype, missing);
	ParseTime (sc, min, max);

	if (picnum.isValid())
	{
		FAnimDef::FAnimFrame frame;

		frame.SpeedMin = min;
		frame.SpeedRange = max - min;
		frame.FramePic = framenum;
		frames.Push (frame);
	}
}

//==========================================================================
//
// ParseFramenum
//
// Reads a frame's texture from ANIMDEFS. It can either be an integral
// offset from basepicnum or a specific texture name.
//
//==========================================================================

static FTextureID ParseFramenum (FScanner &sc, FTextureID basepicnum, int usetype, bool allowMissing)
{
	const BITFIELD texflags = FTextureManager::TEXMAN_Overridable | FTextureManager::TEXMAN_TryAny;
	FTextureID framenum;

	sc.MustGetString ();
	if (IsNum (sc.String))
	{
		framenum = basepicnum + (atoi(sc.String) - 1);
	}
	else
	{
		framenum = TexMan.CheckForTexture (sc.String, usetype, texflags);
		if (!framenum.Exists() && !allowMissing)
		{
			sc.ScriptError ("Unknown texture %s", sc.String);
		}
	}
	return framenum;
}

//==========================================================================
//
// ParseTime
//
// Reads a tics or rand time definition from ANIMDEFS.
//
//==========================================================================

static void ParseTime (FScanner &sc, DWORD &min, DWORD &max)
{
	sc.MustGetString ();
	if (sc.Compare ("tics"))
	{
		sc.MustGetFloat ();
		min = max = DWORD(sc.Float * 1000 / 35);
	}
	else if (sc.Compare ("rand"))
	{
		sc.MustGetFloat ();
		min = DWORD(sc.Float * 1000 / 35);
		sc.MustGetFloat ();
		max = DWORD(sc.Float * 1000 / 35);
	}
	else
	{
		sc.ScriptError ("Must specify a duration for animation frame");
	}
}

//==========================================================================
//
// AnimArray :: ~AnimArray
//
// Frees all animations held in this array before freeing the array.
//
//==========================================================================

AnimArray::~AnimArray()
{
	for (unsigned i = 0; i < Size(); i++)
	{
		if ((*this)[i] != NULL)
		{
			M_Free ((*this)[i]);
			(*this)[i] = NULL;
		}
	}
}

//==========================================================================
//
// AnimArray :: AddAnim
//
// Adds a new animation to the array. If one with the same basepic as the
// new one already exists, it is replaced.
//
//==========================================================================

void AnimArray::AddAnim (FAnimDef *anim)
{
	// Search for existing duplicate.
	for (unsigned int i = 0; i < Anims.Size(); ++i)
	{
		if ((*this)[i]->BasePic == anim->BasePic)
		{
			// Found one!
			free ((*this)[i]);
			(*this)[i] = anim;
			return;
		}
	}
	// Didn't find one, so add it at the end.
	Push (anim);
}

//==========================================================================
//
// AnimArray :: FixAnimations
//
// Copy the "front sky" flag from an animated texture to the rest
// of the textures in the animation, and make every texture in an
// animation range use the same setting for bNoDecals.
//
//==========================================================================

void AnimArray::FixAnimations ()
{
	unsigned int i;
	int j;

	for (i = 0; i < Size(); ++i)
	{
		FAnimDef *anim = operator[] (i);
		if (anim->AnimType == FAnimDef::ANIM_DiscreteFrames)
		{
			if (TexMan[anim->BasePic]->bNoRemap0)
			{
				for (j = 0; j < anim->NumFrames; ++j)
				{
					TexMan[anim->Frames[j].FramePic]->SetFrontSkyLayer ();
				}
			}
		}
		else
		{
			bool nodecals;
			bool noremap = false;
			const char *name;

			name = TexMan[anim->BasePic]->Name;
			nodecals = TexMan[anim->BasePic]->bNoDecals;
			for (j = 0; j < anim->NumFrames; ++j)
			{
				FTexture *tex = TexMan[anim->BasePic + j];
				noremap |= tex->bNoRemap0;
				tex->bNoDecals = nodecals;
			}
			if (noremap)
			{
				for (j = 0; j < anim->NumFrames; ++j)
				{
					TexMan[anim->BasePic + j]->SetFrontSkyLayer ();
				}
			}
		}
	}
}

//==========================================================================
//
// FAnimDef :: SetSwitchTime
//
// Determines when to switch to the next frame.
//
//==========================================================================

void FAnimDef::SetSwitchTime (DWORD mstime)
{
	int speedframe = (AnimType == FAnimDef::ANIM_DiscreteFrames) ? CurFrame : 0;

	SwitchTime = mstime + Frames[speedframe].SpeedMin;
	if (Frames[speedframe].SpeedRange != 0)
	{
		SwitchTime += pr_animatepictures(Frames[speedframe].SpeedRange);
	}
}

//==========================================================================
//
// R_UpdateAnimations
//
// Updates texture translations for each animation and scrolls the skies.
//
//==========================================================================

void R_UpdateAnimations (DWORD mstime)
{
	for (unsigned int j = 0; j < Anims.Size(); ++j)
	{
		FAnimDef *anim = Anims[j];

		// If this is the first time through R_UpdateAnimations, just
		// initialize the anim's switch time without actually animating.
		if (anim->SwitchTime == 0)
		{
			anim->SetSwitchTime (mstime);
		}
		else while (anim->SwitchTime <= mstime)
		{ // Multiple frames may have passed since the last time calling
		  // R_UpdateAnimations, so be sure to loop through them all.

			switch (anim->AnimType)
			{
			default:
			case FAnimDef::ANIM_Forward:
			case FAnimDef::ANIM_DiscreteFrames:
				anim->CurFrame = (anim->CurFrame + 1) % anim->NumFrames;
				break;

			case FAnimDef::ANIM_Backward:
				if (anim->CurFrame == 0)
				{
					anim->CurFrame = anim->NumFrames - 1;
				}
				else
				{
					anim->CurFrame -= 1;
				}
				break;

			case FAnimDef::ANIM_OscillateUp:
				anim->CurFrame = anim->CurFrame + 1;
				if (anim->CurFrame >= anim->NumFrames - 1)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateDown;
				}
				break;

			case FAnimDef::ANIM_OscillateDown:
				anim->CurFrame = anim->CurFrame - 1;
				if (anim->CurFrame == 0)
				{
					anim->AnimType = FAnimDef::ANIM_OscillateUp;
				}
				break;
			}
			anim->SetSwitchTime (mstime);
		}

		if (anim->AnimType == FAnimDef::ANIM_DiscreteFrames)
		{
			TexMan.SetTranslation (anim->BasePic, anim->Frames[anim->CurFrame].FramePic);
		}
		else
		{
			for (unsigned int i = 0; i < anim->NumFrames; i++)
			{
				TexMan.SetTranslation (anim->BasePic + i, anim->BasePic + (i + anim->CurFrame) % anim->NumFrames);
			}
		}
	}

	// Scroll the sky
	double ms = (double)mstime * FRACUNIT;
	sky1pos = ms * level.skyspeed1;
	sky2pos = ms * level.skyspeed2;
}
