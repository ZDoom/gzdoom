/*
** zipdir.c
** Copyright (C) 2008 Randy Heit
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
****************************************************************************
**
** Usage: zipdir <zip file> <directory> ...
**
** Given one or more directories, their contents are scanned recursively.
** If any files are newer than the zip file or the zip file does not exist,
** then everything in the specified directories is stored in the zip. The
** base directory names are not stored in the zip file, but subdirectories
** recursed into are stored.
*/

// HEADER FILES ------------------------------------------------------------

#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#define stat _stat
#else
#include <dirent.h>
#include <fts.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "zip.h"

// MACROS ------------------------------------------------------------------

#ifndef _WIN32
#define __cdecl
#endif

// TYPES -------------------------------------------------------------------

typedef struct file_entry_s
{
	struct file_entry_s *next;
	time_t time_write;
	char path[];
} file_entry_t;

typedef struct dir_tree_s
{
	struct dir_tree_s *next;
	file_entry_t *files;
	size_t path_size;
	char path[];
} dir_tree_t;

typedef struct file_sorted_s
{
	file_entry_t *file;
	char *path_in_zip;
} file_sorted_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void print_usage(const char *cmdname);
dir_tree_t *alloc_dir_tree(const char *dir);
file_entry_t *alloc_file_entry(const char *prefix, const char *path, time_t last_written);
void free_dir_tree(dir_tree_t *tree);
void free_dir_trees(dir_tree_t *tree);
#ifdef _WIN32
void recurse_dir(dir_tree_t *tree, const char *dirpath);
dir_tree_t *add_dir(const char *dirpath);
#endif
dir_tree_t *add_dirs(char **argv);
int count_files(dir_tree_t *trees);
int __cdecl sort_cmp(const void *a, const void *b);
file_sorted_t *sort_files(dir_tree_t *trees, int num_files);
void write_zip(const char *zipname, dir_tree_t *trees);
int append_to_zip(zipFile zipfile, const file_sorted_t *file);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int no_mem;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// print_usage
//
//==========================================================================

void print_usage(const char *cmdname)
{
#ifdef _WIN32
	const char *rchar = strrchr(cmdname, '\\');
	if (rchar != NULL)
	{
		cmdname = rchar+1;
	}
#endif
	fprintf(stderr, "Usage: %s <zip file> <directory> ...\n", cmdname);
}

//==========================================================================
//
// alloc_dir_tree
//
//==========================================================================

dir_tree_t *alloc_dir_tree(const char *dir)
{
	dir_tree_t *tree;
	size_t dirlen;

	dirlen = strlen(dir);
	tree = malloc(sizeof(dir_tree_t) + dirlen + 2);
	if (tree != NULL)
	{
		strcpy(tree->path, dir);
		tree->path_size = dirlen;
		if (dir[dirlen - 1] != '/')
		{
			tree->path_size++;
			tree->path[dirlen] = '/';
			tree->path[dirlen + 1] = '\0';
		}
		tree->files = NULL;
		tree->next = NULL;
	}
	return tree;
}

//==========================================================================
//
// alloc_file_entry
//
//==========================================================================

file_entry_t *alloc_file_entry(const char *prefix, const char *path, time_t last_written)
{
	file_entry_t *entry;

	entry = malloc(sizeof(file_entry_t) + strlen(prefix) + strlen(path) + 1);
	if (entry != NULL)
	{
		strcpy(entry->path, prefix);
		strcat(entry->path, path);
		entry->next = NULL;
		entry->time_write = last_written;
	}
	return entry;
}

//==========================================================================
//
// free_dir_tree
//
//==========================================================================

void free_dir_tree(dir_tree_t *tree)
{
	file_entry_t *entry, *next;

	if (tree != NULL)
	{
		for (entry = tree->files; entry != NULL; entry = next)
		{
			next = entry->next;
			free(entry);
		}
		free(tree);
	}
}

//==========================================================================
//
// free_dir_trees
//
//==========================================================================

void free_dir_trees(dir_tree_t *tree)
{
	dir_tree_t *next;

	for (; tree != NULL; tree = next)
	{
		next = tree->next;
		free_dir_tree(tree);
	}
}

#ifdef _WIN32

//==========================================================================
//
// recurse_dir
//
//==========================================================================

