/* Special x86 routines for copying 8 -> 8,16,32-bit surfaces and
 * doubling them (1x1 -> 2x2) at the same time.
 */

extern void (*ExpCopyFunc)();
extern void (*ExpPaletteFunc)(unsigned int *);

extern byte ExpDepth;
extern void *ExpSrc;
extern void *ExpDst;
extern int ExpSW;
extern int ExpSH;
extern int ExpSG;
extern int ExpDG;
extern int ExpDP;

extern "C" void ExpandSetMode (int depth, int redmask, int gmask, int bmask);

#define ExpandCopy ExpCopyFunc
#define ExpandSetPalette ExpPaletteFunc

inline void ExpandSetSurfaces (
	byte *source, int swidth, int sheight, int spitch,
	byte *dest, int dpitch)
{
	ExpSrc = source;
	ExpDst = dest;
	ExpSW = swidth;
	ExpSH = sheight;
	ExpSG = spitch - swidth;
	ExpDG = 2 * (dpitch - swidth * ExpDepth);
	ExpDP = dpitch;
}

















