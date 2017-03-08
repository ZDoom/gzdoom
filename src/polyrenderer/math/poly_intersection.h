/*
**  Various 3D intersection tests
**  Copyright (c) 1997-2015 The UICore Team
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include "polyrenderer/drawers/poly_triangle.h"
#include <algorithm>
#include <cmath>

class Vec3f;

class Vec4f
{
public:
	Vec4f() = default;
	Vec4f(const Vec4f &) = default;
	Vec4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { }
	Vec4f(float v) : x(v), y(v), z(v), w(v) { }
	Vec4f(const Vec3f &xyz, float w);

	static float dot(const Vec4f &a, const Vec4f &b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
	static float dot3(const Vec4f &a, const Vec4f &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	float length3() const { return std::sqrt(dot3(*this, *this)); }
	float magnitude() const { return std::sqrt(dot(*this, *this)); }

	Vec4f &operator+=(const Vec4f &b) { *this = Vec4f(x + b.x, y + b.y, z + b.z, w + b.w); return *this; }
	Vec4f &operator-=(const Vec4f &b) { *this = Vec4f(x - b.x, y - b.y, z - b.z, w - b.w); return *this; }
	Vec4f &operator*=(const Vec4f &b) { *this = Vec4f(x * b.x, y * b.y, z * b.z, w * b.w); return *this; }
	Vec4f &operator/=(const Vec4f &b) { *this = Vec4f(x / b.x, y / b.y, z / b.z, w / b.w); return *this; }
	Vec4f &operator+=(float b) { *this = Vec4f(x + b, y + b, z + b, w + b); return *this; }
	Vec4f &operator-=(float b) { *this = Vec4f(x - b, y - b, z - b, w - b); return *this; }
	Vec4f &operator*=(float b) { *this = Vec4f(x * b, y * b, z * b, w * b); return *this; }
	Vec4f &operator/=(float b) { *this = Vec4f(x / b, y / b, z / b, w / b); return *this; }

	float x, y, z, w;
};

inline bool operator==(const Vec4f &a, const Vec4f &b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
inline bool operator!=(const Vec4f &a, const Vec4f &b) { return a.x != b.x || a.y != b.y || a.z != b.z || a.w == b.w; }

class Vec3f
{
public:
	Vec3f() = default;
	Vec3f(const Vec3f &) = default;
	Vec3f(const Vec4f &v) : x(v.x), y(v.y), z(v.z) { }
	Vec3f(float x, float y, float z) : x(x), y(y), z(z) { }
	Vec3f(float v) : x(v), y(v), z(v) { }

	static float dot(const Vec3f &a, const Vec3f &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	float length() const { return std::sqrt(dot(*this, *this)); }

	Vec3f &operator+=(const Vec3f &b) { *this = Vec3f(x + b.x, y + b.y, z + b.z); return *this; }
	Vec3f &operator-=(const Vec3f &b) { *this = Vec3f(x - b.x, y - b.y, z - b.z); return *this; }
	Vec3f &operator*=(const Vec3f &b) { *this = Vec3f(x * b.x, y * b.y, z * b.z); return *this; }
	Vec3f &operator/=(const Vec3f &b) { *this = Vec3f(x / b.x, y / b.y, z / b.z); return *this; }
	Vec3f &operator+=(float b) { *this = Vec3f(x + b, y + b, z + b); return *this; }
	Vec3f &operator-=(float b) { *this = Vec3f(x - b, y - b, z - b); return *this; }
	Vec3f &operator*=(float b) { *this = Vec3f(x * b, y * b, z * b); return *this; }
	Vec3f &operator/=(float b) { *this = Vec3f(x / b, y / b, z / b); return *this; }

	float x, y, z;
};

inline bool operator==(const Vec3f &a, const Vec3f &b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(const Vec3f &a, const Vec3f &b) { return a.x != b.x || a.y != b.y || a.z != b.z; }

inline Vec3f operator+(const Vec3f &a, const Vec3f &b) { return Vec3f(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3f operator-(const Vec3f &a, const Vec3f &b) { return Vec3f(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3f operator*(const Vec3f &a, const Vec3f &b) { return Vec3f(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3f operator/(const Vec3f &a, const Vec3f &b) { return Vec3f(a.x / b.x, a.y / b.y, a.z / b.z); }

inline Vec3f operator+(const Vec3f &a, float b) { return Vec3f(a.x + b, a.y + b, a.z + b); }
inline Vec3f operator-(const Vec3f &a, float b) { return Vec3f(a.x - b, a.y - b, a.z - b); }
inline Vec3f operator*(const Vec3f &a, float b) { return Vec3f(a.x * b, a.y * b, a.z * b); }
inline Vec3f operator/(const Vec3f &a, float b) { return Vec3f(a.x / b, a.y / b, a.z / b); }

inline Vec3f operator+(float a, const Vec3f &b) { return Vec3f(a + b.x, a + b.y, a + b.z); }
inline Vec3f operator-(float a, const Vec3f &b) { return Vec3f(a - b.x, a - b.y, a - b.z); }
inline Vec3f operator*(float a, const Vec3f &b) { return Vec3f(a * b.x, a * b.y, a * b.z); }
inline Vec3f operator/(float a, const Vec3f &b) { return Vec3f(a / b.x, a / b.y, a / b.z); }

inline Vec4f::Vec4f(const Vec3f &xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) { }

typedef TriMatrix Mat4f;

class AxisAlignedBoundingBox
{
public:
	AxisAlignedBoundingBox() : aabb_min(), aabb_max() {}
	AxisAlignedBoundingBox(const Vec3f &aabb_min, const Vec3f &aabb_max) : aabb_min(aabb_min), aabb_max(aabb_max) { }
	AxisAlignedBoundingBox(const AxisAlignedBoundingBox &aabb, const Vec3f &barycentric_min, const Vec3f &barycentric_max)
		: aabb_min(mix(aabb.aabb_min, aabb.aabb_max, barycentric_min)), aabb_max(mix(aabb.aabb_min, aabb.aabb_max, barycentric_max)) { }

	Vec3f center() const { return (aabb_max + aabb_min) * 0.5f; }
	Vec3f extents() const { return (aabb_max - aabb_min) * 0.5f; }

	Vec3f aabb_min;
	Vec3f aabb_max;

private:
	template<typename A, typename B, typename C>
	inline A mix(A a, B b, C mix)
	{
		return a * (C(1) - mix) + b * mix;
	}
};

class OrientedBoundingBox
{
public:
	Vec3f center;
	Vec3f extents;
	Vec3f axis_x;
	Vec3f axis_y;
	Vec3f axis_z;
};

class FrustumPlanes
{
public:
	FrustumPlanes();
	explicit FrustumPlanes(const Mat4f &world_to_projection);

	Vec4f planes[6];

private:
	static Vec4f left_frustum_plane(const Mat4f &matrix);
	static Vec4f right_frustum_plane(const Mat4f &matrix);
	static Vec4f top_frustum_plane(const Mat4f &matrix);
	static Vec4f bottom_frustum_plane(const Mat4f &matrix);
	static Vec4f near_frustum_plane(const Mat4f &matrix);
	static Vec4f far_frustum_plane(const Mat4f &matrix);
};

class IntersectionTest
{
public:
	enum Result
	{
		outside,
		inside,
		intersecting,
	};

	enum OverlapResult
	{
		disjoint,
		overlap
	};

	static Result plane_aabb(const Vec4f &plane, const AxisAlignedBoundingBox &aabb);
	static Result plane_obb(const Vec4f &plane, const OrientedBoundingBox &obb);
	static OverlapResult sphere(const Vec3f &center1, float radius1, const Vec3f &center2, float radius2);
	static OverlapResult sphere_aabb(const Vec3f &center, float radius, const AxisAlignedBoundingBox &aabb);
	static OverlapResult aabb(const AxisAlignedBoundingBox &a, const AxisAlignedBoundingBox &b);
	static Result frustum_aabb(const FrustumPlanes &frustum, const AxisAlignedBoundingBox &box);
	static Result frustum_obb(const FrustumPlanes &frustum, const OrientedBoundingBox &box);
	static OverlapResult ray_aabb(const Vec3f &ray_start, const Vec3f &ray_end, const AxisAlignedBoundingBox &box);
};
