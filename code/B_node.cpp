  /********************************
* B_node.c                      *
* Description:                  *
* Node system                   *
*********************************/


#include "doomdef.h"
#include "p_local.h"
#include "p_inter.h"
#include "b_bot.h"
#include "g_game.h"
#include "z_zone.h"
#include "doomstat.h"

#define MAXSTEPMOVE (24*FRACUNIT)

BEGIN_CUSTOM_CVAR (bot_shownodes, "0", 0)
{
	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ( (actor = iterator.Next ()) )
	{
		if (actor->type == MT_SHOWNODE)
			delete actor;
	}

	if (var.value)
	{
		SDWORD node;
		for (node = 0; node < bglobal.nodenum; node++)
		{
			botnode_t *pnode = Numnode_to_pointer (node);
			new AActor (pnode->x, pnode->y, pnode->z, MT_SHOWNODE);
		}
	}
}
END_CUSTOM_CVAR (bot_shownodes)

//MAKEME: Add Perfect DYNAMIC support for different types
void Calc_nodes()
{
	bool doplace;
	bool special; //If there is a special reason to place the node.
	botnode_t *newnode;
	botnode_t *node1;

	//Init
	doplace = true; //Initialize and First node placement.
	special = false;

	if (!bglobal.nodenum) //For first node placement, init linked list.
	{
		newnode = (botnode_t *)Z_Malloc (sizeof(*newnode), PU_LEVEL, NULL);
		newnode->x = players[consoleplayer].mo->x;
		newnode->y = players[consoleplayer].mo->y;
		newnode->z = players[consoleplayer].mo->z+MAXSTEPMOVE;
		newnode->type = n_normal;
		newnode->number = bglobal.nodenum;
		newnode->next = firstnode;
		firstnode = newnode;
		bglobal.nodenum++;
		if (bot_shownodes.value)
			new AActor (newnode->x, newnode->y, newnode->z, MT_SHOWNODE);
		return;
	}

	//Going to place a node???
	node1 = firstnode;
	while (node1)
	{
		Host (node1, 1);

		//Check if old nodes too close.
		if (P_AproxDistance (node1->x - players[consoleplayer].mo->x,
			node1->y - players[consoleplayer].mo->y) < NODEMINDIST)
        {
			if (P_CheckSight(players[consoleplayer].mo, bglobal.body1, true))
				doplace = false; //a node too close.
		}
		node1 = node1->next;
	}

	//Special reasons...
    if ((players[consoleplayer].mo->momx  //Fix for crash when dying while nodecalcing
		|| players[consoleplayer].mo->momy)
		&& players[consoleplayer].mo->oldsector)
    {
        //Check for those special reasons.
        if (bglobal.nodenum && players[consoleplayer].mo->oldsector != players[consoleplayer].mo->subsector
			&& players[consoleplayer].mo->oldsector->sector->floorheight <
			   players[consoleplayer].mo->subsector->sector->floorheight)
        {//If console player moved into a new sector. Place a node if floorheight in new sector
         //is higher than old sector.
            //Printf (PRINT_HIGH, "New sector\n");
            special = true;
        }
        //Check if the special placement will be
		//unnecessary due to too short distance from other nodes (less than SPECIALDIST)
		//on the same z plane. This greatly reduces the use of nodes in large stairs.
		if (special)
		{
			node1 = firstnode;
			while (node1)
			{
				Host (node1, 1); //Gets the node bglobal.body1 (a AActor for the node)
				if ((P_AproxDistance(node1->x-players[consoleplayer].mo->x,
					node1->y-players[consoleplayer].mo->y)<SPECIALDIST)
					&& players[consoleplayer].mo->subsector->sector->floorheight
					== bglobal.body1->subsector->sector->floorheight)
				{
					if (P_CheckSight(players[consoleplayer].mo, bglobal.body1, true))
						special=false; //a node to close and not around corner (due to sight).
				}
				node1=node1->next;
			}

		}
    }
	//Set new old sector for console player.
	players[consoleplayer].mo->oldsector = players[consoleplayer].mo->subsector;

	//Place the shit...
	if (doplace || special)
	{
		if (bglobal.nodenum>=MAXNODES)
		{
			Printf (PRINT_HIGH, "Maximum number of nodes reached\n");
			bot_calcnodes.Set ("0");
			return;
		}
		// Initialize node
		newnode = (botnode_t *)Z_Malloc (sizeof(*newnode), PU_LEVEL, NULL);
		newnode->x = players[consoleplayer].mo->x;
		newnode->y = players[consoleplayer].mo->y;
		newnode->z = players[consoleplayer].mo->z+MAXSTEPMOVE;
		newnode->type = n_normal;
		newnode->number = bglobal.nodenum;
		if (bot_shownodes.value)
			new AActor (newnode->x, newnode->y, newnode->z, MT_SHOWNODE);

		//DEBUG:
		//newnode->mo = new AActor (players[consoleplayer].mo->x, players[consoleplayer].mo->y, players[consoleplayer].mo->z+MAXSTEPMOVE, MT_NODE);
		////////////

		newnode->next = firstnode;
		firstnode = newnode;
		bglobal.nodenum++;
		//Printf (PRINT_HIGH, "Placed node nr: %d\n", bglobal.nodenum);//DEBUG
	}
}



