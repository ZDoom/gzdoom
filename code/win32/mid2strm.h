#ifndef __MID2STRM_H__
#define __MID2STRM_H__

#define CB_STREAMBUF	(32768)					// Size of each stream buffer

// MIDI file constants
//
#define MThd			0x6468544D				// Start of file
#define MTrk			0x6B72544D				// Start of track

#define MIDI_SYSEX		((BYTE)0xF0)			// SysEx begin
#define MIDI_SYSEXEND	((BYTE)0xF7)			// SysEx end
#define MIDI_META		((BYTE)0xFF)			// Meta event begin
#define MIDI_META_TEMPO	((BYTE)0x51)
#define MIDI_META_EOT	((BYTE)0x2F)			// End-of-track

#define MIDI_NOTEOFF	((BYTE)0x80)			// + note + velocity
#define MIDI_NOTEON		((BYTE)0x90)			// + note + velocity
#define MIDI_POLYPRESS  ((BYTE)0xA0)			// + pressure (2 bytes)
#define MIDI_CTRLCHANGE ((BYTE)0xB0)			// + ctrlr + value
#define MIDI_PRGMCHANGE	((BYTE)0xC0)			// + new patch
#define MIDI_CHANPRESS	((BYTE)0xD0)			// + pressure (1 byte)
#define MIDI_PITCHBEND	((BYTE)0xE0)			// + pitch bend (2 bytes)

#define MIDS_SHORTMSG	(0x00000000)
#define MIDS_TEMPO		(0x01000000)

// Description of a stream buffer on the output side
//
typedef struct STREAMBUF *PSTREAMBUF;
typedef struct STREAMBUF
{
	LPBYTE			pBuffer;			// -> Start of actual buffer
	DWORD			tkStart;			// Tick time just before first event
	LPBYTE			pbNextEvent;		// Where to write next event
	DWORD			cbLeft; 			// bytes left in buffer
	PSTREAMBUF		pNext;				// Next buffer
	MIDIHDR			midiHeader;
	BOOL			prepared;
} STREAMBUF;

// Description of output stream open for write
//
typedef struct
{
	DWORD			tkTrack;			// Current tick position in track	
	PSTREAMBUF		pFirst;				// First stream buffer
	PSTREAMBUF		pLast;				// Last (current) stream buffer
} OUTSTREAMSTATE;

// One event we're reading or writing to a track
//
typedef struct
{
	DWORD			tkEvent;			// Absolute time of event
	BYTE			abEvent[4];			// Event type and parameters if channel msg
	DWORD			cbEvent;			// Of data which follows if meta or sysex
	LPBYTE			pEvent;				// -> Event data if applicable
} MEVENT;	


extern DWORD midTimeDiv;
extern byte UsedPatches[256];

PSTREAMBUF	mid2strmConvert (BYTE *inFile, DWORD inSize);
void		mid2strmCleanup (void);
LPBYTE		GetOutStreamBytes (DWORD tkNow, DWORD cbNeeded);
BOOL		AddEventToStream (MEVENT *pMe);

#endif //__MID2STRM_H__