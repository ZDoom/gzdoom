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


   common.h
*/

#ifndef ___COMMON_H_
#define ___COMMON_H_

#include <string>
#include <vector>
#include <stdint.h>
#include "../../music_common/fileio.h"


namespace TimidityPlus
{
using timidity_file = MusicIO::FileInterface;

inline char* tf_gets(char* buff, int n, timidity_file* tf)
{
	return tf->gets(buff, n);
}

inline long tf_read(void* buff, int32_t size, int32_t nitems, timidity_file* tf)
{
	return (long)tf->read(buff, size, nitems);
}

inline long tf_seek(timidity_file* tf, long offset, int whence)
{
	return (long)tf->seek(offset, whence);
}

inline long tf_tell(timidity_file* tf)
{
	return (long)tf->tell();
}

extern timidity_file *open_file(const char *name, MusicIO::SoundFontReaderInterface *);
extern void tf_close(timidity_file *tf);
extern void skip(timidity_file *tf, size_t len);
int tf_getc(timidity_file *tf);
extern int int_rand(int n);	/* random [0..n-1] */
double flt_rand();

extern void *safe_malloc(size_t count);
extern void *safe_realloc(void *old_ptr, size_t new_size);
extern void *safe_large_malloc(size_t count);
extern char *safe_strdup(const char *s);
extern void free_ptr_list(void *ptr_list, int count);
extern int string_to_7bit_range(const char *s, int *start, int *end);
extern int  load_table(char *file);

}
#endif /* ___COMMON_H_ */
