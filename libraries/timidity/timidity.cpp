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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <algorithm>
#include <stdarg.h>
#include <string.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"



namespace Timidity
{

static long ParseCommandLine(const char* args, int* argc, char** argv)
{
	int count;
	char* buffplace;

	count = 0;
	buffplace = NULL;
	if (argv != NULL)
	{
		buffplace = argv[0];
	}

	for (;;)
	{
		while (*args <= ' ' && *args)
		{ // skip white space
			args++;
		}
		if (*args == 0)
		{
			break;
		}
		else if (*args == '\"')
		{ // read quoted string
			char stuff;
			if (argv != NULL)
			{
				argv[count] = buffplace;
			}
			count++;
			args++;
			do
			{
				stuff = *args++;
				if (stuff == '\"')
				{
					stuff = 0;
				}
				else if (stuff == 0)
				{
					args--;
				}
				if (argv != NULL)
				{
					*buffplace = stuff;
				}
				buffplace++;
			} while (stuff);
		}
		else
		{ // read unquoted string
			const char* start = args++, * end;

			while (*args && *args > ' ' && *args != '\"')
				args++;

			end = args;
			if (argv != NULL)
			{
				argv[count] = buffplace;
				while (start < end)
					* buffplace++ = *start++;
				*buffplace++ = 0;
			}
			else
			{
				buffplace += end - start + 1;
			}
			count++;
		}
	}
	if (argc != NULL)
	{
		*argc = count;
	}
	return (long)(buffplace - (char*)0);
}

class FCommandLine
{
public:
	FCommandLine(const char* commandline)
	{
		cmd = commandline;
		_argc = -1;
		_argv = NULL;
		argsize = 0;
	}

	~FCommandLine()
	{
		if (_argv != NULL)
		{
			delete[] _argv;
		}
	}

	int argc()
	{
		if (_argc == -1)
		{
			argsize = ParseCommandLine(cmd, &_argc, NULL);
		}
		return _argc;
	}

	char* operator[] (int i)
	{
		if (_argv == NULL)
		{
			int count = argc();
			_argv = new char* [count + (argsize + sizeof(char*) - 1) / sizeof(char*)];
			_argv[0] = (char*)_argv + count * sizeof(char*);
			ParseCommandLine(cmd, NULL, _argv);
		}
		return _argv[i];
	}


	const char* args() { return cmd; }
	
	void Shift()
	{
		// Only valid after _argv has been filled.
		for (int i = 1; i < _argc; ++i)
		{
			_argv[i - 1] = _argv[i];
		}
	}

private:
	const char* cmd;
	int _argc;
	char** _argv;
	long argsize;
};


int Instruments::read_config_file(const char *name)
{
	char tmp[1024], *cp;
	ToneBank *bank = NULL;
	int i, j, k, line = 0, words;
	static int rcf_count = 0;

	if (rcf_count > 50)
	{
		printMessage(CMSG_ERROR, VERB_NORMAL, "Timidity: Probable source loop in configuration files\n");
		return (-1);
	}
	auto fp = sfreader->open_file(name);
	if (!fp)
	{
		printMessage(CMSG_ERROR, VERB_NORMAL, "Timidity: Unable to open config file\n");
		return -1;
	}

	while (fp->gets(tmp, sizeof(tmp)))
	{
		line++;
		FCommandLine w(tmp);
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
			//printMessage(CMSG_ERROR, VERB_NORMAL, "FIXME: Implement \"timeout\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "copydrumset")	/* "copydrumset" drumset */
			|| !strcmp(w[0], "copybank"))		/* "copybank" bank       */
		{
			/*
			* Copies all the settings of the specified drumset or bank to
			* the current drumset or bank. May be useful later, but not a
			* high priority.
			*/
			//printMessage(CMSG_ERROR, VERB_NORMAL, "FIXME: Implement \"%s\" in TiMidity config.\n", w[0]);
		}
		else if (!strcmp(w[0], "undef"))		/* "undef" progno */
		{
			/*
			* Undefines the tone "progno" of the current tone bank (or
			* drum set?). Not a high priority.
			*/
			//printMessage(CMSG_ERROR, VERB_NORMAL, "FIXME: Implement \"undef\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "altassign")) /* "altassign" prog1 prog2 ... */
		{
			/*
			* Sets the alternate assign for drum set. Whatever that's
			* supposed to mean.
			*/
			//printMessage(CMSG_ERROR, VERB_NORMAL, "FIXME: Implement \"altassign\" in TiMidity config.\n");
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
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No soundfont given\n", name, line);
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
						printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad soundfont option %s\n", name, line, w[i]);
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
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error\n", name, line);
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
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: font subcommand must be 'order' or 'exclude'\n", name, line);
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
			//printMessage(CMSG_ERROR, VERB_NORMAL, "FIXME: Implement \"progbase\" in TiMidity config.\n");
		}
		else if (!strcmp(w[0], "map")) /* "map" name set1 elem1 set2 elem2 */
		{
			/*
			* This extension is the one we will need to implement, as it is
			* used by the "eawpats". Unfortunately I cannot find any
			* documentation whatsoever for it, but it looks like it's used
			* for remapping one instrument to another somehow.
			*/
			//printMessage(CMSG_ERROR, VERB_NORMAL, "FIXME: Implement \"map\" in TiMidity config.\n");
		}

		/* Standard TiMidity config */

		else if (!strcmp(w[0], "dir"))
		{
			if (words < 2)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No directory given\n", name, line);
				return -2;
			}
			for (i = 1; i < words; i++)
			{
				// Q: How does this deal with relative paths? In this form it just does not work.
				sfreader->add_search_path(w[i]);
			}
		}
		else if (!strcmp(w[0], "source"))
		{
			if (words < 2)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No file name given\n", name, line);
				return -2;
			}
			for (i=1; i<words; i++)
			{
				rcf_count++;
				read_config_file(w[i]);
				rcf_count--;
			}
		}
		else if (!strcmp(w[0], "default"))
		{
			if (words != 2)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Must specify exactly one patch name\n", name, line);
				return -2;
			}
			def_instr_name = w[1];
		}
		else if (!strcmp(w[0], "drumset"))
		{
			if (words < 2)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No drum set number given\n", name, line);
				return -2;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Drum set must be between 0 and 127\n", name, line);
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
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: No bank number given\n", name, line);
				return -2;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Tone bank must be between 0 and 127\n", name, line);
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
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error\n", name, line);
				return -2;
			}
			i = atoi(w[0]);
			if (i < 0 || i > 127)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Program must be between 0 and 127\n", name, line);
				return -2;
			}
			if (bank == NULL)
			{
				printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: Must specify tone bank or drum set before assignment\n", name, line);
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
					printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s\n", name, line, w[j]);
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
						printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: note must be between 0 and 127\n", name, line);
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
						printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: panning must be left, right, "
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
						printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: keep must be env or loop\n", name, line);
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
						printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: strip must be env, loop, or tail\n", name, line);
						return -2;
					}
				}
				else
				{
					printMessage(CMSG_ERROR, VERB_NORMAL, "%s: line %d: bad patch option %s\n", name, line, w[j]);
					return -2;
				}
			}
		}
	}
	return 0;
}

