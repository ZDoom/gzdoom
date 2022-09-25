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

#include "hw_collision.h"
#include <algorithm>
#include <functional>
#include <cfloat>
#ifndef NO_SSE
#include <immintrin.h>
#endif

TriangleMeshShape::TriangleMeshShape(const FVector3 *vertices, int num_vertices, const unsigned int *elements, int num_elements)
	: vertices(vertices), num_vertices(num_vertices), elements(elements), num_elements(num_elements)
{
	int num_triangles = num_elements / 3;
	if (num_triangles <= 0)
		return;

	std::vector<int> triangles;
	std::vector<FVector3> centroids;
	triangles.reserve(num_triangles);
	centroids.reserve(num_triangles);
	for (int i = 0; i < num_triangles; i++)
	{
		triangles.push_back(i);

		int element_index = i * 3;
		FVector3 centroid = (vertices[elements[element_index + 0]] + vertices[elements[element_index + 1]] + vertices[elements[element_index + 2]]) * (1.0f / 3.0f);
		centroids.push_back(centroid);
	}

	std::vector<int> work_buffer(num_triangles * 2);

	root = subdivide(&triangles[0], (int)triangles.size(), &centroids[0], &work_buffer[0]);
}

float TriangleMeshShape::sweep(TriangleMeshShape *shape1, SphereShape *shape2, const FVector3 &target)
{
	return sweep(shape1, shape2, shape1->root, target);
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2)
{
	return find_any_hit(shape1, shape2, shape1->root, shape2->root);
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2)
{
	return find_any_hit(shape1, shape2, shape1->root);
}

std::vector<int> TriangleMeshShape::find_all_hits(TriangleMeshShape* shape1, SphereShape* shape2)
{
	std::vector<int> hits;
	find_all_hits(shape1, shape2, shape1->root, hits);
	return hits;
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape, const FVector3 &ray_start, const FVector3 &ray_end)
{
	return find_any_hit(shape, RayBBox(ray_start, ray_end), shape->root);
}

TraceHit TriangleMeshShape::find_first_hit(TriangleMeshShape *shape, const FVector3 &ray_start, const FVector3 &ray_end)
{
	TraceHit hit;

	// Perform segmented tracing to keep the ray AABB box smaller

	FVector3 ray_dir = ray_end - ray_start;
	float tracedist = (float)ray_dir.Length();
	float segmentlen = std::max(100.0f, tracedist / 20.0f);
	for (float t = 0.0f; t < tracedist; t += segmentlen)
	{
		float segstart = t / tracedist;
		float segend = std::min(t + segmentlen, tracedist) / tracedist;

		find_first_hit(shape, RayBBox(ray_start + ray_dir * segstart, ray_start + ray_dir * segend), shape->root, &hit);
		if (hit.fraction < 1.0f)
		{
			hit.fraction = segstart * (1.0f - hit.fraction) + segend * hit.fraction;
			break;
		}
	}

	return hit;
}

float TriangleMeshShape::sweep(TriangleMeshShape *shape1, SphereShape *shape2, int a, const FVector3 &target)
{
	if (sweep_overlap_bv_sphere(shape1, shape2, a, target))
	{
		if (shape1->is_leaf(a))
		{
			return sweep_intersect_triangle_sphere(shape1, shape2, a, target);
		}
		else
		{
			return std::min(sweep(shape1, shape2, shape1->nodes[a].left, target), sweep(shape1, shape2, shape1->nodes[a].right, target));
		}
	}
	return 1.0f;
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, SphereShape *shape2, int a)
{
	if (overlap_bv_sphere(shape1, shape2, a))
	{
		if (shape1->is_leaf(a))
		{
			return overlap_triangle_sphere(shape1, shape2, a);
		}
		else
		{
			if (find_any_hit(shape1, shape2, shape1->nodes[a].left))
				return true;
			else
				return find_any_hit(shape1, shape2, shape1->nodes[a].right);
		}
	}
	return false;
}

