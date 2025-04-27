#pragma once
#include "dobject.h"
#include "tarray.h"
#include "TRS.h"
#include "matrix.h"

#include <variant>

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

FQuaternion InterpolateQuat(const FQuaternion &from, const FQuaternion &to, float t, float invt);

template<typename T>
inline constexpr T DefVal()
{ // [Jay] can't use T DefVal as a template parameter to BoneOverrideComponent without C++20 so we're stuck with this for now, TODO replace with template parameter once gzdoom switches to C++20
	if constexpr(std::is_same_v<T, FQuaternion>)
	{
		return FQuaternion(0,0,0,1);
	}
	return {};
}

template<typename T, T(*Lerp)(const T &from, const T &to, float t, float invt), T(*Add)(const T &from, const T &to)>
struct BoneOverrideComponent
{
	int mode = 0; // 0 = no override, 1 = rotate, 2 = replace
	int prev_mode = 0;
	double switchtic = 0.0;
	double interplen = 0.0;
	T prev = DefVal<T>();
	T cur = DefVal<T>();
	
	void Set(const T &newValue, double tic, double newInterplen, int newMode)
	{
		double prev_interp_amt = interplen > 0.0 ? std::clamp(((tic - switchtic) / interplen), 0.0, 1.0) : 1.0;
		double prev_interp_amt_inv = 1.0 - prev_interp_amt;

		prev = mode > 0 ? Lerp(prev, cur, prev_interp_amt, prev_interp_amt_inv) : DefVal<T>();

		// might break slightly if value is switched from absolute to additive before interpolation finishes,
		// but shouldn't matter too much, since people will probably mostly stick to one single mode per value
		// so not worth the extra complexity (and adding the requirement of needing bone calculation for setters to work) to properly support it
		prev_mode = (mode == 0 && prev_mode != 0 && prev_interp_amt_inv > 0.0) ? 1 : mode;

		cur = (newMode == 0 ? DefVal<T>() : newValue);
		switchtic = tic;
		interplen = newInterplen;
		mode = newMode;
	}
	
	void Modify(T &value, double tic) const
	{
		
		double lerp_amt = interplen > 0.0 ? std::clamp(((tic - switchtic) / interplen), 0.0, 1.0) : 1.0;
		
		if(mode > 0 || (prev_mode > 0 && lerp_amt < 1.0))
		{
			T from = ModifyValue(value, prev, prev_mode);
			T to = ModifyValue(value, cur, mode);
			value = Lerp(from, to, lerp_amt, 1.0 - lerp_amt);
		}
	}
	
	T Get(const T &value, double tic) const
	{
		T newVal = value;
		Modify(newVal, tic);
		return newVal;
	}
	
private:

	inline static T ModifyValue(const T &orig, const T &cur, int mode)
	{
		if(mode == 0) return orig;
		if(mode == 1) return Add(orig, cur);
		return cur;
	}
	
};

struct BoneOverride
{
	static inline FQuaternion AddQuat(const FQuaternion &from, const FQuaternion &to)
	{
		return (from * to).Unit();
	}
	
	static inline FVector3 LerpVec3(const FVector3 &from, const FVector3 &to, float t, float invt)
	{
		return from * invt + to * t;
	}
	
	static inline FVector3 AddVec3(const FVector3 &from, const FVector3 &to)
	{
		return from + to;
	}

	BoneOverrideComponent<FVector3, &LerpVec3, &AddVec3> translation;
	BoneOverrideComponent<FQuaternion, &InterpolateQuat, &AddQuat> rotation;
	BoneOverrideComponent<FVector3, &LerpVec3, &AddVec3> scaling;

	void Modify(TRS &trs, double tic) const
	{
		translation.Modify(trs.translation, tic);
		rotation.Modify(trs.rotation, tic);
		scaling.Modify(trs.scaling, tic);
	}
};

struct BoneInfo
{
	TArray<TRS> bones_anim_only;
	TArray<TRS> bones_with_override;
	TArray<VSMatrix> positions;
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
