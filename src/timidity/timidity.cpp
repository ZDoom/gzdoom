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

#include "timidity.h"
#include "templates.h"
#include "m_alloc.h"
#include "cmdlib.h"
#include "c_cvars.h"
#include "i_system.h"
#include "files.h"

CVAR(String, timidity_config, CONFIG_FILE, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

namespace Timidity
{

ToneBank *tonebank[MAXBANK], *drumset[MAXBANK];

static FString def_instr_name;
int openmode = OM_FILEORLUMP;


#define MAXWORDS 10

static int read_config_file(const char *name, bool ismain)
{
	FileReader *fp;
	char tmp[1024], *w[MAXWORDS], *cp;
	ToneBank *bank = NULL;
	int i, j, k, line = 0, words;
	static int rcf_count = 0;
	int lumpnum;

	if (rcf_count > 50)
	{
		Printf("Timidity: Probable source loop in configuration files\n");
		return (-1);
	}

	if (ismain) openmode = OM_FILEORLUMP;

	if (!(fp = open_filereader(name, openmode, &lumpnum)))
		return -1;

	if (ismain)
	{
		if (lumpnum > 0)
		{
			openmode = OM_LUMP;
			clear_pathlist();	// when reading from a PK3 we won't want to use any external path
		}
		else
		{
			openmode = OM_FILE;
		}
	}

	while (fp->Gets(tmp, sizeof(tmp)))
	{
		line++;
		w[words = 0] = strtok(tmp, " \t\r\n\240");
		if (!w[0]) continue;

		/* Originally the TiMidity++ extensions were prefixed like this */
		if (strcmp(w[0], "#extension") == 0)
			words = -1;
		else if (*w[0] == '#')
			continue;

		while (w[words] && *w[words] != '#' && (words < MAXWORDS))
			w[++words] = strtok(0, " \t\r\n\240");

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
			Printf("FIXME: Implement \"timeout\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "copydrumset")	/* "copydrumset" drumset */
			|| !strcmp(w[0], "copybank"))		/* "copybank" bank       */
		{
			/*
			* Copies all the settings of the specified drumset or bank to
			* the current drumset or bank. May be useful later, but not a
			* high priority.
			*/
			Printf("FIXME: Implement \"%s\" in TiMidity config.\n", w[0]);
		}
		else if (!strcmp(w[0], "undef"))		/* "undef" progno */
		{
			/*
			* Undefines the tone "progno" of the current tone bank (or
			* drum set?). Not a high priority.
			*/
			Printf("FIXME: Implement \"undef\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "altassign")) /* "altassign" prog1 prog2 ... */
		{
			/*
			* Sets the alternate assign for drum set. Whatever that's
			* supposed to mean.
			*/
			Printf("FIXME: Implement \"altassign\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "soundfont") || !strcmp(w[0], "font"))
		{
			/*
			* I can't find any documentation for these, but I guess they're
			* an alternative way of loading/unloading instruments.
			* 
			* "soundfont" sf_file "remove"
			* "soundfont" sf_file ["order=" order] ["cutoff=" cutoff]
			*                     ["reso=" reso] ["amp=" amp]
			* "font" "exclude" bank preset keynote
			* "font" "order" order bank preset keynote
			*/
			Printf("FIXME: Implmement \"%s\" in TiMidity config.\n", w[0]);
		}
		else if (!strcmp(w[0], "progbase"))
		{
			/*
			* The documentation for this makes absolutely no sense to me, but
			* apparently it sets some sort of base offset for tone numbers.
			* Why anyone would want to do this is beyond me.
			*/
			Printf("FIXME: Implement \"progbase\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "map")) /* "map" name set1 elem1 set2 elem2 */
		{
			/*
			* This extension is the one we will need to implement, as it is
			* used by the "eawpats". Unfortunately I cannot find any
			* documentation whatsoever for it, but it looks like it's used
			* for remapping one instrument to another somehow.
			*/
			Printf("FIXME: Implement \"map\" in TiMidity config.\n");
		}

		/* Standard TiMidity config */

		else if (!strcmp(w[0], "dir"))
		{
			if (words < 2)
			{
				Printf("%s: line %d: No directory given\n", name, line);
				delete fp;
				return -2;
			}
			for (i = 1; i < words; i++)
				add_to_pathlist(w[i]);
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
				delete fp;
				return -2;
			}
			def_instr_name = w[1];
		}
		else if (!strcmp(w[0], "drumset"))
		{
			if (words < 2)
			{
				Printf("%s: line %d: No drum set number given\n", name, line);
				delete fp;
				return -2;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				Printf("%s: line %d: Drum set must be between 0 and 127\n", name, line);
				delete fp;
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
				delete fp;
				return -2;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				Printf("%s: line %d: Tone bank must be between 0 and 127\n", name, line);
				delete fp;
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
				delete fp;
				return -2;
			}
			i = atoi(w[0]);
			if (i < 0 || i > 127)
			{
				Printf("%s: line %d: Program must be between 0 and 127\n", name, line);
				delete fp;
				return -2;
			}
			if (bank == NULL)
			{
				Printf("%s: line %d: Must specify tone bank or drum set before assignment\n", name, line);
				delete fp;
				return -2;
			}
			bank->tone[i].name = w[1];
			bank->tone[i].note = bank->tone[i].amp = bank->tone[i].pan =
				bank->tone[i].strip_loop = bank->tone[i].strip_envelope =
				bank->tone[i].strip_tail = -1;

			for (j = 2; j<words; j++)
			{
				if (!(cp=strchr(w[j], '=')))
				{
					Printf("%s: line %d: bad patch option %s\n", name, line, w[j]);
					delete fp;
					return -2;
				}
				*cp++ = 0;
				if (!strcmp(w[j], "amp"))
				{
					k = atoi(cp);
					if ((k < 0 || k > MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9'))
					{
						Printf("%s: line %d: amplification must be between  0 and %d\n", name, line, MAX_AMPLIFICATION);
						delete fp;
						return -2;
					}
					bank->tone[i].amp = k;
				}
				else if (!strcmp(w[j], "note"))
				{
					k = atoi(cp);
					if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9'))
					{
						Printf("%s: line %d: note must be between 0 and 127\n", name, line);
						delete fp;
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
						delete fp;
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
						delete fp;
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
						delete fp;
						return -2;
					}
				}
				else
				{
					Printf("%s: line %d: bad patch option %s\n", name, line, w[j]);
					delete fp;
					return -2;
				}
			}
		}
	}
	/*
	if (ferror(fp))
	{
		Printf("Can't read %s: %s\n", name, strerror(errno));
		close_file(fp);
		return -2;
	}
	*/
	delete fp;
	return 0;
}

void FreeAll()
{
	free_instruments();
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

int LoadConfig(const char *filename)
{
	/* !!! FIXME: This may be ugly, but slightly less so than requiring the
	 *			  default search path to have only one element. I think.
	 *
	 *			  We only need to include the likely locations for the config
	 *			  file itself since that file should contain any other directory
	 *			  that needs to be added to the search path.
	 */
	clear_pathlist();
#ifdef _WIN32
	add_to_pathlist("C:\\TIMIDITY");
	add_to_pathlist("\\TIMIDITY");
	add_to_pathlist(progdir);
#else
	add_to_pathlist("/usr/local/lib/timidity");
	add_to_pathlist("/etc/timidity");
	add_to_pathlist("/etc");
#endif

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
	return LoadConfig(timidity_config);
}

Renderer::Renderer(float sample_rate)
{
	rate = sample_rate;
	patches = NULL;
	resample_buffer_size = 0;
	resample_buffer = NULL;
	adjust_panning_immediately = false;

	control_ratio = clamp(int(rate / CONTROLS_PER_SECOND), 1, MAX_CONTROL_RATIO);

	lost_notes = 0;
	cut_notes = 0;

	default_instrument = NULL;
	default_program = DEFAULT_PROGRAM;
	if (def_instr_name.IsNotEmpty())
		set_default_instrument(def_instr_name);

	voices = DEFAULT_VOICES;
	drumchannels = DEFAULT_DRUMCHANNELS;
}

Renderer::~Renderer()
{
	if (resample_buffer != NULL)
	{
		M_Free(resample_buffer);
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
		if (v->status != VOICE_FREE)
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
	OutputDebugString(buf);
#endif
}

}
