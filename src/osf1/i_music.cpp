#include <stdio.h>

#include "doomtype.h"
#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "z_zone.h"
#include "i_system.h"

#ifndef NOMIDAS
#include "../midas/include/midasdll.h"
#endif	// !NOMIDAS

extern int _nosound;

class MusInfo
{
public:
    MusInfo () { m_Status = STATE_Stopped; }
    virtual ~MusInfo () {}
    virtual void SetVolume (float volume) = 0;
    virtual void Play (bool looping) = 0;
    virtual void Pause () = 0;
    virtual void Resume () = 0;
    virtual void Stop () = 0;
    virtual bool IsPlaying () = 0;
    virtual bool IsMIDI () = 0;
    virtual bool IsValid () = 0;

    enum EState
    {
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
    } m_Status;
    bool m_Looping;
};

class MIDISong : public MusInfo
{
public:
    MIDISong (void *data, int len) {}
    ~MIDISong () {}
    void SetVolume (float volume) {}
    void Play (bool looping) {}
    void Pause () {}
    void Resume () {}
    void Stop () {}
    bool IsPlaying () { return false; }
    bool IsMIDI () { return true; }
    bool IsValid () { return false; }

protected:
    MIDISong () {}
    virtual bool IsMUS () { return false; }
};

class MUSSong : public MIDISong
{
public:
    MUSSong (void *data, int len) : MIDISong (data, len) {}
protected:
    bool IsMUS () { return true; }
};

#ifndef NOMIDAS
class MODSong : public MusInfo
{
public:
    MODSong (void *data, int len);
    ~MODSong ();
    void SetVolume (float volume);
    void Play (bool looping);
    void Pause ();
    void Resume ();
    void Stop ();
    bool IsPlaying ();
    bool IsMIDI () { return false; }
    bool IsValid () { return m_Module != NULL; }

protected:
    MIDASmodulePlayHandle m_Handle;
    MIDASmodule m_Module;
};
#else
class MODSong : public MusInfo
{
public:
    MODSong (void *data, int len) {}
    ~MODSong () {}
    void SetVolume (float volume) {}
    void Play (bool looping) {}
    void Pause () {}
    void Resume () {}
    void Stop () {}
    bool IsPlaying () { return false; }
    bool IsMIDI () { return false; }
    bool IsValid () { return false; }
};
#endif	// NOMIDAS

static MusInfo *currSong;
static int      nomusic = 0;
static char		modName[512];
static int     	musicvolume;
static DWORD	midivolume;

void I_SetMIDIVolume (float volume)
{
	if (currSong && currSong->IsMIDI ())
	{
		currSong->SetVolume (volume);
	}
	else
	{
		DWORD wooba = (DWORD)(volume * 0xffff) & 0xffff;
		midivolume = (wooba << 16) | wooba;
	}
}

void I_SetMusicVolume (int volume)
{
 	if (volume != 127)
	{
		// Internal state variable.
		musicvolume = volume;
		// Now set volume on output device.
		if (currSong && !currSong->IsMIDI ())
			currSong->SetVolume ((float)volume / 64.0f);
	}
}

#ifndef NOMIDAS
void MODSong::SetVolume (float volume)
{
    MIDASsetMusicVolume (m_Handle, (int)(volume * 64));
}
#endif	// !NOMIDAS

BEGIN_COMMAND (snd_listmididevices)
{
    Printf (PRINT_HIGH, "No MIDI devices installed.\n");
}
END_COMMAND (snd_listmididevices)

void I_InitMusic (void)
{
    Printf (PRINT_HIGH, "I_InitMusic\n");

    nomusic = !!Args.CheckParm("-nomusic") || _nosound;

#ifndef NOMIDAS
    /* Create temporary file name for MIDAS */
    if (tmpnam (modName) == NULL)
    {
		const char *temp = getenv ("HOME");
		if (tempnam (temp, modName) == NULL)
			nomusic = true;
    }
#endif	// !NOMIDAS

    if (!nomusic)
		atterm (I_ShutdownMusic);
}

