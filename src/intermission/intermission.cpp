/*
** intermission.cpp
** Framework for intermissions (text screens, slideshows, etc)
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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
#include "d_event.h"
#include "w_wad.h"
#include "intermission/intermission.h"

FIntermissionDescriptorList IntermissionDescriptors;

IMPLEMENT_CLASS(DIntermissionScreen)
IMPLEMENT_CLASS(DIntermissionScreenFader)
IMPLEMENT_CLASS(DIntermissionScreenText)
IMPLEMENT_CLASS(DIntermissionScreenCast)
IMPLEMENT_CLASS(DIntermissionScreenScroller)
IMPLEMENT_CLASS(DIntermissionController)

void DIntermissionScreen::Init(FIntermissionDescriptor *desc)
{
}

bool DIntermissionScreen::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreen::Ticker ()
{
}

void DIntermissionScreen::Drawer ()
{
}

void DIntermissionScreenFader::Init(FIntermissionDescriptor *desc)
{
}

bool DIntermissionScreenFader::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenFader::Ticker ()
{
}

void DIntermissionScreenFader::Drawer ()
{
}

void DIntermissionScreenText::Init(FIntermissionDescriptor *desc)
{
}

bool DIntermissionScreenText::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenText::Ticker ()
{
}

void DIntermissionScreenText::Drawer ()
{
}

void DIntermissionScreenCast::Init(FIntermissionDescriptor *desc)
{
}

bool DIntermissionScreenCast::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenCast::Ticker ()
{
}

void DIntermissionScreenCast::Drawer ()
{
}

void DIntermissionScreenScroller::Init(FIntermissionDescriptor *desc)
{
}

bool DIntermissionScreenScroller::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenScroller::Ticker ()
{
}

void DIntermissionScreenScroller::Drawer ()
{
}




static FIntermissionDescriptor DefaultIntermission;


// Called by main loop.
bool F_Responder (event_t* ev) { return true; }

// Called by main loop.
void F_Ticker () {}

// Called by main loop.
void F_Drawer () {}




#if 0
//==========================================================================
//
// GetFinaleText
//
//==========================================================================

static FString GetFinaleText (const char *msgLumpName)
{
	int msgLump = Wads.CheckNumForFullName(msgLumpName, true);
	if (msgLump != -1)
	{
		return Wads.ReadLump(msgLump).GetString();
	}
	else
	{
		return FString("Unknown message ") + msgLumpName;
	}
}

// starts a cluster transition. This will create a proper intermission object from the passed data
static void CreateTextScreenDescriptor (const char *music, int musicorder, int cdtrack, unsigned int cdid, 
										const char *flat, INTBOOL finalePic, 
										const char *text, INTBOOL textInLump, INTBOOL lookupText)
{
	FIntermissionDescriptor *desc = &DefaultIntermission;
	desc->Reset();
	desc->mTagList.Clear();

	desc->mTagList.Push(ITAG_Class);
	desc->mTagList.Push(1);
	desc->mTagList.Push(DWORD(FName)("IntermissionTextScreen");


	const char *FinaleFlat = (flat != NULL && *flat != 0) ? flat : gameinfo.finaleFlat;
	if (FinaleFlat != NULL)
	{
		if (FinaleFlat[0] == '$')
		{
			FinaleFlat = GStrings(FinaleFlat + 1);
		}
		FTextureID texid = TexMan.CheckForTexture(FinaleFlat, FTexture::TEX_MiscPatch);
		desc->mTagList.Push(finalePic? ITAG_DrawImage : ITAG_FillFlat);
		desc->mTagList.Push(1);
		desc->mTagList.Push(texid.GetIndex);
	}

	if (cdtrack != 0)
	{
		desc->mTagList.Push(ITAG_Cdmusic);
		desc->mTagList.Push(2);
		desc->mTagList.Push(cdtrack);
		desc->mTagList.Push(cdid);
	}
	if (music == NULL) 
	{
		music = gameinfo.finaleMusic;
		musicorder = 0;
	}
	int taglen = 2 + strlen(music)/4;
	int pos = mTagList.Reserve(taglen+2);
	mTagList[pos] = ITAG_Music;
	mTagList[pos+1] = taglen;
	mTagList[pos+2] = musicorder;
	strcpy(char*)&mTagList[pos+3], music);

	FString FinaleText;
	if (textInLump)
	{
		FinaleText = GetFinaleText (text);
	}
	else
	{
		const char *from = (text != NULL) ? text : "Empty message";
		FinaleText = from;
		if (lookupText)
		{
			FinaleText.Insert(0, "$");
		}
	}

	taglen = 2 + FinaleText.Len()/4;
	pos = mTagList.Reserve(taglen+2);
	mTagList[pos] = ITAG_Textscreen_Text;
	mTagList[pos+1] = taglen;
	mTagList[pos+2] = musicorder;
	strcpy(char*)&mTagList[pos+3], FinaleText.GetChars());

	desc->mTagList.Push(ITAG_Done);
	desc->mTagList.Push(0);
	if (ending)
	{
		// todo: copy the endsequence screens
	}
	else
	{
	}


}

void F_StartFinale (const char *music, int musicorder, int cdtrack, unsigned int cdid, const char *flat, 
					const char *text, INTBOOL textInLump, INTBOOL finalePic, INTBOOL lookupText, 
					bool ending, int endsequence)
{
	bool loopmusic = ending ? !gameinfo.noloopfinalemusic : true;
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;

	// Okay - IWAD dependend stuff.
	// This has been changed severely, and some stuff might have changed in the process.
	//
	// [RH] More flexible now (even more severe changes)
	//  FinaleFlat, FinaleText, and music are now determined in G_WorldDone() based on
	//	data in a level_info_t and a cluster_info_t.

	if (cdtrack == 0 || !S_ChangeCDMusic (cdtrack, cdid))
	{
		if (music == NULL)
		{
			S_ChangeMusic (gameinfo.finaleMusic, 0, loopmusic);
		}
		else
		{
 			S_ChangeMusic (music, musicorder, loopmusic);
		}
	}



	FinaleStage = 0;
	V_SetBlend (0,0,0,0);
	TEXTSPEED = 2;

	FinaleHasPic = !!finalePic;
	FinaleCount = 0;
	FinaleEndCount = 70;
	FadeDir = -1;
	FinaleEnding = ending;
	FinaleSequence = endsequence;

	S_StopAllChannels ();

	if (ending)
	{
		if (EndSequences[FinaleSequence].EndType == END_Chess)
		{
			TEXTSPEED = 3;	// Slow the text to its original rate to match the music.
			S_ChangeMusic ("hall", 0, loopmusic);
			FinaleStage = 10;
			GetFinaleText ("win1msg");
			V_SetBlend (0,0,0,256);
		}
		else if (EndSequences[FinaleSequence].EndType == END_Strife)
		{
			if (players[0].mo->FindInventory (QuestItemClasses[24]) ||
				players[0].mo->FindInventory (QuestItemClasses[27]))
			{
				FinalePart = 10;
			}
			else
			{
				FinalePart = 17;
			}
			FinaleStage = 5;
			FinaleEndCount = 0;
		}
	}
}
#endif

void F_StartFinale (const char *music, int musicorder, int cdtrack, unsigned int cdid, const char *flat, 
					const char *text, INTBOOL textInLump, INTBOOL finalePic, INTBOOL lookupText, 
					bool ending, int endsequence)
{}

/*

//==========================================================================
//
//
//==========================================================================

int FindEndSequence (int type, const char *picname)
{
	unsigned int i, num;

	num = EndSequences.Size ();
	for (i = 0; i < num; i++)
	{
		if (EndSequences[i].EndType == type && !EndSequences[i].Advanced &&
			(type != END_Pic || stricmp (EndSequences[i].PicName, picname) == 0))
		{
			return (int)i;
		}
	}
	return -1;
}

//==========================================================================
//
//
//==========================================================================

static void SetEndSequence (char *nextmap, int type)
{
	int seqnum;

	seqnum = FindEndSequence (type, NULL);
	if (seqnum == -1)
	{
		EndSequence newseq;
		newseq.EndType = type;
		seqnum = (int)EndSequences.Push (newseq);
	}
	mysnprintf(nextmap, 11, "enDSeQ%04x", (WORD)seqnum);
}

//==========================================================================
//
//
//==========================================================================

void G_SetForEndGame (char *nextmap)
{
	if (!strncmp(nextmap, "enDSeQ",6)) return;	// If there is already an end sequence please leave it alone!!!

	if (gameinfo.gametype == GAME_Strife)
	{
		SetEndSequence (nextmap, gameinfo.flags & GI_SHAREWARE ? END_BuyStrife : END_Strife);
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		SetEndSequence (nextmap, END_Chess);
	}
	else if (gameinfo.gametype == GAME_Doom && (gameinfo.flags & GI_MAPxx))
	{
		SetEndSequence (nextmap, END_Cast);
	}
	else
	{ // The ExMx games actually have different ends based on the episode,
	  // but I want to keep this simple.
		SetEndSequence (nextmap, END_Pic1);
	}
}
*/