bool Write_nodetable (char *file)
{
	SDWORD count;
	botnode_t *node1;

	if (!bglobal.nodenum)
		return false;

	FLZOFile basefile (file, FFile::EWriting);
	FArchive arc (basefile);

	arc << (DWORD)bglobal.nodenum;
	arc << (DWORD)bglobal.thingnodenum;

	for (count = 0; count < bglobal.nodenum; count++)
	{
		node1 = Numnode_to_pointer (count);
		arc << node1->x
			<< node1->y
			<< node1->z;
		arc << node1->thingnumber
			<< node1->number
			<< node1->type
			<< node1->itemtype 
			<< node1->activate_angle
			<< node1->inuse;
	}
	for (count = 0; count < bglobal.thingnodenum; count++)
	{
		arc << (DWORD)bglobal.thingnode[count];
	}

	arc.Close ();

	bot_calcnodes.Set ("0");

	return true;
}

bool Load_nodetable (char *file)
{
	SDWORD count;
	DWORD oldnodenum;
	botnode_t *node1=NULL;
	botnode_t *newnode=NULL;

	FLZOFile arcfile (file, FFile::EReading);
	if (!arcfile.IsOpen ())
		return false;	//No nodetable found.

	FArchive arc (arcfile);

	oldnodenum = bglobal.nodenum;
	//Read header
	arc >> bglobal.nodenum;;
	if (bglobal.nodenum > ABSMAXNODES) //check for another version of nodetable
	{
		Printf (PRINT_HIGH, "Invalid nodetable, Program may crash\n");
		bglobal.nodenum = oldnodenum; //Keep ongoing table.
		return false;
	}
	else if (bglobal.nodenum > MAXNODES)
	{
		Printf (PRINT_HIGH, "Couldn't allocate memory for %u nodes.\n", bglobal.nodenum);
		bglobal.nodenum = oldnodenum; //Keep ongoing table.
		return false;
	}
	arc >> bglobal.thingnodenum;

	//Clean old linked list.
	node1 = firstnode;
	while (node1)
	{
		newnode = node1->next;
		Z_Free (node1);
		node1 = newnode;
	}

	firstnode = NULL;
	newnode   = NULL;

	botnode_t **prev = &firstnode;

	//Read nodes
	for (count = 0; count < bglobal.nodenum; count++)
	{
		newnode = (botnode_t *)Z_Malloc (sizeof(*newnode), PU_LEVEL, NULL);
		*prev = newnode;
		prev = &newnode->next;
		arc >> newnode->x >> newnode->y >> newnode->z;
		arc >> newnode->thingnumber >> newnode->number
			>> newnode->type
			>> newnode->itemtype;
		arc >> newnode->activate_angle >> newnode->inuse;
		if (bot_shownodes.value)
			new AActor (newnode->x, newnode->y, newnode->z, MT_SHOWNODE);
	}

	for (count = 0; count < bglobal.thingnodenum; count++)
	{
		arc >> bglobal.thingnode[count];
	}

	node1 = firstnode;
	while (node1)
	{
		if (node1->type == n_item)
			bglobal.no_thingnodes = false;

		node1 = node1->next;
	}

	arc.Close ();

	bot_calcnodes.Set ("0");

    //Make sure bots don't have the old nodes that could point to "hell".
    for (count = 0; count < MAXPLAYERS; count++)
    {
        if (players[count].isbot)
        {
            players[count].prev = NULL;
            players[count].target = NULL;
            players[count].last_seen = NULL;
            players[count].dest = 0;
        }
    }

	return true;
}



//Returns false if height difference is greater than
//MAXSTEPMOVE or if the mobjs are the same.
bool Check_height_diff (AActor *mo1, AActor *mo2, bool real)
{
	fixed_t diff;

    if (mo1==mo2)
		return false;

	diff = abs (mo1->subsector->sector->floorheight - mo2->subsector->sector->floorheight);

	//if bot (must be mo1) is the highest one and real set to true.
	//way is possible, so return true even if step too great.
	if (real
		&& mo1->subsector->sector->floorheight > mo2->subsector->sector->floorheight)
	{
		return true;
	}

	if (diff < MAXSTEPMOVE*2)
	{
		return true;
	}
	else return false;
}