void TriangleMeshShape::find_all_hits(TriangleMeshShape* shape1, SphereShape* shape2, int a, std::vector<int>& hits)
{
	if (overlap_bv_sphere(shape1, shape2, a))
	{
		if (shape1->is_leaf(a))
		{
			if (overlap_triangle_sphere(shape1, shape2, a))
			{
				hits.push_back(shape1->nodes[a].element_index / 3);
			}
		}
		else
		{
			find_all_hits(shape1, shape2, shape1->nodes[a].left, hits);
			find_all_hits(shape1, shape2, shape1->nodes[a].right, hits);
		}
	}
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	bool leaf_a = shape1->is_leaf(a);
	bool leaf_b = shape2->is_leaf(b);
	if (leaf_a && leaf_b)
	{
		return overlap_triangle_triangle(shape1, shape2, a, b);
	}
	else if (!leaf_a && !leaf_b)
	{
		if (overlap_bv(shape1, shape2, a, b))
		{
			if (shape1->volume(a) > shape2->volume(b))
			{
				if (find_any_hit(shape1, shape2, shape1->nodes[a].left, b))
					return true;
				else
					return find_any_hit(shape1, shape2, shape1->nodes[a].right, b);
			}
			else
			{
				if (find_any_hit(shape1, shape2, a, shape2->nodes[b].left))
					return true;
				else
					return find_any_hit(shape1, shape2, a, shape2->nodes[b].right);
			}
		}
		return false;
	}
	else if (leaf_a)
	{
		if (overlap_bv_triangle(shape2, shape1, b, a))
		{
			if (find_any_hit(shape1, shape2, a, shape2->nodes[b].left))
				return true;
			else
				return find_any_hit(shape1, shape2, a, shape2->nodes[b].right);
		}
		return false;
	}
	else
	{
		if (overlap_bv_triangle(shape1, shape2, a, b))
		{
			if (find_any_hit(shape1, shape2, shape1->nodes[a].left, b))
				return true;
			else
				return find_any_hit(shape1, shape2, shape1->nodes[a].right, b);
		}
		return false;
	}
}

bool TriangleMeshShape::find_any_hit(TriangleMeshShape *shape, const RayBBox &ray, int a)
{
	if (overlap_bv_ray(shape, ray, a))
	{
		if (shape->is_leaf(a))
		{
			float baryB, baryC;
			return intersect_triangle_ray(shape, ray, a, baryB, baryC) < 1.0f;
		}
		else
		{
			if (find_any_hit(shape, ray, shape->nodes[a].left))
				return true;
			else
				return find_any_hit(shape, ray, shape->nodes[a].right);
		}
	}
	return false;
}

void TriangleMeshShape::find_first_hit(TriangleMeshShape *shape, const RayBBox &ray, int a, TraceHit *hit)
{
	if (overlap_bv_ray(shape, ray, a))
	{
		if (shape->is_leaf(a))
		{
			float baryB, baryC;
			float t = intersect_triangle_ray(shape, ray, a, baryB, baryC);
			if (t < hit->fraction)
			{
				hit->fraction = t;
				hit->triangle = shape->nodes[a].element_index / 3;
				hit->b = baryB;
				hit->c = baryC;
			}
		}
		else
		{
			find_first_hit(shape, ray, shape->nodes[a].left, hit);
			find_first_hit(shape, ray, shape->nodes[a].right, hit);
		}
	}
}

bool TriangleMeshShape::overlap_bv_ray(TriangleMeshShape *shape, const RayBBox &ray, int a)
{
	return IntersectionTest::ray_aabb(ray, shape->nodes[a].aabb) == IntersectionTest::overlap;
}

