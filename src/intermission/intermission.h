#ifndef __INTERMISSION_H
#define __INTERMISSION_H

#include "dobject.h"

struct event_t;

enum
{
	// For all tags the format is:

	// 1 DWORD: tag value
	// 1 DWORD: size of this tag including all parameters
	// n DWORDs: parameters. Strings are DWORD aligned

	// common
	ITAG_Class,						// parameter: 1 name describing the type of screen
	ITAG_Done,						// parameter: 1 string describing the next level. If empty the screen will stay.
	ITAG_Music,						// parameter: 1 string + 1 int describing the music (+order) to be played
	ITAG_Cdmusic,					// parameter: 1 int cdtrack + 1 int cdid
	ITAG_DrawImage,					// parameter: 1 texture ID, x and y coordinates

	// text screens
	ITAG_Textscreen_Text,			// parameter: 1 string
	ITAG_Textscreen_Textlump,		// parameter: 1 int (lump index) Only used when Text not defined.

	// slide show slide
	ITAG_Slide_Sound,				// parameter: 1 sound ID of sound to be played when starting the screen
	ITAG_Slide_Delay,				// parameter: 1 int (seconds this screen remains, cannot be skipped. -1 means to wait for the sound to end.

	// scroller
	ITAG_Scroll_Direction,			// parameter: 1 int (0=up, 1 = down, 2 = left, 3 = right)
	ITAG_Scroll_Speed,				// parameter: 1 int (scroll speed in tics per frame

	// cast call
	ITAG_Cast_NewActor,				// parameter: 1 FName
	ITAG_Cast_State,				// parameter: 0-terminated list of FNames describing the state label
	ITAG_Cast_Sound,				// parameter: 1 sound ID
};

class FIntermissionDescriptor
{
	TArray<DWORD> mTaglist;
	int mReadPosition;

public:
	void Reset();
	bool Advance();
	int GetTag();
	int GetTagValue();
	double GetTagFloat();
	const char *GetTagString();
};

typedef TMap<FName, FIntermissionDescriptor*> FIntermissionDescriptorList;

extern FIntermissionDescriptorList IntermissionDescriptors;

class DIntermissionScreen : public DObject
{
	DECLARE_CLASS (DIntermissionScreen, DObject)

public:

	DIntermissionScreen() {}
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
	virtual void Drawer () =0;
};

class DIntermissionController : public DObject
{
	DECLARE_CLASS (DIntermissionController, DObject)


public:
	static DIntermissionController *CurrentIntermission;

	DIntermissionController() {}
	bool Responder (event_t *ev);
	void Ticker ();
	void Drawer ();
	void Close();
};


// Interface for main loop
bool F_Responder (event_t* ev);
void F_Ticker ();
void F_Drawer ();

// Create an intermission from old cluster data
void F_StartFinale (const char *music, int musicorder, int cdtrack, unsigned int cdid, const char *flat, 
					const char *text, INTBOOL textInLump, INTBOOL finalePic, INTBOOL lookupText, 
					bool ending, int endsequence = 0);



#endif