void Host (botnode_t *n, int hostnum)
{
	if (hostnum == 1)
	{
		if (bglobal.body1 == NULL) //Nothing at all yet...
			bglobal.body1 = new AActor (n->x, n->y, n->z, MT_NODE);
		else
		{
			bglobal.body1->SetOrigin (n->x, n->y, n->z);
		}
	}
	else if (hostnum == 2)
	{
		if (bglobal.body2 == NULL) //Nothing at all yet...
			bglobal.body2 = new AActor (n->x, n->y, n->z, MT_NODE);
		else
		{
			bglobal.body2->SetOrigin (n->x, n->y, n->z);
		}
	}
}

//Creates a temporary mobj (invisible) at the
//given location.
void SetBodyAt (fixed_t x, fixed_t y, fixed_t z, int hostnum)
{
	if (hostnum == 1)
	{
		if (bglobal.body1 == NULL) //Nothing at all yet...
			bglobal.body1 = new AActor (x, y, z, MT_NODE);
		else
		{
			bglobal.body1->SetOrigin (x, y, z);
		}
	}
	else if (hostnum == 2)
	{
		if (bglobal.body2 == NULL) //Nothing at all yet...
			bglobal.body2 = new AActor (x, y, z, MT_NODE);
		else
		{
			bglobal.body2->SetOrigin (x, y, z);
		}
	}
}

void Add_thing_node (item_type_t type)
{
	fixed_t dist;
	botnode_t *node1;
	botnode_t *newnode;

	if (!bglobal.nodenum)
		return;

	//Check if thing already noded.
	node1 = firstnode;
	while (node1)
	{
		if (node1->type == n_item)
		{
			dist = P_AproxDistance (node1->x-players[consoleplayer].mo->x,
									node1->y-players[consoleplayer].mo->y);
			if (dist < 5000000) //There should already be an item node close (50cm) enough.
				return;
		}
		node1=node1->next;
	}

	if (bglobal.nodenum >= MAXNODES)
	{
		Printf (PRINT_HIGH, "Maximum number of nodes reached.\n");
		bot_calcnodes.Set ("0");
		return;
	}

	//Clear paths for the new thing node
    //for(count=0; count<MAXTHINGNODES; count++)
    //        path[bglobal.thingnodenum][count].finished=false;

	// Initialize node
	bglobal.thingnode[bglobal.thingnodenum]=bglobal.nodenum;
	newnode = (botnode_t *)Z_Malloc (sizeof(*newnode), PU_LEVEL, NULL);
	newnode->x = players[consoleplayer].mo->x;
	newnode->y = players[consoleplayer].mo->y;
	newnode->z = players[consoleplayer].mo->z+MAXSTEPMOVE;
	newnode->thingnumber = bglobal.thingnodenum;
	newnode->type = n_item;
	newnode->number = bglobal.nodenum;
	newnode->next = firstnode; //Make list grow
	firstnode = newnode;
	bglobal.nodenum++;
	bglobal.thingnodenum++;
	bglobal.no_thingnodes = false;
	if (bot_shownodes.value)
		new AActor (newnode->x, newnode->y, newnode->z, MT_SHOWNODE);
}

//------------------------------------------
//Returns a pointer to a node
//by knowing it's number.
//If the function should return try to
//return a bad value (happens when the bots
//enemy respawns) the function will return
//the firstnode instead to prevent returning
//a pointer which points to Elvis or something.
botnode_t *Numnode_to_pointer (int num)
{
	botnode_t *node1;

	node1 = firstnode;
	while (node1)
	{
		if (node1->number == num)
			return node1;
		node1 = node1->next;
	}
	//Printf (PRINT_HIGH, "Warning! 'Numnode_to_pointer()' returned a bad value\n");
	return firstnode;
}



/*  Notes...............
	MT_MISC0  //Armour 1
	MT_MISC1  //Armour 2
	MT_MISC2  //Bon1 ?
	MT_MISC3  //Bon2 ?
	MT_MISC10 //Stimpack
	MT_MISC11 //medipack
	MT_MISC12 //Soulsphere
	MT_MISC17 //Ammo?
	MT_MISC18 //Rocket
	MT_MISC19 //Brok?
	MT_MISC20 //Cell
	MT_MISC21 //Cellpack
	MT_MISC22 //Shell
	MT_MISC23 //Box of shells
	MT_MISC24 //Backpack
	MT_MISC25 //Big fucking ganas
	MT_MISC26 //Chainsaw
	MT_MISC27 //Rocket launcher
	MT_MISC28 //Plasma rifle
    MT_CLIP
    MT_CHAINGUN
    MT_SHOTGUN
    MT_SUPERSHOTGUN
	//    MT_INV
	//    MT_INS
	//    MT_MEGA
*/
