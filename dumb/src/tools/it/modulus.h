
#define MUSIC_IT AL_ID('I','M','P','M')

typedef struct MODULUS_MUSIC_INFO {
	char Name[29];
    int Type;
} MODULUS_MUSIC_INFO;

#define ENVELOPE_ON 1
#define ENVELOPE_LOOP_ON 2
#define ENVELOPE_SUSTAINLOOP 4

typedef struct MODULUS_ENVELOPE {
	unsigned char Flag,
    	NumNodes,
        LoopBegin, LoopEnd, SustainLoopBegin, SustainLoopEnd; //in nodes.
    char NodeY[25];
    short NodeTick[25];
} MODULUS_ENVELOPE;

typedef struct MODULUS_VENVELOPE {
    char CurNode;
    unsigned char CurTick;
	char End;
    //float CVolume;
    MODULUS_ENVELOPE *Envelope;
} MODULUS_VENVELOPE;

#define NNA_NOTECUT      1
#define NNA_NOTECONTINUE 2
#define NNA_NOTEOFF      3
#define NNA_NOTEFADE     4

#define DCT_OFF 0
#define DCT_NOTE 1
#define DCT_SAMPLE 2
#define DCT_INSTRUMENT 3

#define DCA_CUT 0
#define DCA_NOTEOFF 1
#define DCA_NOTEFADE 2

typedef struct MOULUS_INSTRUMENT {
	unsigned char Flag;
    char VolumeLoopNodeStart, VolumeLoopNodeEnd;
    char SustainLoopNodeStart, SustainLoopNodeEnd;
    char DuplicateCheckType;
    char DuplicateCheckAction;
    char NewNoteAction;
    int FadeOut;

    unsigned char PitchPanSeperation, //0->64, Bit7: Don't use
    	PitchPanCenter; //Note, from C-0 to B-9
    unsigned char GlobalVolume, //0->128
    	DefaultPan; //0->64, Bit7: Don't use
    

    unsigned char NoteSample[120];
    unsigned char NoteNote[120];

    MODULUS_ENVELOPE VolumeEnvelope, PanningEnvelope, PitchEnvelope;
} MODULUS_INSTRUMENT;

#define SAMPLE_HASSAMPLE       1
#define SAMPLE_16BIT           2
#define SAMPLE_STEREO          4
#define SAMPLE_USELOOP        16
#define SAMPLE_USESUSTAINLOOP 32
#define SAMPLE_PINGPONGLOOP   64
#define SAMPLE_PINGPONGSUSTAINLOOP 128

#define VIBRATO_SINE 0
#define VIBRATO_RAMPDOWN 1
#define VIBRATO_SQUARE 2
#define VIBRATO_RANDOM 3

typedef struct MODULUS_SAMPLE {
	unsigned char GlobalVolume; //0->64
    unsigned char Flag;
    unsigned char Volume;
    int SampleLength; //in samples, not bytes !
    int LoopBegin, LoopEnd; //in samples
    int SustainLoopBegin, SustainLoopEnd;
    int C5Speed; //Number of bytes/sec for C-5

    SAMPLE *Sample;

    char VibratoSpeed; //0->64
    char VibratoDepth; //0->64
    char VibratoWaveForm;
    char VibratoRate; //0->64
} MODULUS_SAMPLE;

typedef struct MODULUS_NOTE {
	char Mask; //If Bit0: Note, Bit1: Instrument, Bit2: Volume, Bit3: Panning, Bit4: Command
	char Channel; //if -1, then end of row.    
    unsigned char Note;
    char Instrument;
    unsigned char Volume, Panning;
    unsigned char Command, CommandValue;
} MODULUS_NOTE;

typedef struct MODULUS_PATTERN {
	int NumRows;
    int NumNotes;
    MODULUS_NOTE *Note;
} MODULUS_PATTERN;

typedef struct MODULUS_VCHANNEL {
	MODULUS_SAMPLE *Sample;  //NULL is unused
    char voice;
    char ChannelVolume;
    char NoteOn;
    char NNA;
    short FadeOutCount, FadeOut;
    float MixVolume, MixPan;
    MODULUS_VENVELOPE *VVolumeEnvelope, *VPanningEnvelope, *VPitchEnvelope;
    MODULUS_VCHANNEL *next, *prev;
} MODULUS_VCHANNEL;

typedef struct MODULUS_CHANNEL {
	unsigned char Volume; //0->64
    unsigned char Pan;    //0->32->64, 100 = surround, Bit7: Disable
    char LastNote, LastInstrument, LastSample;    
    MODULUS_VCHANNEL *VChannel;
} MODULUS_CHANNEL;

#define FLAG_STEREO         1
#define FLAG_USEINSTRUMENTS 4
#define FLAG_LINEARSLIDES   8
#define FLAG_OLDEFFECT     16

typedef struct MODULUS {
	MODULUS_INSTRUMENT *Instrument;
    MODULUS_SAMPLE *Sample;
    MODULUS_PATTERN *Pattern;

	int NumOrders;
    int NumInstruments;
    int NumSamples;
    int NumPatterns;
    int Flags;
    short Version;
    char GlobalVolume;
    char MixVolume;
    unsigned char Speed, Tempo;
    char PanningSeperation;

    unsigned char *Order;

    MODULUS_CHANNEL Channel[64];

} MODULUS;

#define COMMAND_SET_SONG_SPEED        1
#define COMMAND_JUMP_TO_ORDER         2
#define COMMAND_PATTERN_BREAK_TO_ROW  3
#define COMMAND_SET_CHANNEL_VOLUME   13
#define COMMAND_SET_SONG_TEMPO       20
#define COMMAND_SET_GLOBAL_VOLUME    22

typedef struct MODULUS_PLAY {
	MODULUS *Music;
    int Loop, Tick;
    int CurOrder, CurPattern, CurPos;
    int Command, CommandVal0, CommandVal1, CommandVal2;
    int pos;
} MODULUS_PLAY;
extern MODULUS_PLAY *song;

extern int IT_Play_Method;

MODULUS *load_it(char*);
int get_module_size(MODULUS *);

int play_it(MODULUS *j, int loop);
void install_modulus();
void set_mix_volume(int i);

void stop_it();
int is_music_done();
void destroy_it(MODULUS *j);

//Should be internal:
extern MODULUS_PLAY *song;
extern int note_freq[120];

extern void MOD_Interrupt(...);
extern int MOD_Poller(void*);

#define IT_TIMER 0
#define IT_POLL  1

