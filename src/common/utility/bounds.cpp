//-----------------------------------------------------------------------------
// Note: this is a modified version of dlight. It is not the original software.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2013-2014 Samuel Villarreal
// svkaiser@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//   2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//

#include <math.h>
#include <assert.h>

#include "bounds.h"

#define M_INFINITY 1e30f

BBox::BBox()
{
	Clear();
}

BBox::BBox(const FVector3 &vMin, const FVector3 &vMax)
{
	this->min = vMin;
	this->max = vMax;
}

void BBox::Clear()
{
	min = FVector3(M_INFINITY, M_INFINITY, M_INFINITY);
	max = FVector3(-M_INFINITY, -M_INFINITY, -M_INFINITY);
}

void BBox::AddPoint(const FVector3 &vec)
{
	float lowx = min.X;
	float lowy = min.Y;
	float lowz = min.Z;
	float hix = max.X;
	float hiy = max.Y;
	float hiz = max.Z;

	if (vec.X < lowx) { lowx = vec.X; }
	if (vec.Y < lowy) { lowy = vec.Y; }
	if (vec.Z < lowz) { lowz = vec.Z; }
	if (vec.X > hix) { hix = vec.X; }
	if (vec.Y > hiy) { hiy = vec.Y; }
	if (vec.Z > hiz) { hiz = vec.Z; }

	min = FVector3(lowx, lowy, lowz);
	max = FVector3(hix, hiy, hiz);
}

float BBox::Radius() const
{
	int i;
	float r = 0;
	float r1;
	float r2;

	for (i = 0; i < 3; i++)
	{
		r1 = fabsf(min[i]);
		r2 = fabsf(max[i]);

		if (r1 > r2)
		{
			r += r1 * r1;
		}
		else
		{
			r += r2 * r2;
		}
	}

	return sqrtf(r);
}

bool BBox::PointInside(const FVector3 &vec) const
{
	return !(vec[0] < min[0] || vec[1] < min[1] || vec[2] < min[2] ||
		vec[0] > max[0] || vec[1] > max[1] || vec[2] > max[2]);
}

bool BBox::IntersectingBox(const BBox &box) const
{
	return !(box.max[0] < min[0] || box.max[1] < min[1] || box.max[2] < min[2] ||
		box.min[0] > max[0] || box.min[1] > max[1] || box.min[2] > max[2]);
}

bool BBox::IntersectingBox2D(const BBox &box) const
{
	return !(box.max[0] < min[0] || box.max[2] < min[2] ||
		box.min[0] > max[0] || box.min[2] > max[2]);
}

float BBox::DistanceToPlane(Plane &plane)
{
	throw "unimplemented";
	return 0;
	/*FVector3 c;
	float distStart;
	float distEnd;
	float dist = 0;

	c = Center();

	distStart = plane.Distance(c);
	distEnd = fabs((max.X - c.X) * plane.a) +
		fabs((max.Y - c.Y) * plane.b) +
		fabs((max.Z - c.Z) * plane.c);

	dist = distStart - distEnd;

	if (dist > 0)
	{
		// in front
		return dist;
	}

	dist = distStart + distEnd;

	if (dist < 0)
	{
		// behind
		return dist;
	}

	return 0;*/
}

BBox BBox::operator+(const float radius) const
{
	FVector3 vmin = min;
	FVector3 vmax = max;

	vmin.X -= radius;
	vmin.Y -= radius;
	vmin.Z -= radius;

	vmax.X += radius;
	vmax.Y += radius;
	vmax.Z += radius;

	return BBox(vmin, vmax);
}

BBox &BBox::operator+=(const float radius)
{
	min.X -= radius;
	min.Y -= radius;
	min.Z -= radius;
	max.X += radius;
	max.Y += radius;
	max.Z += radius;
	return *this;
}

BBox BBox::operator+(const FVector3 &vec) const
{
	FVector3 vmin = min;
	FVector3 vmax = max;

	vmin.X += vec.X;
	vmin.Y += vec.Y;
	vmin.Z += vec.Z;

	vmax.X += vec.X;
	vmax.Y += vec.Y;
	vmax.Z += vec.Z;

	return BBox(vmin, vmax);
}

BBox BBox::operator-(const float radius) const
{
	FVector3 vmin = min;
	FVector3 vmax = max;

	vmin.X += radius;
	vmin.Y += radius;
	vmin.Z += radius;

	vmax.X -= radius;
	vmax.Y -= radius;
	vmax.Z -= radius;

	return BBox(vmin, vmax);
}

BBox BBox::operator-(const FVector3 &vec) const
{
	FVector3 vmin = min;
	FVector3 vmax = max;

	vmin.X -= vec.X;
	vmin.Y -= vec.Y;
	vmin.Z -= vec.Z;

	vmax.X -= vec.X;
	vmax.Y -= vec.Y;
	vmax.Z -= vec.Z;

	return BBox(vmin, vmax);
}

BBox &BBox::operator-=(const float radius)
{
	min.X += radius;
	min.Y += radius;
	min.Z += radius;
	max.X -= radius;
	max.Y -= radius;
	max.Z -= radius;
	return *this;
}

