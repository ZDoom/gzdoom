/*
** music_dumb.cpp
** Alternative module player, using a modified DUMB for decoding.
** Based on the Foobar2000 component foo_dumb, version 0.9.8.4.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include "i_musicinterns.h"
#include "i_system.h"

#undef CDECL	// w32api's windef.h defines this
#include "../dumb/include/dumb.h"
#include "../dumb/include/internal/it.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class input_mod : public StreamSong
{
public:
	input_mod(DUH *myduh);
	~input_mod();
	//bool SetPosition(int ms);
	bool SetSubsong(int subsong);
	void Play(bool looping, int subsong);
	FString GetStats();

	FString Codec;
	FString TrackerVersion;
	FString FormatVersion;
	int NumChannels;
	int NumPatterns;
	int NumOrders;

protected:
	int srate, interp, volramp;
	int start_order;
	double delta;
	double length;
	bool eof;
	size_t written;
	DUH *duh;
	DUH_SIGRENDERER *sr;
	FCriticalSection crit_sec;

	bool open2(long pos);
	long render(double volume, double delta, long samples, sample_t **buffer);
	int decode_run(void *buffer, unsigned int size);
	static bool read(SoundStream *stream, void *buff, int len, void *userdata);
};

#pragma pack(1)

typedef struct tagITFILEHEADER
{
	uint32_t id;			// 0x4D504D49
	char songname[26];
	uint16_t reserved1;		// 0x1004
	uint16_t ordnum;
	uint16_t insnum;
	uint16_t smpnum;
	uint16_t patnum;
	uint16_t cwtv;
	uint16_t cmwt;
	uint16_t flags;
	uint16_t special;
	uint8_t globalvol;
	uint8_t mv;
	uint8_t speed;
	uint8_t tempo;
	uint8_t sep;
	uint8_t zero;
	uint16_t msglength;
	uint32_t msgoffset;
	uint32_t reserved2;
	uint8_t chnpan[64];
	uint8_t chnvol[64];
} FORCE_PACKED ITFILEHEADER, *PITFILEHEADER;

typedef struct MODMIDICFG
{
	char szMidiGlb[9*32];      // changed from CHAR
	char szMidiSFXExt[16*32];  // changed from CHAR
	char szMidiZXXExt[128*32]; // changed from CHAR
} FORCE_PACKED MODMIDICFG, *LPMODMIDICFG;

#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(Int,  mod_samplerate,				0,	   CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int,  mod_volramp,					2,	   CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int,  mod_interp,					DUMB_LQ_CUBIC,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Bool, mod_autochip,				false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int,  mod_autochip_size_force,		100,   CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int,  mod_autochip_size_scan,		500,   CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int,  mod_autochip_scan_threshold, 12,	   CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CUSTOM_CVAR(Float, mod_dumb_mastervolume, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.5f) self = 0.5f;
	else if (self > 16.f) self = 16.f;
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// time_to_samples
//
//==========================================================================

static inline uint64_t time_to_samples(double p_time,int p_sample_rate)
{
	return (uint64_t)floor((double)p_sample_rate * p_time + 0.5);
}

//==========================================================================
//
// ReadDUH
//
// Reads metadata from a generic module.
//
//==========================================================================

static void ReadDUH(DUH * duh, input_mod *info, bool meta, bool dos)
{
	if (!duh) return;

	DUMB_IT_SIGDATA * itsd = duh_get_it_sigdata(duh);

	if (!itsd) return;

	info->NumChannels = itsd->n_pchannels;
	info->NumPatterns = itsd->n_patterns;
	info->NumOrders = itsd->n_orders;

	if (meta)
	{
		if (itsd->name[0])
		{
			//if (dos) info.meta_add(field_title, string_utf8_from_oem((char*)&itsd->name, sizeof(itsd->name)));
			//else info.meta_add(field_title, pfc::stringcvt::string_utf8_from_ansi((char *)&itsd->name, sizeof(itsd->name)));
		}
		if (itsd->song_message && *itsd->song_message)
		{
			//if (dos) info.meta_add(field_comment, string_utf8_from_oem_multiline((char*)itsd->song_message));
			//else info.meta_add(field_comment, pfc::stringcvt::string_utf8_from_ansi((char *)itsd->song_message));
		}
	}

	info->Codec = duh_get_tag(duh, "FORMAT");
	info->TrackerVersion = duh_get_tag(duh, "TRACKERVERSION");
	info->FormatVersion = duh_get_tag(duh, "FORMATVERSION");

#if 0
	FString name;

	if (itsd->n_samples)
	{
		int i, n;
		//info.info_set_int(field_samples, itsd->n_samples);

		if (meta && itsd->sample)
		{
			for (i = 0, n = itsd->n_samples; i < n; i++)
			{
				if (itsd->sample[i].name[0])
				{
					name = "smpl";
					if (i < 10) name += '0';
					name += '0' + i;
					//if (dos) info.meta_add(name, string_utf8_from_oem((char*)&itsd->sample[i].name, sizeof(itsd->sample[i].name)));
					//else info.meta_add(name, pfc::stringcvt::string_utf8_from_ansi((char *)&itsd->sample[i].name, sizeof(itsd->sample[i].name)));
				}
			}
		}
	}

	if (itsd->n_instruments)
	{
		//info.info_set_int(field_instruments, itsd->n_instruments);

		if (meta && itsd->instrument)
		{
			for (i = 0, n = itsd->n_instruments; i < n; i++)
			{
				if (itsd->instrument[i].name[0])
				{
					name = "inst";
					if (i < 10) name += '0';
					name += '0' + i;
					//if (dos) info.meta_add(name, string_utf8_from_oem((char*)&itsd->instrument[i].name, sizeof(itsd->instrument[i].name)));
					//else info.meta_add(name, pfc::stringcvt::string_utf8_from_ansi((char *)&itsd->instrument[i].name, sizeof(itsd->instrument[i].name)));
				}
			}
		}
	}
#endif
}

//==========================================================================
//
// ReadIT
//
// Reads metadata from an IT file.
//
//==========================================================================

static bool ReadIT(const uint8_t * ptr, unsigned size, input_mod *info, bool meta)
{
	PITFILEHEADER pifh = (PITFILEHEADER) ptr;
	if ((!ptr) || (size < 0x100)) return false;
	if ((LittleLong(pifh->id) != 0x4D504D49) ||
		(LittleShort(pifh->insnum) >= 256) ||
		(!pifh->smpnum) || (LittleShort(pifh->smpnum) > 4000) || // XXX
		(!pifh->ordnum)) return false;
	if (sizeof(ITFILEHEADER) + LittleShort(pifh->ordnum) +
		LittleShort(pifh->insnum)*4 +
		LittleShort(pifh->smpnum)*4 +
		LittleShort(pifh->patnum)*4 > size) return false;

	FString ver;

	ver.Format("IT v%u.%02x", LittleShort(pifh->cmwt) >> 8, LittleShort(pifh->cmwt) & 255);
	info->Codec = ver;

	ver.Format("%u.%02x", LittleShort(pifh->cwtv) >> 8, LittleShort(pifh->cwtv) & 255);
	info->TrackerVersion = ver;

	//if ( pifh->smpnum ) info.info_set_int( field_samples, LittleShort(pifh->smpnum) );
	//if ( pifh->insnum ) info.info_set_int( field_instruments, LittleShort(pifh->insnum) );

	//if ( meta && pifh->songname[0] ) info.meta_add( field_title, string_utf8_from_it( (char*)&pifh->songname, 26 ) );

	unsigned n, l, m_nChannels = 0;

	// bah, some file (jm-romance.it) with message length rounded up to a multiple of 256 (384 instead of 300)
	unsigned msgoffset = LittleLong(pifh->msgoffset);
	unsigned msgend = msgoffset + LittleShort(pifh->msglength);

	uint32_t * offset;
//	FString name;
	
	if (meta)
	{
		offset = (uint32_t *)(ptr + 0xC0 + LittleShort(pifh->ordnum) + LittleShort(pifh->insnum) * 4);

		for (n = 0, l = LittleShort(pifh->smpnum); n < l; n++, offset++)
		{
			uint32_t offset_n = LittleLong( *offset );
			if ( offset_n >= msgoffset && offset_n < msgend ) msgend = offset_n;
			if ((!offset_n) || (offset_n + 0x14 + 26 + 2 >= size)) continue;
			// XXX
			if (ptr[offset_n] == 0 && ptr[offset_n + 1] == 0 &&
				ptr[offset_n + 2] == 'I' && ptr[offset_n + 3] == 'M' &&
				ptr[offset_n + 4] == 'P' && ptr[offset_n + 5] == 'S')
			{
				offset_n += 2;
			}
#if 0
			if (*(ptr + offset_n + 0x14))
			{
				name = "smpl";
				if (n < 10) name << '0';
				name << ('0' + n);
				//info.meta_add(name, string_utf8_from_it((const char *) ptr + offset_n + 0x14, 26));
			}
#endif
		}

		offset = (uint32_t *)(ptr + 0xC0 + LittleShort(pifh->ordnum));

		for (n = 0, l = LittleShort(pifh->insnum); n < l; n++, offset++)
		{
			uint32_t offset_n = LittleLong( *offset );
			if ( offset_n >= msgoffset && offset_n < msgend ) msgend = offset_n;
			if ((!offset_n) || (offset_n + 0x20 + 26 >= size)) continue;
#if 0
			if (*(ptr + offset_n + 0x20))
			{
				name = "inst";
				if (n < 10) name << '0';
				name << ('0' + n);
				//info.meta_add(name, string_utf8_from_it((const char *) ptr + offset_n + 0x20, 26));
			}
#endif
		}
	}

	unsigned pos = 0xC0 + LittleShort(pifh->ordnum) + LittleShort(pifh->insnum) * 4 + LittleShort(pifh->smpnum) * 4 + LittleShort(pifh->patnum) * 4;

	if (pos < size)
	{
		uint16_t val16 = LittleShort( *(uint16_t *)(ptr + pos) );
		pos += 2;
		if (pos + val16 * 8 < size) pos += val16 * 8;
	}

	if (LittleShort(pifh->flags) & 0x80)
	{
		if (pos + sizeof(MODMIDICFG) < size)
		{
			pos += sizeof(MODMIDICFG);
		}
	}

	if ((pos + 8 < size) && (*(uint32_t *)(ptr + pos) == MAKE_ID('P','N','A','M')))
	{
		unsigned len = LittleLong(*(uint32_t *)(ptr + pos + 4));
		pos += 8;
		if ((pos + len <= size) && (len <= 240 * 32) && (len >= 32))
		{
			if (meta)
			{
				l = len / 32;
				for (n = 0; n < l; n++)
				{
#if 0
					if (*(ptr + pos + n * 32))
					{
						name = "patt";
						if (n < 10) name << '0';
						name << ('0' + n);
						//info.meta_add(name, string_utf8_from_it((const char *) ptr + pos + n * 32, 32));
					}
#endif
				}
			}
			pos += len;
		}
	}

	if ((pos + 8 < size) && (*(uint32_t *)(ptr + pos) == MAKE_ID('C','N','A','M')))
	{
		unsigned len = LittleLong(*(uint32_t *)(ptr + pos + 4));
		pos += 8;
		if ((pos + len <= size) && (len <= 64 * 20) && (len >= 20))
		{
			l = len / 20;
			m_nChannels = l;
			if (meta)
			{
				for (n = 0; n < l; n++)
				{
#if 0
					if (*(ptr + pos + n * 20))
					{
						name = "chan";
						if (n < 10) name << '0';
						name << ('0' + n);
						//info.meta_add(name, string_utf8_from_it((const char *) ptr + pos + n * 20, 20));
					}
#endif
				}
			}
			pos += len;
		}
	}

	offset = (uint32_t *)(ptr + 0xC0 + LittleShort(pifh->ordnum) + LittleShort(pifh->insnum) * 4 + LittleShort(pifh->smpnum) * 4);

	uint8_t chnmask[64];

	for (n = 0, l = LittleShort(pifh->patnum); n < l; n++)
	{
		memset(chnmask, 0, sizeof(chnmask));
		uint32_t offset_n = LittleLong( offset[n] );
		if ((!offset_n) || (offset_n + 4 >= size)) continue;
		unsigned len = LittleShort(*(uint16_t *)(ptr + offset_n));
		unsigned rows = LittleShort(*(uint16_t *)(ptr + offset_n + 2));
		if ((rows < 4) || (rows > 256)) continue;
		if (offset_n + 8 + len > size) continue;
		unsigned i = 0;
		const uint8_t * p = ptr + offset_n + 8;
		unsigned nrow = 0;
		while (nrow < rows)
		{
			if (i >= len) break;
			uint8_t b = p[i++];
			if (!b)
			{
				nrow++;
				continue;
			}
			unsigned ch = b & 0x7F;
			if (ch) ch = (ch - 1) & 0x3F;
			if (b & 0x80)
			{
				if (i >= len) break;
				chnmask[ch] = p[i++];
			}
			// Channel used
			if (chnmask[ch] & 0x0F)
			{
				if ((ch >= m_nChannels) && (ch < 64)) m_nChannels = ch+1;
			}
			// Note
			if (chnmask[ch] & 1) i++;
			// Instrument
			if (chnmask[ch] & 2) i++;
			// Volume
			if (chnmask[ch] & 4) i++;
			// Effect
			if (chnmask[ch] & 8) i += 2;
			if (i >= len) break;
		}
	}

	if ( meta && ( LittleShort(pifh->special) & 1 ) && ( msgend - msgoffset ) && ( msgend < size ) )
	{
		const char * str = (const char *) ptr + msgoffset;
		FString msg(str);
		//info.meta_add( field_comment, string_utf8_from_it_multiline( msg ) );
	}

	info->NumChannels = m_nChannels;
	info->NumPatterns = LittleShort(pifh->patnum);
	info->NumOrders = LittleShort(pifh->ordnum);

	return true;
}

//==========================================================================
//
// DUMB memory file system
//
//==========================================================================

typedef struct tdumbfile_mem_status
{
	const uint8_t *ptr;
	unsigned int offset, size;
} dumbfile_mem_status;

static int DUMBCALLBACK dumbfile_mem_skip(void *f, long n)
{
	dumbfile_mem_status * s = (dumbfile_mem_status *) f;
	s->offset += n;
	if (s->offset > s->size)
	{
		s->offset = s->size;
		return 1;
	}
	return 0;
}

static int DUMBCALLBACK dumbfile_mem_getc(void *f)
{
	dumbfile_mem_status * s = (dumbfile_mem_status *) f;
	if (s->offset < s->size)
	{
		return *(s->ptr + s->offset++);
	}
	return -1;
}

static int32 DUMBCALLBACK dumbfile_mem_getnc(char *ptr, int32 n, void *f)
{
	dumbfile_mem_status * s = (dumbfile_mem_status *) f;
	long max = s->size - s->offset;
	if (max > n) max = n;
	if (max)
	{
		memcpy(ptr, s->ptr + s->offset, max);
		s->offset += max;
	}
	return max;
}

static int DUMBCALLBACK dumbfile_mem_seek(void *f, long n)
{
	dumbfile_mem_status * s = (dumbfile_mem_status *) f;
	s->offset = n;
	if (s->offset > s->size)
	{
		s->offset = s->size;
		return 1;
	}
	return 0;
}

static long DUMBCALLBACK dumbfile_mem_get_size(void *f)
{
	dumbfile_mem_status * s = (dumbfile_mem_status *) f;
	return s->size;
}

static DUMBFILE_SYSTEM mem_dfs = {
	NULL, // open
	&dumbfile_mem_skip,
	&dumbfile_mem_getc,
	&dumbfile_mem_getnc,
	NULL, // close
	&dumbfile_mem_seek,
	&dumbfile_mem_get_size
};

//==========================================================================
//
// dumb_read_allfile
//
//==========================================================================

DUMBFILE *dumb_read_allfile(dumbfile_mem_status *filestate, uint8_t *start, FileReader &reader, int lenhave, int lenfull)
{
	filestate->size = lenfull;
	filestate->offset = 0;
	if (lenhave >= lenfull)
		filestate->ptr = (uint8_t *)start;
    else
    {
        uint8_t *mem = new uint8_t[lenfull];
        memcpy(mem, start, lenhave);
        if (reader.Read(mem + lenhave, lenfull - lenhave) != (lenfull - lenhave))
        {
            delete[] mem;
            return NULL;
        }
        filestate->ptr = mem;
    }
    return dumbfile_open_ex(filestate, &mem_dfs);
}

//==========================================================================
//
// MOD_SetAutoChip
//
// Disables interpolation for short samples that meet criteria set by
// the cvars referenced at the top of this function.
//
//==========================================================================

static void MOD_SetAutoChip(DUH *duh)
{
	int size_force = mod_autochip_size_force;
	int size_scan = mod_autochip_size_scan;
	int scan_threshold_8 = ((mod_autochip_scan_threshold * 0x100) + 50) / 100;
	int scan_threshold_16 = ((mod_autochip_scan_threshold * 0x10000) + 50) / 100;
	DUMB_IT_SIGDATA * itsd = duh_get_it_sigdata(duh);

	if (itsd)
	{
		for (int i = 0, j = itsd->n_samples; i < j; i++)
		{
			IT_SAMPLE * sample = &itsd->sample[i];
			if (sample->flags & IT_SAMPLE_EXISTS)
			{
				int channels = sample->flags & IT_SAMPLE_STEREO ? 2 : 1;
				if (sample->length < size_force) sample->max_resampling_quality = 0;
				else if (sample->length < size_scan)
				{
					if ((sample->flags & (IT_SAMPLE_LOOP|IT_SAMPLE_PINGPONG_LOOP)) == IT_SAMPLE_LOOP)
					{
						int loop_start = sample->loop_start * channels;
						int loop_end = sample->loop_end * channels;
						int s1, s2;
						if (sample->flags & IT_SAMPLE_16BIT)
						{
							s1 = ((signed short *)sample->data)[loop_start];
							s2 = ((signed short *)sample->data)[loop_end - channels];
							if (abs(s1 - s2) > scan_threshold_16)
							{
								sample->max_resampling_quality = 0;
								continue;
							}
							if (channels == 2)
							{
								s1 = ((signed short *)sample->data)[loop_start + 1];
								s2 = ((signed short *)sample->data)[loop_end - 1];
								if (abs(s1 - s2) > scan_threshold_16)
								{
									sample->max_resampling_quality = 0;
									continue;
								}
							}
						}
						else
						{
							s1 = ((signed char *)sample->data)[loop_start];
							s2 = ((signed char *)sample->data)[loop_end - channels];
							if (abs(s1 - s2) > scan_threshold_8)
							{
								sample->max_resampling_quality = 0;
								continue;
							}
							if (channels == 2)
							{
								s1 = ((signed char *)sample->data)[loop_start + 1];
								s2 = ((signed char *)sample->data)[loop_end - 1];
								if (abs(s1 - s2) > scan_threshold_8)
								{
									sample->max_resampling_quality = 0;
									continue;
								}
							}
						}
					}
					if ((sample->flags & (IT_SAMPLE_SUS_LOOP|IT_SAMPLE_PINGPONG_SUS_LOOP)) == IT_SAMPLE_SUS_LOOP)
					{
						int sus_loop_start = sample->sus_loop_start * channels;
						int sus_loop_end = sample->sus_loop_end * channels;
						int s1, s2;
						if (sample->flags & IT_SAMPLE_16BIT)
						{
							s1 = ((signed short *)sample->data)[sus_loop_start];
							s2 = ((signed short *)sample->data)[sus_loop_end - channels];
							if (abs(s1 - s2) > scan_threshold_16)
							{
								sample->max_resampling_quality = 0;
								continue;
							}
							if (channels == 2)
							{
								s1 = ((signed short *)sample->data)[sus_loop_start + 1];
								s2 = ((signed short *)sample->data)[sus_loop_end - 1];
								if (abs(s1 - s2) > scan_threshold_16)
								{
									sample->max_resampling_quality = 0;
									continue;
								}
							}
						}
						else
						{
							s1 = ((signed char *)sample->data)[sus_loop_start];
							s2 = ((signed char *)sample->data)[sus_loop_end - channels];
							if (abs(s1 - s2) > scan_threshold_8)
							{
								sample->max_resampling_quality = 0;
								continue;
							}
							if (channels == 2)
							{
								s1 = ((signed char *)sample->data)[sus_loop_start + 1];
								s2 = ((signed char *)sample->data)[sus_loop_end - 1];
								if (abs(s1 - s2) > scan_threshold_8)
								{
									sample->max_resampling_quality = 0;
									continue;
								}
							}
						}
					}

					int k, l = sample->length * channels;
					if (sample->flags & IT_SAMPLE_LOOP) l = sample->loop_end * channels;
					if (sample->flags & IT_SAMPLE_16BIT)
					{
						for (k = channels; k < l; k += channels)
						{
							if (abs(((signed short *)sample->data)[k - channels] - ((signed short *)sample->data)[k]) > scan_threshold_16)
							{
								break;
							}
						}
						if (k < l)
						{
							sample->max_resampling_quality = 0;
							continue;
						}
						if (channels == 2)
						{
							for (k = 2 + 1; k < l; k += 2)
							{
								if (abs(((signed short *)sample->data)[k - 2] - ((signed short *)sample->data)[k]) > scan_threshold_16)
								{
									break;
								}
							}
						}
						if (k < l)
						{
							sample->max_resampling_quality = 0;
							continue;
						}
					}
					else
					{
						for (k = channels; k < l; k += channels)
						{
							if (abs(((signed char *)sample->data)[k - channels] - ((signed char *)sample->data)[k]) > scan_threshold_8)
							{
								break;
							}
						}
						if (k < l)
						{
							sample->max_resampling_quality = 0;
							continue;
						}
						if (channels == 2)
						{
							for (k = 2 + 1; k < l; k += 2)
							{
								if (abs(((signed char *)sample->data)[k - 2] - ((signed char *)sample->data)[k]) > scan_threshold_8)
								{
									break;
								}
							}
						}
						if (k < l)
						{
							sample->max_resampling_quality = 0;
							continue;
						}
					}
				}
			}
		}
	}
}

//==========================================================================
//
// MOD_OpenSong
//
//==========================================================================

MusInfo *MOD_OpenSong(FileReader &reader)
{
	DUH *duh = 0;
	int headsize;
	union
	{
		uint8_t start[64];
		uint32_t dstart[64/4];
	};
	dumbfile_mem_status filestate;
	DUMBFILE *f = NULL;
	input_mod *state = NULL;

	bool is_it = false;
	bool is_dos = true;

    int size = (int)reader.GetLength();
	auto fpos = reader.Tell();

	filestate.ptr = start;
	filestate.offset = 0;
	headsize = MIN((int)sizeof(start), size);

    if (headsize != reader.Read(start, headsize))
    {
        return NULL;
    }

	if (size >= 4 && dstart[0] == MAKE_ID('I','M','P','M'))
	{
		is_it = true;
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_it_quick(f);
		}
	}
	else if (size >= 17 && !memcmp(start, "Extended Module: ", 17))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_xm_quick(f);
		}
	}
	else if (size >= 0x30 && dstart[11] == MAKE_ID('S','C','R','M'))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_s3m_quick(f);
		}
	}
	else if (size >= 1168 &&
		/*start[28] == 0x1A &&*/ start[29] == 2 &&
		( !memcmp( &start[20], "!Scream!", 8 ) ||
		  !memcmp( &start[20], "BMOD2STM", 8 ) ||
		  !memcmp( &start[20], "WUZAMOD!", 8 ) ) )
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_stm_quick(f);
		}
	}
	else if (size >= 2 &&
		((start[0] == 0x69 && start[1] == 0x66) ||
		 (start[0] == 0x4A && start[1] == 0x4E)))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_669_quick(f);
		}
	}
	else if (size >= 0x30 && dstart[11] == MAKE_ID('P','T','M','F'))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_ptm_quick(f);
		}
	}
	else if (size >= 4 && dstart[0] == MAKE_ID('P','S','M',' '))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_psm_quick(f, 0/*start_order*/);
			/*start_order = 0;*/
		}
	}
	else if (size >= 4 && dstart[0] == (uint32_t)MAKE_ID('P','S','M',254))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_old_psm_quick(f);
		}
	}
	else if (size >= 3 && start[0] == 'M' && start[1] == 'T' && start[2] == 'M')
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_mtm_quick(f);
		}
	}
	else if (size >= 12 && dstart[0] == MAKE_ID('R','I','F','F') &&
		(dstart[2] == MAKE_ID('D','S','M','F') ||
		 dstart[2] == MAKE_ID('A','M',' ',' ') ||
		 dstart[2] == MAKE_ID('A','M','F','F')))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_riff_quick(f);
		}
	}
	else if (size >= 32 &&
		!memcmp( start, "ASYLUM Music Format", 19 ) &&
		!memcmp( start + 19, " V1.0", 5 ) )
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_asy_quick(f);
		}
	}
	else if (size >= 8 &&
		dstart[0] == MAKE_ID('O','K','T','A') &&
		dstart[1] == MAKE_ID('S','O','N','G'))
	{
		if ((f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
		{
			duh = dumb_read_okt_quick(f);
		}
	}

	if ( ! duh )
	{
		is_dos = false;
		if (filestate.ptr == (uint8_t *)start)
		{
			if (!(f = dumb_read_allfile(&filestate, start, reader, headsize, size)))
			{
                reader.Seek(fpos, FileReader::SeekSet);
				return NULL;
			}
		}
		else
		{
			filestate.offset = 0;
		}
		// No way to get the filename, so we can't check for a .mod extension, and
		// therefore, trying to load an old 15-instrument SoundTracker module is not
		// safe. We'll restrict MOD loading to 31-instrument modules with known
		// signatures and let the sound system worry about 15-instrument ones.
		// (Assuming it even supports them)
		duh = dumb_read_mod_quick(f, true);
	}

	if (f != NULL)
	{
		dumbfile_close(f);
	}
	if ( duh )
	{
		if (mod_autochip)
		{
			MOD_SetAutoChip(duh);
		}
		state = new input_mod(duh);
		if (!state->IsValid())
		{
			delete state;
			state = NULL;
		}
		else
		{
			if (is_it) ReadIT(filestate.ptr, size, state, false);
			else ReadDUH(duh, state, false, is_dos);
		}
	}
	else
	{
		// Reposition file pointer for other codecs to do their checks.
        reader.Seek(fpos, FileReader::SeekSet);
	}
	if (filestate.ptr != (uint8_t *)start)
	{
		delete[] const_cast<uint8_t *>(filestate.ptr);
	}
	return state;
}

//==========================================================================
//
// input_mod :: read												static
//
//==========================================================================

bool input_mod::read(SoundStream *stream, void *buffer, int sizebytes, void *userdata)
{
	input_mod *state = (input_mod *)userdata;
	if (state->eof)
	{
		memset(buffer, 0, sizebytes);
		return false;
	}
	state->crit_sec.Enter();
	while (sizebytes > 0)
	{
		int written = state->decode_run(buffer, sizebytes / 8);
		if (written < 0)
		{
			state->crit_sec.Leave();
			return false;
		}
		if (written == 0)
		{
			state->crit_sec.Leave();
			memset(buffer, 0, sizebytes);
			return true;
		}
		else
		{
			// Convert to float
			for (int i = 0; i < written * 2; ++i)
			{
				((float *)buffer)[i] = (((int *)buffer)[i] / (float)(1 << 24)) * mod_dumb_mastervolume;
			}
		}
		buffer = (uint8_t *)buffer + written * 8;
		sizebytes -= written * 8;
	}
	state->crit_sec.Leave();
	return true;
}

//==========================================================================
//
// input_mod constructor
//
//==========================================================================

input_mod::input_mod(DUH *myduh)
{
	duh = myduh;
	sr = NULL;
	eof = false;
	interp = mod_interp;
	volramp = mod_volramp;
	written = 0;
	length = 0;
	start_order = 0;
	if (mod_samplerate != 0)
	{
		srate = mod_samplerate;
	}
	else
	{
		srate = (int)GSnd->GetOutputRate();
	}
	m_Stream = GSnd->CreateStream(read, 32*1024, SoundStream::Float, srate, this);
	delta = 65536.0 / srate;
}

//==========================================================================
//
// input_mod destructor
//
//==========================================================================

input_mod::~input_mod()
{
	Stop();
	if (m_Stream != NULL)
	{
		delete m_Stream;
		m_Stream = NULL;
	}
	if (sr) duh_end_sigrenderer(sr);
	if (duh) unload_duh(duh);
}

//==========================================================================
//
// input_mod :: Play
//
//==========================================================================

void input_mod::Play(bool looping, int order)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	start_order = order;
	if (open2(0) && m_Stream->Play(m_Looping, 1))
	{
		m_Status = STATE_Playing;
	}
}