float TriangleMeshShape::intersect_triangle_ray(TriangleMeshShape *shape, const RayBBox &ray, int a, float &barycentricB, float &barycentricC)
{
	const int start_element = shape->nodes[a].element_index;

	FVector3 p[3] =
	{
		shape->vertices[shape->elements[start_element]],
		shape->vertices[shape->elements[start_element + 1]],
		shape->vertices[shape->elements[start_element + 2]]
	};

	// Moeller–Trumbore ray-triangle intersection algorithm:

	FVector3 D = ray.end - ray.start;

	// Find vectors for two edges sharing p[0]
	FVector3 e1 = p[1] - p[0];
	FVector3 e2 = p[2] - p[0];

	// Begin calculating determinant - also used to calculate u parameter
	FVector3 P = D ^ e2; // cross(D, e2);
	float det = e1 | P; // dot(e1, P);

	// Backface check
	//if (det < 0.0f)
	//	return 1.0f;

	// If determinant is near zero, ray lies in plane of triangle
	if (det > -FLT_EPSILON && det < FLT_EPSILON)
		return 1.0f;

	float inv_det = 1.0f / det;

	// Calculate distance from p[0] to ray origin
	FVector3 T = ray.start - p[0];

	// Calculate u parameter and test bound
	float u = (T | P) * inv_det; // dot(T, P) * inv_det;

	// Check if the intersection lies outside of the triangle
	if (u < 0.f || u > 1.f)
		return 1.0f;

	// Prepare to test v parameter
	FVector3 Q = T ^ e1; // cross(T, e1);

	// Calculate V parameter and test bound
	float v = (D | Q) * inv_det; // dot(D, Q) * inv_det;

	// The intersection lies outside of the triangle
	if (v < 0.f || u + v  > 1.f)
		return 1.0f;

	float t = (e2 | Q) * inv_det; //dot(e2, Q) * inv_det;
	if (t <= FLT_EPSILON)
		return 1.0f;

	// Return hit location on triangle in barycentric coordinates
	barycentricB = u;
	barycentricC = v;
	
	return t;
}

bool TriangleMeshShape::sweep_overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const FVector3 &target)
{
	// Convert to ray test by expanding the AABB:

	CollisionBBox aabb = shape1->nodes[a].aabb;
	aabb.Extents.X += shape2->radius;
	aabb.Extents.Y += shape2->radius;
	aabb.Extents.Z += shape2->radius;

	return IntersectionTest::ray_aabb(RayBBox(shape2->center, target), aabb) == IntersectionTest::overlap;
}