void STACK_ARGS I_ShutdownMusic(void)
{
    if (currSong)
    {
		I_UnRegisterSong ((long)currSong);
		currSong = NULL;
    }
    remove (modName);
}


void I_PlaySong (long handle, int _looping)
{
	MusInfo *info = (MusInfo *)handle;

	if (!info || nomusic)
		return;

	info->Stop ();
	info->Play (_looping ? true : false);
        
	if (info->m_Status == MusInfo::STATE_Playing)
		currSong = info;
	else
		currSong = NULL;
}

#ifndef NOMIDAS
void MODSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if ( (m_Handle = MIDASplayModule (m_Module, looping)) )
	{
		MIDASsetMusicVolume (m_Handle, musicvolume);
		m_Status = STATE_Playing;
	}
}
#endif	// NOMIDAS

void I_PauseSong (long handle)
{
    MusInfo *info = (MusInfo *)handle;

    if (info)
		info->Pause ();
}

#ifndef NOMIDAS
void MODSong::Pause ()
{
    if (m_Status == STATE_Playing)
    {
		// FIXME
    }
}
#endif	// NOMIDAS

void I_ResumeSong (long handle)
{
    MusInfo *info = (MusInfo *)handle;

    if (info)
		info->Resume ();
}

#ifndef NOMIDAS
void MODSong::Resume ()
{
    if (m_Status == STATE_Paused)
    {
		// FIXME
    }
}
#endif	// NOMIDAS

void I_StopSong (long handle)
{
    MusInfo *info = (MusInfo *)handle;

    if (info)
		info->Stop ();

    if (info == currSong)
		currSong = NULL;
}

#ifndef NOMIDAS
void MODSong::Stop ()
{
    if (m_Status != STATE_Stopped)
    {
		m_Status = STATE_Stopped;
		MIDASstopModule (m_Handle);
		m_Handle = 0;
    }
}
#endif	// NOMIDAS

void I_UnRegisterSong (long handle)
{
    MusInfo *info = (MusInfo *)handle;

    if (info)
    {
		delete info;
    }
}

#ifndef NOMIDAS
MODSong::~MODSong ()
{
    Stop ();
    MIDASfreeModule (m_Module);
    m_Module = 0;
}
#endif	// NOMIDAS

int I_RegisterSong (void *data, int musicLen)
{
    MusInfo *info;

    if (*(int *)data == (('M')|(('U')<<8)|(('S')<<16)|((0x1a)<<24)))
    {
		// This is a mus file
		info = new MUSSong (data, musicLen);
    }
    else if (*(int *)data == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24)))
    {
		// This is a midi file
		info = new MIDISong (data, musicLen);
    }
    else if (!_nosound)	// no MIDAS => no MODs
    {
		// This is probably a module
		info = new MODSong (data, musicLen);
    }

    if (info && !info->IsValid ())
    {
		delete info;
		info = NULL;
    }

    return info ? (long)info : 0;
}

#ifndef NOMIDAS
MODSong::MODSong (void *data, int len)
{
    m_Handle = 0;
    m_Module = NULL;

    FILE *f;

    if ((f = fopen (modName, "wb")) == NULL)
    {
		Printf (PRINT_HIGH, "Unable to open temporary music file %s\n", modName);
		return;
    }
    if (fwrite (data, len, 1, f) != 1)
    {
		fclose (f);
		remove (modName);
		Printf (PRINT_HIGH, "Unable to write temporary music file\n");
		return;
    }
    fclose (f);

    if ((m_Module = MIDASloadModule (modName)) == NULL)
    {
		// This is not any known music format
		// (or could not load mod)
		Printf (PRINT_HIGH, "MIDAS failed to load song:\n%s\n",
				MIDASgetErrorMessage (MIDASgetLastError()));
    }
    remove (modName);
}
#endif	// !NOMIDAS

// Is the song playing?
bool I_QrySongPlaying (long handle)
{
    MusInfo *info = (MusInfo *)handle;

    return info ? info->IsPlaying () : false;
}

#ifndef NOMIDAS
bool MODSong::IsPlaying ()
{
    return m_Looping ? true : m_Status != STATE_Stopped;
}
#endif	// !NOMIDAS
