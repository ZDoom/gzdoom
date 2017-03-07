// 
//---------------------------------------------------------------------------
// 2D collision tree for 1D shadowmap lights
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

#include "gl/system/gl_system.h"
#include "gl/shaders/gl_shader.h"
#include "gl/dynlights/gl_lightbsp.h"
#include "gl/system/gl_interface.h"
#include "r_state.h"
#include "g_levellocals.h"

int FLightBSP::GetNodesBuffer()
{
	UpdateBuffers();
	return NodesBuffer;
}

int FLightBSP::GetLinesBuffer()
{
	UpdateBuffers();
	return LinesBuffer;
}

void FLightBSP::UpdateBuffers()
{
	if (numnodes != NumNodes || numsegs != NumSegs) // To do: there is probably a better way to detect a map change than this..
		Clear();

	if (NodesBuffer == 0)
		GenerateBuffers();
}

void FLightBSP::GenerateBuffers()
{
	if (!Shape)
		Shape.reset(new Level2DShape());
	UploadNodes();
	UploadSegs();
}

void FLightBSP::UploadNodes()
{
#if 0
	if (Shape->nodes.Size() > 0)
	{
		FILE *file = fopen("nodes.txt", "wb");
		fwrite(&Shape->nodes[0], sizeof(GPUNode) * Shape->nodes.Size(), 1, file);
		fclose(file);
	}
#endif

	int oldBinding = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldBinding);

	glGenBuffers(1, (GLuint*)&NodesBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, NodesBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GPUNode) * Shape->nodes.Size(), &Shape->nodes[0], GL_STATIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldBinding);

	NumNodes = numnodes;
}

void FLightBSP::UploadSegs()
{
	TArray<GPULine> gpulines;
	gpulines.Resize(level.lines.Size());
	for (unsigned int i = 0; i < level.lines.Size(); i++)
	{
		const auto &line = level.lines[i];
		auto &gpuseg = gpulines[i];

		gpuseg.x = (float)line.v1->fX();
		gpuseg.y = (float)line.v1->fY();
		gpuseg.dx = (float)line.v2->fX() - gpuseg.x;
		gpuseg.dy = (float)line.v2->fY() - gpuseg.y;
	}

#if 0
	if (gpulines.Size() > 0)
	{
		FILE *file = fopen("lines.txt", "wb");
		fwrite(&gpulines[0], sizeof(GPULine) * gpulines.Size(), 1, file);
		fclose(file);
	}
#endif

	int oldBinding = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldBinding);

	glGenBuffers(1, (GLuint*)&LinesBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, LinesBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GPULine) * gpulines.Size(), &gpulines[0], GL_STATIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldBinding);

	NumSegs = numsegs;
}

void FLightBSP::Clear()
{
	if (NodesBuffer != 0)
	{
		glDeleteBuffers(1, (GLuint*)&NodesBuffer);
		NodesBuffer = 0;
	}
	if (LinesBuffer != 0)
	{
		glDeleteBuffers(1, (GLuint*)&LinesBuffer);
		LinesBuffer = 0;
	}
	Shape.reset();
}

/////////////////////////////////////////////////////////////////////////////

Level2DShape::Level2DShape()
{
	TArray<int> lines;
	TArray<FVector2> centroids;
	for (unsigned int i = 0; i < level.lines.Size(); i++)
	{
		if (level.lines[i].backsector)
		{
			centroids.Push(FVector2(0.0f, 0.0f));
			continue;
		}

		lines.Push(i);

		FVector2 v1 = { (float)level.lines[i].v1->fX(), (float)level.lines[i].v1->fY() };
		FVector2 v2 = { (float)level.lines[i].v2->fX(), (float)level.lines[i].v2->fY() };
		centroids.Push((v1 + v2) * 0.5f);
	}

	TArray<int> work_buffer;
	work_buffer.Resize(lines.Size() * 2);

	root = subdivide(&lines[0], (int)lines.Size(), &centroids[0], &work_buffer[0]);
}

int Level2DShape::subdivide(int *lines, int num_lines, const FVector2 *centroids, int *work_buffer)
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
		nodes.Push(GPUNode(aabb_min, aabb_max, lines[0]));
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

	// Try split at longest axis, then if that fails the next longest, and then the remaining one
	int left_count, right_count;
	FVector2 axis;
	for (int attempt = 0; attempt < 2; attempt++)
	{
		// Find the split plane for axis
		FVector2 axis = axis_plane[axis_order[attempt]];
		FVector3 plane(axis, -(median | axis));

		// Split lines into two
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

	// Check if something went wrong when splitting and do a random split instead
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
		left_index = subdivide(lines, left_count, centroids, work_buffer);
	if (right_count > 0)
		right_index = subdivide(lines + left_count, right_count, centroids, work_buffer);

	nodes.Push(GPUNode(aabb_min, aabb_max, left_index, right_index));
	return (int)nodes.Size() - 1;
}
