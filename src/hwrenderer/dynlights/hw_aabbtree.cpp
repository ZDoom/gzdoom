// 
//---------------------------------------------------------------------------
// AABB-tree used for ray testing
// Copyright(C) 2017 Magnus Norddahl
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "r_state.h"
#include "g_levellocals.h"
#include "hw_aabbtree.h"

namespace hwrenderer
{

LevelAABBTree::LevelAABBTree()
{
	// Calculate the center of all lines
	TArray<FVector2> centroids;
	for (unsigned int i = 0; i < level.lines.Size(); i++)
	{
		FVector2 v1 = { (float)level.lines[i].v1->fX(), (float)level.lines[i].v1->fY() };
		FVector2 v2 = { (float)level.lines[i].v2->fX(), (float)level.lines[i].v2->fY() };
		centroids.Push((v1 + v2) * 0.5f);
	}

	// Create a list of level lines we want to add:
	TArray<int> line_elements;
	for (unsigned int i = 0; i < level.lines.Size(); i++)
	{
		if (!level.lines[i].backsector)
		{
			line_elements.Push(i);
		}
	}

	// GenerateTreeNode needs a buffer where it can store line indices temporarily when sorting lines into the left and right child AABB buckets
	TArray<int> work_buffer;
	work_buffer.Resize(line_elements.Size() * 2);

	// Generate the AABB tree
	GenerateTreeNode(&line_elements[0], (int)line_elements.Size(), &centroids[0], &work_buffer[0]);

	// Add the lines referenced by the leaf nodes
	lines.Resize(level.lines.Size());
	for (unsigned int i = 0; i < level.lines.Size(); i++)
	{
		const auto &line = level.lines[i];
		auto &treeline = lines[i];

		treeline.x = (float)line.v1->fX();
		treeline.y = (float)line.v1->fY();
		treeline.dx = (float)line.v2->fX() - treeline.x;
		treeline.dy = (float)line.v2->fY() - treeline.y;
	}
}

double LevelAABBTree::RayTest(const DVector3 &ray_start, const DVector3 &ray_end)
{
	// Precalculate some of the variables used by the ray/line intersection test
	DVector2 raydelta = ray_end - ray_start;
	double raydist2 = raydelta | raydelta;
	DVector2 raynormal = DVector2(raydelta.Y, -raydelta.X);
	double rayd = raynormal | ray_start;
	if (raydist2 < 1.0)
		return 1.0f;

	double hit_fraction = 1.0;

	// Walk the tree nodes
	int stack[16];
	int stack_pos = 1;
	stack[0] = nodes.Size() - 1; // root node is the last node in the list
	while (stack_pos > 0)
	{
		int node_index = stack[stack_pos - 1];

		if (!OverlapRayAABB(ray_start, ray_end, nodes[node_index]))
		{
			// If the ray doesn't overlap this node's AABB we're done for this subtree
			stack_pos--;
		}
		else if (nodes[node_index].line_index != -1) // isLeaf(node_index)
		{
			// We reached a leaf node. Do a ray/line intersection test to see if we hit the line.
			hit_fraction = MIN(IntersectRayLine(ray_start, ray_end, nodes[node_index].line_index, raydelta, rayd, raydist2), hit_fraction);
			stack_pos--;
		}
		else if (stack_pos == 16)
		{
			stack_pos--; // stack overflow - tree is too deep!
		}
		else
		{
			// The ray overlaps the node's AABB. Examine its child nodes.
			stack[stack_pos - 1] = nodes[node_index].left_node;
			stack[stack_pos] = nodes[node_index].right_node;
			stack_pos++;
		}
	}

	return hit_fraction;
}

bool LevelAABBTree::OverlapRayAABB(const DVector2 &ray_start2d, const DVector2 &ray_end2d, const AABBTreeNode &node)
{
	// To do: simplify test to use a 2D test
	DVector3 ray_start = DVector3(ray_start2d, 0.0);
	DVector3 ray_end = DVector3(ray_end2d, 0.0);
	DVector3 aabb_min = DVector3(node.aabb_left, node.aabb_top, -1.0);
	DVector3 aabb_max = DVector3(node.aabb_right, node.aabb_bottom, 1.0);

	// Standard 3D ray/AABB overlapping test.
	// The details for the math here can be found in Real-Time Rendering, 3rd Edition.
	// We could use a 2D test here instead, which would probably simplify the math.

	DVector3 c = (ray_start + ray_end) * 0.5f;
	DVector3 w = ray_end - c;
	DVector3 h = (aabb_max - aabb_min) * 0.5f; // aabb.extents();

	c -= (aabb_max + aabb_min) * 0.5f; // aabb.center();

	DVector3 v = DVector3(fabs(w.X), fabs(w.Y), fabs(w.Z));

	if (fabs(c.X) > v.X + h.X || fabs(c.Y) > v.Y + h.Y || fabs(c.Z) > v.Z + h.Z)
		return false; // disjoint;

	if (fabs(c.Y * w.Z - c.Z * w.Y) > h.Y * v.Z + h.Z * v.Y ||
		fabs(c.X * w.Z - c.Z * w.X) > h.X * v.Z + h.Z * v.X ||
		fabs(c.X * w.Y - c.Y * w.X) > h.X * v.Y + h.Y * v.X)
		return false; // disjoint;

	return true; // overlap;
}

double LevelAABBTree::IntersectRayLine(const DVector2 &ray_start, const DVector2 &ray_end, int line_index, const DVector2 &raydelta, double rayd, double raydist2)
{
	// Check if two line segments intersects (the ray and the line).
	// The math below does this by first finding the fractional hit for an infinitely long ray line.
	// If that hit is within the line segment (0 to 1 range) then it calculates the fractional hit for where the ray would hit.
	//
	// This algorithm is homemade - I would not be surprised if there's a much faster method out there.

	const double epsilon = 0.0000001;
	const AABBTreeLine &line = lines[line_index];

	DVector2 raynormal = DVector2(raydelta.Y, -raydelta.X);

	DVector2 line_pos(line.x, line.y);
	DVector2 line_delta(line.dx, line.dy);

	double den = raynormal | line_delta;
	if (fabs(den) > epsilon)
	{
		double t_line = (rayd - (raynormal | line_pos)) / den;
		if (t_line >= 0.0 && t_line <= 1.0)
		{
			DVector2 linehitdelta = line_pos + line_delta * t_line - ray_start;
			double t = (raydelta | linehitdelta) / raydist2;
			return t > 0.0 ? t : 1.0;
		}
	}

	return 1.0;
}

int LevelAABBTree::GenerateTreeNode(int *lines, int num_lines, const FVector2 *centroids, int *work_buffer)
{
	if (num_lines == 0)
		return -1;

	// Find bounding box and median of the lines
	FVector2 median = FVector2(0.0f, 0.0f);
	FVector2 aabb_min, aabb_max;
	aabb_min.X = (float)level.lines[lines[0]].v1->fX();
	aabb_min.Y = (float)level.lines[lines[0]].v1->fY();
	aabb_max = aabb_min;
	for (int i = 0; i < num_lines; i++)
	{
		float x1 = (float)level.lines[lines[i]].v1->fX();
		float y1 = (float)level.lines[lines[i]].v1->fY();
		float x2 = (float)level.lines[lines[i]].v2->fX();
		float y2 = (float)level.lines[lines[i]].v2->fY();

		aabb_min.X = MIN(aabb_min.X, x1);
		aabb_min.X = MIN(aabb_min.X, x2);
		aabb_min.Y = MIN(aabb_min.Y, y1);
		aabb_min.Y = MIN(aabb_min.Y, y2);
		aabb_max.X = MAX(aabb_max.X, x1);
		aabb_max.X = MAX(aabb_max.X, x2);
		aabb_max.Y = MAX(aabb_max.Y, y1);
		aabb_max.Y = MAX(aabb_max.Y, y2);

		median += centroids[lines[i]];
	}
	median /= (float)num_lines;

	if (num_lines == 1) // Leaf node
	{
		nodes.Push(AABBTreeNode(aabb_min, aabb_max, lines[0]));
		return (int)nodes.Size() - 1;
	}

	// Find the longest axis
	float axis_lengths[2] =
	{
		aabb_max.X - aabb_min.X,
		aabb_max.Y - aabb_min.Y
	};
	int axis_order[2] = { 0, 1 };
	FVector2 axis_plane[2] = { FVector2(1.0f, 0.0f), FVector2(0.0f, 1.0f) };
	std::sort(axis_order, axis_order + 2, [&](int a, int b) { return axis_lengths[a] > axis_lengths[b]; });

	// Try sort at longest axis, then if that fails then the other one.
	// We place the sorted lines into work_buffer and then move the result back to the lines list when done.
	int left_count, right_count;
	for (int attempt = 0; attempt < 2; attempt++)
	{
		// Find the sort plane for axis
		FVector2 axis = axis_plane[axis_order[attempt]];
		FVector3 plane(axis, -(median | axis));

		// Sort lines into two based ib whether the line center is on the front or back side of a plane
		left_count = 0;
		right_count = 0;
		for (int i = 0; i < num_lines; i++)
		{
			int line_index = lines[i];

			float side = FVector3(centroids[lines[i]], 1.0f) | plane;
			if (side >= 0.0f)
			{
				work_buffer[left_count] = line_index;
				left_count++;
			}
			else
			{
				work_buffer[num_lines + right_count] = line_index;
				right_count++;
			}
		}

		if (left_count != 0 && right_count != 0)
			break;
	}

	// Check if something went wrong when sorting and do a random sort instead
	if (left_count == 0 || right_count == 0)
	{
		left_count = num_lines / 2;
		right_count = num_lines - left_count;
	}
	else
	{
		// Move result back into lines list:
		for (int i = 0; i < left_count; i++)
			lines[i] = work_buffer[i];
		for (int i = 0; i < right_count; i++)
			lines[i + left_count] = work_buffer[num_lines + i];
	}

	// Create child nodes:
	int left_index = -1;
	int right_index = -1;
	if (left_count > 0)
		left_index = GenerateTreeNode(lines, left_count, centroids, work_buffer);
	if (right_count > 0)
		right_index = GenerateTreeNode(lines + left_count, right_count, centroids, work_buffer);

	// Store resulting node and return its index
	nodes.Push(AABBTreeNode(aabb_min, aabb_max, left_index, right_index));
	return (int)nodes.Size() - 1;
}


}
