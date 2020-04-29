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



#include "doom_aabbtree.h"
#include "g_levellocals.h"

using namespace hwrenderer;

DoomLevelAABBTree::DoomLevelAABBTree(FLevelLocals *lev)
{
	Level = lev;
	// Calculate the center of all lines
	TArray<FVector2> centroids;
	for (unsigned int i = 0; i < Level->lines.Size(); i++)
	{
		FVector2 v1 = { (float)Level->lines[i].v1->fX(), (float)Level->lines[i].v1->fY() };
		FVector2 v2 = { (float)Level->lines[i].v2->fX(), (float)Level->lines[i].v2->fY() };
		centroids.Push((v1 + v2) * 0.5f);
	}

	// Create the static subtree
	if (!GenerateTree(&centroids[0], false))
		return;

	int staticroot = nodes.Size() - 1;

	dynamicStartNode = nodes.Size();
	dynamicStartLine = treelines.Size();

	// Create the dynamic subtree
	if (GenerateTree(&centroids[0], true))
	{
		int dynamicroot = nodes.Size() - 1;

		// Create a shared root node
		FVector2 aabb_min, aabb_max;
		const auto &left = nodes[staticroot];
		const auto &right = nodes[dynamicroot];
		aabb_min.X = MIN(left.aabb_left, right.aabb_left);
		aabb_min.Y = MIN(left.aabb_top, right.aabb_top);
		aabb_max.X = MAX(left.aabb_right, right.aabb_right);
		aabb_max.Y = MAX(left.aabb_bottom, right.aabb_bottom);
		nodes.Push({ aabb_min, aabb_max, staticroot, dynamicroot });
	}

	// Add the lines referenced by the leaf nodes
	treelines.Resize(mapLines.Size());
	for (unsigned int i = 0; i < mapLines.Size(); i++)
	{
		const auto &line = Level->lines[mapLines[i]];
		auto &treeline = treelines[i];

		treeline.x = (float)line.v1->fX();
		treeline.y = (float)line.v1->fY();
		treeline.dx = (float)line.v2->fX() - treeline.x;
		treeline.dy = (float)line.v2->fY() - treeline.y;
	}
}

bool DoomLevelAABBTree::GenerateTree(const FVector2 *centroids, bool dynamicsubtree)
{
	// Create a list of level lines we want to add:
	TArray<int> line_elements;
	auto &maplines = Level->lines;
	for (unsigned int i = 0; i < maplines.Size(); i++)
	{
		if (!maplines[i].backsector)
		{
			bool isPolyLine = maplines[i].sidedef[0] && (maplines[i].sidedef[0]->Flags & WALLF_POLYOBJ);
			if (isPolyLine && dynamicsubtree)
			{
				line_elements.Push(mapLines.Size());
				mapLines.Push(i);
			}
			else if (!isPolyLine && !dynamicsubtree)
			{
				line_elements.Push(mapLines.Size());
				mapLines.Push(i);
			}
		}
	}

	if (line_elements.Size() == 0)
		return false;

	// GenerateTreeNode needs a buffer where it can store line indices temporarily when sorting lines into the left and right child AABB buckets
	TArray<int> work_buffer;
	work_buffer.Resize(line_elements.Size() * 2);

	// Generate the AABB tree
	GenerateTreeNode(&line_elements[0], (int)line_elements.Size(), centroids, &work_buffer[0]);
	return true;
}

bool DoomLevelAABBTree::Update()
{
	bool modified = false;
	for (unsigned int i = dynamicStartLine; i < mapLines.Size(); i++)
	{
		const auto &line = Level->lines[mapLines[i]];

		AABBTreeLine treeline;
		treeline.x = (float)line.v1->fX();
		treeline.y = (float)line.v1->fY();
		treeline.dx = (float)line.v2->fX() - treeline.x;
		treeline.dy = (float)line.v2->fY() - treeline.y;

		if (memcmp(&treelines[i], &treeline, sizeof(AABBTreeLine)))
		{
			TArray<int> path = FindNodePath(i, nodes.Size() - 1);
			if (path.Size())
			{
				float x1 = (float)line.v1->fX();
				float y1 = (float)line.v1->fY();
				float x2 = (float)line.v2->fX();
				float y2 = (float)line.v2->fY();

				int nodeIndex = path[0];
				nodes[nodeIndex].aabb_left = MIN(x1, x2);
				nodes[nodeIndex].aabb_right = MAX(x1, x2);
				nodes[nodeIndex].aabb_top = MIN(y1, y2);
				nodes[nodeIndex].aabb_bottom = MAX(y1, y2);

				for (unsigned int j = 1; j < path.Size(); j++)
				{
					auto &cur = nodes[path[j]];
					const auto &left = nodes[cur.left_node];
					const auto &right = nodes[cur.right_node];
					cur.aabb_left = MIN(left.aabb_left, right.aabb_left);
					cur.aabb_top = MIN(left.aabb_top, right.aabb_top);
					cur.aabb_right = MAX(left.aabb_right, right.aabb_right);
					cur.aabb_bottom = MAX(left.aabb_bottom, right.aabb_bottom);
				}

				treelines[i] = treeline;
				modified = true;
			}
		}
	}
	return modified;
}


int DoomLevelAABBTree::GenerateTreeNode(int *lines, int num_lines, const FVector2 *centroids, int *work_buffer)
{
	if (num_lines == 0)
		return -1;

	// Find bounding box and median of the lines
	FVector2 median = FVector2(0.0f, 0.0f);
	FVector2 aabb_min, aabb_max;
	auto &maplines = Level->lines;
	aabb_min.X = (float)maplines[mapLines[lines[0]]].v1->fX();
	aabb_min.Y = (float)maplines[mapLines[lines[0]]].v1->fY();
	aabb_max = aabb_min;
	for (int i = 0; i < num_lines; i++)
	{
		float x1 = (float)maplines[mapLines[lines[i]]].v1->fX();
		float y1 = (float)maplines[mapLines[lines[i]]].v1->fY();
		float x2 = (float)maplines[mapLines[lines[i]]].v2->fX();
		float y2 = (float)maplines[mapLines[lines[i]]].v2->fY();

		aabb_min.X = MIN(aabb_min.X, x1);
		aabb_min.X = MIN(aabb_min.X, x2);
		aabb_min.Y = MIN(aabb_min.Y, y1);
		aabb_min.Y = MIN(aabb_min.Y, y2);
		aabb_max.X = MAX(aabb_max.X, x1);
		aabb_max.X = MAX(aabb_max.X, x2);
		aabb_max.Y = MAX(aabb_max.Y, y1);
		aabb_max.Y = MAX(aabb_max.Y, y2);

		median += centroids[mapLines[lines[i]]];
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

			float side = FVector3(centroids[mapLines[lines[i]]], 1.0f) | plane;
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

