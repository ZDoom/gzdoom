#ifndef __MUS2STRM_H__
#define __MUS2STRM_H__

#include "mid2strm.h"

#define MUS_RELEASE		0
#define MUS_PLAY		1
#define MUS_PITCHBEND	2
#define MUS_SYSEVENT	3
#define MUS_CTRLCHANGE	4
#define MUS_UNKNOWN1	5
#define MUS_END			6
#define MUS_UNKNOWN2	7	

#define MUSMAGIC		"MUS\032"

typedef unsigned short int2;

typedef struct
{
	char		ID[4];			/* identifier "MUS" 0x1A */
	int2		ScoreLength;
	int2		ScoreStart;
	int2		channels; 		/* count of primary channels */
	int2		SecChannels;	/* count of secondary channels (?) */
	int2		InstrCnt;
} MUSheader;

struct Track
{
	unsigned long	 current;
	char			 vel;
	long			 DeltaTime;
	unsigned char	 LastEvent;
	char			*data;		/* Primary data */
};

extern OUTSTREAMSTATE ots;


PSTREAMBUF	mus2strmConvert (BYTE *inFile, DWORD inSize);
void		mus2strmCleanup (void);


#endif //__MUS2STRM_H__