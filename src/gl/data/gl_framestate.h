#ifndef __FRAMESTATE_H
#define __FRAMESTATE_H

struct FSpecialColormap;

enum
{
	FXM_DEFAULT,
	FXM_COLORRANGE,
	FXM_COLOR,

	FXM_IGNORE		// only to be used as parameter to ChangeFixedColormap
};

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
	float mFixedColormapRange[4];
};

struct FrameStateIndices
{
	int mLastSettingCounter;
	int iViewMatrix;
	int iProjectionMatrix;
	int iCameraPos;
	int iClipHeight;
	int iLightMode;
	int iFogMode;
	int iFixedColormap;
	int iFixedColormapStart;
	int iFixedColormapRange;

	void set(unsigned int prog);
};

class FFrameState
{
	int mUpdateCounter;

	void DoApplyToShader(FrameStateIndices *in);

public:
	FrameStateData mData;
	
	FFrameState();
	void UpdateFor3D();
	void UpdateFor2D(bool weapon);
	void ApplyToShader(FrameStateIndices *in)
	{
		if (in->mLastSettingCounter != mUpdateCounter)
		{
			DoApplyToShader(in);
		}
	}

	void SetFixedColormap(FSpecialColormap *map);
};

extern FFrameState gl_FrameState;

#endif
