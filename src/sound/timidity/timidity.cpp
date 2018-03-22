/*

	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	timidity.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory>

#include "templates.h"
#include "cmdlib.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "files.h"
#include "w_wad.h"
#include "i_soundfont.h"
#include "i_musicinterns.h"
#include "v_text.h"
#include "timidity.h"

CUSTOM_CVAR(String, midi_config, CONFIG_FILE, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	Timidity::FreeAll();
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_GUS)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CVAR(Int, midi_voices, 32, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(String, gus_patchdir, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Bool, midi_dmxgus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)	// This was 'true' but since it requires special setup that's not such a good idea.
{
	Timidity::FreeAll();
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_GUS)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CVAR(Int, gus_memsize, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

namespace Timidity
{

ToneBank *tonebank[MAXBANK], *drumset[MAXBANK];

static FString def_instr_name;
std::unique_ptr<FSoundFontReader> gus_sfreader;

static bool InitReader(const char *config_file)
{
	auto reader = sfmanager.OpenSoundFont(config_file, SF_GUS|SF_SF2);
	if (reader == nullptr)
	{
		Printf(TEXTCOLOR_RED "%s: Unable to load sound font", config_file);
		return false;	// No sound font could be opened.
	}
	gus_sfreader.reset(reader);
	//config_name = config_file;
	return true;
}


static int read_config_file(const char *name, bool ismain)
{
	FileReader fp;
	char tmp[1024], *cp;
	ToneBank *bank = NULL;
	int i, j, k, line = 0, words;
	static int rcf_count = 0;

	if (rcf_count > 50)
	{
		Printf("Timidity: Probable source loop in configuration files\n");
		return (-1);
	}
	if (ismain)
	{
		if (!InitReader(name)) return -1;
		fp = gus_sfreader->OpenMainConfigFile();
		FreeAll();
	}
	else
	{
		fp = gus_sfreader->LookupFile(name).first;
	}
	if (!fp .isOpen())
	{
		return -1;
	}

	while (fp.Gets(tmp, sizeof(tmp)))
	{
		line++;
		FCommandLine w(tmp, true);
		words = w.argc();
		if (words == 0) continue;

		/* Originally the TiMidity++ extensions were prefixed like this */
		if (strcmp(w[0], "#extension") == 0)
		{
			w.Shift();
			words--;
		}
		else if (*w[0] == '#')
		{
			continue;
		}

		for (i = 0; i < words; ++i)
		{
			if (*w[i] == '#')
			{
				words = i;
				break;
			}
		}

		/*
		 * TiMidity++ adds a number of extensions to the config file format.
		 * Many of them are completely irrelevant to SDL_sound, but at least
		 * we shouldn't choke on them.
		 *
		 * Unfortunately the documentation for these extensions is often quite
		 * vague, gramatically strange or completely absent.
		 */
		if (
			!strcmp(w[0], "comm")			/* "comm" program second        */
			|| !strcmp(w[0], "HTTPproxy")	/* "HTTPproxy" hostname:port    */
			|| !strcmp(w[0], "FTPproxy")	/* "FTPproxy" hostname:port     */
			|| !strcmp(w[0], "mailaddr")	/* "mailaddr" your-mail-address */
			|| !strcmp(w[0], "opt")			/* "opt" timidity-options       */
			)
		{
			/*
			* + "comm" sets some kind of comment -- the documentation is too
			*   vague for me to understand at this time.
			* + "HTTPproxy", "FTPproxy" and "mailaddr" are for reading data
			*   over a network, rather than from the file system.
			* + "opt" specifies default options for TiMidity++.
			*
			* These are all quite useless for our version of TiMidity, so
			* they can safely remain no-ops.
			*/
		}
		else if (!strcmp(w[0], "timeout"))	/* "timeout" program second */
		{
			/*
			* Specifies a timeout value of the program. A number of seconds
			* before TiMidity kills the note. This may be useful to implement
			* later, but I don't see any urgent need for it.
			*/
			//Printf("FIXME: Implement \"timeout\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "copydrumset")	/* "copydrumset" drumset */
			|| !strcmp(w[0], "copybank"))		/* "copybank" bank       */
		{
			/*
			* Copies all the settings of the specified drumset or bank to
			* the current drumset or bank. May be useful later, but not a
			* high priority.
			*/
			//Printf("FIXME: Implement \"%s\" in TiMidity config.\n", w[0]);
		}
		else if (!strcmp(w[0], "undef"))		/* "undef" progno */
		{
			/*
			* Undefines the tone "progno" of the current tone bank (or
			* drum set?). Not a high priority.
			*/
			//Printf("FIXME: Implement \"undef\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "altassign")) /* "altassign" prog1 prog2 ... */
		{
			/*
			* Sets the alternate assign for drum set. Whatever that's
			* supposed to mean.
			*/
			//Printf("FIXME: Implement \"altassign\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "soundfont"))
		{
			/*
			* "soundfont" sf_file "remove"
			* "soundfont" sf_file ["order=" order] ["cutoff=" cutoff]
			*                     ["reso=" reso] ["amp=" amp]
			*/
			if (words < 2)
			{
				Printf("%s: line %d: No soundfont given\n", name, line);
				return -2;
			}
			if (words > 2 && !strcmp(w[2], "remove"))
			{
				font_remove(w[1]);
			}
			else
			{
				int order = 0;

				for (i = 2; i < words; ++i)
				{
					if (!(cp = strchr(w[i], '=')))
					{
						Printf("%s: line %d: bad soundfont option %s\n", name, line, w[i]);
						return -2;
					}
				}
				font_add(w[1], order);
			}
		}
		else if (!strcmp(w[0], "font"))
		{
			/*
			* "font" "exclude" bank preset keynote
			* "font" "order" order bank preset keynote
			*/
			int order, drum = -1, bank = -1, instr = -1;

			if (words < 3)
			{
				Printf("%s: line %d: syntax error\n", name, line);
				return -2;
			}

			if (!strcmp(w[1], "exclude"))
			{
				order = 254;
				i = 2;
			}
			else if (!strcmp(w[1], "order"))
			{
				order = atoi(w[2]);
				i = 3;
			}
			else
			{
				Printf("%s: line %d: font subcommand must be 'order' or 'exclude'\n", name, line);
				return -2;
			}
			if (i < words)
			{
				drum = atoi(w[i++]);
			}
			if (i < words)
			{
				bank = atoi(w[i++]);
			}
			if (i < words)
			{
				instr = atoi(w[i++]);
			}
			if (drum != 128)
			{
				instr = bank;
				bank = drum;
				drum = 0;
			}
			else
			{
				drum = 1;
			}
			font_order(order, drum, bank, instr);
		}
		else if (!strcmp(w[0], "progbase"))
		{
			/*
			* The documentation for this makes absolutely no sense to me, but
			* apparently it sets some sort of base offset for tone numbers.
			* Why anyone would want to do this is beyond me.
			*/
			//Printf("FIXME: Implement \"progbase\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "map")) /* "map" name set1 elem1 set2 elem2 */
		{
			/*
			* This extension is the one we will need to implement, as it is
			* used by the "eawpats". Unfortunately I cannot find any
			* documentation whatsoever for it, but it looks like it's used
			* for remapping one instrument to another somehow.
			*/
			//Printf("FIXME: Implement \"map\" in TiMidity config.\n");
		}

		/* Standard TiMidity config */

		else if (!strcmp(w[0], "dir"))
		{
			if (words < 2)
			{
				Printf("%s: line %d: No directory given\n", name, line);
				return -2;
			}
			for (i = 1; i < words; i++)
			{
				// Q: How does this deal with relative paths? In this form it just does not work.
				gus_sfreader->AddPath(w[i]);
			}
		}
		else if (!strcmp(w[0], "source"))
		{
			if (words < 2)
			{
				Printf("%s: line %d: No file name given\n", name, line);
				return -2;
			}
			for (i=1; i<words; i++)
			{
				rcf_count++;
				read_config_file(w[i], false);
				rcf_count--;
			}
		}
		else if (!strcmp(w[0], "default"))
		{
			if (words != 2)
			{
				Printf("%s: line %d: Must specify exactly one patch name\n", name, line);
				return -2;
			}
			def_instr_name = w[1];
		}
		else if (!strcmp(w[0], "drumset"))
		{
			if (words < 2)
			{
				Printf("%s: line %d: No drum set number given\n", name, line);
				return -2;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				Printf("%s: line %d: Drum set must be between 0 and 127\n", name, line);
				return -2;
			}
			if (drumset[i] == NULL)
			{
				drumset[i] = new ToneBank;
			}
			bank = drumset[i];
		}
		else if (!strcmp(w[0], "bank"))
		{
			if (words < 2)
			{
				Printf("%s: line %d: No bank number given\n", name, line);
				return -2;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				Printf("%s: line %d: Tone bank must be between 0 and 127\n", name, line);
				return -2;
			}
			if (tonebank[i] == NULL)
			{
				tonebank[i] = new ToneBank;
			}
			bank = tonebank[i];
		}
		else
		{
			if ((words < 2) || (*w[0] < '0' || *w[0] > '9'))
			{
				Printf("%s: line %d: syntax error\n", name, line);
				return -2;
			}
			i = atoi(w[0]);
			if (i < 0 || i > 127)
			{
				Printf("%s: line %d: Program must be between 0 and 127\n", name, line);
				return -2;
			}
			if (bank == NULL)
			{
				Printf("%s: line %d: Must specify tone bank or drum set before assignment\n", name, line);
				return -2;
			}
			bank->tone[i].note = bank->tone[i].pan =
				bank->tone[i].fontbank = bank->tone[i].fontpreset =
				bank->tone[i].fontnote = bank->tone[i].strip_loop = 
				bank->tone[i].strip_envelope = bank->tone[i].strip_tail = -1;

			if (!strcmp(w[1], "%font"))
			{
				bank->tone[i].name = w[2];
				bank->tone[i].fontbank = atoi(w[3]);
				bank->tone[i].fontpreset = atoi(w[4]);
				if (words > 5 && (bank->tone[i].fontbank == 128 || (w[5][0] >= '0' && w[5][0] <= '9')))
				{
					bank->tone[i].fontnote = atoi(w[5]);
					j = 6;
				}
				else
				{
					j = 5;
				}
				font_add(w[2], 254);
			}
			else
			{
				bank->tone[i].name = w[1];
				j = 2;
			}

			for (; j<words; j++)
			{
				if (!(cp=strchr(w[j], '=')))
				{
					Printf("%s: line %d: bad patch option %s\n", name, line, w[j]);
					return -2;
				}
				*cp++ = 0;
				if (!strcmp(w[j], "amp"))
				{
					/* Ignored */
				}
				else if (!strcmp(w[j], "note"))
				{
					k = atoi(cp);
					if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9'))
					{
						Printf("%s: line %d: note must be between 0 and 127\n", name, line);
						return -2;
					}
					bank->tone[i].note = k;
				}
				else if (!strcmp(w[j], "pan"))
				{
					if (!strcmp(cp, "center"))
						k = 64;
					else if (!strcmp(cp, "left"))
						k = 0;
					else if (!strcmp(cp, "right"))
						k = 127;
					else
						k = ((atoi(cp)+100) * 100) / 157;
					if ((k < 0 || k > 127) ||
						(k == 0 && *cp != '-' && (*cp < '0' || *cp > '9')))
					{
						Printf("%s: line %d: panning must be left, right, "
							"center, or between -100 and 100\n", name, line);
						return -2;
					}
					bank->tone[i].pan = k;
				}
				else if (!strcmp(w[j], "keep"))
				{
					if (!strcmp(cp, "env"))
						bank->tone[i].strip_envelope = 0;
					else if (!strcmp(cp, "loop"))
						bank->tone[i].strip_loop = 0;
					else
					{
						Printf("%s: line %d: keep must be env or loop\n", name, line);
						return -2;
					}
				}
				else if (!strcmp(w[j], "strip"))
				{
					if (!strcmp(cp, "env"))
						bank->tone[i].strip_envelope = 1;
					else if (!strcmp(cp, "loop"))
						bank->tone[i].strip_loop = 1;
					else if (!strcmp(cp, "tail"))
						bank->tone[i].strip_tail = 1;
					else
					{
						Printf("%s: line %d: strip must be env, loop, or tail\n", name, line);
						return -2;
					}
				}
				else
				{
					Printf("%s: line %d: bad patch option %s\n", name, line, w[j]);
					return -2;
				}
			}
		}
	}
	return 0;
}

void FreeAll()
{
	free_instruments();
	font_freeall();
	for (int i = 0; i < MAXBANK; ++i)
	{
		if (tonebank[i] != NULL)
		{
			delete tonebank[i];
			tonebank[i] = NULL;
		}
		if (drumset[i] != NULL)
		{
			delete drumset[i];
			drumset[i] = NULL;
		}
	}
}

static FString currentConfig;

int LoadConfig(const char *filename)
{
	/* !!! FIXME: This may be ugly, but slightly less so than requiring the
	 *			  default search path to have only one element. I think.
	 *
	 *			  We only need to include the likely locations for the config
	 *			  file itself since that file should contain any other directory
	 *			  that needs to be added to the search path.
	 */
	if (currentConfig.CompareNoCase(filename) == 0) return 0;

	/* Some functions get aggravated if not even the standard banks are available. */
	if (tonebank[0] == NULL)
	{
		tonebank[0] = new ToneBank;
		drumset[0] = new ToneBank;
	}

	return read_config_file(filename, true);
}

int LoadConfig()
{
	if (midi_dmxgus)
	{
		return LoadDMXGUS();
	}
	else
	{
		return LoadConfig(midi_config);
	}
}

int LoadDMXGUS()
{
	if (currentConfig.CompareNoCase("DMXGUS") == 0) return 0;

	int lump = Wads.CheckNumForName("DMXGUS");
	if (lump == -1) lump = Wads.CheckNumForName("DMXGUSC");
	if (lump == -1) return LoadConfig(midi_config);

	auto data = Wads.OpenLumpReader(lump);
	if (data.GetLength() == 0) return LoadConfig(midi_config);

	// Check if we got some GUS data before using it.
	FString ultradir = getenv("ULTRADIR");
	if (ultradir.IsEmpty() && *(*gus_patchdir) == 0) return LoadConfig(midi_config);

	currentConfig = "DMXGUS";
	FreeAll();

	auto psreader = new FPatchSetReader;

	// The GUS put its patches in %ULTRADIR%/MIDI so we can try that
	if (ultradir.IsNotEmpty())
	{
		ultradir += "/midi";
		psreader->AddPath(ultradir);
	}
	// Load DMXGUS lump and patches from gus_patchdir
	if (*(*gus_patchdir) != 0) psreader->AddPath(gus_patchdir);
	gus_sfreader.reset(psreader);

	char readbuffer[1024];
	auto size = data.GetLength();
	long read = 0;
	uint8_t remap[256];

	FString patches[256];
	memset(remap, 255, sizeof(remap));
	char temp[16];
	int current = -1;
	int status = -1;
	int gusbank = (gus_memsize >= 1 && gus_memsize <= 4) ? gus_memsize : -1;

	data.Seek(0, FileReader::SeekSet);

	while (data.Gets(readbuffer, 1024) && read < size)
	{
		int i = 0;
		while (readbuffer[i] != 0 && i < 1024)
		{
			// Don't try to parse comments
			if (readbuffer[i] == '#') break;
			// Actively ignore spaces
			else if (readbuffer[i] == ' ') {}
			// Comma separate values
			else if (status >= 0 && status <= 4 && readbuffer[i] == ',')
			{
				if (++status == gusbank)
				{
					remap[current] = 0;
				}
			}
			// Status -1: outside of a line
			// Status 0: reading patch value
			else if (status == -1 && readbuffer[i] >= '0' && readbuffer[i] <= '9')
			{
				current = readbuffer[i] - '0';
				status = 0;
			}
			else if (status == 0 && readbuffer[i] >= '0' && readbuffer[i] <= '9')
			{
				current *= 10;
				current += readbuffer[i] - '0';
			}
			// Status 1 through 4: remaps (256K, 512K, 768K, and 1024K resp.)
			else if (status == gusbank && readbuffer[i] >= '0' && readbuffer[i] <= '9')
			{
				remap[current] *= 10;
				remap[current] += readbuffer[i] - '0';
			}
			// Status 5: parsing patch name
			else if (status == 5 && i < 1015)
			{
				memcpy(temp, readbuffer + i, 8);
				for (int j = 0; j < 8; ++j)
				{
					if (temp[j] < 33)
					{
						temp[j] = 0;
						break;
					}
				}
				temp[8] = 0;
				patches[current] = temp;
				// Prepare to parse next line
				status = -1;
				break;
			}

			++i;
		}
		read += i;
		if (i == 0) continue;
		readbuffer[i-1] = 0;
	}

	/* Some functions get aggravated if not even the standard banks are available. */
	if (tonebank[0] == NULL)
	{
		tonebank[0] = new ToneBank;
		drumset[0] = new ToneBank;
	}

	// From 0 to 127: tonebank[0]; from 128 to 255: drumset[0].
	ToneBank *bank = tonebank[0];
	for (int k = 0; k < 256; ++k)
	{
		int j = (gusbank > 0) ? remap[k] : k;
		if (k == 128) bank = drumset[0];
		// No need to bother with things that don't exist
		if (patches[j].IsEmpty())
			continue;

		int val = k % 128;
		bank->tone[val].note = bank->tone[val].pan =
			bank->tone[val].fontbank = bank->tone[val].fontpreset =
			bank->tone[val].fontnote = bank->tone[val].strip_loop = 
			bank->tone[val].strip_envelope = bank->tone[val].strip_tail = -1;
		bank->tone[val].name = patches[j];
	}

	return 0;
}

DLS_Data *LoadDLS(FILE *src);
void FreeDLS(DLS_Data *data);

Renderer::Renderer(float sample_rate, const char *args)
{
	int res = 0;
	// Load explicitly stated sound font if so desired.
	if (args != nullptr && *args != 0)
	{
		if (!stricmp(args, "DMXGUS")) res = LoadDMXGUS();
		res = LoadConfig(args);
	}
	else if (tonebank[0] == nullptr)
	{
		res = LoadConfig();
	}

	if (res < 0)
	{
		I_Error("Failed to load any MIDI patches");
	}

	// These can be left empty here if an error occured during sound font initialization.
	if (tonebank[0] == NULL)
	{
		tonebank[0] = new ToneBank;
		drumset[0] = new ToneBank;
	}


	rate = sample_rate;
	patches = NULL;
	resample_buffer_size = 0;
	resample_buffer = NULL;
	voice = NULL;
	adjust_panning_immediately = false;

	control_ratio = clamp(int(rate / CONTROLS_PER_SECOND), 1, MAX_CONTROL_RATIO);

	lost_notes = 0;
	cut_notes = 0;

	default_instrument = NULL;
	default_program = DEFAULT_PROGRAM;
	if (def_instr_name.IsNotEmpty())
		set_default_instrument(def_instr_name);

	voices = MAX(*midi_voices, 16);
	voice = new Voice[voices];
	drumchannels = DEFAULT_DRUMCHANNELS;
#if 0
	FILE *f = fopen("c:\\windows\\system32\\drivers\\gm.dls", "rb");
	patches = LoadDLS(f);
	fclose(f);
#endif
}

Renderer::~Renderer()
{
	if (resample_buffer != NULL)
	{
		M_Free(resample_buffer);
	}
	if (voice != NULL)
	{
		delete[] voice;
	}
	if (patches != NULL)
	{
		FreeDLS(patches);
	}
}

void Renderer::ComputeOutput(float *buffer, int count)
{
	// count is in samples, not bytes.
	if (count <= 0)
	{
		return;
	}
	Voice *v = &voice[0];

	memset(buffer, 0, sizeof(float)*count*2);		// An integer 0 is also a float 0.
	if (resample_buffer_size < count)
	{
		resample_buffer_size = count;
		resample_buffer = (sample_t *)M_Realloc(resample_buffer, count * sizeof(float) * 2);
	}
	for (int i = 0; i < voices; i++, v++)
	{
		if (v->status & VOICE_RUNNING)
		{
			mix_voice(this, buffer, v, count);
		}
	}
}

void Renderer::MarkInstrument(int banknum, int percussion, int instr)
{
	ToneBank *bank;

	if (banknum >= MAXBANK)
	{
		return;
	}
	if (banknum != 0)
	{
		/* Mark the standard bank in case it's not defined by this one. */
		MarkInstrument(0, percussion, instr);
	}
	if (percussion)
	{
		bank = drumset[banknum];
	}
	else
	{
		bank = tonebank[banknum];
	}
	if (bank == NULL)
	{
		return;
	}
	if (bank->instrument[instr] == NULL)
	{
		bank->instrument[instr] = MAGIC_LOAD_INSTRUMENT;
	}
}

void cmsg(int type, int verbosity_level, const char *fmt, ...)
{
	/*
	va_list args;
	va_start(args, fmt);
	VPrintf(PRINT_HIGH, fmt, args);
	msg.VFormat(fmt, args);
	*/
#ifdef _WIN32
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	I_DebugPrint(buf);
#endif
}

}
