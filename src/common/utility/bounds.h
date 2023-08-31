//-----------------------------------------------------------------------------
// Note: this is from ZDRay
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

#pragma once

#include "vectors.h"

class BBox
{
public:
	BBox();
	BBox(const FVector3& vMin, const FVector3& vMax);

	void Clear();
	FVector3 Center() const;
	FVector3 Extents() const;
	float Radius() const;
	void AddPoint(const FVector3& vec);
	bool PointInside(const FVector3& vec) const;
	bool IntersectingBox(const BBox& box) const;
	bool IntersectingBox2D(const BBox& box) const;
	float DistanceToPlane(Plane& plane);
	bool LineIntersect(const FVector3& start, const FVector3& end);
	void ToPoints(float* points) const;
	void ToVectors(FVector3* vectors) const;

	BBox operator+(const float radius) const;
	BBox& operator+=(const float radius);
	BBox operator+(const FVector3& vec) const;
	BBox operator-(const float radius) const;
	BBox operator-(const FVector3& vec) const;
	BBox& operator-=(const float radius);
	BBox operator*(const FVector3& vec) const;
	BBox& operator*=(const FVector3& vec);
	BBox& operator=(const BBox& bbox);
	FVector3 operator[](int index) const;
	FVector3& operator[](int index);

	FVector3 min;
	FVector3 max;
};