#ifndef LIBXMP_MED_H
#define LIBXMP_MED_H

#include "../common.h"
#include "../hio.h"

#define MMD_INST_TYPES 9

#ifdef DEBUG
extern const char *const mmd_inst_type[];
#endif

/* Structures as defined in the MED/OctaMED MMD0 and MMD1 file formats,
 * revision 1, described by Teijo Kinnunen in Apr 25 1992
 */

struct PlaySeq {
    char name[32];		/* (0)  31 chars + \0 */
    uint32 reserved[2];		/* (32) for possible extensions */
    uint16 length;		/* (40) # of entries */
    uint16 seq[1];		/* (42) block numbers.. */
};


struct MMD0sample {
    uint16 rep, replen;		/* offs: 0(s), 2(s) */
    uint8 midich;		/* offs: 4(s) */
    uint8 midipreset;		/* offs: 5(s) */
    uint8 svol;			/* offs: 6(s) */
    int8 strans;		/* offs: 7(s) */
};


struct MMD0song {
    struct MMD0sample sample[63];	/* 63 * 8 bytes = 504 bytes */
    uint16 numblocks;		/* offs: 504 */
    uint16 songlen;		/* offs: 506 */
    uint8 playseq[256];		/* offs: 508 */
    uint16 deftempo;		/* offs: 764 */
    int8 playtransp;		/* offs: 766 */
#define FLAG_FILTERON   0x1	/* the hardware audio filter is on */
#define FLAG_JUMPINGON  0x2	/* mouse pointer jumping on */
#define FLAG_JUMP8TH    0x4	/* ump every 8th line (not in OctaMED Pro) */
#define FLAG_INSTRSATT  0x8	/* sng+samples indicator (not useful in MMDs) */
#define FLAG_VOLHEX	0x10	/* volumes are HEX */
#define FLAG_STSLIDE    0x20	/* use ST/NT/PT compatible sliding */
#define FLAG_8CHANNEL   0x40	/* this is OctaMED 5-8 channel song */
#define FLAG_SLOWHQ     0X80	/* HQ V2-4 compatibility mode */
    uint8 flags;		/* offs: 767 */
#define FLAG2_BMASK	0x1F	/* (bits 0-4) BPM beat length (in lines) */
#define FLAG2_BPM       0x20	/* BPM mode on */
#define FLAG2_MIX	0x80	/* Module uses mixing */
    uint8 flags2;		/* offs: 768 */
    uint8 tempo2;		/* offs: 769 */
    uint8 trkvol[16];		/* offs: 770 */
    uint8 mastervol;		/* offs: 786 */
    uint8 numsamples;		/* offs: 787 */
};				/* length = 788 bytes */


/* This structure is exactly as long as the MMDsong structure. Common fields
 * are located at same offsets. You can also see, that there's a lot of room
 * for expansion in this structure.
 */
struct MMD2song {
    struct MMD0sample sample[63];
    uint16 numblocks;
    uint16 songlen;		/* NOTE: number of sections in MMD2 */
    struct PlaySeq **playseqtable;
    uint16 *sectiontable;	/* UWORD section numbers */
    uint8 *trackvols;		/* UBYTE track volumes */
    uint16 numtracks;		/* max. number of tracks in the song
				 * (also the number of entries in
				 * 'trackvols' table) */
    uint16 numpseqs;		/* number of PlaySeqs in 'playseqtable' */
    int8 *trackpans;		/* NULL means 'all centered */
#define FLAG3_STEREO	0x1	/* Mixing in stereo */
#define FLAG3_FREEPAN	0x2	/* Mixing flag: free pan */
    uint32 flags3;		/* see defs below */
    uint16 voladj;		/* volume adjust (%), 0 means 100 */
    uint16 channels;		/* mixing channels, 0 means 4 */
    uint8 mix_echotype;		/* 0 = nothing, 1 = normal, 2 = cross */
    uint8 mix_echodepth;	/* 1 - 6, 0 = default */
    uint16 mix_echolen;		/* echo length in milliseconds */
    int8 mix_stereosep;		/* stereo separation */
    uint8 pad0[223];		/* reserved for future expansion */
/* Fields below are MMD0/MMD1-compatible (except pad1[]) */
    uint16 deftempo;
    int8 playtransp;
    uint8 flags;
    uint8 flags2;
    uint8 tempo2;
    uint8 pad1[16];		/* used to be trackvols, in MMD2 reserved */
    uint8 mastervol;
    uint8 numsamples;
};				/* length = 788 bytes */


