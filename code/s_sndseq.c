#include <string.h>
#include <stdio.h>

#include "doomtype.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "cmdlib.h"

#define SEQ_MASK			0xc0000000
#define SEQ_END				0x00000000
#define SEQ_PLAYUNTILDONE	0x40000000
#define SEQ_PLAYTIME		0x80000000
#define SEQ_PLAYREPEAT		0xc0000000

#define SEQ_TIMESHIFT		16
#define SEQ_TIME			0x3fff0000
#define SEQ_SOUND			0x0000ffff

#define SEQ_NOSOUND			0x0000ffff

typedef struct {
	char name[MAX_SNDNAME+1];
	unsigned int script[1];	// + more until end of sequence script
} sndseq_t;

#define MAX_SEQUENCES	32
#define MAX_SEQSIZE		128
static sndseq_t *Sequences[MAX_SEQUENCES];

static int numsequences;

void S_ParseSndSeq (void)
{
	int lastlump, lump;
	char *data;
	char name[MAX_SNDNAME+1];
	unsigned int script[MAX_SEQSIZE];
	int cursize;
	int curseq = -1;
	BOOL awaitingname = false;

	lastlump = 0;
	while ((lump = W_FindLump ("SNDSEQ", &lastlump)) != -1) {
		data = W_CacheLumpNum (lump, PU_CACHE);

		while ( (data = COM_Parse (data)) ) {
			if (*com_token == ':') {
				// Start of new sound sequence
				awaitingname = true;
				curseq = numsequences++;
				cursize = 1;
				script[0] = SEQ_NOSOUND;
			} else if (curseq != -1) {
				if (awaitingname) {
					strncpy (name, com_token, MAX_SNDNAME);
					name[MAX_SNDNAME] = 0;
					awaitingname = false;
					continue;
				}

				if (!stricmp (com_token, "playuntildone")) {
					data = COM_Parse (data);
					script[cursize++] = SEQ_PLAYUNTILDONE | (S_FindSound (com_token) & SEQ_SOUND);
				} else if (!stricmp (com_token, "playrepeat")) {
					data = COM_Parse (data);
					script[cursize++] = SEQ_PLAYREPEAT | (S_FindSound (com_token) & SEQ_SOUND);
				} else if (!stricmp (com_token, "stopsound")) {
					data = COM_Parse (data);
					script[0] = S_FindSound (com_token);
				} else if (!stricmp (com_token, "playtime")) {
					data = COM_Parse (data);
					script[cursize] = SEQ_PLAYTIME | (S_FindSound (com_token) & SEQ_SOUND);
					data = COM_Parse (data);
					script[cursize++] |= (atoi (com_token) << SEQ_TIMESHIFT) & SEQ_TIME;
				} else if (!stricmp (com_token, "end")) {
					script[cursize++] = SEQ_END;
					Sequences[curseq] = Z_Malloc (sizeof(sndseq_t) + sizeof(int)*(cursize-1), PU_STATIC, 0);
					strcpy (Sequences[curseq]->name, name);
					memcpy (Sequences[curseq]->script, script, sizeof(int)*cursize);
					curseq = -1;
				}
			}
		}
	}
}