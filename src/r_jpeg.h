#ifndef __R_JPEG_H
#define __R_JPEG_H

extern "C"
{
#include <jpeglib.h>
}

class FileReader;


struct FLumpSourceMgr : public jpeg_source_mgr
{
	FileReader *Lump;
	JOCTET Buffer[4096];
	bool StartOfFile;

	FLumpSourceMgr (FileReader *lump, j_decompress_ptr cinfo);
	static void InitSource (j_decompress_ptr cinfo);
	static boolean FillInputBuffer (j_decompress_ptr cinfo);
	static void SkipInputData (j_decompress_ptr cinfo, long num_bytes);
	static void TermSource (j_decompress_ptr cinfo);
};


void JPEG_ErrorExit (j_common_ptr cinfo);
void JPEG_OutputMessage (j_common_ptr cinfo);


#endif
