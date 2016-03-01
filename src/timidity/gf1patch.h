/* GF1 Patch definition: */
enum
{
	HEADER_SIZE					= 12,
	ID_SIZE						= 10,
	DESC_SIZE					= 60,
	RESERVED_SIZE				= 40,
	PATCH_HEADER_RESERVED_SIZE	= 36,
	LAYER_RESERVED_SIZE			= 40,
	PATCH_DATA_RESERVED_SIZE	= 36,
	INST_NAME_SIZE				= 16,
	ENVELOPES					= 6,
	MAX_LAYERS					= 4
};
#define GF1_HEADER_TEXT			"GF1PATCH110"

#ifdef _MSC_VER
#pragma pack(push, 1)
#define GCC_PACKED
#else
#define GCC_PACKED __attribute__((__packed__))
#endif

struct GF1PatchHeader
{
	char Header[HEADER_SIZE];
	char GravisID[ID_SIZE];		/* Id = "ID#000002" */
	char Description[DESC_SIZE];
	BYTE Instruments;
	BYTE Voices;
	BYTE Channels;
	WORD WaveForms;
	WORD MasterVolume;
	DWORD DataSize;
	BYTE Reserved[PATCH_HEADER_RESERVED_SIZE];
} GCC_PACKED;

struct GF1InstrumentData
{
	WORD Instrument;
	char InstrumentName[INST_NAME_SIZE];
	int  InstrumentSize;
	BYTE Layers;
	BYTE Reserved[RESERVED_SIZE];
} GCC_PACKED;

struct GF1LayerData
{
	BYTE LayerDuplicate;
	BYTE Layer;
	int  LayerSize;
	BYTE Samples;
	BYTE Reserved[LAYER_RESERVED_SIZE];
} GCC_PACKED;

struct GF1PatchData
{
	char  WaveName[7];
	BYTE  Fractions;
	int   WaveSize;
	int   StartLoop;
	int   EndLoop;
	WORD  SampleRate;
	int   LowFrequency;
	int   HighFrequency;
	int   RootFrequency;
	SWORD Tune;
	BYTE  Balance;
	BYTE  EnvelopeRate[ENVELOPES];
	BYTE  EnvelopeOffset[ENVELOPES];
	BYTE  TremoloSweep;
	BYTE  TremoloRate;
	BYTE  TremoloDepth;
	BYTE  VibratoSweep;
	BYTE  VibratoRate;
	BYTE  VibratoDepth;
	BYTE  Modes;
	SWORD ScaleFrequency;
	WORD  ScaleFactor;		/* From 0 to 2048 or 0 to 2 */
	BYTE  Reserved[PATCH_DATA_RESERVED_SIZE];
} GCC_PACKED;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef GCC_PACKED
