#include <stdio.h>
#include "fmod.h"
#include "fmod_output.h"
#include "SDL.h"

#define CONVERTBUFFER_SIZE 	4096	// in bytes

#define D(x)

#define FALSE 	0
#define TRUE 	1

typedef int BOOL;

struct AudioData
{
	FMOD_OUTPUT_STATE *Output;
	BOOL ConvertU8toS8;
	BOOL ConvertU16toS16;
	int BytesPerSample;
};

FMOD_SOUND_FORMAT Format_SDLtoFMOD(Uint16 format)
{
	if ((format & (AUDIO_U8 | AUDIO_U16LSB)) == AUDIO_U8)
	{
		return FMOD_SOUND_FORMAT_PCM8;
	}
	return FMOD_SOUND_FORMAT_PCM16;
}

Uint16 Format_FMODtoSDL(FMOD_SOUND_FORMAT format)
{
	switch (format)
	{
	case FMOD_SOUND_FORMAT_PCM8:	return AUDIO_S8;
	case FMOD_SOUND_FORMAT_PCM16:	return AUDIO_S16SYS;
	default: 						return AUDIO_S16SYS;
	}
}

static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int len)
{
	struct AudioData *data = (struct AudioData *)userdata;
	int i;

	data->Output->readfrommixer(data->Output, stream, len / data->BytesPerSample);
	
	if (data->ConvertU8toS8)
	{
		for (i = 0; i < len; ++i)
		{
			stream[i] -= 0x80;
		}
	}
	else if (data->ConvertU16toS16)
	{
		len /= 2;
		for (i = 0; i < len; ++i)
		{
			((short *)stream)[i] -= 0x8000;
		}
	}
}

static FMOD_RESULT F_CALLBACK GetNumDrivers(FMOD_OUTPUT_STATE *output_state, int *numdrivers)
{
	if (numdrivers == NULL)
	{
		return FMOD_ERR_INVALID_PARAM;
	}
	*numdrivers = 1;
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK GetDriverName(FMOD_OUTPUT_STATE *output_state, int id, char *name, int namelen)
{
	if (id != 0 || name == NULL)
	{
		return FMOD_ERR_INVALID_PARAM;
	}
	strncpy(name, "SDL default", namelen);
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK GetDriverCaps(FMOD_OUTPUT_STATE *output_state, int id, FMOD_CAPS *caps)
{
	if (id != 0 || caps == NULL)
	{
		return FMOD_ERR_INVALID_PARAM;
	}
	*caps = FMOD_CAPS_OUTPUT_FORMAT_PCM8 | FMOD_CAPS_OUTPUT_FORMAT_PCM16 | FMOD_CAPS_OUTPUT_MULTICHANNEL;
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK Init(FMOD_OUTPUT_STATE *output_state, int selecteddriver,
	FMOD_INITFLAGS flags, int *outputrate, int outputchannels,
	FMOD_SOUND_FORMAT *outputformat, int dspbufferlength, int dspnumbuffers,
	void *extradriverdata)
{
	SDL_AudioSpec desired, obtained;
	struct AudioData *data;
	
	if (selecteddriver != 0 || outputrate == NULL || outputformat == NULL)
	{
		D(printf("invalid param\n"));
		return FMOD_ERR_INVALID_PARAM;
	}
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		D(printf("init subsystem failed\n"));
		return FMOD_ERR_OUTPUT_INIT;
	}
	data = malloc(sizeof(*data));
	if (data == NULL)
	{
		D(printf("nomem\n"));
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return FMOD_ERR_MEMORY;
	}
	desired.freq = *outputrate;
	desired.format = Format_FMODtoSDL(*outputformat);
	desired.channels = outputchannels;
	desired.samples = dspbufferlength;
	desired.callback = AudioCallback;
	desired.userdata = data;
	if (SDL_OpenAudio(&desired, &obtained) < 0)
	{
		D(printf("openaudio failed\n"));
		free(data);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return FMOD_ERR_OUTPUT_INIT;
	}
	if (obtained.channels != outputchannels)
	{ // Obtained channels don't match what we wanted.
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		free(data);
		return FMOD_ERR_OUTPUT_CREATEBUFFER;
	}
	data->Output = output_state;
	data->ConvertU8toS8 = FALSE;
	data->ConvertU16toS16 = FALSE;
	if (obtained.format == AUDIO_U8)
	{
		data->ConvertU8toS8 = TRUE;
		D(printf("convert u8 to s8\n"));
	}
	else if (obtained.format == AUDIO_U16SYS)
	{
		data->ConvertU16toS16 = TRUE;
		D(printf("convert u16 to s16\n"));
	}
	output_state->plugindata = data;
	*outputrate = obtained.freq;
	*outputformat = Format_SDLtoFMOD(obtained.format);
	data->BytesPerSample = *outputformat == FMOD_SOUND_FORMAT_PCM16 ? 2 : 1;
	data->BytesPerSample *= desired.channels;
	D(printf("init ok\n"));
	SDL_PauseAudio(0);
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK Close(FMOD_OUTPUT_STATE *output_state)
{
	struct AudioData *data = (struct AudioData *)output_state->plugindata;
	
	D(printf("Close\n"));
	if (data != NULL)
	{
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		free(data);
	}
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK Update(FMOD_OUTPUT_STATE *update)
{
	return FMOD_OK;
}

static FMOD_RESULT F_CALLBACK GetHandle(FMOD_OUTPUT_STATE *output_state, void **handle)
{
	D(printf("Gethandle\n"));
	// SDL's audio isn't multi-instanced, so this is pretty meaningless
	if (handle == NULL)
	{
		return FMOD_ERR_INVALID_PARAM;
	}
	*handle = output_state->plugindata;
	return FMOD_OK;
}

static FMOD_OUTPUT_DESCRIPTION Desc =
{
	"SDL Output",		// name
	1,					// version
	0,					// polling
	GetNumDrivers,
	GetDriverName,
	GetDriverCaps,
	Init,
	Close,
	Update,
	GetHandle,
	NULL,				// getposition
	NULL,				// lock
	NULL				// unlock
};

F_DECLSPEC F_DLLEXPORT FMOD_OUTPUT_DESCRIPTION * F_API FMODGetOutputDescription()
{
	return &Desc;
}