struct MMD0 {
    uint32 id;
    uint32 modlen;
    struct MMD0song *song;
    uint16 psecnum;			/* MMD2 only */
    uint16 pseq;			/* MMD2 only */
    struct MMD0Block **blockarr;
#define MMD_LOADTOFASTMEM 0x1
    uint8 mmdflags;			/* MMD2 only */
    uint8 reserved[3];
    struct InstrHdr **smplarr;
    uint32 reserved2;
    struct MMD0exp *expdata;
    uint32 reserved3;
    uint16 pstate;			/* some data for the player routine */
    uint16 pblock;
    uint16 pline;
    uint16 pseqnum;
    int16 actplayline;
    uint8 counter;
    uint8 extra_songs;			/* number of songs - 1 */
};					/* length = 52 bytes */


struct MMD0Block {
    uint8 numtracks, lines;
};


struct BlockCmdPageTable {
    uint16 num_pages;
    uint16 reserved;
    uint16 *page[1];
};


struct BlockInfo {
    uint32 *hlmask;
    uint8 *blockname;
    uint32 blocknamelen;
    struct BlockCmdPageTable *pagetable;
    uint32 reserved[5];
};


struct MMD1Block {
    uint16 numtracks;
    uint16 lines;
    struct BlockInfo *info;
};


struct InstrHdr {
    uint32 length;
#define S_16 0x10			/* 16-bit sample */
#define MD16 0x18			/* 16-bit sample (Aura) */
#define STEREO 0x20			/* Stereo sample, not interleaved */
    int16 type;
    /* Followed by actual data */
};


struct SynthWF {
    uint16 length;			/* length in words */
    int8 wfdata[1];			/* the waveform */
};


struct SynthInstr {
    uint32 length;			/* length of this struct */
    int16 type;				/* -1 or -2 (offs: 4) */
    uint8 defaultdecay;
    uint8 reserved[3];
    uint16 rep;
    uint16 replen;
    uint16 voltbllen;			/* offs: 14 */
    uint16 wftbllen;			/* offs: 16 */
    uint8 volspeed;			/* offs: 18 */
    uint8 wfspeed;			/* offs: 19 */
    uint16 wforms;			/* offs: 20 */
    uint8 voltbl[128];			/* offs: 22 */
    uint8 wftbl[128];			/* offs: 150 */
    uint32 wf[64];			/* offs: 278 */
};


/* OctaMED SoundStudio 1 and prior use the InstrExt default_pitch field as a
 * default note value for the default note key 'F'. Pressing 'F' will insert a
 * note event with this note value.
 *
 * MED Soundstudio 2 in mix mode treats note 0x01 as a default note event,
 * which is emitted by the default note key instead of a regular note event.
 * It also makes this more complicated, despite not having changed the file
 * format: the user must enter a frequency in Hz instead of a note number,
 * where 8363 Hz corresponds to the event C-2. This frequency is converted to a
 * note number upon saving the module. Multi-octave instruments do not support
 * this feature as they are not supported by MED Soundstudio 2.
 *
 * This editor-only behavior would be irrelevant, except when default_pitch
 * is zero, the player uses the default frequency 22050 Hz instead. This
 * results in a note between E-3 and F-3. Since this feature is currently
 * implemented in the instrument map, use the mix mode note for F-3 instead.
 */
#define MMD3_DEFAULT_NOTE	53

struct InstrExt {
    uint8 hold;
    uint8 decay;
    uint8 suppress_midi_off;
    int8  finetune;
    /* Below fields saved by >= V5 */
    uint8 default_pitch;
#define SSFLG_LOOP	0x01		/* Loop On/Off */
#define SSFLG_EXTPSET	0x02		/* Ext. Preset */
#define SSFLG_DISABLED	0x04		/* Disabled */
#define SSFLG_PINGPONG	0x08		/* Ping-pong looping */
    uint8 instr_flags;
    uint16 long_midi_preset;
    /* Below fields saved by >= V5.02 */
    uint8 output_device;
    uint8 reserved;
    /* Below fields saved by >= V7 */
    uint32 long_repeat;
    uint32 long_replen;
};


