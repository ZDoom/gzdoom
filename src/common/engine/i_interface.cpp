#include "i_interface.h"
#include "st_start.h"
#include "gamestate.h"
#include "startupinfo.h"
#include "c_cvars.h"
#include "gstrings.h"
#include "version.h"
#include "m_argv.h"
#include "m_random.h"

static_assert(sizeof(void*) == 8,
	"Only LP64/LLP64 builds are officially supported. "
	"Please do not attempt to build for other platforms; "
	"even if the program succeeds in a MAP01 smoke test, "
	"there are e.g. known visual artifacts "
	"<https://forum.zdoom.org/viewtopic.php?f=7&t=75673> "
	"that lead to a bad user experience.");

// Some global engine variables taken out of the backend code.
FStartupScreen* StartWindow;
SystemCallbacks sysCallbacks;
FString endoomName;
bool batchrun;
float menuBlurAmount;

bool AppActive = true;
int chatmodeon;
gamestate_t 	gamestate = GS_STARTUP;
bool ToggleFullscreen;
int 			paused;
bool			pauseext;

FStartupInfo GameStartupInfo;

CVAR(Bool, vid_fps, false, 0)
CVAR(Bool, queryiwad, QUERYIWADDEFAULT, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, saveargs, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, savenetfile, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, savenetargs, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultiwad, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultargs, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetiwad, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetargs, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetplayers, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnethostport, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetticdup, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetmode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetgamemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, defaultnetaltdm, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetaddress, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetjoinport, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetpage, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnethostteam, 255, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, defaultnetjointeam, 255, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, defaultnetextratic, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(String, defaultnetsavefile, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Bool, ui_generic)
EXTERN_CVAR(Int, vid_preferbackend)
EXTERN_CVAR(Bool, vid_fullscreen)

CUSTOM_CVAR(String, language, "auto", CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
{
	GStrings.UpdateLanguage(self);
	UpdateGenericUI(ui_generic);
	if (sysCallbacks.LanguageChanged) sysCallbacks.LanguageChanged(self);
}

// Some of this info has to be passed and managed from the front end since it's game-engine specific.
FStartupSelectionInfo::FStartupSelectionInfo(const TArray<WadStuff>& wads, FArgs& args, int startFlags) : Wads(&wads), Args(&args), DefaultStartFlags(startFlags)
{
	DefaultQueryIWAD = queryiwad;
	DefaultLanguage = language;
	DefaultBackend = vid_preferbackend;
	DefaultFullscreen = vid_fullscreen;

	if (defaultiwad[0] != '\0')
	{
		for (int i = 0; i < wads.SSize(); ++i)
		{
			if (!wads[i].Name.CompareNoCase(defaultiwad))
			{
				DefaultIWAD = i;
				break;
			}
		}
	}
	DefaultArgs = defaultargs;
	bSaveArgs = saveargs;

	if (defaultnetiwad[0] != '\0')
	{
		for (int i = 0; i < wads.SSize(); ++i)
		{
			if (!wads[i].Name.CompareNoCase(defaultnetiwad))
			{
				DefaultNetIWAD = i;
				break;
			}
		}
	}
	DefaultNetArgs = defaultnetargs;
	DefaultNetPage = defaultnetpage;
	DefaultNetSaveFile = defaultnetsavefile;
	bSaveNetFile = savenetfile;
	bSaveNetArgs = savenetargs;

	DefaultNetPlayers = defaultnetplayers;
	DefaultNetHostPort = defaultnethostport;
	DefaultNetTicDup = defaultnetticdup;
	DefaultNetMode = defaultnetmode;
	DefaultNetGameMode = defaultnetgamemode;
	DefaultNetAltDM = defaultnetaltdm;
	DefaultNetHostTeam = defaultnethostteam;
	DefaultNetExtraTic = defaultnetextratic;

	DefaultNetAddress = defaultnetaddress;
	DefaultNetJoinPort = defaultnetjoinport;
	DefaultNetJoinTeam = defaultnetjointeam;
}

// Return whatever IWAD the user selected.
int FStartupSelectionInfo::SaveInfo()
{
	DefaultLanguage.StripLeftRight();

	DefaultArgs.StripLeftRight();

	DefaultNetArgs.StripLeftRight();
	AdditionalNetArgs.StripLeftRight();
	DefaultNetAddress.StripLeftRight();
	DefaultNetSaveFile.StripLeftRight();

	queryiwad = DefaultQueryIWAD;
	language = DefaultLanguage.GetChars();
	vid_fullscreen = DefaultFullscreen;
	if (DefaultBackend != vid_preferbackend)
		vid_preferbackend = DefaultBackend;

	if (bNetStart)
	{
		savenetfile = bSaveNetFile;
		savenetargs = bSaveNetArgs;

		defaultnetiwad = (*Wads)[DefaultNetIWAD].Name.GetChars();
		defaultnetpage = DefaultNetPage;
		defaultnetsavefile = savenetfile ? DefaultNetSaveFile.GetChars() : "";
		defaultnetargs = savenetargs ? DefaultNetArgs.GetChars() : "";

		if (bHosting)
		{
			defaultnetplayers = DefaultNetPlayers;
			defaultnethostport = DefaultNetHostPort;
			defaultnetticdup = DefaultNetTicDup;
			defaultnetmode = DefaultNetMode;
			defaultnetgamemode = DefaultNetGameMode;
			defaultnetaltdm = DefaultNetAltDM;
			defaultnethostteam = DefaultNetHostTeam;
			defaultnetextratic = DefaultNetExtraTic;
		}
		else
		{
			defaultnetaddress = DefaultNetAddress.GetChars();
			defaultnetjoinport = DefaultNetJoinPort;
			defaultnetjointeam = DefaultNetJoinTeam;
		}

		if (!DefaultNetArgs.IsEmpty())
			Args->AppendArgsString(DefaultNetArgs);
		if (!AdditionalNetArgs.IsEmpty())
			Args->AppendArgsString(AdditionalNetArgs);

		return DefaultNetIWAD;
	}

	defaultiwad = (*Wads)[DefaultIWAD].Name.GetChars();
	saveargs = bSaveArgs;
	defaultargs = saveargs ? DefaultArgs.GetChars() : "";

	if (!DefaultArgs.IsEmpty())
		Args->AppendArgsString(DefaultArgs);

	return DefaultIWAD;
}

FString GameUUID;
static FRandom pr_uuid("GameUUID");

FString GenerateUUID()
{
	FString uuid;
	uuid.AppendFormat("%08X-%04X-4%03X-9%03X-%08X%04X", pr_uuid.GenRand32(), pr_uuid(UINT16_MAX), pr_uuid(4095), pr_uuid(4095), pr_uuid.GenRand32(), pr_uuid(UINT16_MAX));
	return uuid;
}
