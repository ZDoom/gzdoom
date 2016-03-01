typedef WORD SFGenerator;

struct SFRange
{
	BYTE Lo;
	BYTE Hi;
};

struct SFPreset
{
	char Name[21];
	BYTE LoadOrder:7;
	BYTE bHasGlobalZone:1;
	WORD Program;
	WORD Bank;
	WORD BagIndex;
	/* Don't care about library, genre, and morphology */
};

struct SFBag
{
	WORD GenIndex;
//	WORD ModIndex;		// If I am feeling ambitious, I might add support for modulators some day.
	SFRange KeyRange;
	SFRange VelRange;
	int Target;			// Either an instrument or sample index
};

struct SFInst
{
	char Name[21];
	BYTE Pad:7;
	BYTE bHasGlobalZone:1;
	WORD BagIndex;
};

struct SFSample
{
	float *InMemoryData;
	DWORD Start;
	DWORD End;
	DWORD StartLoop;
	DWORD EndLoop;
	DWORD SampleRate;
	BYTE  OriginalPitch;
	SBYTE PitchCorrection;
	WORD  SampleLink;
	WORD  SampleType;
	char  Name[21];
};

// Sample type bit fields (all but ROM are mutually exclusive)
enum
{
	SFST_Mono = 1,
	SFST_Right = 2,
	SFST_Left = 4,
	SFST_Linked = 8,	/* SF2.04 defines this bit but not its function */
	SFST_Bad = 16384,	/* Used internally */
	SFST_ROM = 32768
};

// Generator definitions

struct SFGenList
{
	SFGenerator Oper;
	union
	{
		SFRange Range;
		SWORD Amount;
		WORD uAmount;
	};
};

enum
{
	GEN_startAddrsOffset,
	GEN_endAddrsOffset,
	GEN_startloopAddrsOffset,
	GEN_endloopAddrsOffset,
	GEN_startAddrsCoarseOffset,
	GEN_modLfoToPitch,
	GEN_vibLfoToPitch,
	GEN_modEnvToPitch,
	GEN_initialFilterFC,
	GEN_initialFilterQ,
	GEN_modLfoToFilterFc,
	GEN_modEnvToFilterFc,
	GEN_endAddrsCoarseOffset,
	GEN_modLfoToVolume,
	GEN_unused1,
	GEN_chorusEffectsSend,
	GEN_reverbEffectsSend,
	GEN_pan,
	GEN_unused2,
	GEN_unused3,
	GEN_unused4,
	GEN_delayModLFO,
	GEN_freqModLFO,
	GEN_delayVibLFO,
	GEN_freqVibLFO,
	GEN_delayModEnv,
	GEN_attackModEnv,
	GEN_holdModEnv,
	GEN_decayModEnv,
	GEN_sustainModEnv,
	GEN_releaseModEnv,
	GEN_keynumToModEnvHold,
	GEN_keynumToModEnvDecay,
	GEN_delayVolEnv,
	GEN_attackVolEnv,
	GEN_holdVolEnv,
	GEN_decayVolEnv,
	GEN_sustainVolEnv,
	GEN_releaseVolEnv,
	GEN_keynumToVolEnvHold,
	GEN_keynumToVolEnvDecay,
	GEN_instrument,
	GEN_reserved1,
	GEN_keyRange,
	GEN_velRange,
	GEN_startloopAddrsCoarseOffset,
	GEN_keynum,
	GEN_velocity,
	GEN_initialAttenuation,
	GEN_reserved2,
	GEN_endloopAddrsCoarseOffset,
	GEN_coarseTune,
	GEN_fineTune,
	GEN_sampleID,
	GEN_sampleModes,
	GEN_reserved3,
	GEN_scaleTuning,
	GEN_exclusiveClass,
	GEN_overridingRootKey,

	GEN_NumGenerators
};

// Modulator definitions

struct SFModulator
{
	WORD Index:7;
	WORD CC:1;
	WORD Dir:1;			/* 0 = min->max, 1 = max->min */
	WORD Polarity:1;	/* 0 = unipolar, 1 = bipolar */
	WORD Type:6;
};

struct SFModList
{
	SFModulator SrcOper;
	SFGenerator DestOper;
	SWORD		Amount;
	SFModulator AmtSrcOper;
	WORD		Transform;
};

// Modulator sources when CC is 0

enum
{
	SFMod_One = 0,			// Pseudo-controller that always has the value 1
	SFMod_NoteVelocity = 2,
	SFMod_KeyNumber = 3,
	SFMod_PolyPressure = 10,
	SFMod_ChannelPressure = 13,
	SFMod_PitchWheel = 14,
	SFMod_PitchSens = 16,
	SFMod_Link = 127
};

// Modulator types

enum
{
	SFModType_Linear,
	SFModType_Concave,		// log(fabs(value)/(max value)^2)
	SFModType_Convex,
	SFModType_Switch
};

