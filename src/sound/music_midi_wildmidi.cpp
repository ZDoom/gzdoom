#include "i_musicinterns.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "templates.h"
#include "version.h"


CVAR(String, wildmidi_config, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static FString currentConfig;

// added because Timidity's output is rather loud.
CUSTOM_CVAR (Float, wildmidi_mastervolume, 1.0f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
}

CUSTOM_CVAR (Int, wildmidi_frequency, 44100, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to Timidity's limits
	if (self < 11000)
		self = 11000;
	else if (self > 65000)
		self = 65000;
}

//==========================================================================
//
// WildMidiMIDIDevice Constructor
//
//==========================================================================

WildMidiMIDIDevice::WildMidiMIDIDevice()
{
	mMidi = NULL;
	mLoop = false;
}

//==========================================================================
//
// WildMidiMIDIDevice Destructor
//
//==========================================================================

WildMidiMIDIDevice::~WildMidiMIDIDevice ()
{
	if (mMidi != NULL) WildMidi_Close(mMidi);
	// do not shut down the device so that it can be reused for the next song being played.
}

//==========================================================================
//
// WildMidiMIDIDevice :: Preprocess
//
//==========================================================================

bool WildMidiMIDIDevice::Preprocess(MIDIStreamer *song, bool looping)
{
	TArray<BYTE> midi;

	// Write MIDI song to temporary file
	song->CreateSMF(midi, looping ? 0 : 1);

	mMidi = WildMidi_OpenBuffer(&midi[0], midi.Size());
	if (mMidi == NULL)
	{
		Printf(PRINT_BOLD, "Could not open temp music file\n");
	}
	mLoop = looping;
	return false;
}

//==========================================================================
//
// WildMidiMIDIDevice :: Open
//
//==========================================================================

int WildMidiMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	if (currentConfig.CompareNoCase(wildmidi_config) != 0)
	{
		if (currentConfig.IsNotEmpty()) WildMidi_Shutdown();
		currentConfig = "";
		if (!WildMidi_Init(wildmidi_config, wildmidi_frequency, WM_MO_ENHANCED_RESAMPLING))
		{
			currentConfig = wildmidi_config;
		}
		else
		{
			return 1;
		}
	}

	Stream = GSnd->CreateStream(FillStream, 32 * 1024, 0, wildmidi_frequency, this);
	if (Stream == NULL)
	{
		Printf(PRINT_BOLD, "Could not create music stream.\n");
		return 1;
	}

	return 0;
}


//==========================================================================
//
// WildMidiMIDIDevice :: FillStream
//
//==========================================================================

bool WildMidiMIDIDevice::FillStream(SoundStream *stream, void *buff, int len, void *userdata)
{
	char *buffer = (char*)buff;
	WildMidiMIDIDevice *song = (WildMidiMIDIDevice *)userdata;
	if (song->mMidi != NULL)
	{
		while (len > 0)
		{
			int written = WildMidi_GetOutput(song->mMidi, buffer, len);
			if (written < 0)
			{
				// error
				memset(buffer, 0, len);
				return false;
			}
			buffer += written;
			len -= written;

			if (len > 0)
			{
				if (!song->mLoop)
				{
					memset(buffer, 0, len);
					return written > 0;
				}
				else
				{
					// loop the sound (i.e. go back to start.)
					unsigned long spos = 0;
					WildMidi_FastSeek(song->mMidi, &spos);
				}

			}
		}
	}
	
	return true;
}

//==========================================================================
//
// WildMidiMIDIDevice :: IsOpen
//
//==========================================================================

bool WildMidiMIDIDevice::IsOpen() const
{
	return mMidi != NULL;
}

