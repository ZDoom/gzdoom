#include "hardware.h"
#include "v_video.h"

class SDLVideo : public IVideo
{
 public:
	SDLVideo (int parm);
	~SDLVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	bool FullscreenChanged (bool fs);
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	int GetModeCount ();
	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height);

private:
	int IteratorMode;
	int IteratorBits;
	bool IteratorFS;
};
