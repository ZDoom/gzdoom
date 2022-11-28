/** @file SkylineBinPack.h
	@author Jukka Jylänki

	@brief Implements different bin packer algorithms that use the SKYLINE data structure.

	This work is released to Public Domain, do whatever you want with it.
*/
#pragma once

#include "tarray.h"

#include "Rect.h"
#include "GuillotineBinPack.h"

/** Implements bin packing algorithms that use the SKYLINE data structure to store the bin contents. Uses
	GuillotineBinPack as the waste map. */
class SkylineBinPack
{
public:
	/// Instantiates a bin of size (0,0). Call Init to create a new bin.
	SkylineBinPack();

	/// Instantiates a bin of the given size.
	SkylineBinPack(int binWidth, int binHeight, bool useWasteMap);

	/// (Re)initializes the packer to an empty bin of width x height units. Call whenever
	/// you need to restart with a new bin.
	void Init(int binWidth, int binHeight, bool useWasteMap);

	/// Inserts the given list of rectangles in an offline/batch mode, possibly rotated.
	/// @param rects The list of rectangles to insert. This vector will be destroyed in the process.
	/// @param dst [out] This list will contain the packed rectangles. The indices will not correspond to that of rects.
	/// @param method The rectangle placement rule to use when packing.
	void Insert(TArray<RectSize> &rects, TArray<Rect> &dst);

	/// Inserts a single rectangle into the bin, possibly rotated.
	Rect Insert(int width, int height);

	/// Adds a rectangle to the waste list. It must have been previously returned by
	/// Insert or the results are undefined.
	void AddWaste(const Rect &rect);

	/// Computes the ratio of used surface area to the total bin area.
	float Occupancy() const;

private:
	int binWidth;
	int binHeight;

#ifdef _DEBUG
	DisjointRectCollection disjointRects;
#endif

	/// Represents a single level (a horizontal line) of the skyline/horizon/envelope.
	struct SkylineNode
	{
		/// The starting x-coordinate (leftmost).
		int x;

		/// The y-coordinate of the skyline level line.
		int y;

		/// The line width. The ending coordinate (inclusive) will be x+width-1.
		int width;
	};

	TArray<SkylineNode> skyLine;

	unsigned long usedSurfaceArea;

	/// If true, we use the GuillotineBinPack structure to recover wasted areas into a waste map.
	bool useWasteMap;
	GuillotineBinPack wasteMap;

	Rect InsertBottomLeft(int width, int height);
	Rect InsertMinWaste(int width, int height);

	Rect FindPositionForNewNodeBottomLeft(int width, int height, int &bestHeight, int &bestWidth, int &bestIndex) const;
	Rect FindPositionForNewNodeMinWaste(int width, int height, int &bestHeight, int &bestWastedArea, int &bestIndex) const;

	bool RectangleFits(int skylineNodeIndex, int width, int height, int &y) const;
	bool RectangleFits(int skylineNodeIndex, int width, int height, int &y, int &wastedArea) const;
	int ComputeWastedArea(int skylineNodeIndex, int width, int height, int y) const;

	void AddWasteMapArea(int skylineNodeIndex, int width, int height, int y);

	void AddSkylineLevel(int skylineNodeIndex, const Rect &rect);

	/// Merges all skyline nodes that are at the same level.
	void MergeSkylines();
};