void recurse_dir(dir_tree_t *tree, const char *dirpath)
{
	struct _finddata_t fileinfo;
	intptr_t handle;
	char *dirmatch;

	dirmatch = malloc(strlen(dirpath) + 2);
	if (dirmatch == NULL)
	{
		no_mem = 1;
		return;
	}
	strcpy(dirmatch, dirpath);
	strcat(dirmatch, "*");
	if ((handle = _findfirst(dirmatch, &fileinfo)) == -1)
	{
		fprintf(stderr, "Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		do
		{
			if (fileinfo.attrib & _A_HIDDEN)
			{
				// Skip hidden files and directories. (Prevents SVN bookkeeping
				// info from being included.)
				continue;
			}
			if (fileinfo.attrib & _A_SUBDIR)
			{
				char *newdir;

				if (fileinfo.name[0] == '.' &&
					(fileinfo.name[1] == '\0' ||
					 (fileinfo.name[1] == '.' && fileinfo.name[2] == '\0')))
				{
					// Do not record . and .. directories.
					continue;
				}
				newdir = malloc(strlen(dirpath) + strlen(fileinfo.name) + 2);
				strcpy(newdir, dirpath);
				strcat(newdir, fileinfo.name);
				strcat(newdir, "/");
				recurse_dir(tree, newdir);
			}
			else
			{
				file_entry_t *entry;

				entry = alloc_file_entry(dirpath, fileinfo.name, fileinfo.time_write);
				if (entry == NULL)
				{
					no_mem = 1;
					break;
				}
				entry->next = tree->files;
				tree->files = entry;
			}
		} while (_findnext(handle, &fileinfo) == 0);
		_findclose(handle);
	}
	free(dirmatch);
}

//==========================================================================
//
// add_dir
//
//==========================================================================

dir_tree_t *add_dir(const char *dirpath)
{
	dir_tree_t *tree = alloc_dir_tree(dirpath);

	if (tree != NULL)
	{
		recurse_dir(tree, tree->path);
	}
	return tree;
}

//==========================================================================
//
// add_dirs
// Windows version
//
// Given NULL-terminated array of directory paths, create trees for them.
//
//==========================================================================

dir_tree_t *add_dirs(char **argv)
{
	dir_tree_t *tree, *trees = NULL;
	char *s;

	while (*argv != NULL)
	{
		for (s = *argv; *s != '\0'; ++s)
		{
			if (*s == '\\')
			{
				*s = '/';
			}
		}
		tree = add_dir(*argv);
		tree->next = trees;
		trees = tree;
		if (no_mem)
		{
			break;
		}
		argv++;
	}
	return trees;
}

#else

//==========================================================================
//
// add_dirs
// 4.4BSD version
//
// Given NULL-terminated array of directory paths, create trees for them.
//
//==========================================================================

dir_tree_t *add_dirs(char **argv)
{
	FTS *fts;
	FTSENT *ent;
	dir_tree_t *tree, *trees = NULL;
	file_entry_t *file;

	fts = fts_open(argv, FTS_LOGICAL, NULL);
	if (fts == NULL)
	{
		fprintf(stderr, "Failed to start directory traversal: %s\n", strerror(errno));
		return NULL;
	}
	while ((ent = fts_read(fts)) != NULL)
	{
		if (ent->fts_info == FTS_D && ent->fts_name[0] == '.')
		{
			// Skip hidden directories. (Prevents SVN bookkeeping
			// info from being included.)
			fts_set(fts, ent, FTS_SKIP);
		}
		if (ent->fts_info == FTS_D && ent->fts_level == 0)
		{
			tree = alloc_dir_tree(ent->fts_path);
			if (tree == NULL)
			{
				no_mem = 1;
				break;
			}
			tree->next = trees;
			trees = tree;
		}
		if (ent->fts_info != FTS_F)
		{
			// We're only interested in remembering files.
			continue;
		}
		file = alloc_file_entry("", ent->fts_path, ent->fts_statp->st_mtime);
		if (file == NULL)
		{
			no_mem = 1;
			break;
		}
		file->next = tree->files;
		tree->files = file;
	}
	fts_close(fts);
	return trees;
}
#endif

//==========================================================================
//
// count_files
//
//==========================================================================

int count_files(dir_tree_t *trees)
{
	dir_tree_t *tree;
	file_entry_t *file;
	int count;

	for (count = 0, tree = trees; tree != NULL; tree = tree->next)
	{
		for (file = tree->files; file != NULL; file = file->next)
		{
			count++;
		}
	}
	return count;
}

//==========================================================================
//
// sort_cmp
//
// Arbitrarily-selected sorting for the zip files: Files in the root
// directory sort after files in subdirectories. Otherwise, everything
// sorts by name.
//
//==========================================================================

int __cdecl sort_cmp(const void *a, const void *b)
{
	const file_sorted_t *sort1 = (const file_sorted_t *)a;
	const file_sorted_t *sort2 = (const file_sorted_t *)b;
	int in_dir1, in_dir2;

	in_dir1 = (strchr(sort1->path_in_zip, '/') != NULL);
	in_dir2 = (strchr(sort2->path_in_zip, '/') != NULL);
	if (in_dir1 == 1 && in_dir2 == 0)
	{
		return -1;
	}
	if (in_dir1 == 0 && in_dir2 == 1)
	{
		return 1;
	}
	return strcmp(((const file_sorted_t *)a)->path_in_zip,
		((const file_sorted_t *)b)->path_in_zip);
}

//==========================================================================
//
// sort_files
//
//==========================================================================

file_sorted_t *sort_files(dir_tree_t *trees, int num_files)
{
	file_sorted_t *sorter;
	dir_tree_t *tree;
	file_entry_t *file;
	int i;

	sorter = malloc(sizeof(*sorter) * num_files);
	if (sorter != NULL)
	{
		for (i = 0, tree = trees; tree != NULL; tree = tree->next)
		{
			for (file = tree->files; file != NULL; file = file->next)
			{
				sorter[i].file = file;
				sorter[i].path_in_zip = file->path + tree->path_size;
				i++;
			}
		}
		qsort(sorter, num_files, sizeof(*sorter), sort_cmp);
	}
	return sorter;
}

//==========================================================================
//
// write_zip
//
//==========================================================================

void write_zip(const char *zipname, dir_tree_t *trees)
{
	int i, num_files;
	file_sorted_t *sorted;
	zipFile zip;

	num_files = count_files(trees);
	sorted = sort_files(trees, num_files);
	if (sorted == NULL)
	{
		no_mem = 1;
		return;
	}
	zip = zipOpen(zipname, APPEND_STATUS_CREATE);
	if (zip == NULL)
	{
		fprintf(stderr, "Could not open %s: %s\n", zipname, strerror(errno));
	}
	else
	{
		for (i = 0; i < num_files; ++i)
		{
			append_to_zip(zip, sorted + i);
		}
		zipClose(zip, NULL);
		printf("Wrote %d/%d files to %s\n", i, num_files, zipname);
	}
	free(sorted);
}

//==========================================================================
//
// append_to_zip
//
// Write a given file to the zipFile.
// 
// zipfile: zip object to be written to
//    file: file to read data from
// 
// returns: 0 = success, 1 = error
//
//==========================================================================

int append_to_zip(zipFile zipfile, const file_sorted_t *file)
{
	char *readbuf;
	FILE *lumpfile;
	size_t readlen;
	size_t len;
	zip_fileinfo zip_inf;
	struct tm *ltime;

	// clear zip_inf structure
	memset(&zip_inf, 0, sizeof(zip_inf));

	// try to determine local time
	ltime = localtime(&file->file->time_write);
	// if succeeded,
	if (ltime != NULL)
	{
		// put it into the zip_inf structure
		zip_inf.tmz_date.tm_sec  = ltime->tm_sec;
		zip_inf.tmz_date.tm_min  = ltime->tm_min;
		zip_inf.tmz_date.tm_hour = ltime->tm_hour;
		zip_inf.tmz_date.tm_mday = ltime->tm_mday;
		zip_inf.tmz_date.tm_mon  = ltime->tm_mon;
		zip_inf.tmz_date.tm_year = ltime->tm_year;
	}

	// lumpfile = source file
	lumpfile = fopen(file->file->path, "rb");
	if (lumpfile == NULL)
	{
		fprintf(stderr, "Could not open %s: %s\n", file->file->path, strerror(errno));
		return 1;
	}
	// len = source size
	fseek (lumpfile, 0, SEEK_END);
	len = ftell(lumpfile);
	fseek (lumpfile, 0, SEEK_SET);

	// allocate a buffer for the whole source file
	readbuf = malloc(len);
	if (readbuf == NULL)
	{
		fclose(lumpfile);
		fprintf(stderr, "Could not allocate %u bytes\n", (int)len);
		return 1;
	}
	// read the whole source file into buffer
	readlen = fread(readbuf, 1, len, lumpfile);
	fclose(lumpfile);

	// if read less bytes than expected, 
	if (readlen != len)
	{
		// diagnose and return error
		free(readbuf);
		fprintf(stderr, "Unable to read %s\n", file->file->path);
		return 1;
	}
	// file loaded

	// create zip entry, giving entry name and zip_inf data,
	// write data into zipfile, and close the zip entry
	if (Z_OK != zipAddFileToZip(zipfile, file->path_in_zip, &zip_inf, readbuf, (unsigned)len))
	{
		free(readbuf);
		fprintf(stderr, "Unable to write %s to zip\n", file->file->path);
		return 1;
	}
	// all done
	free(readbuf);
	return 0;
}

int __cdecl main (int argc, char **argv)
{
	dir_tree_t *tree, *trees;
	file_entry_t *file;
	struct stat zipstat;
	int needwrite;

	if (argc < 3)
	{
		print_usage(argv[0]);
		return 1;
	}

	trees = add_dirs(&argv[2]);
	if(no_mem)
	{
		free_dir_trees(trees);
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}

	needwrite = 0;
	if (stat(argv[1], &zipstat) != 0)
	{
		if (errno == ENOENT)
		{
			needwrite = 1;
		}
		else
		{
			fprintf(stderr, "Could not stat %s: %s\n", argv[1], strerror(errno));
		}
	}
	else
	{
		// Check the files in each tree. If any one of them was modified more
		// recently than the zip, then it needs to be recreated.
		for (tree = trees; tree != NULL; tree = tree->next)
		{
			for (file = tree->files; file != NULL; file = file->next)
			{
				if (file->time_write > zipstat.st_mtime)
				{
					needwrite = 1;
					break;
				}
			}
		}
	}
	if (needwrite)
	{
		write_zip(argv[1], trees);
	}
	free_dir_trees(trees);
	if (no_mem)
	{
		fprintf(stderr, "Out of memory.\n");
		return 1;
	}
	return 0;
}
