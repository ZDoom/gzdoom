// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// $Log:$
//
// DESCRIPTION:
//		DOOM strings, by language.
//
//-----------------------------------------------------------------------------


#ifndef __DSTRINGS__
#define __DSTRINGS__

void D_InitStrings (void);

void ReplaceString (char **ptr, char *str);


// Misc. other strings.
#define SAVEGAMENAME	"zdoomsv"

// QuitDOOM messages
#define NUM_QUITMESSAGES   14

extern char* endmsg[];

// [RH] String handling has changed significantly and is no longer static per build.
typedef enum {
	str_notchanged,
	str_foreign,
	str_patched,
	str_custom
} strtype_t;

typedef struct gamestring_s {
	strtype_t	 type;
	char		*builtin;
	char		*string;
} gamestring_t;

// Replacement string defines for the ones that used to reside in d_englsh.h and d_french.h
#define D_DEVSTR			(Strings[0].string)
#define D_CDROM				(Strings[1].string)

#define PRESSKEY			(Strings[2].string)
#define PRESSYN				(Strings[3].string)
#define QUITMSG				(Strings[4].string)
#define LOADNET				(Strings[5].string)
#define QLOADNET			(Strings[6].string)
#define QSAVESPOT			(Strings[7].string)
#define SAVEDEAD			(Strings[8].string)
#define QSPROMPT			(Strings[9].string)
#define QLPROMPT			(Strings[10].string)

#define NEWGAME				(Strings[11].string)
#define NIGHTMARE			(Strings[12].string)
#define SWSTRING			(Strings[13].string)

#define MSGOFF				(Strings[14].string)
#define MSGON				(Strings[15].string)
#define NETEND				(Strings[16].string)
#define ENDGAME				(Strings[17].string)

#define DOSY				(Strings[18].string)

#define EMPTYSTRING			(Strings[19].string)

#define GOTARMOR			(Strings[20].string)
#define GOTMEGA				(Strings[21].string)
#define	GOTHTHBONUS			(Strings[22].string)
#define GOTARMBONUS			(Strings[23].string)
#define	GOTSTIM				(Strings[24].string)
#define GOTMEDINEED			(Strings[25].string)
#define GOTMEDIKIT			(Strings[26].string)
#define GOTSUPER			(Strings[27].string)

#define GOTBLUECARD			(Strings[28].string)
#define GOTYELWCARD			(Strings[29].string)
#define GOTREDCARD			(Strings[30].string)
#define GOTBLUESKUL			(Strings[31].string)
#define GOTYELWSKUL			(Strings[32].string)
#define GOTREDSKULL			(Strings[33].string)

#define GOTINVUL			(Strings[34].string)
#define GOTBERSERK			(Strings[35].string)
#define GOTINVIS			(Strings[36].string)
#define GOTSUIT				(Strings[37].string)
#define GOTMAP				(Strings[38].string)
#define GOTVISOR			(Strings[39].string)
#define GOTMSPHERE			(Strings[40].string)

#define GOTCLIP				(Strings[41].string)
#define GOTCLIPBOX			(Strings[42].string)
#define GOTROCKET			(Strings[43].string)
#define GOTROCKBOX			(Strings[44].string)
#define GOTCELL				(Strings[45].string)
#define	GOTCELLBOX			(Strings[46].string)
#define	GOTSHELLS			(Strings[47].string)
#define GOTSHELLBOX			(Strings[48].string)
#define GOTBACKPACK			(Strings[49].string)

#define GOTBFG9000			(Strings[50].string)
#define GOTCHAINGUN			(Strings[51].string)
#define GOTCHAINSAW			(Strings[52].string)
#define GOTLAUNCHER			(Strings[53].string)
#define GOTPLASMA			(Strings[54].string)
#define GOTSHOTGUN			(Strings[55].string)
#define GOTSHOTGUN2			(Strings[56].string)

#define PD_BLUEO			(Strings[57].string)
#define PD_REDO				(Strings[58].string)
#define PD_YELLOWO			(Strings[59].string)
#define PD_BLUEK			(Strings[60].string)
#define PD_REDK				(Strings[61].string)
#define PD_YELLOWK			(Strings[62].string)

#define GGSAVED				(Strings[63].string)

#define HUSTR_MSGU			(Strings[64].string)

#define HUSTR_E1M1			(Strings[65].string)
#define HUSTR_E1M2			(Strings[66].string)
#define HUSTR_E1M3			(Strings[67].string)
#define HUSTR_E1M4			(Strings[68].string)
#define HUSTR_E1M5			(Strings[69].string)
#define HUSTR_E1M6			(Strings[70].string)
#define HUSTR_E1M7			(Strings[71].string)
#define HUSTR_E1M8			(Strings[72].string)
#define HUSTR_E1M9			(Strings[73].string)

#define HUSTR_E2M1			(Strings[74].string)
#define HUSTR_E2M2			(Strings[75].string)
#define HUSTR_E2M3			(Strings[76].string)
#define HUSTR_E2M4			(Strings[77].string)
#define HUSTR_E2M5			(Strings[78].string)
#define HUSTR_E2M6			(Strings[79].string)
#define HUSTR_E2M7			(Strings[80].string)
#define HUSTR_E2M8			(Strings[81].string)
#define HUSTR_E2M9			(Strings[82].string)