//==========================================================================
//
// input_mod :: SetSubsong
//
//==========================================================================

bool input_mod::SetSubsong(int order)
{
	if (order == start_order)
	{
		return false;
	}
	crit_sec.Enter();
	DUH_SIGRENDERER *oldsr = sr;
	sr = NULL;
	start_order = order;
	if (!open2(0))
	{
		sr = oldsr;
		crit_sec.Leave();
		return false;
	}
	duh_end_sigrenderer(oldsr);
	crit_sec.Leave();
	return true;
}

//==========================================================================
//
// input_mod :: open2
//
//==========================================================================

bool input_mod::open2(long pos)
{
	if (start_order != 0)
	{
		sr = dumb_it_start_at_order(duh, 2, start_order);
		if (sr && pos) duh_sigrenderer_generate_samples(sr, 0, 1, pos, 0);
	}
	else
	{
		sr = duh_start_sigrenderer(duh, 0, 2, pos);
	}
	if (!sr)
	{
		return false;
	}

	DUMB_IT_SIGRENDERER *itsr = duh_get_it_sigrenderer(sr);
	dumb_it_set_resampling_quality(itsr, interp);
	dumb_it_set_ramp_style(itsr, volramp);
	if (!m_Looping)
	{
		dumb_it_set_loop_callback(itsr, &dumb_it_callback_terminate, NULL);
	}
	dumb_it_set_xm_speed_zero_callback(itsr, &dumb_it_callback_terminate, NULL);
	dumb_it_set_global_volume_zero_callback(itsr, &dumb_it_callback_terminate, NULL);
	return true;
}

