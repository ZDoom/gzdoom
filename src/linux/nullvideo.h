#include "hardware.h"
#include "v_video.h"

class NullVideo : public IVideo
{
 public:
	NullVideo (int parm);
	~NullVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	bool FullscreenChanged (bool fs);
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	int GetModeCount ();
	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height);

private:
	bool lastIt;
};
