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

#include <algorithm>
#include "hw_aabbtree.h"

namespace hwrenderer
{

TArray<int> LevelAABBTree::FindNodePath(unsigned int line, unsigned int node)
{
	const AABBTreeNode &n = nodes[node];

	if (n.aabb_left > treelines[line].x || n.aabb_right < treelines[line].x ||
		n.aabb_top > treelines[line].y || n.aabb_bottom < treelines[line].y)
	{
		return {};
	}

	TArray<int> path;
	if (n.line_index == -1)
	{
		path = FindNodePath(line, n.left_node);
		if (path.Size() == 0)
			path = FindNodePath(line, n.right_node);

		if (path.Size())
			path.Push(node);
	}
	else if (n.line_index == (int)line)
	{
		path.Push(node);
	}
	return path;
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
	int stack[32];
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
			hit_fraction = std::min(IntersectRayLine(ray_start, ray_end, nodes[node_index].line_index, raydelta, rayd, raydist2), hit_fraction);
			stack_pos--;
		}
		else if (stack_pos == 32)
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
	const AABBTreeLine &line = treelines[line_index];

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


}
