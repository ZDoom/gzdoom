#ifndef __I_VIDEO_H__
#define __I_VIDEO_H__


class DFrameBuffer;


// [RH] Set the display mode
DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old);

// Pause a bit.
// [RH] Despite the name, it apparently never waited for the VBL, even in
// the original DOS version (if the Heretic/Hexen source is any indicator).
void I_WaitVBL(int count);

bool I_CheckResolution (int width, int height, int bpp);
void I_ClosestResolution (int *width, int *height, int bits);

enum EDisplayType
{
	DISPLAY_WindowOnly,
	DISPLAY_FullscreenOnly,
	DISPLAY_Both
};

#endif // __I_VIDEO_H__
