#ifndef __FRAMESTATE_H
#define __FRAMESTATE_H


struct FrameStateData
{
	float mViewMatrix[16];
	float mProjectionMatrix[16];
	float mCameraPos[4];
	float mClipHeight;
	int mLightMode;
	int mFogMode;
	int mFixedColormap;				// 0, when no fixed colormap, 1 for a light value, 2 for a color blend
	float mFixedColormapStart[4];
	float mFixedColormapEnd[4];
};


class FFrameState
{
	bool bDSA;
	unsigned int mBufferId;

public:
	FrameStateData mData;
	
	FFrameState();
	~FFrameState();
	void UpdateFor3D();
	void UpdateFor2D(bool weapon);
	void UpdateViewMatrix();	// there are a few cases where this needs to be changed independently from the rest of the state
};

#endif