BBox BBox::operator*(const FVector3 &vec) const
{
	BBox box = *this;

	if (vec.X < 0) { box.min.X += (vec.X - 1); }
	else { box.max.X += (vec.X + 1); }
	if (vec.Y < 0) { box.min.Y += (vec.Y - 1); }
	else { box.max.Y += (vec.Y + 1); }
	if (vec.Z < 0) { box.min.Z += (vec.Z - 1); }
	else { box.max.Z += (vec.Z + 1); }

	return box;
}

BBox &BBox::operator*=(const FVector3 &vec)
{
	if (vec.X < 0) { min.X += (vec.X - 1); }
	else { max.X += (vec.X + 1); }
	if (vec.Y < 0) { min.Y += (vec.Y - 1); }
	else { max.Y += (vec.Y + 1); }
	if (vec.Z < 0) { min.Z += (vec.Z - 1); }
	else { max.Z += (vec.Z + 1); }

	return *this;
}

BBox &BBox::operator=(const BBox &bbox)
{
	min = bbox.min;
	max = bbox.max;

	return *this;
}

FVector3 BBox::operator[](int index) const
{
	assert(index >= 0 && index < 2);
	return index == 0 ? min : max;
}

FVector3 &BBox::operator[](int index)
{
	assert(index >= 0 && index < 2);
	return index == 0 ? min : max;
}

bool BBox::LineIntersect(const FVector3 &start, const FVector3 &end)
{
	float ld[3];
	FVector3 center = Center();
	FVector3 extents = max - center;
	FVector3 lineDir = (end - start) * 0.5f;
	FVector3 lineCenter = lineDir + start;
	FVector3 dir = lineCenter - center;

	ld[0] = fabs(lineDir.X);
	if (fabs(dir.X) > extents.X + ld[0]) { return false; }
	ld[1] = fabs(lineDir.Y);
	if (fabs(dir.Y) > extents.Y + ld[1]) { return false; }
	ld[2] = fabs(lineDir.Z);
	if (fabs(dir.Z) > extents.Z + ld[2]) { return false; }

	FVector3 crossprod = lineDir ^ dir;

	if (fabs(crossprod.X) > extents.Y * ld[2] + extents.Z * ld[1]) { return false; }
	if (fabs(crossprod.Y) > extents.X * ld[2] + extents.Z * ld[0]) { return false; }
	if (fabs(crossprod.Z) > extents.X * ld[1] + extents.Y * ld[0]) { return false; }

	return true;
}

// Assumes points is an array of 24
void BBox::ToPoints(float *points) const
{
	points[0 * 3 + 0] = max[0];
	points[0 * 3 + 1] = min[1];
	points[0 * 3 + 2] = min[2];
	points[1 * 3 + 0] = max[0];
	points[1 * 3 + 1] = min[1];
	points[1 * 3 + 2] = max[2];
	points[2 * 3 + 0] = min[0];
	points[2 * 3 + 1] = min[1];
	points[2 * 3 + 2] = max[2];
	points[3 * 3 + 0] = min[0];
	points[3 * 3 + 1] = min[1];
	points[3 * 3 + 2] = min[2];
	points[4 * 3 + 0] = max[0];
	points[4 * 3 + 1] = max[1];
	points[4 * 3 + 2] = min[2];
	points[5 * 3 + 0] = max[0];
	points[5 * 3 + 1] = max[1];
	points[5 * 3 + 2] = max[2];
	points[6 * 3 + 0] = min[0];
	points[6 * 3 + 1] = max[1];
	points[6 * 3 + 2] = max[2];
	points[7 * 3 + 0] = min[0];
	points[7 * 3 + 1] = max[1];
	points[7 * 3 + 2] = min[2];
}

// Assumes vectors is an array of 8
void BBox::ToVectors(FVector3 *vectors) const
{
	vectors[0][0] = max[0];
	vectors[0][1] = min[1];
	vectors[0][2] = min[2];
	vectors[1][0] = max[0];
	vectors[1][1] = min[1];
	vectors[1][2] = max[2];
	vectors[2][0] = min[0];
	vectors[2][1] = min[1];
	vectors[2][2] = max[2];
	vectors[3][0] = min[0];
	vectors[3][1] = min[1];
	vectors[3][2] = min[2];
	vectors[4][0] = max[0];
	vectors[4][1] = max[1];
	vectors[4][2] = min[2];
	vectors[5][0] = max[0];
	vectors[5][1] = max[1];
	vectors[5][2] = max[2];
	vectors[6][0] = min[0];
	vectors[6][1] = max[1];
	vectors[6][2] = max[2];
	vectors[7][0] = min[0];
	vectors[7][1] = max[1];
	vectors[7][2] = min[2];
}

inline FVector3 BBox::Center() const
{
	return FVector3(
		(max.X + min.X) * 0.5f,
		(max.Y + min.Y) * 0.5f,
		(max.Z + min.Z) * 0.5f);
}

inline FVector3 BBox::Extents() const
{
	return FVector3(
		(max.X - min.X) * 0.5f,
		(max.Y - min.Y) * 0.5f,
		(max.Z - min.Z) * 0.5f);
}