//==========================================================================
//
// input_mod :: render
//
//==========================================================================

long input_mod::render(double volume, double delta, long samples, sample_t **buffer)
{
	long written = duh_sigrenderer_generate_samples(sr, volume, delta, samples, buffer);

	if (written < samples)
	{
		if (!m_Looping)
		{
			eof = true;
		}
		else
		{
			duh_end_sigrenderer(sr);
			sr = NULL;
			if (!open2(0))
			{
				eof = true;
			}
		}
	}
	return written;
}

//==========================================================================
//
// input_mod :: decode_run
//
// Given a buffer of 32-bit PCM stereo pairs and a size specified in
// samples, returns the number of samples written to the buffer.
//
//==========================================================================

int input_mod::decode_run(void *buffer, unsigned int size)
{
	if (eof) return 0;

	DUMB_IT_SIGRENDERER *itsr = duh_get_it_sigrenderer(sr);
	int dt = int(delta * 65536.0 + 0.5);
	long samples = long((((LONG_LONG)itsr->time_left << 16) | itsr->sub_time_left) / dt);
	if (samples == 0 || samples > (long)size) samples = size;
	sample_t **buf = (sample_t **)&buffer;
	int written = 0;

retry:
	dumb_silence(buf[0], size * 2);
	written = render(1, delta, samples, buf);

	if (eof) return false;
	else if (written == 0) goto retry;
	else if (written == -1) return -1;

	return written;
}