struct MMDInfo {
    struct MMDInfo *next;		/* next MMDInfo structure */
    uint16 reserved;
    uint16 type;				/* data type (1 = ASCII) */
    uint32 length;			/* data length in bytes */
    /* data follows... */
};


struct MMDARexxTrigCmd {
    struct MMDARexxTrigCmd *next;	/* the next command, or NULL */
    uint8 cmdnum;			/* command number (01..FF) */
    uint8 pad;
    int16 cmdtype;			/* command type (OMACTION_...) */
    char *cmd;				/* command, or NULL */
    char *port;				/* port, or NULL */
    uint16 cmd_len;			/* length of 'cmd' string (without
					 * term. 0) */
    uint16 port_len;			/* length of 'port' string (without
					 * term. 0) */
};		/* current (V7) structure size: 20 */


struct MMDARexx {
    uint16 res;				/* reserved, must be zero! */
    uint16 trigcmdlen;			/* size of trigcmd entries
					 * (MUST be used!!) */
    struct MMDARexxTrigCmd *trigcmd;	/* chain of MMDARexxTrigCmds or NULL */
};


struct MMDMIDICmd3x {
    uint8 struct_vers;			/* current version = 0 */
    uint8 pad;
    uint16 num_of_settings;		/* number of Cmd3x settings
					 * (currently set to 15) */
    uint8 *ctrlr_types;			/* controller types */
    uint16 *ctrlr_numbers;		/* controller numbers */
};


struct MMDInstrInfo {
    uint8 name[40];
};

struct MMD0exp {
    struct MMD0 *nextmod;		/* pointer to the next module */
    struct InstrExt *exp_smp;		/* pointer to InstrExt */
    uint16 s_ext_entries;		/* size of InstrExt structure array */
    uint16 s_ext_entrsz;		/* size of each InstrExt structure */
    uint8 *annotxt;			/* pointer to the annotation text */
    uint32 annolen;			/* length of 'annotxt' */
    struct MMDInstrInfo *iinfo;		/* pointer to MMDInstrInfo */
    uint16 i_ext_entries;		/* size of MMDInstrInfo struct array */
    uint16 i_ext_entrsz;		/* size of each MMDInstrInfo struct */
    uint32 jumpmask;			/* mouse pointer jump control */
    uint16 *rgbtable;			/* screen colors */
    uint8 channelsplit[4];		/* channel splitting control */
    struct NotationInfo *n_info;	/* info for the notation editor */
    uint8 *songname;			/* song name of the current song */
    uint32 songnamelen;			/* song name length */
    struct MMDDumpData *dumps;		/* MIDI dump data */
    struct MMDInfo *mmdinfo;		/* more information about the song */
    struct MMDARexx *mmdrexx;		/* embedded ARexx commands */
    struct MMDMIDICmd3x *mmdcmd3x;	/* settings for command 3x */
    uint32 reserved2[3];			/* future expansion fields */
    uint32 tag_end;
};


struct NotationInfo {
    uint8 n_of_sharps;			/* number of sharps or flats */
#define NFLG_FLAT   1
#define NFLG_3_4    2
    uint8 flags;
    int16 trksel[5];			/* number of the selected track */
    uint8 trkshow[16];			/* tracks shown */
    uint8 trkghost[16];			/* tracks ghosted */
    int8 notetr[63];			/* note transpose for each instrument */
    uint8 pad;
};


struct MMDDumpData {
    uint16 numdumps;
    uint16 reserved[3];
};


struct MMDDump {
    uint32 length;			/* length of the MIDI message dump */
    uint8 *data;			/* pointer to MIDI dump data */
    uint16 ext_len;			/* MMDDump struct extension length */
    /* if ext_len >= 20: */
    uint8 name[20];			/* name of the dump */
};

extern const int mmd_num_oct[6];

void mmd_xlat_fx(struct xmp_event *, int, int, int, int);
int mmd_alloc_tables(struct module_data *, int, struct SynthInstr *);

int mmd_load_instrument(HIO_HANDLE *, struct module_data *, int, int,
		struct MMD0exp *, struct InstrExt *, struct MMD0sample *, int);

void mmd_set_bpm(struct module_data *, int, int, int, int);
void mmd_info_text(HIO_HANDLE *, struct module_data *, int);

#endif /* LIBXMP_MED_H */