float TriangleMeshShape::sweep_intersect_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a, const FVector3 &target)
{
	const int start_element = shape1->nodes[a].element_index;

	FVector3 p[3] =
	{
		shape1->vertices[shape1->elements[start_element]],
		shape1->vertices[shape1->elements[start_element + 1]],
		shape1->vertices[shape1->elements[start_element + 2]]
	};

	FVector3 c = shape2->center;
	FVector3 e = target;
	float r = shape2->radius;

	// Dynamic intersection test between a ray and the minkowski sum of the sphere and polygon:

	FVector3 n = ((p[1] - p[0]) ^ (p[2] - p[0])).Unit(); // normalize(cross(p[1] - p[0], p[2] - p[0]));
	FVector4 plane(n, -(n | p[0])); // plane(n, -dot(n, p[0]));

	// Step 1: Plane intersect test

	float sc = (plane | FVector4(c, 1.0f)); // dot(plane, FVector4(c, 1.0f));
	float se = (plane | FVector4(e, 1.0f)); // dot(plane, FVector4(e, 1.0f));
	bool same_side = sc * se > 0.0f;

	if (same_side && std::abs(sc) > r && std::abs(se) > r)
		return 1.0f;

	// Step 1a: Check if point is in polygon (using crossing ray test in 2d)
	{
		float t = (sc - r) / (sc - se);

		FVector3 vt = c + (e - c) * t;

		FVector3 u0 = p[1] - p[0];
		FVector3 u1 = p[2] - p[0];

		FVector2 v_2d[3] =
		{
			FVector2(0.0f, 0.0f),
			FVector2((u0 | u0), 0.0f), // FVector2(dot(u0, u0), 0.0f),
			FVector2(0.0f, (u1 | u1)) // FVector2(0.0f, dot(u1, u1))
		};

		FVector2 point((u0 | vt), (u1 | vt)); // point(dot(u0, vt), dot(u1, vt));

		bool inside = false;
		FVector2 e0 = v_2d[2];
		bool y0 = e0.Y >= point.Y;
		for (int i = 0; i < 3; i++)
		{
			FVector2 e1 = v_2d[i];
			bool y1 = e1.Y >= point.Y;

			if (y0 != y1 && ((e1.Y - point.Y) * (e0.X - e1.X) >= (e1.X - point.X) * (e0.Y - e1.Y)) == y1)
				inside = !inside;

			y0 = y1;
			e0 = e1;
		}

		if (inside)
			return t;
	}

	// Step 2: Edge intersect test

	FVector3 ke[3] =
	{
		p[1] - p[0],
		p[2] - p[1],
		p[0] - p[2],
	};

	FVector3 kg[3] =
	{
		p[0] - c,
		p[1] - c,
		p[2] - c,
	};

	FVector3 ks = e - c;

	float kgg[3];
	float kgs[3];
	float kss[3];

	for (int i = 0; i < 3; i++)
	{
		float kee = (ke[i] | ke[i]); // dot(ke[i], ke[i]);
		float keg = (ke[i] | kg[i]); // dot(ke[i], kg[i]);
		float kes = (ke[i] | ks); // dot(ke[i], ks);
		kgg[i] = (kg[i] | kg[i]); // dot(kg[i], kg[i]);
		kgs[i] = (kg[i] | ks); // dot(kg[i], ks);
		kss[i] = (ks | ks); // dot(ks, ks);

		float aa = kee * kss[i] - kes * kes;
		float bb = 2 * (keg * kes - kee * kgs[i]);
		float cc = kee * (kgg[i] - r * r) - keg * keg;

		float sign = (bb >= 0.0f) ? 1.0f : -1.0f;
		float q = -0.5f * (bb + sign * std::sqrt(bb * bb - 4 * aa * cc));
		float t0 = q / aa;
		float t1 = cc / q;

		float t;
		if (t0 < 0.0f || t0 > 1.0f)
			t = t1;
		else if (t1 < 0.0f || t1 > 1.0f)
			t = t0;
		else
			t = std::min(t0, t1);

		if (t >= 0.0f && t <= 1.0f)
		{
			FVector3 ct = c + ks * t;
			float d = ((ct - p[i]) | ke[i]); // dot(ct - p[i], ke[i]);
			if (d >= 0.0f && d <= kee)
				return t;
		}
	}

	// Step 3: Point intersect test

	for (int i = 0; i < 3; i++)
	{
		float aa = kss[i];
		float bb = -2.0f * kgs[i];
		float cc = kgg[i] - r * r;

		float sign = (bb >= 0.0f) ? 1.0f : -1.0f;
		float q = -0.5f * (bb + sign * std::sqrt(bb * bb - 4 * aa * cc));
		float t0 = q / aa;
		float t1 = cc / q;

		float t;
		if (t0 < 0.0f || t0 > 1.0f)
			t = t1;
		else if (t1 < 0.0f || t1 > 1.0f)
			t = t0;
		else
			t = std::min(t0, t1);

		if (t >= 0.0f && t <= 1.0f)
			return t;
	}

	return 1.0f;
}

bool TriangleMeshShape::overlap_bv(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	return IntersectionTest::aabb(shape1->nodes[a].aabb, shape2->nodes[b].aabb) == IntersectionTest::overlap;
}

bool TriangleMeshShape::overlap_bv_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	return false;
}

bool TriangleMeshShape::overlap_bv_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int a)
{
	return IntersectionTest::sphere_aabb(shape2->center, shape2->radius, shape1->nodes[a].aabb) == IntersectionTest::overlap;
}

bool TriangleMeshShape::overlap_triangle_triangle(TriangleMeshShape *shape1, TriangleMeshShape *shape2, int a, int b)
{
	return false;
}

