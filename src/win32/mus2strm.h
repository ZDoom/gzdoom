#ifndef __MUS2STRM_H__
#define __MUS2STRM_H__

#include "mid2strm.h"

PSTREAMBUF	mus2strmConvert (BYTE *inFile, DWORD inSize);
void		mus2strmCleanup (void);


#endif //__MUS2STRM_H__
