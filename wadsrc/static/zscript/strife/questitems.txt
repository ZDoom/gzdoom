/*
 * Quest Item Usage:
 *
 *  1 You got Beldin's ring
 *  2 You got the Chalice
 *  3 You got 300 gold, so it's time to visit Irale and the governor
 *  4 Accepted the governor's power coupling mission
 *  5 Accepted the governor's mission to kill Derwin
 *  6 You broke the Front's power coupling
 *  7 You took out the scanning team
 *  8 You got the broken power coupling
 *  9 You got the ear
 * 10 You got the prison pass
 * 11 You got the prison key
 * 12 You got the severed hand
 * 13 You've freed the prisoners!
 * 14 You've Blown Up the Crystal
 * 15 You got the guard uniform
 * 16 You've Blown Up the Gates (/Piston);
 * 17 You watched the Sigil slideshow on map10;
 * 18 You got the Oracle pass
 * 19 You met Quincy and talked to him about the Bishop
 * 20;
 * 21 You Killed the Bishop!
 * 22 The Oracle has told you to kill Macil
 * 23 You've Killed The Oracle!
 * 24 You Killed Macil!
 * 25 You've destroyed the Converter!
 * 26 You've Killed The Loremaster!
 * 27 You've Blown Up the Computer
 * 28 You got the catacomb key
 * 29 You destroyed the mind control device in the mines
 * 30;
 * 31;
 */

class QuestItem : Inventory
{
	States
	{
	Spawn:
		TOKN A -1;
		Stop;
	}
}

// Quest Items -------------------------------------------------------------

class QuestItem1 : QuestItem
{
}

class QuestItem2 : QuestItem
{
}

class QuestItem3 : QuestItem
{
}

class QuestItem4 : QuestItem
{
	Default
	{
		Tag "$TAG_QUEST4";
	}
}

class QuestItem5 : QuestItem
{
	Default
	{
		Tag "$TAG_QUEST5";
	}
}

class QuestItem6 : QuestItem
{
	Default
	{
		Tag "TAG_QUEST6";
	}
}

class QuestItem7 : QuestItem
{
}

class QuestItem8 : QuestItem
{
}

class QuestItem9 : QuestItem
{
}

class QuestItem10 : QuestItem
{
}

class QuestItem11 : QuestItem
{
}

class QuestItem12 : QuestItem
{
}

class QuestItem13 : QuestItem
{
}

class QuestItem14 : QuestItem
{
}

class QuestItem15 : QuestItem
{
}

class QuestItem16 : QuestItem
{
}

class QuestItem17 : QuestItem
{
}

class QuestItem18 : QuestItem
{
}

class QuestItem19 : QuestItem
{
}

class QuestItem20 : QuestItem
{
}

class QuestItem21 : QuestItem
{
}

class QuestItem22 : QuestItem
{
}

class QuestItem23 : QuestItem
{
}

class QuestItem24 : QuestItem
{
}

class QuestItem25 : QuestItem
{
}

class QuestItem26 : QuestItem
{
}

class QuestItem27 : QuestItem
{
}

class QuestItem28 : QuestItem
{
}

class QuestItem29 : QuestItem
{
}

class QuestItem30 : QuestItem
{
}

class QuestItem31 : QuestItem
{
}

