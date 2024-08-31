#pragma once
// Directory searching routines

#include <stdint.h>
#include <vector>
#include <string>

namespace FileSys {
	
struct FileListEntry
{
	std::string FileName;		// file name only
	std::string FilePath;		// full path to file
	std::string FilePathRel;	// path relative to the scanned directory.
	size_t Length = 0;
	bool isDirectory = false;
	bool isReadonly = false;
	bool isHidden = false;
	bool isSystem = false;
};

using FileList = std::vector<FileListEntry>;

struct FCompressedBuffer;
bool ScanDirectory(std::vector<FileListEntry>& list, const char* dirpath, const char* match, bool nosubdir = false, bool readhidden = false);
bool FS_DirEntryExists(const char* pathname, bool* isdir);

inline void FixPathSeparator(char* path)
{
	while (*path)
	{
		if (*path == '\\')
			*path = '/';
		path++;
	}
}

}
