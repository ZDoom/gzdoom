
//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
struct WBPlayerStruct native version("2.4")
{
	// Player stats, kills, collected items etc.
	native int			skills;
	native int			sitems;
	native int			ssecret;
	native int			stime;
	native int			frags[MAXPLAYERS];
	native int			fragcount;	// [RH] Cumulative frags for this player
}

struct WBStartStruct native version("2.4")
{
	native int			finished_ep;
	native int			next_ep;
	native @WBPlayerStruct plyr[MAXPLAYERS];

	native String		current;	// [RH] Name of map just finished
	native String		next;		// next level, [RH] actual map name
	native String		nextname;	// next level, printable name
	native String		thisname;	// this level, printable name
	native String		nextauthor;	// next level, printable name
	native String		thisauthor;	// this level, printable name

	native TextureID	LName0;
	native TextureID	LName1;

	native int			totalkills;
	native int			maxkills;
	native int			maxitems;
	native int			maxsecret;
	native int			maxfrags;

	// the par time and sucktime
	native int			partime;	// in tics
	native int			sucktime;	// in minutes

	// total time for the entire current game
	native int			totaltime;

	// index of this player in game
	native int			pnum;
}

