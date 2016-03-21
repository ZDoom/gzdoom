/*
** t_fspic.cpp
** Fragglescript HUD pics (incomplete and untested!)
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

#include "t_script.h"
#include "doomtype.h"
#include "p_local.h"
#include "farchive.h"
#include "sbar.h"
#include "v_video.h"



struct FHudPic
{
	FTextureID       texturenum;
	int       xpos;
	int       ypos;
	bool   draw;

	void Serialize(FArchive & arc)
	{
		arc << xpos << ypos << draw << texturenum;
	}

};

//======================================================================
//
//======================================================================
class DHUDPicManager : public DHUDMessage
{
	// This is no real hudmessage but this way I don't need any external code to handle this
	// because the hudmessage and thinker code handles everything automatically
	DECLARE_CLASS(DHUDPicManager, DHUDMessage)
	float basetrans;

public:

	TArray<FHudPic> piclist;

	DHUDPicManager();
	~DHUDPicManager() {}
	void Serialize(FArchive & ar);
	virtual void DoDraw (int linenum, int x, int y, int hudheight, float translucent);
	void DoDraw (int, int, int, bool, int) { assert(false); }
} ;

IMPLEMENT_CLASS(DHUDPicManager)

//======================================================================
//
//======================================================================
DHUDPicManager::DHUDPicManager()
{
	HUDWidth=HUDHeight=0;
	basetrans=0.8f;
	//SetID(0xffffffff);
	NumLines=1;
	HoldTics=0;	// stay forever!
	//logtoconsole=false;
}


//======================================================================
//
//======================================================================
void DHUDPicManager::Serialize(FArchive & ar)
{
	Super::Serialize(ar);
	
	short count=piclist.Size();
	ar << count << basetrans;
	if (ar.IsLoading()) piclist.Resize(count);
	for(int i=0;i<count;i++) piclist[i].Serialize(ar);
}


//======================================================================
//
//======================================================================
void DHUDPicManager::DoDraw (int linenum, int x, int y, int hudheight, float translucent)
{
	for(unsigned int i=0; i<piclist.Size();i++) if (piclist[i].texturenum.isValid() && piclist[i].draw)
	{
		FTexture * tex = TexMan[piclist[i].texturenum];
		if (tex) screen->DrawTexture(tex, piclist[i].xpos, piclist[i].ypos, DTA_320x200, true, 
										DTA_AlphaF, translucent*basetrans, TAG_DONE);
	}
}


//======================================================================
//
//======================================================================
static TArray<FHudPic> & GetPicList()
{
	//TThinkerIterator<DHUDPicManager> it;
	DHUDPicManager * pm=NULL;//it.Next();

	if (!pm) pm=new DHUDPicManager;
	return pm->piclist;
}

//======================================================================
//
// External interface
//
//======================================================================

//======================================================================
//
//======================================================================
int HU_GetFSPic(FTextureID texturenum, int xpos, int ypos)
{
	TArray<FHudPic> &piclist=GetPicList();
	unsigned int i;

	for(i=0;i<piclist.Size();i++) if (piclist[i].texturenum.isValid()) continue;
	if (i==piclist.Size()) i=piclist.Reserve(1);

	FHudPic * pic=&piclist[i];

    piclist[i].texturenum = texturenum;
    piclist[i].xpos = xpos;
    piclist[i].ypos = ypos;
    piclist[i].draw = false;
    return i;
}


//======================================================================
//
//======================================================================
int HU_DeleteFSPic(unsigned handle)
{
	TArray<FHudPic> &piclist=GetPicList();
	
	if(handle >= piclist.Size()) return -1;
	piclist[handle].texturenum.SetInvalid();
	return 0;
}


//======================================================================
//
//======================================================================
int HU_ModifyFSPic(unsigned handle, FTextureID texturenum, int xpos, int ypos)
{
	TArray<FHudPic> &piclist=GetPicList();
	
	if(handle >= piclist.Size()) return -1;
	if(!piclist[handle].texturenum.isValid()) return -1;
	
	piclist[handle].texturenum = texturenum;
	piclist[handle].xpos = xpos;
	piclist[handle].ypos = ypos;
	return 0;
}

//======================================================================
//
//======================================================================
int HU_FSDisplay(unsigned handle, bool newval)
{
	TArray<FHudPic> &piclist=GetPicList();
	
	if(handle >= piclist.Size()) return -1;
	if(!piclist[handle].texturenum.isValid()) return -1;

	piclist[handle].draw = newval;
	return 0;
}

