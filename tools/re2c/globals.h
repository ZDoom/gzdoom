/* $Id: globals.h,v 1.31 2006/05/14 13:38:26 helly Exp $ */
#ifndef	_globals_h
#define	_globals_h

#include "basics.h"
#include <set>
#include <algorithm>
#include <string>
#include "stream_lc.h"

namespace re2c
{

extern file_info sourceFileInfo;
extern file_info outputFileInfo;

extern bool bFlag;
extern bool dFlag;
extern bool eFlag;
extern bool fFlag;
extern bool gFlag;
extern bool iFlag;
extern bool sFlag;
extern bool wFlag;

extern bool bSinglePass;
extern bool bLastPass;

extern bool bUsedYYAccept;
extern bool bUsedYYMaxFill;
extern bool bUsedYYMarker;

extern bool bUseStartLabel;
extern std::string startLabelName;
extern uint maxFill;
extern uint next_label;
extern uint cGotoThreshold;

/* configurations */
extern uint topIndent;
extern std::string indString;
extern bool yybmHexTable;
extern bool bUseStateAbort;
extern bool bUseStateNext;
extern bool bWroteGetState;
extern bool bUseYYFill;

extern uint asc2ebc[256];
extern uint ebc2asc[256];

extern uint *xlat, *talx;

extern uint next_fill_index;
extern uint last_fill_index;
extern std::set<uint> vUsedLabels;

extern uint nRealChars;

inline char octCh(uint c)
{
	return '0' + c % 8;
}

inline char hexCh(uint c)
{
	static const char * sHex = "0123456789ABCDEF";
	
	return sHex[c & 0x0F];
}

} // end namespace re2c

#endif
