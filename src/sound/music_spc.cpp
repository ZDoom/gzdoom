/*
** music_spc.cpp
** SPC codec for FMOD Ex, using snes_spc for decoding.
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

#include "templates.h"
#include "c_cvars.h"
#include "SNES_SPC.h"
#include "SPC_Filter.h"
#include "fmod_wrap.h"
#include "doomdef.h"
#include "m_swap.h"
#include "m_fixed.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct XID6Tag
{
	BYTE ID;
	BYTE Type;
	WORD Value;
};

struct SPCCodecData
{
	SNES_SPC *SPC;
	SPC_Filter *Filter;
	FMOD_CODEC_WAVEFORMAT WaveFormat;
	bool bPlayed;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

FMOD_RESULT SPC_CreateCodec(FMOD::System *sys);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static FMOD_RESULT SPC_DoOpen(FMOD_CODEC_STATE *codec, SPCCodecData *userdata);
static FMOD_RESULT F_CALLBACK SPC_Open(FMOD_CODEC_STATE *codec, FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo);
static FMOD_RESULT F_CALLBACK SPC_Close(FMOD_CODEC_STATE *codec);
static FMOD_RESULT F_CALLBACK SPC_Read(FMOD_CODEC_STATE *codec, void *buffer, unsigned int size, unsigned int *read);
static FMOD_RESULT F_CALLBACK SPC_SetPosition(FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, FMOD_TIMEUNIT postype);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Int, snd_samplerate)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Float, spc_amp, 1.875f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FMOD_CODEC_DESCRIPTION SPCCodecDescription = {
	"Super Nintendo SPC File",
	0x00000001,
	1,
	FMOD_TIMEUNIT_MS | FMOD_TIMEUNIT_PCM,
	SPC_Open,
	SPC_Close,
	SPC_Read,
	NULL,
	SPC_SetPosition,
	NULL,
	NULL,
	NULL
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SPC_CreateCodec
//
//==========================================================================

FMOD_RESULT SPC_CreateCodec(FMOD::System *sys)
{
	return sys->createCodec(&SPCCodecDescription);
}

//==========================================================================
//
// SPC_Open
//
//==========================================================================

static FMOD_RESULT SPC_DoOpen(FMOD_CODEC_STATE *codec, SPCCodecData *userdata)
{
	FMOD_RESULT result;
	char spcfile[SNES_SPC::spc_file_size];
	unsigned int bytesread;
	SNES_SPC *spc;
	SPC_Filter *filter;

	if (codec->filesize < 66048)
	{ // Anything smaller than this is definitely not a (valid) SPC.
		return FMOD_ERR_FORMAT;
	}
	result = codec->fileseek(codec->filehandle, 0, 0);
	if (result != FMOD_OK)
	{
		return result;
	}
	// Check the signature.
	result = codec->fileread(codec->filehandle, spcfile, SNES_SPC::signature_size, &bytesread, NULL);
	if (result != FMOD_OK || bytesread != SNES_SPC::signature_size ||
		memcmp(spcfile, SNES_SPC::signature, SNES_SPC::signature_size) != 0)
	{
		result = FMOD_ERR_FORMAT;
		// If the length is right and this is just a different version, try
		// to pretend it's the current one.
		if (bytesread == SNES_SPC::signature_size)
		{
			memcpy(spcfile + 28, SNES_SPC::signature + 28, 7);
			if (memcmp(spcfile, SNES_SPC::signature, SNES_SPC::signature_size) == 0)
			{
				result = FMOD_OK;
			}
		}
	}
	if (result != FMOD_OK)
	{
		return result;
	}
	// Load the rest of the file.
	result = codec->fileread(codec->filehandle, spcfile + SNES_SPC::signature_size,
		SNES_SPC::spc_file_size - SNES_SPC::signature_size, &bytesread, NULL);
	if (result != FMOD_OK || bytesread < SNES_SPC::spc_min_file_size - SNES_SPC::signature_size)
	{
		return FMOD_ERR_FILE_BAD;
	}
	bytesread += SNES_SPC::signature_size;

	// Create the SPC.
	spc = (userdata != NULL) ? userdata->SPC : new SNES_SPC;
	spc->init();
	if (spc->load_spc(spcfile, bytesread) != NULL)
	{
		if (userdata != NULL)
		{
			delete spc;
		}
		return FMOD_ERR_FILE_BAD;
	}
	spc->clear_echo();

	// Create the filter.
	filter = (userdata != NULL) ? userdata->Filter : new SPC_Filter;
	filter->set_gain(int(SPC_Filter::gain_unit * spc_amp));

	// Search for amplification tag in extended ID666 info.
	if (codec->filesize > SNES_SPC::spc_file_size + 8 && bytesread == SNES_SPC::spc_file_size)
	{
		DWORD id;

		result = codec->fileread(codec->filehandle, &id, 4, &bytesread, NULL);
		if (result == FMOD_OK && bytesread == 4 && id == MAKE_ID('x','i','d','6'))
		{
			DWORD size;

			result = codec->fileread(codec->filehandle, &size, 4, &bytesread, NULL);
			if (result == FMOD_OK && bytesread == 4)
			{
				size = LittleLong(size);
				DWORD pos = SNES_SPC::spc_file_size + 8;

				while (pos < size)
				{
					XID6Tag tag;

					result = codec->fileread(codec->filehandle, &tag, 4, &bytesread, NULL);
					if (result != FMOD_OK || bytesread != 4)
					{
						break;
					}
					if (tag.Type == 0)
					{
						// Don't care about these
					}
					else
					{
						if ((pos += LittleShort(tag.Value)) <= size)
						{
							if (tag.Type == 4 && tag.ID == 0x36)
							{
								DWORD amp;
								result = codec->fileread(codec->filehandle, &amp, 4, &bytesread, NULL);
								if (result == FMOD_OK && bytesread == 4)
								{
									filter->set_gain(LittleLong(amp) >> 8);
								}
								break;
							}
						}
						if (FMOD_OK != codec->fileseek(codec->filehandle, pos, NULL))
						{
							break;
						}
					}
				}
			}
		}
	}
	if (userdata == NULL)
	{
		userdata = new SPCCodecData;
		userdata->SPC = spc;
		userdata->Filter = filter;
		memset(&userdata->WaveFormat, 0, sizeof(userdata->WaveFormat));
		userdata->WaveFormat.format = FMOD_SOUND_FORMAT_PCM16;
		userdata->WaveFormat.channels = 2;
		userdata->WaveFormat.frequency = SNES_SPC::sample_rate;
		userdata->WaveFormat.lengthbytes = SNES_SPC::spc_file_size;
		userdata->WaveFormat.lengthpcm = ~0u;
		userdata->WaveFormat.blockalign = 4;
		codec->numsubsounds = 0;
		codec->waveformat = &userdata->WaveFormat;
		codec->plugindata = userdata;
	}
	userdata->bPlayed = false;
	return FMOD_OK;
}

//==========================================================================
//
// SPC_Open
//
//==========================================================================

static FMOD_RESULT F_CALLBACK SPC_Open(FMOD_CODEC_STATE *codec, FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo)
{
	return SPC_DoOpen(codec, NULL);
}

//==========================================================================
//
// SPC_Close
//
//==========================================================================

static FMOD_RESULT F_CALLBACK SPC_Close(FMOD_CODEC_STATE *codec)
{
	SPCCodecData *data = (SPCCodecData *)codec->plugindata;
	if (data != NULL)
	{
		delete data->Filter;
		delete data->SPC;
		delete data;
	}
	return FMOD_OK;
}

//==========================================================================
//
// SPC_Read
//
//==========================================================================

static FMOD_RESULT F_CALLBACK SPC_Read(FMOD_CODEC_STATE *codec, void *buffer, unsigned int size, unsigned int *read)
{
	SPCCodecData *data = (SPCCodecData *)codec->plugindata;
	if (read != NULL)
	{
		*read = size;
	}
	data->bPlayed = true;
	if (data->SPC->play(size >> 1, (short *)buffer) != NULL)
	{
		return FMOD_ERR_PLUGIN;
	}
	data->Filter->run((short *)buffer, size >> 1);
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK SPC_SetPosition(FMOD_CODEC_STATE *codec, int subsound, unsigned int position, FMOD_TIMEUNIT postype)
{
	SPCCodecData *data = (SPCCodecData *)codec->plugindata;
	FMOD_RESULT result;

	if (data->bPlayed)
	{ // Must reload the file after it's already played something.
		result = SPC_DoOpen(codec, data);
		if (result != FMOD_OK)
		{
			return result;
		}
	}
	if (position != 0)
	{
		if (postype == FMOD_TIMEUNIT_PCM)
		{
			data->SPC->skip(position * 4);
		}
		else if (postype == FMOD_TIMEUNIT_MS)
		{
			data->SPC->skip(Scale(position, SNES_SPC::sample_rate, 1000) * 4);
		}
		else
		{
			return FMOD_ERR_UNSUPPORTED;
		}
	}
	return FMOD_OK;
}
