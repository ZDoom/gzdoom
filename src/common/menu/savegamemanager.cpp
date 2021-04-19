/*
** loadsavemenu.cpp
** The load game and save game menus
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010-2020 Christoph Oelckers
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

#include "menu.h"
#include "version.h"
#include "m_png.h"
#include "filesystem.h"
#include "v_text.h"
#include "gstrings.h"
#include "serializer.h"
#include "vm.h"
#include "i_system.h"
#include "v_video.h"
#include "findfile.h"
#include "v_draw.h"
#include "savegamemanager.h"



//=============================================================================
//
// Save data maintenance 
//
//=============================================================================

void FSavegameManagerBase::ClearSaveGames()
{
	for (unsigned i = 0; i<SaveGames.Size(); i++)
	{
		if (!SaveGames[i]->bNoDelete)
			delete SaveGames[i];
	}
	SaveGames.Clear();
}

FSavegameManagerBase::~FSavegameManagerBase()
{
	ClearSaveGames();
}

//=============================================================================
//
// Save data maintenance 
//
//=============================================================================

int FSavegameManagerBase::RemoveSaveSlot(int index)
{
	int listindex = SaveGames[0]->bNoDelete ? index - 1 : index;
	if (listindex < 0) return index;

	remove(SaveGames[index]->Filename.GetChars());
	UnloadSaveData();

	FSaveGameNode *file = SaveGames[index];

	if (quickSaveSlot == SaveGames[index])
	{
		quickSaveSlot = nullptr;
	}
	if (!file->bNoDelete) delete file;

	if (LastSaved == listindex) LastSaved = -1;
	else if (LastSaved > listindex) LastSaved--;
	if (LastAccessed == listindex) LastAccessed = -1;
	else if (LastAccessed > listindex) LastAccessed--;

	SaveGames.Delete(index);
	if ((unsigned)index >= SaveGames.Size()) index--;
	ExtractSaveData(index);
	return index;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, RemoveSaveSlot)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(sel);
	ACTION_RETURN_INT(self->RemoveSaveSlot(sel));
}

//=============================================================================
//
//
//
//=============================================================================

int FSavegameManagerBase::InsertSaveNode(FSaveGameNode *node)
{
	if (SaveGames.Size() == 0)
	{
		return SaveGames.Push(node);
	}

	if (node->bOldVersion)
	{ // Add node at bottom of list
		return SaveGames.Push(node);
	}
	else
	{	// Add node at top of list
		unsigned int i = 0;
		//if (SaveGames[0] == &NewSaveNode) i++; // To not insert above the "new savegame" dummy entry.
		for (; i < SaveGames.Size(); i++)
		{
			if (SaveGames[i]->bOldVersion || node->SaveTitle.CompareNoCase(SaveGames[i]->SaveTitle) <= 0)
			{
				break;
			}
		}
		SaveGames.Insert(i, node);
		return i;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManagerBase::NotifyNewSave(const FString &file, const FString &title, bool okForQuicksave, bool forceQuicksave)
{
	FSaveGameNode *node;

	if (file.IsEmpty())
		return;

	ReadSaveStrings();

	// See if the file is already in our list
	for (unsigned i = 0; i<SaveGames.Size(); i++)
	{
		FSaveGameNode *node = SaveGames[i];
#ifdef __unix__
		if (node->Filename.Compare(file) == 0)
#else
		if (node->Filename.CompareNoCase(file) == 0)
#endif
		{
			node->SaveTitle = title;
			node->bOldVersion = false;
			node->bMissingWads = false;
			if (okForQuicksave)
			{
				if (quickSaveSlot == nullptr || quickSaveSlot == (FSaveGameNode*)1 || forceQuicksave) quickSaveSlot = node;
				LastAccessed = LastSaved = i;
			}
			return;
		}
	}

	node = new FSaveGameNode;
	node->SaveTitle = title;
	node->Filename = file;
	node->bOldVersion = false;
	node->bMissingWads = false;
	int index = InsertSaveNode(node);

	if (okForQuicksave)
	{
		if (quickSaveSlot == nullptr || quickSaveSlot == (FSaveGameNode*)1 || forceQuicksave) quickSaveSlot = node;
		LastAccessed = LastSaved = index;
	}
	else
	{
		LastAccessed = ++LastSaved;
	}
}


//=============================================================================
//
// Loads the savegame
//
//=============================================================================

void FSavegameManagerBase::LoadSavegame(int Selected)
{
	PerformLoadGame(SaveGames[Selected]->Filename.GetChars(), true);
	if (quickSaveSlot == (FSaveGameNode*)1)
	{
		quickSaveSlot = SaveGames[Selected];
	}
	M_ClearMenus();
	LastAccessed = Selected;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, LoadSavegame)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(sel);
	self->LoadSavegame(sel);
	return 0;
}

//=============================================================================
//
// 
//
//=============================================================================

void FSavegameManagerBase::DoSave(int Selected, const char *savegamestring)
{
	RemoveNewSaveNode();
	if (Selected != 0)
	{
		auto node = SaveGames[Selected];
		PerformSaveGame(node->Filename.GetChars(), savegamestring);
	}
	else
	{
		// Find an unused filename and save as that
		FString filename;
		int i;

		for (i = 0;; ++i)
		{
			filename = BuildSaveName("save", i);
			if (!FileExists(filename))
			{
				break;
			}
		}
		PerformSaveGame(filename, savegamestring);
	}
	M_ClearMenus();
}

DEFINE_ACTION_FUNCTION(FSavegameManager, DoSave)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(sel);
	PARAM_STRING(name);
	self->DoSave(sel, name);
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

unsigned FSavegameManagerBase::ExtractSaveData(int index)
{
	FResourceFile *resf;
	FSaveGameNode *node;

	if (index == -1)
	{
		if (SaveGames.Size() > 0 && SaveGames[0]->bNoDelete)
		{
			index = LastSaved + 1;
		}
		else
		{
			index = LastAccessed < 0? 0 : LastAccessed;
		}
	}

	UnloadSaveData();

	if ((unsigned)index < SaveGames.Size() &&
		(node = SaveGames[index]) &&
		!node->Filename.IsEmpty() &&
		!node->bOldVersion &&
		(resf = FResourceFile::OpenResourceFile(node->Filename.GetChars(), true)) != nullptr)
	{
		FResourceLump *info = resf->FindLump("info.json");
		if (info == nullptr)
		{
			// this should not happen because the file has already been verified.
			return index;
		}

		void* data = info->Lock();
		FSerializer arc;
		if (!arc.OpenReader((const char*)data, info->LumpSize))
		{
			info->Unlock();
			return index;
		}
		info->Unlock();

		SaveCommentString = ExtractSaveComment(arc);

		FResourceLump *pic = resf->FindLump("savepic.png");
		if (pic != nullptr)
		{
			FileReader picreader;

			picreader.OpenMemoryArray([=](TArray<uint8_t> &array)
			{
				auto cache = pic->Lock();
				array.Resize(pic->LumpSize);
				memcpy(&array[0], cache, pic->LumpSize);
				pic->Unlock();
				return true;
			});
			PNGHandle *png = M_VerifyPNG(picreader);
			if (png != nullptr)
			{
				SavePic = PNGTexture_CreateFromFile(png, node->Filename);
				delete png;
				if (SavePic && SavePic->GetDisplayWidth() == 1 && SavePic->GetDisplayHeight() == 1)
				{
					delete SavePic;
					SavePic = nullptr;
				}
			}
		}
		delete resf;
	}
	return index;
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManagerBase::UnloadSaveData()
{
	if (SavePic != nullptr)
	{
		delete SavePic;
	}

	SaveCommentString = "";
	SavePic = nullptr;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, UnloadSaveData)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	self->UnloadSaveData();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManagerBase::ClearSaveStuff()
{
	UnloadSaveData();
	if (quickSaveSlot == (FSaveGameNode*)1)
	{
		quickSaveSlot = nullptr;
	}
}

DEFINE_ACTION_FUNCTION(FSavegameManager, ClearSaveStuff)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	self->ClearSaveStuff();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

bool FSavegameManagerBase::DrawSavePic(int x, int y, int w, int h)
{
	if (SavePic == nullptr) return false;
	DrawTexture(twod, SavePic, x, y, 	DTA_DestWidth, w, DTA_DestHeight, h, DTA_Masked, false,	TAG_DONE);
	return true;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, DrawSavePic)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(x);
	PARAM_INT(y);
	PARAM_INT(w);
	PARAM_INT(h);
	ACTION_RETURN_BOOL(self->DrawSavePic(x, y, w, h));
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManagerBase::SetFileInfo(int Selected)
{
	if (!SaveGames[Selected]->Filename.IsEmpty())
	{
		SaveCommentString.Format("File on disk:\n%s", SaveGames[Selected]->Filename.GetChars());
	}
}

DEFINE_ACTION_FUNCTION(FSavegameManager, SetFileInfo)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(i);
	self->SetFileInfo(i);
	return 0;
}


//=============================================================================
//
//
//
//=============================================================================

unsigned FSavegameManagerBase::SavegameCount()
{
	return SaveGames.Size();
}

DEFINE_ACTION_FUNCTION(FSavegameManager, SavegameCount)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	ACTION_RETURN_INT(self->SavegameCount());
}

//=============================================================================
//
//
//
//=============================================================================

FSaveGameNode *FSavegameManagerBase::GetSavegame(int i)
{
	return SaveGames[i];
}

DEFINE_ACTION_FUNCTION(FSavegameManager, GetSavegame)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(i);
	ACTION_RETURN_POINTER(self->GetSavegame(i));
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManagerBase::InsertNewSaveNode()
{
	NewSaveNode.SaveTitle = GStrings("NEWSAVE");
	NewSaveNode.bNoDelete = true;
	SaveGames.Insert(0, &NewSaveNode);
}

DEFINE_ACTION_FUNCTION(FSavegameManager, InsertNewSaveNode)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	self->InsertNewSaveNode();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

bool FSavegameManagerBase::RemoveNewSaveNode()
{
	if (SaveGames[0] == &NewSaveNode)
	{
		SaveGames.Delete(0);
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, RemoveNewSaveNode)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	ACTION_RETURN_INT(self->RemoveNewSaveNode());
}


DEFINE_ACTION_FUNCTION(FSavegameManager, ReadSaveStrings)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	self->ReadSaveStrings();
	return 0;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, ExtractSaveData)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManagerBase);
	PARAM_INT(sel);
	ACTION_RETURN_INT(self->ExtractSaveData(sel));
}


DEFINE_FIELD(FSaveGameNode, SaveTitle);
DEFINE_FIELD(FSaveGameNode, Filename);
DEFINE_FIELD(FSaveGameNode, bOldVersion);
DEFINE_FIELD(FSaveGameNode, bMissingWads);
DEFINE_FIELD(FSaveGameNode, bNoDelete);

DEFINE_FIELD_X(SavegameManager, FSavegameManagerBase, WindowSize);
DEFINE_FIELD_X(SavegameManager, FSavegameManagerBase, quickSaveSlot);
DEFINE_FIELD_X(SavegameManager, FSavegameManagerBase, SaveCommentString);