//==========================================================================
//
// input_mod :: GetStats
//
//==========================================================================

FString input_mod::GetStats()
{
	//return StreamSong::GetStats();
	DUMB_IT_SIGRENDERER *itsr = duh_get_it_sigrenderer(sr);
	DUMB_IT_SIGDATA *itsd = duh_get_it_sigdata(duh);
	FString out;

	int channels = 0;
	for (int i = 0; i < DUMB_IT_N_CHANNELS; i++)
	{
		IT_PLAYING * playing = itsr->channel[i].playing;
		if (playing && !(playing->flags & IT_PLAYING_DEAD)) channels++;
	}
	for (int i = 0; i < DUMB_IT_N_NNA_CHANNELS; i++)
	{
		if (itsr->playing[i]) channels++;
	}

	if (itsr == NULL || itsd == NULL)
	{
		out = "Problem getting stats";
	}
	else
	{
		out.Format("%s, Order:%3d/%d Patt:%2d/%d Row:%2d/%2d Chan:%2d/%2d Speed:%2d Tempo:%3d",
			Codec.GetChars(),
			itsr->order, NumOrders,
			(itsd->order && itsr->order < itsd->n_orders ? itsd->order[itsr->order] : 0), NumPatterns,
			itsr->row, itsr->n_rows,
			channels, NumChannels,
			itsr->speed,
			itsr->tempo
			);
	}
	return out;
}

//==========================================================================
//
// dumb_decode_vorbis
//
//==========================================================================

extern "C" short *DUMBCALLBACK dumb_decode_vorbis(int outlen, const void *oggstream, int sizebytes)
{
	return GSnd->DecodeSample(outlen, oggstream, sizebytes, CODEC_Vorbis);
}
