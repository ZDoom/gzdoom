/*
**  Level mesh collision detection
**  Copyright (c) 2018 Magnus Norddahl
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

#include "common/utility/vectors.h"
#include <vector>
#include <cmath>

class SphereShape
{
public:
	SphereShape() { }
	SphereShape(const FVector3 &center, float radius) : center(center), radius(radius) { }

	FVector3 center;
	float radius = 0.0f;
};

struct TraceHit
{
	float fraction = 1.0f;
	int triangle = -1;
	float b = 0.0f;
	float c = 0.0f;
};

class CollisionBBox
{
public:
	CollisionBBox() = default;

	CollisionBBox(const FVector3 &aabb_min, const FVector3 &aabb_max)
	{
		min = aabb_min;
		max = aabb_max;
		auto halfmin = aabb_min * 0.5f;
		auto halfmax = aabb_max * 0.5f;
		Center = halfmax + halfmin;
		Extents = halfmax - halfmin;
	}

	FVector3 min;
	FVector3 max;
	FVector3 Center;
	FVector3 Extents;
	float ssePadding = 0.0f; // Needed to safely load Extents directly into a sse register
};

class RayBBox
{
public:
	RayBBox(const FVector3 &ray_start, const FVector3 &ray_end) : start(ray_start), end(ray_end)
	{
		c = (ray_start + ray_end) * 0.5f;
		w = ray_end - c;
		v.X = std::abs(w.X);
		v.Y = std::abs(w.Y);
		v.Z = std::abs(w.Z);
	}

	FVector3 start, end;
	FVector3 c, w, v;
	float ssePadding = 0.0f; // Needed to safely load v directly into a sse register
};

class TriangleMeshShape
{
public:
	TriangleMeshShape(const FVector3 *vertices, int num_vertices, const unsigned int *elements, int num_elements);

	int get_min_depth() const;
	int get_max_depth() const;
	float get_average_depth() const;
	float get_balanced_depth() const;

	const CollisionBBox &get_bbox() const { return nodes[root].aabb; }

	static float sweep(TriangleMeshShape *shape1, SphereShape *shape2, const FVector3 &target);

	static bool find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2);
	static bool find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2);
	static bool find_any_hit(TriangleMeshShape *shape, const FVector3 &ray_start, const FVector3 &ray_end);

	static std::vector<int> find_all_hits(TriangleMeshShape* shape1, SphereShape* shape2);

	static TraceHit find_first_hit(TriangleMeshShape *shape, const FVector3 &ray_start, const FVector3 &ray_end);

	struct Node
	{
		Node() = default;
		Node(const FVector3 &aabb_min, const FVector3 &aabb_max, int element_index) : aabb(aabb_min, aabb_max), element_index(element_index) { }
		Node(const FVector3 &aabb_min, const FVector3 &aabb_max, int left, int right) : aabb(aabb_min, aabb_max), left(left), right(right) { }

		CollisionBBox aabb;
		int left = -1;
		int right = -1;
		int element_index = -1;
	};

	const std::vector<Node>& get_nodes() const { return nodes; }
	int get_root() const { return root; }

private:
	const FVector3 *vertices = nullptr;
	const int num_vertices = 0;
	const unsigned int *elements = nullptr;
	int num_elements = 0;

	std::vector<Node> nodes;
	int root = -1;

	static float sweep(TriangleMeshShape *shape1, SphereShape *shape2, int a, const FVector3 &target);

	static bool find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	static bool find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2, int a);
	static bool find_any_hit(TriangleMeshShape *shape1, const RayBBox &ray, int a);

	static void find_all_hits(TriangleMeshShape* shape1, SphereShape* shape2, int a, std::vector<int>& hits);

	static void find_first_hit(TriangleMeshShape *shape1, const RayBBox &ray, int a, TraceHit *hit);

	inline static bool overlap_bv_ray(TriangleMeshShape *shape, const RayBBox &ray, int a);
	inline static float intersect_triangle_ray(TriangleMeshShape *shape, const RayBBox &ray, int a, float &barycentricB, float &barycentricC);

	inline static bool sweep_overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const FVector3 &target);
	inline static float sweep_intersect_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const FVector3 &target);

	inline static bool overlap_bv(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	inline static bool overlap_bv_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	inline static bool overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a);
	inline static bool overlap_triangle_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b);
	inline static bool overlap_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a);

	inline bool is_leaf(int node_index);
	inline float volume(int node_index);

	int subdivide(int *triangles, int num_triangles, const FVector3 *centroids, int *work_buffer);
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

	static OverlapResult sphere_aabb(const FVector3 &center, float radius, const CollisionBBox &aabb);
	static OverlapResult aabb(const CollisionBBox &a, const CollisionBBox &b);
	static OverlapResult ray_aabb(const RayBBox &ray, const CollisionBBox &box);
};