bool TriangleMeshShape::overlap_triangle_sphere(TriangleMeshShape *shape1, SphereShape *shape2, int shape1_node_index)
{
	// http://realtimecollisiondetection.net/blog/?p=103

	int element_index = shape1->nodes[shape1_node_index].element_index;

	FVector3 P = shape2->center;
	FVector3 A = shape1->vertices[shape1->elements[element_index]] - P;
	FVector3 B = shape1->vertices[shape1->elements[element_index + 1]] - P;
	FVector3 C = shape1->vertices[shape1->elements[element_index + 2]] - P;
	float r = shape2->radius;
	float rr = r * r;

	// Testing if sphere lies outside the triangle plane
	FVector3 V = ((B - A) ^ (C - A)); // cross(B - A, C - A);
	float d = A | V; // dot(A, V);
	float e = V | V; // dot(V, V);
	bool sep1 = d * d > rr * e;

	// Testing if sphere lies outside a triangle vertex
	float aa = A | A; // dot(A, A);
	float ab = A | B; // dot(A, B);
	float ac = A | C; // dot(A, C);
	float bb = B | B; // dot(B, B);
	float bc = B | C; // dot(B, C);
	float cc = C | C; // dot(C, C);
	bool sep2 = (aa > rr) && (ab > aa) && (ac > aa);
	bool sep3 = (bb > rr) && (ab > bb) && (bc > bb);
	bool sep4 = (cc > rr) && (ac > cc) && (bc > cc);

	// Testing if sphere lies outside a triangle edge
	FVector3 AB = B - A;
	FVector3 BC = C - B;
	FVector3 CA = A - C;
	float d1 = ab - aa;
	float d2 = bc - bb;
	float d3 = ac - cc;
	float e1 = (AB | AB); // dot(AB, AB)
	float e2 = (BC | BC); // dot(BC, BC)
	float e3 = (CA | CA); // dot(CA, CA)
	FVector3 Q1 = A * e1 - AB * d1;
	FVector3 Q2 = B * e2 - BC * d2;
	FVector3 Q3 = C * e3 - CA * d3;
	FVector3 QC = C * e1 - Q1;
	FVector3 QA = A * e2 - Q2;
	FVector3 QB = B * e3 - Q3;
	bool sep5 = ((Q1 | Q1) > rr * e1 * e1) && ((Q1 | QC) > 0.0f); // (dot(Q1, Q1) > rr * e1 * e1) && (dot(Q1, QC) > 0.0f);
	bool sep6 = ((Q2 | Q2) > rr * e2 * e2) && ((Q2 | QA) > 0.0f); // (dot(Q2, Q2) > rr * e2 * e2) && (dot(Q2, QA) > 0.0f);
	bool sep7 = ((Q3 | Q3) > rr * e3 * e3) && ((Q3 | QB) > 0.0f); // (dot(Q3, Q3) > rr * e3 * e3) && (dot(Q3, QB) > 0.0f);

	bool separated = sep1 || sep2 || sep3 || sep4 || sep5 || sep6 || sep7;
	return (!separated);
}

bool TriangleMeshShape::is_leaf(int node_index)
{
	return nodes[node_index].element_index != -1;
}

float TriangleMeshShape::volume(int node_index)
{
	const FVector3 &extents = nodes[node_index].aabb.Extents;
	return extents.X * extents.Y * extents.Z;
}

int TriangleMeshShape::get_min_depth() const
{
	std::function<int(int, int)> visit;
	visit = [&](int level, int node_index) -> int {
		const Node &node = nodes[node_index];
		if (node.element_index == -1)
			return std::min(visit(level + 1, node.left), visit(level + 1, node.right));
		else
			return level;
	};
	return visit(1, root);
}

int TriangleMeshShape::get_max_depth() const
{
	std::function<int(int, int)> visit;
	visit = [&](int level, int node_index) -> int {
		const Node &node = nodes[node_index];
		if (node.element_index == -1)
			return std::max(visit(level + 1, node.left), visit(level + 1, node.right));
		else
			return level;
	};
	return visit(1, root);
}

float TriangleMeshShape::get_average_depth() const
{
	std::function<float(int, int)> visit;
	visit = [&](int level, int node_index) -> float {
		const Node &node = nodes[node_index];
		if (node.element_index == -1)
			return visit(level + 1, node.left) + visit(level + 1, node.right);
		else
			return (float)level;
	};
	float depth_sum = visit(1, root);
	int leaf_count = (num_elements / 3);
	return depth_sum / leaf_count;
}

