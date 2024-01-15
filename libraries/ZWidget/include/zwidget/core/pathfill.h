#pragma once

#include <vector>
#include <stdint.h>
#include <limits.h> 
#include <string.h>
#include "core/rect.h"

/*
// 3x3 transform matrix
class PathFillTransform
{
public:
	PathFillTransform() { for (int i = 0; i < 9; i++) matrix[i] = 0.0f; matrix[0] = matrix[4] = matrix[8] = 1.0; }
	PathFillTransform(const float* mat3x3) { for (int i = 0; i < 9; i++) matrix[i] = (double)mat3x3[i]; }
	PathFillTransform(const double* mat3x3) { for (int i = 0; i < 9; i++) matrix[i] = mat3x3[i]; }

	double matrix[9];
};
*/

enum class PathFillMode
{
	alternate,
	winding
};

enum class PathFillCommand
{
	line,
	quadradic,
	cubic
};

class PathFillSubpath
{
public:
	PathFillSubpath() : points(1) { }

	std::vector<Point> points;
	std::vector<PathFillCommand> commands;
	bool closed = false;
};

class PathFillDesc
{
public:
	PathFillDesc() : subpaths(1) { }

	PathFillMode fill_mode = PathFillMode::alternate;
	std::vector<PathFillSubpath> subpaths;

	void MoveTo(const Point& point)
	{
		if (!subpaths.back().commands.empty())
		{
			subpaths.push_back(PathFillSubpath());
		}
		subpaths.back().points.front() = point;
	}

	void LineTo(const Point& point)
	{
		auto& subpath = subpaths.back();
		subpath.points.push_back(point);
		subpath.commands.push_back(PathFillCommand::line);
	}

	void BezierTo(const Point& control, const Point& point)
	{
		auto& subpath = subpaths.back();
		subpath.points.push_back(control);
		subpath.points.push_back(point);
		subpath.commands.push_back(PathFillCommand::quadradic);
	}

	void BezierTo(const Point& control1, const Point& control2, const Point& point)
	{
		auto& subpath = subpaths.back();
		subpath.points.push_back(control1);
		subpath.points.push_back(control2);
		subpath.points.push_back(point);
		subpath.commands.push_back(PathFillCommand::cubic);
	}

	void Close()
	{
		if (!subpaths.back().commands.empty())
		{
			subpaths.back().closed = true;
			subpaths.push_back(PathFillSubpath());
		}
	}

	void Rasterize(uint8_t* dest, int width, int height, bool blend = false);
};
