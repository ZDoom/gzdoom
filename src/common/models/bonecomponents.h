#pragma once
#include "dobject.h"
#include "tarray.h"
#include "TRS.h"
#include "matrix.h"

#include <variant>


class DBoneComponents : public DObject
{
	DECLARE_CLASS(DBoneComponents, DObject);
public:
	TArray<TArray<TRS>>			trscomponents;
	TArray<TArray<VSMatrix>>	trsmatrix;

	DBoneComponents() = default;
};

struct ModelAnimFrameInterp
{
	float inter = -1.0f;
	int frame1 = -1;
	int frame2 = -1;
};

struct ModelAnimFramePrecalculatedIQM
{
	TArray<TRS> precalcBones;
};

enum EModelAnimFlags
{
	MODELANIM_NONE			= 1 << 0, // no animation
	MODELANIM_LOOP			= 1 << 1, // animation loops, otherwise it stays on the last frame once it ends
};

struct ModelAnim
{
	int firstFrame = 0;
	int lastFrame = 0;
	int loopFrame = 0;
	float framerate = 0;
	double startFrame = 0;
	int flags = MODELANIM_NONE;
	double startTic = 0; // when the current animation started (changing framerates counts as restarting) (or when animation starts if interpolating from previous animation)
	double switchOffset = 0; // when the animation was changed -- where to interpolate the switch from
};

static_assert(sizeof(ModelAnim) == sizeof(double) * 6);

using ModelAnimFrame = std::variant<std::nullptr_t, ModelAnimFrameInterp, ModelAnimFramePrecalculatedIQM>;

double getCurrentFrame(const ModelAnim &anim, double tic, bool *looped);
void calcFrame(const ModelAnim &anim, double tic, ModelAnimFrameInterp &inter);
void calcFrames(const ModelAnim &curAnim, double tic, ModelAnimFrameInterp &to, float &inter);
