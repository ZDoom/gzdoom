#ifndef __S_SNDSEQ_H__
#define __S_SNDSEQ_H__

#include <stddef.h>
#include "p_mobj.h"
#include "s_sound.h"

typedef enum {
	SEQ_PLATFORM,
	SEQ_DOOR,
	SEQ_ENVIRONMENT,
	SEQ_NUMSEQTYPES,
	SEQ_NOTRANS
} seqtype_t;

void S_ParseSndSeq (void);
void SN_StartSequence (mobj_t *mobj, int sequence, seqtype_t type);
void SN_StartSequenceName (mobj_t *mobj, const char *name);
void SN_StopSequence (mobj_t *mobj);
void SN_UpdateActiveSequences (void);
void SN_StopAllSequences (void);
ptrdiff_t SN_GetSequenceOffset (int sequence, unsigned int *sequencePtr);
void SN_ChangeNodeData (int nodeNum, int seqOffset, int delayTics,
	float volume, int currentSoundID);

typedef struct seqnode_s seqnode_t;
struct seqnode_s
{
	unsigned int *sequencePtr;
	int	sequence;
	mobj_t *mobj;
	int currentSoundID;
	int delayTics;
	float volume;
	int stopSound;
	int atten;
	seqnode_t *next, *prev;
};

typedef struct {
	char			name[MAX_SNDNAME+1];
	int				stopsound;
	unsigned int	script[1];	// + more until end of sequence script
} sndseq_t;

extern sndseq_t **Sequences;
extern int ActiveSequences;
extern int NumSequences;
extern seqnode_t *SequenceListHead;

#endif //__S_SNDSEQ_H__