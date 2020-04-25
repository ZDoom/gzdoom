#ifndef __I_VIDEO_H__
#define __I_VIDEO_H__

#include <cstdint>

class DFrameBuffer;


class IVideo
{
public:
	virtual ~IVideo() {}

	virtual DFrameBuffer *CreateFrameBuffer() = 0;

	bool SetResolution();

	virtual void DumpAdapters();
};

void I_InitGraphics();
void I_ShutdownGraphics();

extern IVideo *Video;

void I_PolyPresentInit();
uint8_t *I_PolyPresentLock(int w, int h, bool vsync, int &pitch);
void I_PolyPresentUnlock(int x, int y, int w, int h);
void I_PolyPresentDeinit();


// Pause a bit.
// [RH] Despite the name, it apparently never waited for the VBL, even in
// the original DOS version (if the Heretic/Hexen source is any indicator).
void I_WaitVBL(int count);


#endif // __I_VIDEO_H__
