#ifndef __MID2STRM_H__
#define __MID2STRM_H__

#include "mus2midi.h"

#define CB_STREAMBUF	(32768)					// Size of each stream buffer

// MIDI file constants
//
#define MThd			0x6468544D				// Start of file
#define MTrk			0x6B72544D				// Start of track

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
BOOL		AddEventToStream (MEVENT *pMe);

#endif //__MID2STRM_H__