float TriangleMeshShape::get_balanced_depth() const
{
	return std::log2((float)(num_elements / 3));
}

int TriangleMeshShape::subdivide(int *triangles, int num_triangles, const FVector3 *centroids, int *work_buffer)
{
	if (num_triangles == 0)
		return -1;

	// Find bounding box and median of the triangle centroids
	FVector3 median;
	FVector3 min, max;
	min = vertices[elements[triangles[0] * 3]];
	max = min;
	for (int i = 0; i < num_triangles; i++)
	{
		int element_index = triangles[i] * 3;
		for (int j = 0; j < 3; j++)
		{
			const FVector3 &vertex = vertices[elements[element_index + j]];

			min.X = std::min(min.X, vertex.X);
			min.Y = std::min(min.Y, vertex.Y);
			min.Z = std::min(min.Z, vertex.Z);

			max.X = std::max(max.X, vertex.X);
			max.Y = std::max(max.Y, vertex.Y);
			max.Z = std::max(max.Z, vertex.Z);
		}

		median += centroids[triangles[i]];
	}
	median /= (float)num_triangles;

	if (num_triangles == 1) // Leaf node
	{
		nodes.push_back(Node(min, max, triangles[0] * 3));
		return (int)nodes.size() - 1;
	}

	// Find the longest axis
	float axis_lengths[3] =
	{
		max.X - min.X,
		max.Y - min.Y,
		max.Z - min.Z
	};

	int axis_order[3] = { 0, 1, 2 };
	std::sort(axis_order, axis_order + 3, [&](int a, int b) { return axis_lengths[a] > axis_lengths[b]; });

	// Try split at longest axis, then if that fails the next longest, and then the remaining one
	int left_count, right_count;
	FVector3 axis;
	for (int attempt = 0; attempt < 3; attempt++)
	{
		// Find the split plane for axis
		switch (axis_order[attempt])
		{
		default:
		case 0: axis = FVector3(1.0f, 0.0f, 0.0f); break;
		case 1: axis = FVector3(0.0f, 1.0f, 0.0f); break;
		case 2: axis = FVector3(0.0f, 0.0f, 1.0f); break;
		}
		FVector4 plane(axis, -(median | axis)); // plane(axis, -dot(median, axis));

		// Split triangles into two
		left_count = 0;
		right_count = 0;
		for (int i = 0; i < num_triangles; i++)
		{
			int triangle = triangles[i];
			int element_index = triangle * 3;

			float side = (FVector4(centroids[triangles[i]], 1.0f) | plane); // dot(FVector4(centroids[triangles[i]], 1.0f), plane);
			if (side >= 0.0f)
			{
				work_buffer[left_count] = triangle;
				left_count++;
			}
			else
			{
				work_buffer[num_triangles + right_count] = triangle;
				right_count++;
			}
		}

		if (left_count != 0 && right_count != 0)
			break;
	}

	// Check if something went wrong when splitting and do a random split instead
	if (left_count == 0 || right_count == 0)
	{
		left_count = num_triangles / 2;
		right_count = num_triangles - left_count;
	}
	else
	{
		// Move result back into triangles list:
		for (int i = 0; i < left_count; i++)
			triangles[i] = work_buffer[i];
		for (int i = 0; i < right_count; i++)
			triangles[i + left_count] = work_buffer[num_triangles + i];
	}

	// Create child nodes:
	int left_index = -1;
	int right_index = -1;
	if (left_count > 0)
		left_index = subdivide(triangles, left_count, centroids, work_buffer);
	if (right_count > 0)
		right_index = subdivide(triangles + left_count, right_count, centroids, work_buffer);

	nodes.push_back(Node(min, max, left_index, right_index));
	return (int)nodes.size() - 1;
}

/////////////////////////////////////////////////////////////////////////////

