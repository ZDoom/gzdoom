#ifndef DISABLE_SSE

#include "doomtype.h"
#include "nodebuild.h"

#define FAR_ENOUGH 17179869184.f		// 4<<32

// You may notice that this function is identical to ClassifyLine2.
// The reason it is SSE2 is because this file is explicitly compiled
// with SSE2 math enabled, but the other files are not.

extern "C" int ClassifyLineSSE2 (node_t &node, const FSimpleVert *v1, const FSimpleVert *v2, int sidev[2])
{
	double d_x1 = double(node.x);
	double d_y1 = double(node.y);
	double d_dx = double(node.dx);
	double d_dy = double(node.dy);
	double d_xv1 = double(v1->x);
	double d_xv2 = double(v2->x);
	double d_yv1 = double(v1->y);
	double d_yv2 = double(v2->y);

	double s_num1 = (d_y1 - d_yv1) * d_dx - (d_x1 - d_xv1) * d_dy;
	double s_num2 = (d_y1 - d_yv2) * d_dx - (d_x1 - d_xv2) * d_dy;

	int nears = 0;

	if (s_num1 <= -FAR_ENOUGH)
	{
		if (s_num2 <= -FAR_ENOUGH)
		{
			sidev[0] = sidev[1] = 1;
			return 1;
		}
		if (s_num2 >= FAR_ENOUGH)
		{
			sidev[0] = 1;
			sidev[1] = -1;
			return -1;
		}
		nears = 1;
	}
	else if (s_num1 >= FAR_ENOUGH)
	{
		if (s_num2 >= FAR_ENOUGH)
		{
			sidev[0] = sidev[1] = -1;
			return 0;
		}
		if (s_num2 <= -FAR_ENOUGH)
		{
			sidev[0] = -1;
			sidev[1] = 1;
			return -1;
		}
		nears = 1;
	}
	else
	{
		nears = 2 | int(fabs(s_num2) < FAR_ENOUGH);
	}

	if (nears)
	{
		double l = 1.f / (d_dx*d_dx + d_dy*d_dy);
		if (nears & 2)
		{
			double dist = s_num1 * s_num1 * l;
			if (dist < SIDE_EPSILON*SIDE_EPSILON)
			{
				sidev[0] = 0;
			}
			else
			{
				sidev[0] = s_num1 > 0.0 ? -1 : 1;
			}
		}
		else
		{
			sidev[0] = s_num1 > 0.0 ? -1 : 1;
		}
		if (nears & 1)
		{
			double dist = s_num2 * s_num2 * l;
			if (dist < SIDE_EPSILON*SIDE_EPSILON)
			{
				sidev[1] = 0;
			}
			else
			{
				sidev[1] = s_num2 > 0.0 ? -1 : 1;
			}
		}
		else
		{
			sidev[1] = s_num2 > 0.0 ? -1 : 1;
		}
	}
	else
	{
		sidev[0] = s_num1 > 0.0 ? -1 : 1;
		sidev[1] = s_num2 > 0.0 ? -1 : 1;
	}

	if ((sidev[0] | sidev[1]) == 0)
	{ // seg is coplanar with the splitter, so use its orientation to determine
	  // which child it ends up in. If it faces the same direction as the splitter,
	  // it goes in front. Otherwise, it goes in back.

		if (node.dx != 0)
		{
			if ((node.dx > 0 && v2->x > v1->x) || (node.dx < 0 && v2->x < v1->x))
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
		else
		{
			if ((node.dy > 0 && v2->y > v1->y) || (node.dy < 0 && v2->y < v1->y))
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}
	else if (sidev[0] <= 0 && sidev[1] <= 0)
	{
		return 0;
	}
	else if (sidev[0] >= 0 && sidev[1] >= 0)
	{
		return 1;
	}
	return -1;
}

#endif
