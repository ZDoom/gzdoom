/** @file Rect.h
	@author Jukka Jylänki

	This work is released to Public Domain, do whatever you want with it.
*/
#pragma once

#include <vector>

struct RectSize
{
	int width;
	int height;
};

struct Rect
{
	int x;
	int y;
	int width;
	int height;
};

/// Performs a lexicographic compare on (rect short side, rect long side).
/// @return -1 if the smaller side of a is shorter than the smaller side of b, 1 if the other way around.
///   If they are equal, the larger side length is used as a tie-breaker.
///   If the rectangles are of same size, returns 0.
int CompareRectShortSide(const Rect &a, const Rect &b);

/// Performs a lexicographic compare on (x, y, width, height).
int NodeSortCmp(const Rect &a, const Rect &b);

/// Returns true if a is contained in b.
bool IsContainedIn(const Rect &a, const Rect &b);

#ifdef _DEBUG
class DisjointRectCollection
{
public:
	TArray<Rect> rects;

	bool Add(const Rect &r)
	{
		// Degenerate rectangles are ignored.
		if (r.width == 0 || r.height == 0)
			return true;

		if (!Disjoint(r))
			return false;
		rects.Push(r);
		return true;
	}

	bool Del(const Rect &r)
	{
		for(unsigned i = 0; i < rects.Size(); ++i)
		{
			if(r.x == rects[i].x && r.y == rects[i].y && r.width == rects[i].width && r.height == rects[i].height)
			{
				rects.Delete(i);
				return true;
			}
		}
		return false;
	}

	void Clear()
	{
		rects.Clear();
	}

	bool Disjoint(const Rect &r) const
	{
		// Degenerate rectangles are ignored.
		if (r.width == 0 || r.height == 0)
			return true;

		for(unsigned i = 0; i < rects.Size(); ++i)
			if (!Disjoint(rects[i], r))
				return false;
		return true;
	}

	static bool Disjoint(const Rect &a, const Rect &b)
	{
		if (a.x + a.width <= b.x ||
			b.x + b.width <= a.x ||
			a.y + a.height <= b.y ||
			b.y + b.height <= a.y)
			return true;
		return false;
	}
};
#endif