#define HUSTR_E3M1			(Strings[83].string)
#define HUSTR_E3M2			(Strings[84].string)
#define HUSTR_E3M3			(Strings[85].string)
#define HUSTR_E3M4			(Strings[86].string)
#define HUSTR_E3M5			(Strings[87].string)
#define HUSTR_E3M6			(Strings[88].string)
#define HUSTR_E3M7			(Strings[89].string)
#define HUSTR_E3M8			(Strings[90].string)
#define HUSTR_E3M9			(Strings[91].string)

#define HUSTR_E4M1			(Strings[92].string)
#define HUSTR_E4M2			(Strings[93].string)
#define HUSTR_E4M3			(Strings[94].string)
#define HUSTR_E4M4			(Strings[95].string)
#define HUSTR_E4M5			(Strings[96].string)
#define HUSTR_E4M6			(Strings[97].string)
#define HUSTR_E4M7			(Strings[98].string)
#define HUSTR_E4M8			(Strings[99].string)
#define HUSTR_E4M9			(Strings[100].string)

#define HUSTR_1				(Strings[101].string)
#define HUSTR_2				(Strings[102].string)
#define HUSTR_3				(Strings[103].string)
#define HUSTR_4				(Strings[104].string)
#define HUSTR_5				(Strings[105].string)
#define HUSTR_6				(Strings[106].string)
#define HUSTR_7				(Strings[107].string)
#define HUSTR_8				(Strings[108].string)
#define HUSTR_9				(Strings[109].string)
#define HUSTR_10			(Strings[110].string)
#define HUSTR_11			(Strings[111].string)

#define HUSTR_12			(Strings[112].string)
#define HUSTR_13			(Strings[113].string)
#define HUSTR_14			(Strings[114].string)
#define HUSTR_15			(Strings[115].string)
#define HUSTR_16			(Strings[116].string)
#define HUSTR_17			(Strings[117].string)
#define HUSTR_18			(Strings[118].string)
#define HUSTR_19			(Strings[119].string)
#define HUSTR_20			(Strings[120].string)

#define HUSTR_21			(Strings[121].string)
#define HUSTR_22			(Strings[122].string)
#define HUSTR_23			(Strings[123].string)
#define HUSTR_24			(Strings[124].string)
#define HUSTR_25			(Strings[125].string)
#define HUSTR_26			(Strings[126].string)
#define HUSTR_27			(Strings[127].string)
#define HUSTR_28			(Strings[128].string)
#define HUSTR_29			(Strings[129].string)
#define HUSTR_30			(Strings[130].string)

#define HUSTR_31			(Strings[131].string)
#define HUSTR_32			(Strings[132].string)

#define PHUSTR_1			(Strings[133].string)
#define PHUSTR_2			(Strings[134].string)
#define PHUSTR_3			(Strings[135].string)
#define PHUSTR_4			(Strings[136].string)
#define PHUSTR_5			(Strings[137].string)
#define PHUSTR_6			(Strings[138].string)
#define PHUSTR_7			(Strings[139].string)
#define PHUSTR_8			(Strings[140].string)
#define PHUSTR_9			(Strings[141].string)
#define PHUSTR_10			(Strings[142].string)
#define PHUSTR_11			(Strings[143].string)

#define PHUSTR_12			(Strings[144].string)
#define PHUSTR_13			(Strings[145].string)
#define PHUSTR_14			(Strings[146].string)
#define PHUSTR_15			(Strings[147].string)
#define PHUSTR_16			(Strings[148].string)
#define PHUSTR_17			(Strings[149].string)
#define PHUSTR_18			(Strings[150].string)
#define PHUSTR_19			(Strings[151].string)
#define PHUSTR_20			(Strings[152].string)

#define PHUSTR_21			(Strings[153].string)
#define PHUSTR_22			(Strings[154].string)
#define PHUSTR_23			(Strings[155].string)
#define PHUSTR_24			(Strings[156].string)
#define PHUSTR_25			(Strings[157].string)
#define PHUSTR_26			(Strings[158].string)
#define PHUSTR_27			(Strings[159].string)
#define PHUSTR_28			(Strings[160].string)
#define PHUSTR_29			(Strings[161].string)
#define PHUSTR_30			(Strings[162].string)

#define PHUSTR_31			(Strings[163].string)
#define PHUSTR_32			(Strings[164].string)

#define THUSTR_1			(Strings[165].string)
#define THUSTR_2			(Strings[166].string)
#define THUSTR_3			(Strings[167].string)
#define THUSTR_4			(Strings[168].string)
#define THUSTR_5			(Strings[169].string)
#define THUSTR_6			(Strings[170].string)
#define THUSTR_7			(Strings[171].string)
#define THUSTR_8			(Strings[172].string)
#define THUSTR_9			(Strings[173].string)
#define THUSTR_10			(Strings[174].string)
#define THUSTR_11			(Strings[175].string)

