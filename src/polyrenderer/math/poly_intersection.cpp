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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "poly_intersection.h"

IntersectionTest::Result IntersectionTest::plane_aabb(const Vec4f &plane, const AxisAlignedBoundingBox &aabb)
{
	Vec3f center = aabb.center();
	Vec3f extents = aabb.extents();
	float e = extents.x * std::abs(plane.x) + extents.y * std::abs(plane.y) + extents.z * std::abs(plane.z);
	float s = center.x * plane.x + center.y * plane.y + center.z * plane.z + plane.w;
	if (s - e > 0)
		return inside;
	else if (s + e < 0)
		return outside;
	else
		return intersecting;
}

IntersectionTest::Result IntersectionTest::plane_obb(const Vec4f &plane, const OrientedBoundingBox &obb)
{
	Vec3f n(plane);
	float d = plane.w;
	float e = obb.extents.x * std::abs(Vec3f::dot(obb.axis_x, n)) + obb.extents.y * std::abs(Vec3f::dot(obb.axis_y, n)) + obb.extents.z * std::abs(Vec3f::dot(obb.axis_z, n));
	float s = Vec3f::dot(obb.center, n) + d;
	if (s - e > 0)
		return inside;
	else if (s + e < 0)
		return outside;
	else
		return intersecting;
}

IntersectionTest::OverlapResult IntersectionTest::sphere(const Vec3f &center1, float radius1, const Vec3f &center2, float radius2)
{
	Vec3f h = center1 - center2;
	float square_distance = Vec3f::dot(h, h);
	float radius_sum = radius1 + radius2;
	if (square_distance > radius_sum * radius_sum)
		return disjoint;
	else
		return overlap;
}

IntersectionTest::OverlapResult IntersectionTest::sphere_aabb(const Vec3f &center, float radius, const AxisAlignedBoundingBox &aabb)
{
	Vec3f a = aabb.aabb_min - center;
	Vec3f b = center - aabb.aabb_max;
	a.x = std::max(a.x, 0.0f);
	a.y = std::max(a.y, 0.0f);
	a.z = std::max(a.z, 0.0f);
	b.x = std::max(b.x, 0.0f);
	b.y = std::max(b.y, 0.0f);
	b.z = std::max(b.z, 0.0f);
	Vec3f e = a + b;
	float d = Vec3f::dot(e, e);
	if (d > radius * radius)
		return disjoint;
	else
		return overlap;
}

IntersectionTest::OverlapResult IntersectionTest::aabb(const AxisAlignedBoundingBox &a, const AxisAlignedBoundingBox &b)
{
	if (a.aabb_min.x > b.aabb_max.x || b.aabb_min.x > a.aabb_max.x ||
		a.aabb_min.y > b.aabb_max.y || b.aabb_min.y > a.aabb_max.y ||
		a.aabb_min.z > b.aabb_max.z || b.aabb_min.z > a.aabb_max.z)
	{
		return disjoint;
	}
	else
	{
		return overlap;
	}
}

IntersectionTest::Result IntersectionTest::frustum_aabb(const FrustumPlanes &frustum, const AxisAlignedBoundingBox &box)
{
	bool is_intersecting = false;
	for (int i = 0; i < 6; i++)
	{
		Result result = plane_aabb(frustum.planes[i], box);
		if (result == outside)
			return outside;
		else if (result == intersecting)
			is_intersecting = true;
		break;
	}
	if (is_intersecting)
		return intersecting;
	else
		return inside;
}

IntersectionTest::Result IntersectionTest::frustum_obb(const FrustumPlanes &frustum, const OrientedBoundingBox &box)
{
	bool is_intersecting = false;
	for (int i = 0; i < 6; i++)
	{
		Result result = plane_obb(frustum.planes[i], box);
		if (result == outside)
			return outside;
		else if (result == intersecting)
			is_intersecting = true;
	}
	if (is_intersecting)
		return intersecting;
	else
		return inside;
}

