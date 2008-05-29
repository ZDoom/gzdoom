#ifdef FORTIFY
#include "fortify.h"
#endif
#include <stdio.h>
#ifdef MSS
#include "mss.h"
#endif

#include <string.h>

#include "allegro.h"
#include "modulus.h"
#include "typedef.hpp"

int	detect_it(char *f) {
	int	sig;
	PACKFILE *fn = pack_fopen(f, "rb");
	
	if (fn == NULL)
		return FALSE;

	sig	= pack_mgetl(fn);
	if (sig	!= AL_ID('I','M','P','M')) {
		pack_fclose(fn);
		return FALSE;
	}
	pack_fclose(fn);

	return TRUE;
}

MODULUS	*create_it() {
	MODULUS	*m = (MODULUS*)malloc(sizeof(MODULUS));
	if (!m)
		return NULL;
	memset(m, 0, sizeof(MODULUS));
	return m;
}

void destroy_it(MODULUS	*j)	{

	if (song->Music	== j)
		stop_it();

	//remove patterns:
	for	(int i=0; i<j->NumPatterns;	i++) {
		free(j->Pattern[i].Note);
	}
	if (j->Pattern)
		free(j->Pattern);
	//remove instruments;
   	if (j->Instrument)
		free(j->Instrument);
	//remove samples;
	for	(int i=0; i<j->NumSamples; i++)	{
		destroy_sample(j->Sample[i].Sample);		
	}
	if (j->Sample)
		free(j->Sample);
	//remove orders:
	if (j->Order)
		free(j->Order);
	//remove channels:
	for	(int i=0; i<64;	i++) {
		if (j->Channel[i].VChannel)	{
			MODULUS_VCHANNEL *vchn = song->Music->Channel[i].VChannel;
			MODULUS_VCHANNEL *prev = NULL;
	
			if (!vchn)
				continue;
		
			for	(;;) {
				deallocate_voice(vchn->voice);
					
				prev = vchn;
				vchn = vchn->next;
				free(prev);

				if (!vchn)
					break;
			}
		}	 
	}
	free(j);
}

//#define DEBUG_IT_SIZE

int	get_module_size(MODULUS	*j)	{
	int	a, b, c, d = 0,	e;
	a =	sizeof(MODULUS)	+ j->NumOrders;
	b =	j->NumInstruments *	sizeof(MODULUS_INSTRUMENT);
	c =	j->NumSamples *	sizeof(MODULUS_SAMPLE);
	
	for	(int i=0; i<j->NumSamples; i++)
		d += j->Sample[i].SampleLength * (j->Sample[i].Flag	& 2	? sizeof(short)	: 1) * (j->Sample[i].Flag &	4 ?	2: 1);
		
	e =	4 +	sizeof(MODULUS_PATTERN)	* j->NumPatterns;
	
	for	(int i=0; i<j->NumPatterns;	i++)
		e += j->Pattern[i].NumNotes	* sizeof(MODULUS_NOTE);
	#ifdef DEBUG_IT_SIZE
	printf("Base: %i, Instruments(%i): %i, Samples(%i): %i, Data: %i, Patterns(%i): %i\n", a, j->NumInstruments, b,	j->NumSamples, c, d, j->NumPatterns, e);
	#endif

	return a+b+c+d+e;
}

#define	MAX_IT_CHN 64

//#define DEBUG_HEADER
//#define DEBUG_INSTRUMENTS
//#define DEBUG_SAMPLES
//#define DEBUG_PATTERNS

static dword *sourcebuf	= NULL;
static dword *sourcepos	= NULL;
static byte	rembits	= 0;

int	readblock(PACKFILE *f) {
	long size;
	int	c =	pack_igetw(f);
	if (c == -1)
		return 0;
	size = c;

	sourcebuf =	(dword*)malloc(size+4);
	if (!sourcebuf)
		return 0;
		
	c =	pack_fread(sourcebuf, size,	f);
	if (c <	1) {
		free(sourcebuf);
		sourcebuf =	NULL;
		return 0;		 
	}
	sourcepos =	sourcebuf;
	rembits	= 32;
	return 1;
}

void freeblock() {
	if (sourcebuf)
		free(sourcebuf);
	sourcebuf =	NULL;	 
}

