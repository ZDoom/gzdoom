#ifndef __S_SNDSEQ_H__
#define __S_SNDSEQ_H__

#include <stddef.h>
#include "actor.h"
#include "s_sound.h"
#include "r_defs.h"

typedef enum {
	SEQ_PLATFORM,
	SEQ_DOOR,
	SEQ_ENVIRONMENT,
	SEQ_NUMSEQTYPES,
	SEQ_NOTRANS
} seqtype_t;

struct sector_t;

void S_ParseSndSeq (void);
void SN_StartSequence (AActor *mobj, int sequence, seqtype_t type);
void SN_StartSequence (AActor *mobj, const char *name);
void SN_StartSequence (sector_t *sector, int sequence, seqtype_t type);
void SN_StartSequence (sector_t *sector, const char *name);
void SN_StartSequence (fixed_t spot[3], int sequence, seqtype_t type);
void SN_StartSequence (fixed_t spot[3], const char *name);
void SN_StopSequence (AActor *mobj);
void SN_StopSequence (sector_t *sector);
void SN_StopSequence (fixed_t spot[3]);
void SN_UpdateActiveSequences (void);
void SN_StopAllSequences (void);
ptrdiff_t SN_GetSequenceOffset (int sequence, unsigned int *sequencePtr);
void SN_ChangeNodeData (int nodeNum, int seqOffset, int delayTics,
	float volume, int currentSoundID);

class DSeqNode : public DObject
{
	DECLARE_CLASS (DSeqNode, DObject)
public:
	virtual ~DSeqNode ();
	void Serialize (FArchive &arc);
	virtual void MakeSound () {}
	virtual void MakeLoopedSound () {}
	virtual void *Source () { return NULL; }
	virtual bool IsPlaying () { return false; }
	void RunThink ();
	inline static DSeqNode *FirstSequence() { return SequenceListHead; }
	inline DSeqNode *NextSequence() const { return m_Next; }
	void ChangeData (int seqOffset, int delayTics, float volume, int currentSoundID);

	static void SerializeSequences (FArchive &arc);

protected:
	DSeqNode ();
	DSeqNode (int sequence);

	unsigned int *m_SequencePtr;
	int m_Sequence;

	int m_CurrentSoundID;
	int m_DelayTics;
	float m_Volume;
	int m_StopSound;
	int m_Atten;

private:
	static DSeqNode *SequenceListHead;
	DSeqNode *m_Next, *m_Prev;

	void ActivateSequence (int sequence);

	friend void SN_StopAllSequences (void);
};

typedef struct
{
	char			name[MAX_SNDNAME+1];
	int				stopsound;
	unsigned int	script[1];	// + more until end of sequence script
} sndseq_t;

void SN_StartSequence (AActor *mobj, int sequence, seqtype_t type);
void SN_StartSequence (AActor *mobj, const char *name);
void SN_StartSequence (sector_t *sector, int sequence, seqtype_t type);
void SN_StartSequence (sector_t *sector, const char *name);
void SN_StartSequence (polyobj_t *poly, int sequence, seqtype_t type);
void SN_StartSequence (polyobj_t *poly, const char *name);
void SN_StopSequence (AActor *mobj);
void SN_StopSequence (sector_t *sector);
void SN_StopSequence (polyobj_t *poly);
void SN_UpdateActiveSequences (void);
ptrdiff_t SN_GetSequenceOffset (int sequence, unsigned int *sequencePtr);
void SN_DoStop (void *);
void SN_ChangeNodeData (int nodeNum, int seqOffset, int delayTics,
	float volume, int currentSoundID);

extern sndseq_t **Sequences;
extern int ActiveSequences;
extern int NumSequences;

#endif //__S_SNDSEQ_H__