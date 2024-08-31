#ifndef __GL_CLIPPER
#define __GL_CLIPPER

#include "doomtype.h"
#include "xs_Float.h"
#include "r_utility.h"
#include "memarena.h"

class ClipNode
{
	friend class Clipper;
	
	ClipNode *prev, *next;
	angle_t start, end;

	bool operator== (const ClipNode &other)
	{
		return other.start == start && other.end == end;
	}
};


class Clipper
{
	static unsigned starttime;
	FMemArena nodearena;
	ClipNode * freelist = nullptr;

	ClipNode * clipnodes = nullptr;
	ClipNode * cliphead = nullptr;
	ClipNode * silhouette = nullptr;	// will be preserved even when RemoveClipRange is called
	FRenderViewpoint *viewpoint = nullptr;
	bool blocked = false;

	static angle_t AngleToPseudo(angle_t ang);
	static angle_t PitchToPseudo(double ang);
	bool IsRangeVisible(angle_t startangle, angle_t endangle);
	void RemoveRange(ClipNode * cn);
	void AddClipRange(angle_t startangle, angle_t endangle);
	void RemoveClipRange(angle_t startangle, angle_t endangle);
	void DoRemoveClipRange(angle_t start, angle_t end);

public:

	bool amRadar = false;
	Clipper();

	void Clear();

	void Free(ClipNode *node)
	{
		node->next = freelist;
		freelist = node;
	}

	ClipNode * GetNew()
	{
		if (freelist)
		{
			ClipNode * p = freelist;
			freelist = p->next;
			return p;
		}
		else return (ClipNode*)nodearena.Alloc(sizeof(ClipNode));
	}

	ClipNode * NewRange(angle_t start, angle_t end)
	{
		ClipNode * c = GetNew();

		c->start = start;
		c->end = end;
		c->next = c->prev = NULL;
		return c;
	}
    
    void SetViewpoint(FRenderViewpoint &vp)
    {
        viewpoint = &vp;
    }

	void SetSilhouette();

	bool SafeCheckRange(angle_t startAngle, angle_t endAngle)
	{
		if(startAngle > endAngle)
		{
			return (IsRangeVisible(startAngle, ANGLE_MAX) || IsRangeVisible(0, endAngle));
		}
		
		return IsRangeVisible(startAngle, endAngle);
	}

	void SafeAddClipRange(angle_t startangle, angle_t endangle)
	{
		if(startangle > endangle)
		{
			// The range has to added in two parts.
			AddClipRange(startangle, ANGLE_MAX);
			AddClipRange(0, endangle);
		}
		else
		{
			// Add the range as usual.
			AddClipRange(startangle, endangle);
		}
	}
    
    void SafeAddClipRange(const vertex_t *v1, const vertex_t *v2)
    {
        angle_t a2 = PointToPseudoAngle(v1->p.X, v1->p.Y);
        angle_t a1 = PointToPseudoAngle(v2->p.X, v2->p.Y);
        SafeAddClipRange(a1,a2);
    }

	void SafeAddClipRangeRealAngles(angle_t startangle, angle_t endangle)
	{
		SafeAddClipRange(AngleToPseudo(startangle), AngleToPseudo(endangle));
	}

	void SafeAddClipRangeDegPitches(double startpitch, double endpitch)
	{
		SafeAddClipRange(PitchToPseudo(startpitch), PitchToPseudo(endpitch));
	}

	void SafeRemoveClipRange(angle_t startangle, angle_t endangle)
	{
		if(startangle > endangle)
		{
			// The range has to added in two parts.
			RemoveClipRange(startangle, ANGLE_MAX);
			RemoveClipRange(0, endangle);
		}
		else
		{
			// Add the range as usual.
			RemoveClipRange(startangle, endangle);
		}
	}

	void SafeRemoveClipRangeRealAngles(angle_t startangle, angle_t endangle)
	{
		SafeRemoveClipRange(AngleToPseudo(startangle), AngleToPseudo(endangle));
	}

	void SetBlocked(bool on)
	{
		blocked = on;
	}

	bool IsBlocked() const
	{
		return blocked;
	}

	angle_t PointToPseudoAngle(double x, double y);
	angle_t PointToPseudoPitch(double x, double y, double z);
	angle_t PointToPseudoOrthoAngle(double x, double y);
	angle_t PointToPseudoOrthoPitch(double x, double y, double z);

	bool CheckBox(const float *bspcoord);
	bool CheckBoxOrthoPitch(const float *bspcoord);

	// Used to speed up angle calculations during clipping
	inline angle_t GetClipAngle(vertex_t *v)
	{
		return unsigned(v->angletime) == starttime ? v->viewangle : (v->angletime = starttime, v->viewangle = PointToPseudoAngle(v->p.X, v->p.Y));
	}

};

#endif
