#if !defined( QMUS2MID_H )
#define QMUS2MID_H

typedef unsigned short	int2;	/* a two-byte int, use short.*/
typedef unsigned int	int4;	/* a four-byte int, use int unless int is
								  16 bits, then use long. Don't use long
								  on an alpha.	*/

#define NOTMUSFILE		1		/* Not a MUS file */
#define COMUSFILE		2		/* Can't open MUS file */
#define COTMPFILE		3		/* Can't open TMP file */
#define CWMIDFILE		4		/* Can't write MID file */
#define MUSFILECOR		5		/* MUS file corrupted */
#define TOOMCHAN		6		/* Too many channels */
#define MEMALLOC		7		/* Memory allocation error */

/* some (old) compilers mistake the "MUS\x1A" construct (interpreting
   it as "MUSx1A")		*/

#define MUSMAGIC	 "MUS\032"					  /* this seems to work */
#define MIDIMAGIC	 "MThd\000\000\000\006\000\001"
#define TRACKMAGIC1  "\000\377\003\035"
#define TRACKMAGIC2  "\000\377\057\000"
#define TRACKMAGIC3  "\000\377\002\026"
#define TRACKMAGIC4  "\000\377\131\002\000\000"
#define TRACKMAGIC5  "\000\377\121\003\011\243\032"
#define TRACKMAGIC6  "\000\377\057\000"


typedef struct
{
  char		  ID[4];			/* identifier "MUS" 0x1A */
  int2		  ScoreLength;
  int2		  ScoreStart;
  int2		  channels; 		/* count of primary channels */
  int2		  SecChannels;		/* count of secondary channels (?) */
  int2		  InstrCnt;
  int2		  dummy;
  /* variable-length part starts here */
  int2		  *instruments;
} MUSheader;

struct Track
{
  unsigned long  current;
  char			 vel;
  long			 DeltaTime;
  unsigned char  LastEvent;
  char			 *data; 		   /* Primary data */
};


int qmus2mid( const char *mus, const char *mid, int nodisplay,
			  int2 division, int BufferSize, int nocomp );

int convert( const char *mus, const char *mid, int nodisplay,
						  int div, int size, int nocomp, int *ow );

#endif


