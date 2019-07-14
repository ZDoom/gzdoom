
class SeqNode native
{
	enum ESeqType 
	{
		PLATFORM,
		DOOR,
		ENVIRONMENT,
		NUMSEQTYPES,
		NOTRANS
	};

	native bool AreModesSameID(int sequence, int type, int mode1);
	native bool AreModesSame(Name name, int mode1);
	native Name GetSequenceName();
	native void AddChoice (int seqnum, int type);
	native static Name GetSequenceSlot (int sequence, int type);
	native static void MarkPrecacheSounds(int sequence, int type);
}