IntersectionTest::OverlapResult IntersectionTest::ray_aabb(const Vec3f &ray_start, const Vec3f &ray_end, const AxisAlignedBoundingBox &aabb)
{
	Vec3f c = (ray_start + ray_end) * 0.5f;
	Vec3f w = ray_end - c;
	Vec3f h = aabb.extents();

	c -= aabb.center();

	Vec3f v(std::abs(w.x), std::abs(w.y), std::abs(w.z));

	if (std::abs(c.x) > v.x + h.x || std::abs(c.y) > v.y + h.y || std::abs(c.z) > v.z + h.z)
		return disjoint;

	if (std::abs(c.y * w.z - c.z * w.y) > h.y * v.z + h.z * v.y ||
		std::abs(c.x * w.z - c.z * w.x) > h.x * v.z + h.z * v.x ||
		std::abs(c.x * w.y - c.y * w.x) > h.x * v.y + h.y * v.x)
		return disjoint;

	return overlap;
}

/////////////////////////////////////////////////////////////////////////////

FrustumPlanes::FrustumPlanes()
{
}

FrustumPlanes::FrustumPlanes(const Mat4f &world_to_projection)
{
	planes[0] = near_frustum_plane(world_to_projection);
	planes[1] = far_frustum_plane(world_to_projection);
	planes[2] = left_frustum_plane(world_to_projection);
	planes[3] = right_frustum_plane(world_to_projection);
	planes[4] = top_frustum_plane(world_to_projection);
	planes[5] = bottom_frustum_plane(world_to_projection);
}

Vec4f FrustumPlanes::left_frustum_plane(const Mat4f &m)
{
	Vec4f plane(
		m.matrix[3 + 0 * 4] + m.matrix[0 + 0 * 4],
		m.matrix[3 + 1 * 4] + m.matrix[0 + 1 * 4],
		m.matrix[3 + 2 * 4] + m.matrix[0 + 2 * 4],
		m.matrix[3 + 3 * 4] + m.matrix[0 + 3 * 4]);
	plane /= plane.length3();
	return plane;
}

Vec4f FrustumPlanes::right_frustum_plane(const Mat4f &m)
{
	Vec4f plane(
		m.matrix[3 + 0 * 4] - m.matrix[0 + 0 * 4],
		m.matrix[3 + 1 * 4] - m.matrix[0 + 1 * 4],
		m.matrix[3 + 2 * 4] - m.matrix[0 + 2 * 4],
		m.matrix[3 + 3 * 4] - m.matrix[0 + 3 * 4]);
	plane /= plane.length3();
	return plane;
}

Vec4f FrustumPlanes::top_frustum_plane(const Mat4f &m)
{
	Vec4f plane(
		m.matrix[3 + 0 * 4] - m.matrix[1 + 0 * 4],
		m.matrix[3 + 1 * 4] - m.matrix[1 + 1 * 4],
		m.matrix[3 + 2 * 4] - m.matrix[1 + 2 * 4],
		m.matrix[3 + 3 * 4] - m.matrix[1 + 3 * 4]);
	plane /= plane.length3();
	return plane;
}

Vec4f FrustumPlanes::bottom_frustum_plane(const Mat4f &m)
{
	Vec4f plane(
		m.matrix[3 + 0 * 4] + m.matrix[1 + 0 * 4],
		m.matrix[3 + 1 * 4] + m.matrix[1 + 1 * 4],
		m.matrix[3 + 2 * 4] + m.matrix[1 + 2 * 4],
		m.matrix[3 + 3 * 4] + m.matrix[1 + 3 * 4]);
	plane /= plane.length3();
	return plane;
}

Vec4f FrustumPlanes::near_frustum_plane(const Mat4f &m)
{
	Vec4f plane(
		m.matrix[3 + 0 * 4] + m.matrix[2 + 0 * 4],
		m.matrix[3 + 1 * 4] + m.matrix[2 + 1 * 4],
		m.matrix[3 + 2 * 4] + m.matrix[2 + 2 * 4],
		m.matrix[3 + 3 * 4] + m.matrix[2 + 3 * 4]);
	plane /= plane.length3();
	return plane;
}

Vec4f FrustumPlanes::far_frustum_plane(const Mat4f &m)
{
	Vec4f plane(
		m.matrix[3 + 0 * 4] - m.matrix[2 + 0 * 4],
		m.matrix[3 + 1 * 4] - m.matrix[2 + 1 * 4],
		m.matrix[3 + 2 * 4] - m.matrix[2 + 2 * 4],
		m.matrix[3 + 3 * 4] - m.matrix[2 + 3 * 4]);
	plane /= plane.length3();
	return plane;
}
