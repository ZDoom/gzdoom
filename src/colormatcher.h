#ifndef __COLORMATCHER_H__
#define __COLORMATCHER_H__

// This class implements a "pretty good" color matcher for palettized images.
// In many cases, it is able to choose the "best" color with only a few
// comparisons. In all other cases, it chooses an "almost best" color in only
// a few comparisons. This is much faster than scanning the entire palette
// when you want to pick a color, so the somewhat lesser quality should be
// acceptable unless quality is the highest priority.
//
// The reason for the lesser quality is because this algorithm stores groups
// of colors into cells in a color cube and then spreads those color groups
// around into neighboring cells. When asked to pick a color, it looks in a
// single cell and sees which of the colors stored in that cell is closest
// to the requested color. For borderline cases, the chosen color may not
// actually be the best color in the entire palette, but it is still close.
//
// The accuracy of this class depends on the input palette and on HIBITS.
// HIBITS 4 seems to be a reasonable compromise between preprocessing time,
// space, and accuracy.

class FColorMatcher
{
public:
	FColorMatcher ();
	FColorMatcher (const DWORD *palette);
	FColorMatcher (const FColorMatcher &other);

	void SetPalette (const DWORD *palette);
	byte Pick (int r, int g, int b);
	FColorMatcher &operator= (const FColorMatcher &other);

private:
	enum { CHIBITS=4, CLOBITS=8-CHIBITS, CHISIZE=1<<CHIBITS, CLOSIZE=1<<CLOBITS};
	struct Seed;
	struct PalEntry;

	const PalEntry *Pal;
	byte FirstColor[CHISIZE+1][CHISIZE+1][CHISIZE+1];
	byte NextColor[256];

	int FillPlane (int r1, int r2, int g1, int g2, int b1, int b2,
		byte seedspread[CHISIZE+1][CHISIZE+1][CHISIZE+1],
		Seed *seeds, int thisseed);
};

#endif //__COLORMATCHER_H__
