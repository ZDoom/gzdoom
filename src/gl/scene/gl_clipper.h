#ifndef __GL_CLIPPER
#define __GL_CLIPPER

#include "doomtype.h"
#include "xs_Float.h"
#include "r_utility.h"

class ClipNode
{
	friend class Clipper;
	friend class ClipNodesFreer;
	
	ClipNode *prev, *next;
	angle_t start, end;
	static ClipNode * freelist;

	bool operator== (const ClipNode &other)
	{
		return other.start == start && other.end == end;
	}

	void Free()
	{
		next=freelist;
		freelist=this;
	}

	static ClipNode * GetNew()
	{
		if (freelist)
		{
			ClipNode * p=freelist;
			freelist=p->next;
			return p;
		}
		else return new ClipNode;
	}

	static ClipNode * NewRange(angle_t start, angle_t end)
	{
		ClipNode * c=GetNew();

		c->start=start;
		c->end=end;
		c->next=c->prev=NULL;
		return c;
	}

};


class Clipper
{
	ClipNode * clipnodes;
	ClipNode * cliphead;
	ClipNode * silhouette;	// will be preserved even when RemoveClipRange is called
	bool blocked;

	static angle_t AngleToPseudo(angle_t ang);
	bool IsRangeVisible(angle_t startangle, angle_t endangle);
	void RemoveRange(ClipNode * cn);
	void AddClipRange(angle_t startangle, angle_t endangle);
	void RemoveClipRange(angle_t startangle, angle_t endangle);
	void DoRemoveClipRange(angle_t start, angle_t end);

public:

	static int anglecache;

	Clipper()
	{
		blocked = false;
		clipnodes=cliphead=NULL;
	}

	~Clipper();

	void Clear();


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

	void SafeAddClipRangeRealAngles(angle_t startangle, angle_t endangle)
	{
		SafeAddClipRange(AngleToPseudo(startangle), AngleToPseudo(endangle));
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

	bool CheckBox(const float *bspcoord);
};


extern Clipper clipper;

angle_t R_PointToPseudoAngle(double x, double y);
void R_SetView();

// Used to speed up angle calculations during clipping
inline angle_t vertex_t::GetClipAngle()
{
	return angletime == Clipper::anglecache? viewangle : (angletime = Clipper::anglecache, viewangle = R_PointToPseudoAngle(p.X, p.Y));
}

#endif