IntersectionTest::OverlapResult IntersectionTest::sphere_aabb(const FVector3 &center, float radius, const CollisionBBox &aabb)
{
	FVector3 a = aabb.min - center;
	FVector3 b = center - aabb.max;
	a.X = std::max(a.X, 0.0f);
	a.Y = std::max(a.Y, 0.0f);
	a.Z = std::max(a.Z, 0.0f);
	b.X = std::max(b.X, 0.0f);
	b.Y = std::max(b.Y, 0.0f);
	b.Z = std::max(b.Z, 0.0f);
	FVector3 e = a + b;
	float d = (e | e); // dot(e, e);
	if (d > radius * radius)
		return disjoint;
	else
		return overlap;
}

IntersectionTest::OverlapResult IntersectionTest::aabb(const CollisionBBox& a, const CollisionBBox& b)
{
	if (a.min.X > b.max.X || b.min.X > a.max.X ||
		a.min.Y > b.max.Y || b.min.Y > a.max.Y ||
		a.min.Z > b.max.Z || b.min.Z > a.max.Z)
	{
		return disjoint;
	}
	else
	{
		return overlap;
	}
}

static const uint32_t clearsignbitmask[] = { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff };

IntersectionTest::OverlapResult IntersectionTest::ray_aabb(const RayBBox &ray, const CollisionBBox &aabb)
{
#ifndef NO_SSE

	__m128 v = _mm_loadu_ps(&ray.v.X);
	__m128 w = _mm_loadu_ps(&ray.w.X);
	__m128 h = _mm_loadu_ps(&aabb.Extents.X);
	__m128 c = _mm_sub_ps(_mm_loadu_ps(&ray.c.X), _mm_loadu_ps(&aabb.Center.X));

	__m128 clearsignbit = _mm_loadu_ps(reinterpret_cast<const float*>(clearsignbitmask));

	__m128 abs_c = _mm_and_ps(c, clearsignbit);
	int mask = _mm_movemask_ps(_mm_cmpgt_ps(abs_c, _mm_add_ps(v, h)));
	if (mask & 7)
		return disjoint;

	__m128 c1 = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 0, 1)); // c.Y, c.X, c.X
	__m128 c2 = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 1, 2, 2)); // c.Z, c.Z, c.Y
	__m128 w1 = _mm_shuffle_ps(w, w, _MM_SHUFFLE(3, 1, 2, 2)); // w.Z, w.Z, w.Y
	__m128 w2 = _mm_shuffle_ps(w, w, _MM_SHUFFLE(3, 0, 0, 1)); // w.Y, w.X, w.X
	__m128 lhs = _mm_and_ps(_mm_sub_ps(_mm_mul_ps(c1, w1), _mm_mul_ps(c2, w2)), clearsignbit);

	__m128 h1 = _mm_shuffle_ps(h, h, _MM_SHUFFLE(3, 0, 0, 1)); // h.Y, h.X, h.X
	__m128 h2 = _mm_shuffle_ps(h, h, _MM_SHUFFLE(3, 1, 2, 2)); // h.Z, h.Z, h.Y
	__m128 v1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 1, 2, 2)); // v.Z, v.Z, v.Y
	__m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 0, 0, 1)); // v.Y, v.X, v.X
	__m128 rhs = _mm_add_ps(_mm_mul_ps(h1, v1), _mm_mul_ps(h2, v2));

	mask = _mm_movemask_ps(_mm_cmpgt_ps(lhs, rhs));
	return (mask & 7) ? disjoint : overlap;

#else
	const FVector3 &v = ray.v;
	const FVector3 &w = ray.w;
	const FVector3 &h = aabb.Extents;
	auto c = ray.c - aabb.Center;

	if (std::abs(c.X) > v.X + h.X || std::abs(c.Y) > v.Y + h.Y || std::abs(c.Z) > v.Z + h.Z)
		return disjoint;

	if (std::abs(c.Y * w.Z - c.Z * w.Y) > h.Y * v.Z + h.Z * v.Y ||
		std::abs(c.X * w.Z - c.Z * w.X) > h.X * v.Z + h.Z * v.X ||
		std::abs(c.X * w.Y - c.Y * w.X) > h.X * v.Y + h.Y * v.X)
		return disjoint;

	return overlap;
#endif
}