dword readbits(char	b) {
	dword val;
	if (b <= rembits) {
		val	= *sourcepos & ((1 << b) - 1);
		*sourcepos >>= b;
		rembits	-= b;
	}
	else {
		dword nbits	= b	- rembits;
		val	= *sourcepos;
		sourcepos++;
		val	|= ((*sourcepos	& ((1 << nbits)	- 1)) << rembits);
		*sourcepos >>= nbits;
		rembits	= 32 - nbits;
	}
	return val;
}

void decompress8(PACKFILE *f, void *data, int len, int tver) {
	char *destbuf =	(char*)data;
	char *destpos =	destbuf;
	int	blocklen, blockpos;
	byte bitwidth;
	word val;
	char d1, d2;

	memset(destbuf,	0, len);

	while (len>0) {
		//Read a block of compressed data:
		if (!readblock(f))
			return;
		//Set up a few variables
		blocklen = (len	< 0x8000) ?	len	: 0x8000; //Max block length is 0x8000 bytes
		blockpos = 0;
		bitwidth = 9;
		d1 = d2	= 0;
		//Start the decompression:
		while (blockpos	< blocklen)	{
			//Read a value:
			val	= readbits(bitwidth);
			//Check for bit width change:
			
			if (bitwidth < 7) {	//Method 1:
				if (val	== (1 << (bitwidth - 1))) {
					val	= readbits(3) +	1;
					bitwidth = (val	< bitwidth)	? val :	val	+ 1;
					continue;
				}
			}
			else if	(bitwidth <	9) { //Method 2
				byte border	= (0xFF	>> (9 -	bitwidth)) - 4;

				if (val	> border &&	val	<= (border + 8)) {
					val	-= border;
					bitwidth = (val	< bitwidth)	? val :	val	+ 1;
					continue;
				}
			}
			else if	(bitwidth == 9)	{ //Method 3
				if (val	& 0x100) {
					bitwidth = (val	+ 1) & 0xFF;
					continue;
				}
			}
			else { //Illegal width, abort ?
				freeblock();
				return;
			}

			//Expand the value to signed byte:
			char v;	//The sample value:
			if (bitwidth < 8) {
				byte shift = 8 - bitwidth;
				v =	(val <<	shift);
				v >>= shift;
			}
			else
				v =	(char)val;
				
			//And integrate the sample value
			//(It always has to end with integration doesn't it ? ;-)
			d1 += v;
			d2 += d1;

			//Store !
			*destpos = ((tver == 0x215)	? d2 : d1);
			destpos++;
			blockpos++;
		}
		freeblock();
		len	-= blocklen;
	}
	return;
}

void decompress16(PACKFILE *f, void	*data, int len,	int	tver) {
	//make the output buffer:
	short *destbuf = (short*)data;
	short *destpos = destbuf;
	int	blocklen, blockpos;
	byte bitwidth;
	long val;
	short d1, d2;

	memset(destbuf,	0, len);

	while (len>0) {
		//Read a block of compressed data:
		if (!readblock(f))
			return;
		//Set up a few variables
		blocklen = (len	< 0x4000) ?	len	: 0x4000; // Max block length is 0x4000 bytes
		blockpos = 0;
		bitwidth = 17;
		d1 = d2	= 0;
		//Start the decompression:
		while (blockpos	< blocklen)	{
			val	= readbits(bitwidth);
			//Check for bit width change:
			
			if (bitwidth < 7) {	//Method 1:
				if (val	== (1 << (bitwidth - 1))) {
					val	= readbits(4) +	1;
					bitwidth = (val	< bitwidth)	? val :	val	+ 1;
					continue;
				}
			}
			else if	(bitwidth <	17)	{ //Method 2
				word border	= (0xFFFF >> (17 - bitwidth)) -	8;

				if (val	> border &&	val	<= (border + 16)) {
					val	-= border;
					bitwidth = val < bitwidth ?	val	: val +	1;
					continue;
				}
			}
			else if	(bitwidth == 17) { //Method 3
				if (val	& 0x10000) {
					bitwidth = (val	+ 1) & 0xFF;
					continue;
				}
			}
			else { //Illegal width, abort ?
				freeblock();
				return;
			}

			//Expand the value to signed byte:
			short v; //The sample value:
			if (bitwidth < 16) {
				byte shift = 16	- bitwidth;
				v =	(val <<	shift);
				v >>= shift;
			}
			else
				v =	(short)val;
				
			//And integrate the sample value
			//(It always has to end with integration doesn't it ? ;-)
			d1 += v;
			d2 += d1;

			//Store !
			*destpos = ((tver == 0x215)	? d2 : d1);
			destpos++;
			blockpos++;
		}
		freeblock();
	   	len	-= blocklen;
	}
	return;
}

