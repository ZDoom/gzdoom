
#include <mutex>
#include <string>
#include "c_cvars.h"
#include "i_sound.h"
#include "i_music.h"
#include "s_sound.h"
#include "files.h"
#include "zmusic/midiconfig.h"
#include "zmusic/mididefs.h"

#include "zmusic/..//mididevices/mididevice.h"	// this is still needed...

void I_InitMusicWin32 ();

class MIDISource;
class MIDIDevice;
class OPLmusicFile;


// Data interface

// Module played via foo_dumb -----------------------------------------------

class StreamSource;

// stream song ------------------------------------------

MusInfo *OpenStreamSong(StreamSource *source);
const char *GME_CheckFormat(uint32_t header);
MusInfo* CDDA_OpenSong(MusicIO::FileInterface* reader);
MusInfo* CD_OpenSong(int track, int id);
MusInfo* CreateMIDIStreamer(MIDISource *source, EMidiDevice devtype, const char* args);
void MIDIDumpWave(MIDISource* source, EMidiDevice devtype, const char* devarg, const char* outname, int subsong, int samplerate);

// --------------------------------------------------------------------------

extern MusInfo *currSong;
void MIDIDeviceChanged(int newdev, bool force = false);

EXTERN_CVAR (Float, snd_mastervolume)
EXTERN_CVAR (Float, snd_musicvolume)
