/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   common.c

   */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <string.h>
#include <ctype.h>
#include "m_random.h"
#include "common.h"
#include "pathexpander.h"
#include "cmdlib.h"

namespace TimidityPlus
{

static PathExpander tppPathExpander;


/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
	auto p = malloc(count);
	if (p == nullptr)
	{
		// I_FatalError("Out of memory");
	}
	return p;
}

void *safe_large_malloc(size_t count)
{
	return safe_malloc(count);
}

void *safe_realloc(void *ptr, size_t count)
{
	auto p = realloc(ptr, count);
	if (p == nullptr)
	{
		// I_FatalError("Out of memory");
	}
	return p;
}

char *safe_strdup(const char *s)
{
	if (s == nullptr) s = "";
	auto p = strdup(s);
	if (p == nullptr)
	{
		// I_FatalError("Out of memory");
	}
	return p;
}

/* free ((void **)ptr_list)[0..count-1] and ptr_list itself */
void free_ptr_list(void *ptr_list, int count)
{
	int i;
	for (i = 0; i < count; i++)
		free(((void **)ptr_list)[i]);
	free(ptr_list);
}

static int atoi_limited(const char *string, int v_min, int v_max)
{
	int value = atoi(string);

	if (value <= v_min)
		value = v_min;
	else if (value > v_max)
		value = v_max;
	return value;
}

int string_to_7bit_range(const char *string_, int *start, int *end)
{
	const char *string = string_;

	if (isdigit(*string))
	{
		*start = atoi_limited(string, 0, 127);
		while (isdigit(*++string));
	}
	else
		*start = 0;
	if (*string == '-')
	{
		string++;
		*end = isdigit(*string) ? atoi_limited(string, 0, 127) : 127;
		if (*start > *end)
			*end = *start;
	}
	else
		*end = *start;
	return string != string_;
}


static FRandom pr_rnd;

int int_rand(int n)
{
	return (int)pr_rnd.GenRand_Real1() * n;
}

double flt_rand()
{
	return (int)pr_rnd.GenRand_Real1();
}

PathList::PathList()
{
}

/* This adds a directory to the path list */
void PathList::addPath(const char *str)
{
	if (*str == 0) return;
	for (size_t i = 0; i < paths.size(); i++)
	{
		if (pathcmp(paths[i].c_str(), str, 0) == 0)
		{
			// move string to the back.
			std::string ss = paths[i];
			paths.erase(paths.begin() + i);
			paths.push_back(ss);
			return;
		}
	}
	paths.push_back(str);
}

int PathList::pathcmp(const char *p1, const char *p2, int ignore_case)
{
	int c1, c2;

#ifdef _WIN32
	ignore_case = 1;	/* Always ignore the case */
#endif

	do {
		c1 = *p1++ & 0xff;
		c2 = *p2++ & 0xff;
		if (ignore_case)
		{
			c1 = tolower(c1);
			c2 = tolower(c2);
		}
		if (c1 == '/') c1 = *p1 ? 0x100 : 0;
		if (c1 == '/') c2 = *p2 ? 0x100 : 0;
	} while (c1 == c2 && c1 /* && c2 */);

	return c1 - c2;
}

FileReader *PathList::tryOpenPath(const char *name, bool ismain)
{
	FileReader *fp;
	int lumpnum;

	if (ismain)
	{
		tppPathExpander.openmode = PathExpander::OM_FILEORLUMP;
		tppPathExpander.clearPathlist();
#ifdef _WIN32
		tppPathExpander.addToPathlist("C:\\TIMIDITY");
		tppPathExpander.addToPathlist("\\TIMIDITY");
		tppPathExpander.addToPathlist(progdir);
#else
		tppPathExpander.addToPathlist("/usr/local/lib/timidity");
		tppPathExpander.addToPathlist("/etc/timidity");
		tppPathExpander.addToPathlist("/etc");
#endif
	}

	if (!(fp = tppPathExpander.openFileReader(name, &lumpnum)))
		return NULL;

	if (ismain)
	{
		if (lumpnum > 0)
		{
			tppPathExpander.openmode = PathExpander::OM_LUMP;
			tppPathExpander.clearPathlist();	// when reading from a PK3 we don't want to use any external path
		}
		else
		{
			tppPathExpander.openmode = PathExpander::OM_FILE;
		}
	}
	return fp;
}

std::pair<FileReader *, std::string> PathList::openFile(const char *name, bool ismainfile)
{

	if (name && *name)
	{
		/* First try the given name */

		if (!isAbsPath(name))
		{
			for (int i = (int)paths.size() - 1; i >= 0; i--)
			{
				std::string s = paths[i];
				auto c = s.at(s.length() - 1);
				if (c != '/' && c != '#' && name[0] != '#')
				{
					s += '/';
				}
				s += name;
				auto fr = tryOpenPath(s.c_str(), ismainfile);
				if (fr!= nullptr) return std::make_pair(fr, s);
			}
			auto fr = tryOpenPath(name, ismainfile);
			if (fr != nullptr) return std::make_pair(fr, name);
		}
		else
		{
			// an absolute path is never looked up.
			FileReader *fr = new FileReader;
			if (fr->Open(name))
			{
				return std::make_pair(fr, std::string(name));
			}
			delete fr;
		}
	}
	return std::make_pair(nullptr, std::string());
}

int PathList::isAbsPath(const char *name)
{
	if (name[0] == '/') return 1;

#ifdef _WIN32
	/* [A-Za-z]: (for Windows) */
	if (isalpha(name[0]) && name[1] == ':')	return 1;
#endif /* _WIN32 */
	return 0;
}


struct timidity_file *open_file(const char *name, bool ismainfile, PathList &pathList)
{
	auto file = pathList.openFile(name, ismainfile);
	if (!file.first) return nullptr;
	auto tf = new timidity_file;
	tf->url = file.first;
	tf->filename = file.second;
	return tf;
}

/* This closes files opened with open_file */
void close_file(struct timidity_file *tf)
{
	if (tf->url != NULL)
	{
		tf->url->Close();
	}
	delete tf;
}

/* This is meant for skipping a few bytes. */
void skip(struct timidity_file *tf, size_t len)
{
	tf->url->Seek((long)len, SEEK_CUR);
}

char *tf_gets(char *buff, int n, struct timidity_file *tf)
{
	return tf->url->Gets(buff, n);
}

int tf_getc(struct timidity_file *tf)
{
	unsigned char c;
	auto read = tf->url->Read(&c, 1);
	return read == 0 ? EOF : c;
}

long tf_read(void *buff, int32_t size, int32_t nitems, struct timidity_file *tf)
{
	return tf->url->Read(buff, size * nitems) / size;
}

long tf_seek(struct timidity_file *tf, long offset, int whence)
{

	return tf->url->Seek(offset, whence);
}

long tf_tell(struct timidity_file *tf)
{
	return tf->url->Tell();
}

}