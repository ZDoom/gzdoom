/*
 * Quest Item Usage:
 *
 *  1 You got Beldin's ring
 *  2 You got the Chalice
 *  3 You got 300 gold, so it's time to visit Irale and the governor
 *  4 Accepted the governor's power coupling mission
 *  5 Accepted the governor's mission to kill Derwin
 *  6 You broke the Front's power coupling
 *  7
 *  8 You got the broken power coupling
 *  9 You got the ear
 * 10 You got the prison pass
 * 11 You got the prison key
 * 12 You got the severed hand
 * 13 You've freed the prisoners!
 * 14 You've Blown Up the Crystal
 * 15 You got the guard uniform
 * 16 You've Blown Up the Gates (/Piston)
 * 17 You watched the Sigil slideshow on map10
 * 18 You got the Oracle pass
 * 19 You met Quincy and talked to him about the Bishop
 * 20
 * 21 You Killed the Bishop!
 * 22 The Oracle has told you to kill Macil
 * 23 You've Killed The Oracle!
 * 24 You Killed Macil!
 * 25 You've destroyed the Converter!
 * 26 You've Killed The Loremaster!
 * 27 You've Blown Up the Computer
 * 28 You got the catacomb key
 * 29 You destroyed the mind control device in the mines
 * 30
 * 31
 */

#include "a_pickups.h"
#include "a_strifeglobal.h"

FState AQuestItem::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AQuestItem, Strife, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Quest Item 1 -------------------------------------------------------------

class AQuestItem1 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem1, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem1, Strife, -1, 0)
	PROP_StrifeType (312)
	PROP_StrifeTeaserType (293)
END_DEFAULTS

// Quest Item 2 -------------------------------------------------------------

class AQuestItem2 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem2, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem2, Strife, -1, 0)
	PROP_StrifeType (313)
	PROP_StrifeTeaserType (294)
	// "Blown_Up_the_Crystal" in the Teaser
END_DEFAULTS

// Quest Item 3 -------------------------------------------------------------

class AQuestItem3 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem3, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem3, Strife, -1, 0)
	PROP_StrifeType (314)
	PROP_StrifeTeaserType (295)
	// "Blown_Up_the_Gates" in the Teaser
END_DEFAULTS

// Quest Item 4 -------------------------------------------------------------

class AQuestItem4 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem4, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem4, Strife, -1, 0)
	PROP_StrifeType (315)
	PROP_StrifeTeaserType (296)
	PROP_Tag ("quest4")
END_DEFAULTS

// Quest Item 5 -------------------------------------------------------------

class AQuestItem5 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem5, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem5, Strife, -1, 0)
	PROP_StrifeType (316)
	PROP_StrifeTeaserType (297)
	PROP_Tag ("quest5")
END_DEFAULTS

// Quest Item 6 -------------------------------------------------------------

class AQuestItem6 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem6, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem6, Strife, -1, 0)
	PROP_StrifeType (317)
	PROP_StrifeTeaserType (298)
	PROP_Tag ("quest6")
END_DEFAULTS

// Quest Item 7 -------------------------------------------------------------

class AQuestItem7 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem7, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem7, Strife, -1, 0)
	PROP_StrifeType (318)
END_DEFAULTS

// Quest Item 8 -------------------------------------------------------------

class AQuestItem8 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem8, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem8, Strife, -1, 0)
	PROP_StrifeType (319)
END_DEFAULTS

// Quest Item 9 -------------------------------------------------------------

class AQuestItem9 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem9, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem9, Strife, -1, 0)
	PROP_StrifeType (320)
END_DEFAULTS

// Quest Item 10 ------------------------------------------------------------

class AQuestItem10 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem10, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem10, Strife, -1, 0)
	PROP_StrifeType (321)
END_DEFAULTS

// Quest Item 11 ------------------------------------------------------------

class AQuestItem11 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem11, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem11, Strife, -1, 0)
	PROP_StrifeType (322)
END_DEFAULTS

// Quest Item 12 ------------------------------------------------------------

class AQuestItem12 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem12, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem12, Strife, -1, 0)
	PROP_StrifeType (323)
END_DEFAULTS

// Quest Item 13 ------------------------------------------------------------

class AQuestItem13 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem13, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem13, Strife, -1, 0)
	PROP_StrifeType (324)
END_DEFAULTS

// Quest Item 14 ------------------------------------------------------------

class AQuestItem14 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem14, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem14, Strife, -1, 0)
	PROP_StrifeType (325)
	PROP_Tag ("You've_Blown_Up_the_Crystal")
END_DEFAULTS

// Quest Item 15 ------------------------------------------------------------

class AQuestItem15 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem15, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem15, Strife, -1, 0)
	PROP_StrifeType (326)
END_DEFAULTS

// Quest Item 16 ------------------------------------------------------------

class AQuestItem16 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem16, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem16, Strife, -1, 0)
	PROP_StrifeType (327)
	PROP_Tag ("You've_Blown_Up_the_Gates")
END_DEFAULTS

// Quest Item 17 ------------------------------------------------------------