static char* gets(const char *&input, const char *eof, char* strbuf, size_t len)
{
	if (ptrdiff_t(len) > eof - input) len = eof - input;
	if (len <= 0) return nullptr;

	char* p = strbuf;
	while (len > 1)
	{
		if (*input == 0)
		{
			input++;
			break;
		}
		if (*input != '\r')
		{
			*p++ = *input;
			len--;
			if (*input == '\n')
			{
				input++;
				break;
			}
		}
		input++;
	}
	if (p == strbuf) return nullptr;
	*p++ = 0;
	return strbuf;
}

int Instruments::LoadDMXGUS(int gus_memsize, const char* dmxgusdata, size_t dmxgussize)
{
	char readbuffer[1024];
	const char* eof = dmxgusdata + dmxgussize;
	long read = 0;
	uint8_t remap[256];

	std::string patches[256];
	memset(remap, 255, sizeof(remap));
	char temp[16];
	int current = -1;
	int status = -1;
	int gusbank = (gus_memsize >= 1 && gus_memsize <= 4) ? gus_memsize : -1;

	while (gets(dmxgusdata, eof, readbuffer, 1024))
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
		if (patches[j].length() == 0)
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

void FreeDLS(DLS_Data *data);
Renderer::Renderer(float sample_rate, int voices_, Instruments *inst)
{
	int res = 0;

	instruments = inst;

	rate = sample_rate;
	patches = NULL;
	resample_buffer_size = 0;
	resample_buffer = NULL;
	voice = NULL;
	adjust_panning_immediately = false;

	control_ratio = std::min(1, std::max(MAX_CONTROL_RATIO, int(rate / CONTROLS_PER_SECOND)));

	lost_notes = 0;
	cut_notes = 0;

	default_instrument = NULL;
	default_program = DEFAULT_PROGRAM;
	if (inst->def_instr_name.length() > 0)
		set_default_instrument(inst->def_instr_name.c_str());

	voices = std::max(voices_, 16);
	voice = new Voice[voices];
	drumchannels = DEFAULT_DRUMCHANNELS;
}

Renderer::~Renderer()
{
	if (resample_buffer != NULL)
	{
		free(resample_buffer);
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
		resample_buffer = (sample_t *)realloc(resample_buffer, count * sizeof(float) * 2);
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
		bank = instruments->drumset[banknum];
	}
	else
	{
		bank = instruments->tonebank[banknum];
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

static void default_cmsg(int type, int verbosity_level, const char* fmt, ...)
{
	if (verbosity_level >= VERB_NOISY) return;	// Don't waste time on diagnostics.

	va_list args;
	va_start(args, fmt);

	switch (type)
	{
	case CMSG_ERROR:
		vprintf("Error: %s\n", args);
		break;

	case CMSG_WARNING:
		vprintf("Warning: %s\n", args);
		break;

	case CMSG_INFO:
		vprintf("Info: %s\n", args);
		break;
	}
}

// Allow hosting applications to capture the messages and deal with them themselves.
void (*printMessage)(int type, int verbosity_level, const char* fmt, ...) = default_cmsg;


}
