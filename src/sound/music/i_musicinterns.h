
#include <stdint.h>
#include <mutex>
#include <string>
#include "zmusic/zmusic.h"

class MIDISource;
class MIDIDevice;
class OPLmusicFile;
class StreamSource;
class MusInfo;

MusInfo *OpenStreamSong(StreamSource *source);
const char *GME_CheckFormat(uint32_t header);
MusInfo* CDDA_OpenSong(MusicIO::FileInterface* reader);
MusInfo* CD_OpenSong(int track, int id);
MusInfo* CreateMIDIStreamer(MIDISource *source, EMidiDevice devtype, const char* args);

// Registers a song handle to song data.

MusInfo *I_RegisterSong (MusicIO::FileInterface *reader, EMidiDevice device, const char *Args);
MusInfo *I_RegisterCDSong (int track, int cdid = 0);

void TimidityPP_Shutdown();
extern "C" void dumb_exit();

inline void ZMusic_Shutdown()
{
	// free static data in the backends.
	TimidityPP_Shutdown();
	dumb_exit();
}