#define THUSTR_12			(Strings[176].string)
#define THUSTR_13			(Strings[177].string)
#define THUSTR_14			(Strings[178].string)
#define THUSTR_15			(Strings[179].string)
#define THUSTR_16			(Strings[180].string)
#define THUSTR_17			(Strings[181].string)
#define THUSTR_18			(Strings[182].string)
#define THUSTR_19			(Strings[183].string)
#define THUSTR_20			(Strings[184].string)

#define THUSTR_21			(Strings[185].string)
#define THUSTR_22			(Strings[186].string)
#define THUSTR_23			(Strings[187].string)
#define THUSTR_24			(Strings[188].string)
#define THUSTR_25			(Strings[189].string)
#define THUSTR_26			(Strings[190].string)
#define THUSTR_27			(Strings[191].string)
#define THUSTR_28			(Strings[192].string)
#define THUSTR_29			(Strings[193].string)
#define THUSTR_30			(Strings[194].string)

#define THUSTR_31			(Strings[195].string)
#define THUSTR_32			(Strings[196].string)

#define HUSTR_CHATMACRO1	"I'm ready to kick butt!"
#define HUSTR_CHATMACRO2	"I'm OK."
#define HUSTR_CHATMACRO3	"I'm not looking too good!"
#define HUSTR_CHATMACRO4	"Help!"
#define HUSTR_CHATMACRO5	"You suck!"
#define HUSTR_CHATMACRO6	"Next time, scumbag..."
#define HUSTR_CHATMACRO7	"Come here!"
#define HUSTR_CHATMACRO8	"I'll take care of it."
#define HUSTR_CHATMACRO9	"Yes"
#define HUSTR_CHATMACRO0	"No"

#define HUSTR_TALKTOSELF1	(Strings[197].string)
#define HUSTR_TALKTOSELF2	(Strings[198].string)
#define HUSTR_TALKTOSELF3	(Strings[199].string)
#define HUSTR_TALKTOSELF4	(Strings[200].string)
#define HUSTR_TALKTOSELF5	(Strings[201].string)

#define HUSTR_MESSAGESENT	(Strings[202].string)

#define AMSTR_FOLLOWON		(Strings[203].string)
#define AMSTR_FOLLOWOFF		(Strings[204].string)

#define AMSTR_GRIDON		(Strings[205].string)
#define AMSTR_GRIDOFF		(Strings[206].string)

#define AMSTR_MARKEDSPOT	(Strings[207].string)
#define AMSTR_MARKSCLEARED	(Strings[208].string)

#define STSTR_MUS			(Strings[209].string)
#define STSTR_NOMUS			(Strings[210].string)
#define STSTR_DQDON			(Strings[211].string)
#define STSTR_DQDOFF		(Strings[212].string)

#define STSTR_KFAADDED		(Strings[213].string)
#define STSTR_FAADDED		(Strings[214].string)

#define STSTR_NCON			(Strings[215].string)
#define STSTR_NCOFF			(Strings[216].string)

#define	STSTR_BEHOLD		(Strings[217].string)
#define STSTR_BEHOLDX		(Strings[218].string)

#define STSTR_CHOPPERS		(Strings[219].string)
#define STSTR_CLEV			(Strings[220].string)

#define E1TEXT				(Strings[221].string)
#define E2TEXT				(Strings[222].string)
#define E3TEXT				(Strings[223].string)
#define E4TEXT				(Strings[224].string)

#define C1TEXT				(Strings[225].string)
#define C2TEXT				(Strings[226].string)
#define C3TEXT				(Strings[227].string)
#define C4TEXT				(Strings[228].string)
#define C5TEXT				(Strings[229].string)
#define C6TEXT				(Strings[230].string)

#define P1TEXT				(Strings[231].string)
#define P2TEXT				(Strings[232].string)
#define P3TEXT				(Strings[233].string)
#define P4TEXT				(Strings[234].string)
#define P5TEXT				(Strings[235].string)
#define P6TEXT				(Strings[236].string)

#define T1TEXT				(Strings[237].string)
#define T2TEXT				(Strings[238].string)
#define T3TEXT				(Strings[239].string)
#define T4TEXT				(Strings[240].string)
#define T5TEXT				(Strings[241].string)
#define T6TEXT				(Strings[242].string)

#define CC_ZOMBIE			(Strings[243].string)
#define CC_SHOTGUN			(Strings[244].string)
#define CC_HEAVY			(Strings[245].string)
#define CC_IMP				(Strings[246].string)
#define CC_DEMON			(Strings[247].string)
#define CC_LOST				(Strings[248].string)
#define CC_CACO				(Strings[249].string)
#define CC_HELL				(Strings[250].string)
#define CC_BARON			(Strings[251].string)
#define CC_ARACH			(Strings[252].string)
#define CC_PAIN				(Strings[253].string)
#define CC_REVEN			(Strings[254].string)
#define CC_MANCU			(Strings[255].string)
#define CC_ARCH				(Strings[256].string)
#define CC_SPIDER			(Strings[257].string)
#define CC_CYBER			(Strings[258].string)
#define CC_HERO				(Strings[259].string)

#define NUMSTRINGS			260

extern gamestring_t Strings[];

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
