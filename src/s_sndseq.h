#ifndef __S_SNDSEQ_H__
#define __S_SNDSEQ_H__

#include <stddef.h>
#include "dobject.h"
#include "s_sound.h"

typedef enum {
	SEQ_PLATFORM,
	SEQ_DOOR,
	SEQ_ENVIRONMENT,
	SEQ_NUMSEQTYPES,
	SEQ_NOTRANS
} seqtype_t;

struct sector_t;

class DSeqNode : public DObject
{
	DECLARE_CLASS (DSeqNode, DObject)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);
	void StopAndDestroy ();
	void Destroy ();
	void Tick ();
	void ChangeData (int seqOffset, int delayTics, float volume, FSoundID currentSoundID);
	void AddChoice (int seqnum, seqtype_t type);
	int GetModeNum() const { return m_ModeNum; }
	FName GetSequenceName() const;
	static void StaticMarkHead() { GC::Mark(SequenceListHead); }

	virtual void MakeSound (int loop, FSoundID id) {}
	virtual void *Source () { return NULL; }
	virtual bool IsPlaying () { return false; }
	virtual DSeqNode *SpawnChild (int seqnum) { return NULL; }

	inline static DSeqNode *FirstSequence() { return SequenceListHead; }
	inline DSeqNode *NextSequence() const { return m_Next; }

	static void SerializeSequences (FArchive &arc);

protected:
	DSeqNode ();
	DSeqNode (int sequence, int modenum);

	SDWORD *m_SequencePtr;
	int m_Sequence;

	FSoundID m_CurrentSoundID;
	int m_StopSound;
	int m_DelayUntilTic;
	float m_Volume;
	float m_Atten;
	int m_ModeNum;

	TArray<int> m_SequenceChoices;
	TObjPtr<DSeqNode> m_ChildSeqNode;
	TObjPtr<DSeqNode> m_ParentSeqNode;

private:
	static DSeqNode *SequenceListHead;
	DSeqNode *m_Next, *m_Prev;

	void ActivateSequence (int sequence);

	friend void SN_StopAllSequences (void);
};

void SN_StopAllSequences (void);

struct FSoundSequence
{
	FName	 SeqName;
	FName	 Slot;
	FSoundID StopSound;
	SDWORD	 Script[1];	// + more until end of sequence script
};

void S_ParseSndSeq (int levellump);
DSeqNode *SN_StartSequence (AActor *mobj, int sequence, seqtype_t type, int modenum, bool nostop=false);
DSeqNode *SN_StartSequence (AActor *mobj, const char *name, int modenum);
DSeqNode *SN_StartSequence (AActor *mobj, FName seqname, int modenum);
DSeqNode *SN_StartSequence (sector_t *sector, int chan, int sequence, seqtype_t type, int modenum, bool nostop=false);
DSeqNode *SN_StartSequence (sector_t *sector, int chan, const char *name, int modenum);
DSeqNode *SN_StartSequence (sector_t *sec, int chan, FName seqname, int modenum);
DSeqNode *SN_StartSequence (FPolyObj *poly, int sequence, seqtype_t type, int modenum, bool nostop=false);
DSeqNode *SN_StartSequence (FPolyObj *poly, const char *name, int modenum);
DSeqNode *SN_CheckSequence (sector_t *sector, int chan);
void SN_StopSequence (AActor *mobj);
void SN_StopSequence (sector_t *sector, int chan);
void SN_StopSequence (FPolyObj *poly);
bool SN_AreModesSame(int sequence, seqtype_t type, int mode1, int mode2);
bool SN_AreModesSame(const char *name, int mode1, int mode2);
void SN_UpdateActiveSequences (void);
ptrdiff_t SN_GetSequenceOffset (int sequence, SDWORD *sequencePtr);
void SN_DoStop (void *);
void SN_ChangeNodeData (int nodeNum, int seqOffset, int delayTics,
	float volume, int currentSoundID);
FName SN_GetSequenceSlot (int sequence, seqtype_t type);
void SN_MarkPrecacheSounds (int sequence, seqtype_t type);
bool SN_IsMakingLoopingSound (sector_t *sector);

#endif //__S_SNDSEQ_H__