MODULUS	*load_it(char *file) {
	PACKFILE *f;
	MODULUS	*j = create_it();
	int	tver, tver2, flag, msglen, msgoffs;
	int	*insoffs = NULL, *samoffs = NULL, *patoffs = NULL;
	
	if (!j)
		return NULL;

	if (!detect_it(file))
		return NULL;

	f =	pack_fopen(file, "rb");

	if (!f)	{
		#ifdef DEBUG_HEADER
			printf("Error Opening!\n");
		#endif
		return NULL;
	}

	pack_fseek(f, 30);
	pack_igetw(f); //I have no idea...
	
	j->NumOrders = pack_igetw(f);
	j->NumInstruments =	pack_igetw(f);
	j->NumSamples =	pack_igetw(f);
	j->NumPatterns = pack_igetw(f);

	#ifdef DEBUG_HEADER
	printf("Loading IT: %i Orders %i Instruments, %i Samples, %i Patterns\n", j->NumOrders,	j->NumInstruments, j->NumSamples, j->NumPatterns);
	#endif
	
	tver = pack_igetw(f);
	j->Version = tver2 = pack_igetw(f);

	#ifdef DEBUG_HEADER
	printf("Tracker ver: %X, %X\n",	tver, tver2);
	#endif

	j->Flags = pack_igetw(f);
	flag = pack_igetw(f);

	j->GlobalVolume	= pack_getc(f);
	j->MixVolume = pack_getc(f);
	j->Speed = pack_getc(f);
	j->Tempo = pack_getc(f);
	j->PanningSeperation = pack_getc(f);

	#ifdef DEBUG_HEADER
	printf("Global Volume: %i, Mixing Volume: %i, Speed: %i, Tempo: %i, PanSep: %i\n", j->GlobalVolume,	j->MixVolume, j->Speed,	j->Tempo, j->PanningSeperation);
	#endif

	pack_getc(f);	//Damn....I need more info on this.

	msglen = pack_igetw(f);
	msgoffs	= pack_igetl(f);

	pack_fseek(f, 4);

	#ifdef DEBUG_HEADER
	printf("Channel Pan:");
	#endif

	for	(int i=0; i<MAX_IT_CHN;	i++) {
		j->Channel[i].Pan =	pack_getc(f);
		#ifdef DEBUG_HEADER
		printf(" %i", j->Channel[i].Pan);
		#endif
	}
	#ifdef DEBUG_HEADER
	printf("\nChannel Vol:");
	#endif
	for	(int i=0; i<MAX_IT_CHN;	i++) {
		j->Channel[i].Volume = pack_getc(f);
		#ifdef DEBUG_HEADER
		printf(" %i", j->Channel[i].Volume);
		#endif
	}
	#ifdef DEBUG_HEADER
	printf("\n");
	#endif
		
	j->Order = (unsigned char *)malloc(j->NumOrders);
	pack_fread(j->Order, j->NumOrders, f);

	if (j->NumInstruments)
		insoffs = (int*)malloc(4 * j->NumInstruments);
	if (j->NumSamples)
		samoffs = (int*)malloc(4 * j->NumSamples);
	if (j->NumPatterns)
		patoffs	= (int*)malloc(4 * j->NumPatterns);

	pack_fread(insoffs,	4 * j->NumInstruments, f);
	pack_fread(samoffs, 4 * j->NumSamples, f);
	pack_fread(patoffs,	4 * j->NumPatterns, f);

	if (flag&1)	{ //Song message attached
		//Ignore.
	}
	if (flag & 4) {	//skip something:
		short u;
		char dummy[8];
		u =	pack_igetw(f);
		for	(int i=0; i<u; u++)
			pack_fread(dummy, 8, f);
	}
	if (flag & 8) {	//MIDI commands ???
		char dummy[33];
		for	(int i=0; i<9+16+128; i++)
			pack_fread(dummy, 32, f);

	}

	if (j->NumInstruments)
		j->Instrument =	(MODULUS_INSTRUMENT*)malloc(sizeof(MODULUS_INSTRUMENT) * j->NumInstruments);
	#ifdef DEBUG_INSTRUMENTS
	if (!j->Instrument)
		printf("No Mem for Instruments!\n");
	#endif

	
	for	(int i=0; i<j->NumInstruments; i++)	{
		pack_fclose(f);
		f =	pack_fopen(file, "rb");
		#ifdef DEBUG_INSTRUMENTS
		if (!f)
			printf("Error Opening!\n");
		#endif
		pack_fseek(f, insoffs[i] + 17);

		j->Instrument[i].NewNoteAction = pack_getc(f);
		j->Instrument[i].DuplicateCheckType	= pack_getc(f);
		j->Instrument[i].DuplicateCheckAction =	pack_getc(f);
		j->Instrument[i].FadeOut = pack_igetw(f);
		j->Instrument[i].PitchPanSeperation	= pack_getc(f);
		j->Instrument[i].PitchPanCenter	= pack_getc(f);
		j->Instrument[i].GlobalVolume =	pack_getc(f);
		j->Instrument[i].DefaultPan	= pack_getc(f);
		#ifdef DEBUG_INSTRUMENTS
		printf("I%02i @ 0x%X, NNA %i, DCT %i, DCA %i, FO %i, PPS %i, PPC %i, GVol %i, DPan %i\n", i, insoffs[i], j->Instrument[i].NewNoteAction, j->Instrument[i].DuplicateCheckType, j->Instrument[i].DuplicateCheckAction, j->Instrument[i].FadeOut, j->Instrument[i].PitchPanSeperation,	j->Instrument[i].PitchPanCenter, j->Instrument[i].GlobalVolume,	j->Instrument[i].DefaultPan);
		#endif

		pack_fseek(f, 38);

		for	(int k=0; k<120; k++) {
			j->Instrument[i].NoteNote[k] = pack_getc(f);
			j->Instrument[i].NoteSample[k] = pack_getc(f) -	1;
		}

		j->Instrument[i].VolumeEnvelope.Flag = pack_getc(f);
		j->Instrument[i].VolumeEnvelope.NumNodes = pack_getc(f);
		j->Instrument[i].VolumeEnvelope.LoopBegin =	pack_getc(f);
		j->Instrument[i].VolumeEnvelope.LoopEnd	= pack_getc(f);
		j->Instrument[i].VolumeEnvelope.SustainLoopBegin = pack_getc(f);
		j->Instrument[i].VolumeEnvelope.SustainLoopEnd = pack_getc(f);
		for	(int k=0; k<j->Instrument[i].VolumeEnvelope.NumNodes; k++) {
			j->Instrument[i].VolumeEnvelope.NodeY[k] = pack_getc(f);
			j->Instrument[i].VolumeEnvelope.NodeTick[k]	= pack_igetw(f);
		}
		pack_fseek(f, 75 - j->Instrument[i].VolumeEnvelope.NumNodes	* 3);
		
		j->Instrument[i].PanningEnvelope.Flag =	pack_getc(f);
		j->Instrument[i].PanningEnvelope.NumNodes =	pack_getc(f);
		j->Instrument[i].PanningEnvelope.LoopBegin = pack_getc(f);
		j->Instrument[i].PanningEnvelope.LoopEnd = pack_getc(f);
		j->Instrument[i].PanningEnvelope.SustainLoopBegin =	pack_getc(f);
		j->Instrument[i].PanningEnvelope.SustainLoopEnd	= pack_getc(f);
		for	(int k=0; k<j->Instrument[i].PanningEnvelope.NumNodes; k++)	{
			j->Instrument[i].PanningEnvelope.NodeY[k] =	pack_getc(f);
			j->Instrument[i].PanningEnvelope.NodeTick[k] = pack_igetw(f);
		}
		pack_fseek(f, 75 - j->Instrument[i].PanningEnvelope.NumNodes * 3);
		
		j->Instrument[i].PitchEnvelope.Flag	= pack_getc(f);
		j->Instrument[i].PitchEnvelope.NumNodes	= pack_getc(f);
		j->Instrument[i].PitchEnvelope.LoopBegin = pack_getc(f);
		j->Instrument[i].PitchEnvelope.LoopEnd = pack_getc(f);
		j->Instrument[i].PitchEnvelope.SustainLoopBegin	= pack_getc(f);
		j->Instrument[i].PitchEnvelope.SustainLoopEnd =	pack_getc(f);
		for	(int k=0; k<j->Instrument[i].PitchEnvelope.NumNodes; k++) {
			j->Instrument[i].PitchEnvelope.NodeY[k]	= pack_getc(f);
			j->Instrument[i].PitchEnvelope.NodeTick[k] = pack_igetw(f);
		}
	}

	if (j->NumSamples)
		j->Sample =	(MODULUS_SAMPLE*)malloc(sizeof(MODULUS_SAMPLE) * j->NumSamples);

	#ifdef DEBUG_SAMPLES
	if (!j->Sample)
		printf("No Mem for Samples!\n");
	#endif
			
	for	(int i=0; i<j->NumSamples; i++)	{
		int	sam_samptr,	convert;
	
		pack_fclose(f);
		f =	pack_fopen(file, "rb");
		#ifdef DEBUG_SAMPLES
		if (!f)
			printf("Error opening!\n");
		#endif
		
		pack_fseek(f, samoffs[i] + 17);

		j->Sample[i].GlobalVolume =	pack_getc(f);
		j->Sample[i].Flag =	pack_getc(f);
		j->Sample[i].Volume	= pack_getc(f);

		#ifdef DEBUG_SAMPLES
		printf("S%02i @ 0x%X, Vol: %i/%i, Flag: %i", i,	samoffs[i],	j->Sample[i].GlobalVolume, j->Sample[i].Volume,	j->Sample[i].Flag);
		#endif
		
		pack_fseek(f, 26);
		
		convert	= pack_getc(f);
		pack_getc(f); //Panning ?

		j->Sample[i].SampleLength =	pack_igetl(f);
		j->Sample[i].LoopBegin = pack_igetl(f);
		j->Sample[i].LoopEnd = pack_igetl(f);
		j->Sample[i].C5Speed = pack_igetl(f);
		j->Sample[i].SustainLoopBegin =	pack_igetl(f);
		j->Sample[i].SustainLoopEnd	= pack_igetl(f);

		#ifdef DEBUG_SAMPLES
		printf(", SLen: %i, LpB: %i, LpE: %i, C5S: %i\n", j->Sample[i].SampleLength, j->Sample[i].LoopBegin, j->Sample[i].LoopEnd, j->Sample[i].C5Speed);
		#endif

		sam_samptr = pack_igetl(f);
		
		j->Sample[i].VibratoSpeed =	pack_getc(f);
		j->Sample[i].VibratoDepth =	pack_getc(f);
		j->Sample[i].VibratoRate = pack_getc(f);
		j->Sample[i].VibratoWaveForm = pack_getc(f);

		#ifdef DEBUG_SAMPLES
		printf("SusLpB: %i, SusLpE: %i, VibSp: %i, VibDep: %i, VibWav: %i, VibRat: %i\n", j->Sample[i].SustainLoopBegin, j->Sample[i].SustainLoopEnd, j->Sample[i].VibratoSpeed, j->Sample[i].VibratoDepth,	j->Sample[i].VibratoWaveForm, j->Sample[i].VibratoRate);
		#endif

		if (j->Sample[i].Flag &	1 == 0)
			continue;
		
		pack_fclose(f);
		f =	pack_fopen(file, "rb");
		pack_fseek(f, sam_samptr);

		int	len	= j->Sample[i].SampleLength	* (j->Sample[i].Flag & 2 ? sizeof(short) : 1) *	(j->Sample[i].Flag & 4 ? 2:	1);

		#ifdef DEBUG_SAMPLES
		printf("Len: %i, Size: %i KB\n", j->Sample[i].SampleLength,	len/1024);
		#endif

		SAMPLE *sam	= create_sample(j->Sample[i].Flag &	2 ?	16 : 8,	j->Sample[i].Flag &	4 ?	TRUE : FALSE, j->Sample[i].C5Speed,	j->Sample[i].SampleLength);
		
		if (j->Sample[i].Flag &	8) { // If the sample is packed, then we must unpack it
			if (j->Sample[i].Flag &	2)
				decompress16(f,	sam->data, j->Sample[i].SampleLength, tver2);
			else
				decompress8(f, sam->data, j->Sample[i].SampleLength, tver2);
		} else {
			pack_fread(sam->data, len, f);
		}
		
		if (j->Sample[i].Flag &	SAMPLE_USELOOP)	{
			sam->loop_start	= j->Sample[i].LoopBegin;
			sam->loop_end =	j->Sample[i].LoopEnd;
		}

		j->Sample[i].Sample	= sam;
		
		void *dat =	sam->data;

		if (convert	& 2) { //Change the byte order for 16-bit samples:
			if (sam->bits == 16) {
				for	(int k=0; k<len; k+=2) {
					int	l =	((char*)dat)[k];
					((char*)dat)[k]	= ((char*)dat)[k+1];
					((char*)dat)[k+1] =	l;
					
				}
			}
			else {
				for	(int k=0; k<len; k+=2) {
					int	l =	((char*)dat)[k];
					((char*)dat)[k]	= ((char*)dat)[k+1];
					((char*)dat)[k+1] =	l;
					
				}
			}
		}
		if (convert	& 1) { //Convert to unsigned
	   		if (sam->bits == 8)	{
	   			for	(int k=0; k<len; k++) {
		   			((char*)dat)[k]	^= 0x80;
		   		}
	   		}
	   		else {
	   			for	(int k=0; k<(len>>1); k++) {
		   			((short*)dat)[k] ^=	0x8000;
		   		}
			}			 
		}
	}

	if (j->NumPatterns)
		j->Pattern = (MODULUS_PATTERN*)malloc(sizeof(MODULUS_PATTERN) *	j->NumPatterns);
	unsigned char *buf = (unsigned char*)alloca(65536);
	unsigned char *cmask = (unsigned char*)alloca(64),
		*cnote = (unsigned char*)alloca(64),
		*cinstrument = (unsigned char*)alloca(64),
		*cvol =	(unsigned char*)alloca(64),
		*ccom =	(unsigned char*)alloca(64),
		*ccomval = (unsigned char*)alloca(64);
	
	for	(int i=0; i<j->NumPatterns;	i++) {
		int	numnotes = 0, len, pos = 0,	mask = 0, chn =	0;

		memset(cmask, 0, 64);
		memset(cnote, 0, 64);
		memset(cinstrument,	0, 64);
		memset(cvol, 0,	64);
		memset(ccom, 0,	64);
		memset(ccomval,	0, 64);
	
		pack_fclose(f);
		f =	pack_fopen(file, "rb");
		pack_fseek(f, patoffs[i]);
		
		len	= pack_igetw(f);
		j->Pattern[i].NumRows =	pack_igetw(f);

		pack_fseek(f, 4);
		pack_fread(buf,	len, f);

		while (pos < len) {
			int	b =	buf[pos];
			pos++;
			if (!b)	{ //If end of row:
				numnotes++;
				continue;
			}
			chn	= (b - 1) &	63;

			if (b &	128) {
				mask = buf[pos];
				pos++;
				cmask[chn] = mask;
			}
			else
				mask = cmask[chn];
				
			if (mask)
				numnotes++;
			if (mask & 1)
				pos++;
			if (mask & 2)
				pos++;
			if (mask & 4)
				pos++;
			if (mask & 8)
				pos+=2;	//Guessing here
		}
		j->Pattern[i].NumNotes = numnotes;
		j->Pattern[i].Note = (MODULUS_NOTE*)malloc(sizeof(MODULUS_NOTE)	* numnotes);
		memset(j->Pattern[i].Note, 0, sizeof(MODULUS_NOTE) * numnotes);
		
		pos	= 0;
		memset(cmask, 0, 64);
		mask = 0;
		numnotes = 0;
		while (pos < len) {
			int	b =	buf[pos];
			#ifdef DEBUG_PATTERNS
			printf("NumNote: %i ", numnotes);
			#endif
			
			pos++;			  
			if (!b)	{ //If end of row:
				j->Pattern[i].Note[numnotes].Channel = -1;
				numnotes++;
				#ifdef DEBUG_PATTERNS
				printf("Channel: -1\n");
				#endif
				continue;
			}
			chn	= (b - 1) &	63;

			if (b &	128) {
				mask = buf[pos];
				pos++;
				cmask[chn] = mask;
			}
			else
				mask = cmask[chn];
			#ifdef DEBUG_PATTERNS
			printf("Channel: %i Mask: %i ",	chn, mask);
			#endif
				
			if (mask)
				j->Pattern[i].Note[numnotes].Channel = chn;

			if (mask & 1) {
				j->Pattern[i].Note[numnotes].Note =	buf[pos];
				j->Pattern[i].Note[numnotes].Mask |= 1;
				cnote[chn] = buf[pos];				  
				#ifdef DEBUG_PATTERNS
				printf("Note: %i ",	buf[pos]);
				#endif
				pos++;
			}
			if (mask & 2) {
				j->Pattern[i].Note[numnotes].Instrument	= buf[pos];
				j->Pattern[i].Note[numnotes].Mask |= 2;
				cinstrument[chn] = buf[pos];
				#ifdef DEBUG_PATTERNS
				printf("Inst: %i ",	buf[pos]);
				#endif
				pos++;
			}
			if (mask & 4) {
				if (buf[pos] <=	64 || (buf[pos]	>= 128 && buf[pos] <= 192))
					if (buf[pos] <=	64)	{
						j->Pattern[i].Note[numnotes].Volume	= buf[pos];
						j->Pattern[i].Note[numnotes].Mask |= 4;
					}
					else {
						j->Pattern[i].Note[numnotes].Panning = buf[pos]	- 128;
						j->Pattern[i].Note[numnotes].Mask |= 8;
					}
				#ifdef DEBUG_PATTERNS
				printf("Vol: %i ", buf[pos]);
				#endif
				cvol[chn] =	buf[pos];
				pos++;
			}
			if (mask & 8) {
				j->Pattern[i].Note[numnotes].Command = buf[pos];
				j->Pattern[i].Note[numnotes].CommandValue =	buf[pos+1];
				j->Pattern[i].Note[numnotes].Mask |= 16;
				ccom[chn] =	buf[pos];
				ccomval[chn] = buf[pos+1];
				#ifdef DEBUG_PATTERNS
				printf("Com: %i CommArg: %i ", buf[pos], buf[pos+1]);
				#endif
				pos+=2;
			}
			if (mask & 16) {
				j->Pattern[i].Note[numnotes].Note =	cnote[chn];
				j->Pattern[i].Note[numnotes].Mask |= 1;
				#ifdef DEBUG_PATTERNS
				printf("LNote: %i ", cnote[chn]);
				#endif
			}
			if (mask & 32) {
				j->Pattern[i].Note[numnotes].Instrument	= cinstrument[chn];
				j->Pattern[i].Note[numnotes].Mask |= 2;
				#ifdef DEBUG_PATTERNS
				printf("LInst: %i ", cinstrument[chn]);
				#endif
			}
			if (mask & 64) {
				if (cvol[chn] <= 64	|| (cvol[chn] >= 128 &&	cvol[chn] <= 192))
					if (cvol[chn] <= 64) {
						j->Pattern[i].Note[numnotes].Volume	= cvol[chn];
						j->Pattern[i].Note[numnotes].Mask |= 4;
					}
					else {
						j->Pattern[i].Note[numnotes].Panning = cvol[chn] - 128;
						j->Pattern[i].Note[numnotes].Mask |= 8;
					}
				#ifdef DEBUG_PATTERNS
				printf("LVol: %i ",	cvol[chn]);
				#endif
			}			 
			if (mask & 128)	{
				j->Pattern[i].Note[numnotes].Command = ccom[chn];
				j->Pattern[i].Note[numnotes].CommandValue =	ccomval[chn];
				j->Pattern[i].Note[numnotes].Mask |= 16;
				#ifdef DEBUG_PATTERNS
				printf("LCom: %i LComArg: %i ",	ccom[chn], ccomval[chn]);
				#endif
			}
			#ifdef DEBUG_PATTERNS
			printf("\n");
			#endif
			if (mask)
				numnotes++;
			#ifdef DEBUG_PATTERNS
			rest(1000);
			#endif
		}
	}
	if (insoffs)
		free(insoffs);
	if (samoffs)
		free(samoffs);
	if (patoffs)
		free(patoffs);

	return j;
}
