/* $Id: globals.h,v 1.4 2004/05/13 03:47:52 nuffer Exp $ */
#ifndef	_globals_h
#define	_globals_h

#include "basics.h"

extern char *fileName;
extern char *outputFileName;
extern bool sFlag;
extern bool bFlag;
extern unsigned int oline;

extern uchar asc2ebc[256];
extern uchar ebc2asc[256];

extern uchar *xlat, *talx;

#endif
