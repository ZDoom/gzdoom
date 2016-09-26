
#pragma once

#include "ssa_vec4f.h"
#include "ssa_float.h"
#include "ssa_int.h"

class SSAViewport
{
public:
	SSAViewport(SSAInt x, SSAInt y, SSAInt width, SSAInt height)
	: x(x), y(y), width(width), height(height), right(x + width), bottom(y + height),
	  half_width(SSAFloat(width) * 0.5f), half_height(SSAFloat(height) * 0.5f),
	  rcp_half_width(1.0f / (SSAFloat(width) * 0.5f)),
	  rcp_half_height(1.0f / (SSAFloat(height) * 0.5f))
	{
	}

	SSAInt x, y;
	SSAInt width, height;
	SSAInt right, bottom;
	SSAFloat half_width;
	SSAFloat half_height;
	SSAFloat rcp_half_width;
	SSAFloat rcp_half_height;

	SSAVec4f clip_to_window(SSAVec4f clip) const
	{
		SSAFloat w = clip[3];
		SSAVec4f normalized = SSAVec4f::insert_element(clip / SSAVec4f::shuffle(clip, 3, 3, 3, 3), w, 3);
		return normalized_to_window(normalized);
	}

	SSAVec4f normalized_to_window(SSAVec4f normalized) const
	{
		return SSAVec4f(
			SSAFloat(x) + (normalized[0] + 1.0f) * half_width,
			SSAFloat(y) + (normalized[1] + 1.0f) * half_height,
			0.0f - normalized[2],
			normalized[3]);
	}
};

class SSABarycentricWeight
{
public:
	SSABarycentricWeight(SSAViewport vp, SSAVec4f v1, SSAVec4f v2);
	SSAFloat from_window_x(SSAInt x) const;
	SSAFloat from_window_y(SSAInt y) const;

	SSAViewport viewport;
	SSAVec4f v1;
	SSAVec4f v2;
};

inline SSABarycentricWeight::SSABarycentricWeight(SSAViewport viewport, SSAVec4f v1, SSAVec4f v2)
: viewport(viewport), v1(v1), v2(v2)
{
}

inline SSAFloat SSABarycentricWeight::from_window_x(SSAInt x) const
{
/*	SSAFloat xnormalized = (x + 0.5f - viewport.x) * viewport.rcp_half_width - 1.0f;
	SSAFloat dx = v2.x-v1.x;
	SSAFloat dw = v2.w-v1.w;
	SSAFloat a = (v2.x - xnormalized * v2.w) / (dx - xnormalized * dw);
	return a;*/

	SSAFloat xnormalized = (SSAFloat(x) + 0.5f - SSAFloat(viewport.x)) * viewport.rcp_half_width - 1.0f;
	SSAFloat dx = v2[0]-v1[0];
	SSAFloat dw = v2[3]-v1[3];
	SSAFloat t = (xnormalized * v1[3] - v1[0]) / (dx - xnormalized * dw);
	return 1.0f - t;
}

inline SSAFloat SSABarycentricWeight::from_window_y(SSAInt y) const
{
/*	SSAFloat ynormalized = (y + 0.5f - viewport.y) * viewport.rcp_half_height - 1.0f;
	SSAFloat dy = v2.y-v1.y;
	SSAFloat dw = v2.w-v1.w;
	SSAFloat a = (v2.y - ynormalized * v2.w) / (dy - ynormalized * dw);
	return a;*/

	SSAFloat ynormalized = (SSAFloat(y) + 0.5f - SSAFloat(viewport.y)) * viewport.rcp_half_height - 1.0f;
	SSAFloat dy = v2[1]-v1[1];
	SSAFloat dw = v2[3]-v1[3];
	SSAFloat t = (ynormalized * v1[3] - v1[1]) / (dy - ynormalized * dw);
	return 1.0f - t;
}

/*
	y = (v1.y + t * dy) / (v1.w + t * dw)

	y * v1.w + y * t * dw = v1.y + t * dy
	y * v1.w - v1.y = t * (dy - y * dw)
	t = (y * v1.w - v1.y) / (dy - y * dw)
*/