// Modulator transforms

enum
{
	SFModTrans_Linear = 0,
	SFModTrans_Abs = 2
};

// All possible generators in a single structure

struct SFGenComposite
{
	union
	{
		SFRange keyRange;	// For normal use
		struct				// For intermediate percussion use
		{
			BYTE drumset;
			BYTE key;
		};
	};
	SFRange velRange;
	union
	{
		WORD instrument;	// At preset level
		WORD sampleID;		// At instrument level
	};
	SWORD modLfoToPitch;
	SWORD vibLfoToPitch;
	SWORD modEnvToPitch;
	SWORD initialFilterFc;
	SWORD initialFilterQ;
	SWORD modLfoToFilterFc;
	SWORD modEnvToFilterFc;
	SWORD modLfoToVolume;
	SWORD chorusEffectsSend;
	SWORD reverbEffectsSend;
	SWORD pan;
	SWORD delayModLFO;
	SWORD freqModLFO;
	SWORD delayVibLFO;
	SWORD freqVibLFO;
	SWORD delayModEnv;
	SWORD attackModEnv;
	SWORD holdModEnv;
	SWORD decayModEnv;
	SWORD sustainModEnv;
	SWORD releaseModEnv;
	SWORD keynumToModEnvHold;
	SWORD keynumToModEnvDecay;
	SWORD delayVolEnv;
	SWORD attackVolEnv;
	SWORD holdVolEnv;
	SWORD decayVolEnv;
	SWORD sustainVolEnv;
	SWORD releaseVolEnv;
	SWORD keynumToVolEnvHold;
	SWORD keynumToVolEnvDecay;
	SWORD initialAttenuation;
	SWORD coarseTune;
	SWORD fineTune;
	SWORD scaleTuning;

	// The following are only for instruments:
	SWORD startAddrsOffset,		startAddrsCoarseOffset;
	SWORD endAddrsOffset,		endAddrsCoarseOffset;
	SWORD startLoopAddrsOffset,	startLoopAddrsCoarseOffset;
	SWORD endLoopAddrsOffset,	endLoopAddrsCoarseOffset;
	SWORD keynum;
	SWORD velocity;
	WORD  sampleModes;
	SWORD exclusiveClass;
	SWORD overridingRootKey;
};

// Intermediate percussion representation

struct SFPerc
{
	SFPreset *Preset;
	SFGenComposite Generators;
	BYTE LoadOrder;
};

// Container for all parameters from a SoundFont file

struct SFFile : public Timidity::FontFile
{
	SFFile(FString filename);
	~SFFile();
	Timidity::Instrument *LoadInstrument(struct Timidity::Renderer *song, int drum, int bank, int program);
	Timidity::Instrument *LoadInstrumentOrder(struct Timidity::Renderer *song, int order, int drum, int bank, int program);
	void		 SetOrder(int order, int drum, int bank, int program);
	void		 SetAllOrders(int order);

	bool		 FinalStructureTest();
	void		 CheckBags();
	void		 CheckZones(int start, int stop, bool instr);
	void		 TranslatePercussions();
	void		 TranslatePercussionPreset(SFPreset *preset);
	void		 TranslatePercussionPresetZone(SFPreset *preset, SFBag *zone);

	void		 SetInstrumentGenerators(SFGenComposite *composite, int start, int stop);
	void		 AddPresetGenerators(SFGenComposite *composite, int start, int stop, SFPreset *preset);
	void		 AddPresetGenerators(SFGenComposite *composite, int start, int stop, bool gen_set[GEN_NumGenerators]);

	Timidity::Instrument *LoadPercussion(Timidity::Renderer *song, SFPerc *perc);
	Timidity::Instrument *LoadPreset(Timidity::Renderer *song, SFPreset *preset);
	void		 LoadSample(SFSample *sample);
	void		 ApplyGeneratorsToRegion(SFGenComposite *gen, SFSample *sfsamp, Timidity::Renderer *song, Timidity::Sample *sp);

	SFPreset	*Presets;
	SFBag		*PresetBags;
	SFGenList	*PresetGenerators;
	SFInst		*Instruments;
	SFBag		*InstrBags;
	SFGenList	*InstrGenerators;
	SFSample	*Samples;
	TArray<SFPerc> Percussion;
	int			 MinorVersion;
	DWORD		 SampleDataOffset;
	DWORD		 SampleDataLSBOffset;
	DWORD		 SizeSampleData;
	DWORD		 SizeSampleDataLSB;
	int			 NumPresets;
	int			 NumPresetBags;
	int			 NumPresetGenerators;
	int			 NumInstruments;
	int			 NumInstrBags;
	int			 NumInstrGenerators;
	int			 NumSamples;
};

SFFile *ReadSF2(const char *filename, FileReader *f);