class AQuestItem17 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem17, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem17, Strife, -1, 0)
	PROP_StrifeType (328)
END_DEFAULTS

// Quest Item 18 ------------------------------------------------------------

class AQuestItem18 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem18, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem18, Strife, -1, 0)
	PROP_StrifeType (329)
END_DEFAULTS

// Quest Item 19 ------------------------------------------------------------

class AQuestItem19 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem19, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem19, Strife, -1, 0)
	PROP_StrifeType (330)
END_DEFAULTS

// Quest Item 20 ------------------------------------------------------------

class AQuestItem20 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem20, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem20, Strife, -1, 0)
	PROP_StrifeType (331)
END_DEFAULTS

// Quest Item 21 ------------------------------------------------------------

class AQuestItem21 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem21, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem21, Strife, -1, 0)
	PROP_StrifeType (332)
	PROP_Tag ("You_Killed_the_Bishop!")
END_DEFAULTS

// Quest Item 22 ------------------------------------------------------------

class AQuestItem22 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem22, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem22, Strife, -1, 0)
	PROP_StrifeType (333)
END_DEFAULTS

// Quest Item 23 ------------------------------------------------------------

class AQuestItem23 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem23, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem23, Strife, -1, 0)
	PROP_StrifeType (334)
	PROP_Tag ("You've_Killed_The_Oracle!")
END_DEFAULTS

// Quest Item 24 ------------------------------------------------------------

class AQuestItem24 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem24, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem24, Strife, -1, 0)
	PROP_StrifeType (335)
	PROP_Tag ("You_Killed_Macil!")
END_DEFAULTS

// Quest Item 25 ------------------------------------------------------------

class AQuestItem25 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem25, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem25, Strife, -1, 0)
	PROP_StrifeType (336)
END_DEFAULTS

// Quest Item 26 ------------------------------------------------------------

class AQuestItem26 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem26, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem26, Strife, -1, 0)
	PROP_StrifeType (337)
	PROP_Tag ("You've_Killed_The_Loremaster!")
END_DEFAULTS

// Quest Item 27 ------------------------------------------------------------

class AQuestItem27 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem27, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem27, Strife, -1, 0)
	PROP_StrifeType (338)
	PROP_Tag ("You've_Blown_Up_the_Computer")
END_DEFAULTS

// Quest Item 28 ------------------------------------------------------------

class AQuestItem28 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem28, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem28, Strife, -1, 0)
	PROP_StrifeType (339)
END_DEFAULTS

// Quest Item 29 ------------------------------------------------------------

class AQuestItem29 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem29, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem29, Strife, -1, 0)
	PROP_StrifeType (340)
END_DEFAULTS

// Quest Item 30 ------------------------------------------------------------

class AQuestItem30 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem30, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem30, Strife, -1, 0)
	PROP_StrifeType (341)
END_DEFAULTS

// Quest Item 31 ------------------------------------------------------------

class AQuestItem31 : public AQuestItem
{
	DECLARE_STATELESS_ACTOR (AQuestItem31, AQuestItem)
};

IMPLEMENT_STATELESS_ACTOR (AQuestItem31, Strife, -1, 0)
	PROP_StrifeType (342)
END_DEFAULTS

// Quest Item Class Pointers ------------------------------------------------

const TypeInfo *QuestItemClasses[31] =
{
	RUNTIME_CLASS(AQuestItem1),
	RUNTIME_CLASS(AQuestItem2),
	RUNTIME_CLASS(AQuestItem3),
	RUNTIME_CLASS(AQuestItem4),
	RUNTIME_CLASS(AQuestItem5),
	RUNTIME_CLASS(AQuestItem6),
	RUNTIME_CLASS(AQuestItem7),
	RUNTIME_CLASS(AQuestItem8),
	RUNTIME_CLASS(AQuestItem9),
	RUNTIME_CLASS(AQuestItem10),
	RUNTIME_CLASS(AQuestItem11),
	RUNTIME_CLASS(AQuestItem12),
	RUNTIME_CLASS(AQuestItem13),
	RUNTIME_CLASS(AQuestItem14),
	RUNTIME_CLASS(AQuestItem15),
	RUNTIME_CLASS(AQuestItem16),
	RUNTIME_CLASS(AQuestItem17),
	RUNTIME_CLASS(AQuestItem18),
	RUNTIME_CLASS(AQuestItem19),
	RUNTIME_CLASS(AQuestItem20),
	RUNTIME_CLASS(AQuestItem21),
	RUNTIME_CLASS(AQuestItem22),
	RUNTIME_CLASS(AQuestItem23),
	RUNTIME_CLASS(AQuestItem24),
	RUNTIME_CLASS(AQuestItem25),
	RUNTIME_CLASS(AQuestItem26),
	RUNTIME_CLASS(AQuestItem27),
	RUNTIME_CLASS(AQuestItem28),
	RUNTIME_CLASS(AQuestItem29),
	RUNTIME_CLASS(AQuestItem30),
	RUNTIME_CLASS(AQuestItem31)
};
