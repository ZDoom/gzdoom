// These are different statnums for thinkers. The idea to maintain multiple
// lists for different types of thinkers is taken from Build. Every thinker
// is ticked by statnum, so a thinker with a low statnum will always tick
// before a thinker with a high statnum. If a thinker is not explicitly
// created with a statnum, it will be given MAX_STATNUM.

enum
{ // Thinkers that don't actually think
	STAT_INFO,								// An info queue
	STAT_DECAL,								// A decal

  // Thinkers that do think
	STAT_FIRST_THINKING=32,
	STAT_SCROLLER=STAT_FIRST_THINKING,		// A DScroller thinker
	STAT_PLAYER,							// A player actor
	STAT_BOSSTARGET,						// A boss brain target
	STAT_LIGHTNING,							// The lightning thinker
};
