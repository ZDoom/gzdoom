/** @file SkylineBinPack.cpp
	@author Jukka Jylänki

	@brief Implements different bin packer algorithms that use the SKYLINE data structure.

	This work is released to Public Domain, do whatever you want with it.
*/

#include <cassert>
#include <limits.h>
#include "templates.h"

#include "SkylineBinPack.h"

using namespace std;

SkylineBinPack::SkylineBinPack()
:binWidth(0),
binHeight(0)
{
}

SkylineBinPack::SkylineBinPack(int width, int height, bool useWasteMap)
{
	Init(width, height, useWasteMap);
}

void SkylineBinPack::Init(int width, int height, bool useWasteMap_)
{
	binWidth = width;
	binHeight = height;

	useWasteMap = useWasteMap_;

#ifdef _DEBUG
	disjointRects.Clear();
#endif

	usedSurfaceArea = 0;
	skyLine.Clear();
	SkylineNode node;
	node.x = 0;
	node.y = 0;
	node.width = binWidth;
	skyLine.Push(node);

	if (useWasteMap)
	{
		wasteMap.Init(width, height);
		wasteMap.GetFreeRectangles().Clear();
	}
}

void SkylineBinPack::Insert(TArray<RectSize> &rects, TArray<Rect> &dst)
{
	dst.Clear();

	while(rects.Size() > 0)
	{
		Rect bestNode;
		int bestScore1 = INT_MAX;
		int bestScore2 = INT_MAX;
		int bestSkylineIndex = -1;
		int bestRectIndex = -1;
		for(unsigned i = 0; i < rects.Size(); ++i)
		{
			Rect newNode;
			int score1;
			int score2;
			int index;
			newNode = FindPositionForNewNodeMinWaste(rects[i].width, rects[i].height, score2, score1, index);
#ifdef _DEBUG
			assert(disjointRects.Disjoint(newNode));
#endif
			if (newNode.height != 0)
			{
				if (score1 < bestScore1 || (score1 == bestScore1 && score2 < bestScore2))
				{
					bestNode = newNode;
					bestScore1 = score1;
					bestScore2 = score2;
					bestSkylineIndex = index;
					bestRectIndex = i;
				}
			}
		}

		if (bestRectIndex == -1)
			return;

		// Perform the actual packing.
#ifdef _DEBUG
		assert(disjointRects.Disjoint(bestNode));
		disjointRects.Add(bestNode);
#endif
		AddSkylineLevel(bestSkylineIndex, bestNode);
		usedSurfaceArea += rects[bestRectIndex].width * rects[bestRectIndex].height;
		rects.Delete(bestRectIndex);
		dst.Push(bestNode);
	}
}

Rect SkylineBinPack::Insert(int width, int height)
{
	// First try to pack this rectangle into the waste map, if it fits.
	Rect node = wasteMap.Insert(width, height, true, GuillotineBinPack::RectBestShortSideFit, 
		GuillotineBinPack::SplitMaximizeArea);
#ifdef _DEBUG
	assert(disjointRects.Disjoint(node));
#endif

	if (node.height != 0)
	{
		Rect newNode;
		newNode.x = node.x;
		newNode.y = node.y;
		newNode.width = node.width;
		newNode.height = node.height;
		usedSurfaceArea += width * height;
#ifdef _DEBUG
		assert(disjointRects.Disjoint(newNode));
		disjointRects.Add(newNode);
#endif
		return newNode;
	}
	
	return InsertBottomLeft(width, height);
}

bool SkylineBinPack::RectangleFits(int skylineNodeIndex, int width, int height, int &y) const
{
	int x = skyLine[skylineNodeIndex].x;
	if (x + width > binWidth)
		return false;
	int widthLeft = width;
	int i = skylineNodeIndex;
	y = skyLine[skylineNodeIndex].y;
	while(widthLeft > 0)
	{
		y = MAX(y, skyLine[i].y);
		if (y + height > binHeight)
			return false;
		widthLeft -= skyLine[i].width;
		++i;
		assert(i < (int)skyLine.Size() || widthLeft <= 0);
	}
	return true;
}

int SkylineBinPack::ComputeWastedArea(int skylineNodeIndex, int width, int height, int y) const
{
	int wastedArea = 0;
	const int rectLeft = skyLine[skylineNodeIndex].x;
	const int rectRight = rectLeft + width;
	for(; skylineNodeIndex < (int)skyLine.Size() && skyLine[skylineNodeIndex].x < rectRight; ++skylineNodeIndex)
	{
		if (skyLine[skylineNodeIndex].x >= rectRight || skyLine[skylineNodeIndex].x + skyLine[skylineNodeIndex].width <= rectLeft)
			break;

		int leftSide = skyLine[skylineNodeIndex].x;
		int rightSide = MIN(rectRight, leftSide + skyLine[skylineNodeIndex].width);
		assert(y >= skyLine[skylineNodeIndex].y);
		wastedArea += (rightSide - leftSide) * (y - skyLine[skylineNodeIndex].y);
	}
	return wastedArea;
}

bool SkylineBinPack::RectangleFits(int skylineNodeIndex, int width, int height, int &y, int &wastedArea) const
{
	bool fits = RectangleFits(skylineNodeIndex, width, height, y);
	if (fits)
		wastedArea = ComputeWastedArea(skylineNodeIndex, width, height, y);
	
	return fits;
}

void SkylineBinPack::AddWasteMapArea(int skylineNodeIndex, int width, int height, int y)
{
	int wastedArea = 0;
	const int rectLeft = skyLine[skylineNodeIndex].x;
	const int rectRight = rectLeft + width;
	for(; skylineNodeIndex < (int)skyLine.Size() && skyLine[skylineNodeIndex].x < rectRight; ++skylineNodeIndex)
	{
		if (skyLine[skylineNodeIndex].x >= rectRight || skyLine[skylineNodeIndex].x + skyLine[skylineNodeIndex].width <= rectLeft)
			break;

		int leftSide = skyLine[skylineNodeIndex].x;
		int rightSide = MIN(rectRight, leftSide + skyLine[skylineNodeIndex].width);
		assert(y >= skyLine[skylineNodeIndex].y);

		Rect waste;
		waste.x = leftSide;
		waste.y = skyLine[skylineNodeIndex].y;
		waste.width = rightSide - leftSide;
		waste.height = y - skyLine[skylineNodeIndex].y;

#ifdef _DEBUG
		assert(disjointRects.Disjoint(waste));
#endif
		wasteMap.GetFreeRectangles().Push(waste);
	}
}

void SkylineBinPack::AddWaste(const Rect &waste)
{
	wasteMap.GetFreeRectangles().Push(waste);
#ifdef _DEBUG
	disjointRects.Del(waste);
	wasteMap.DelDisjoint(waste);
#endif
}

void SkylineBinPack::AddSkylineLevel(int skylineNodeIndex, const Rect &rect)
{
	// First track all wasted areas and mark them into the waste map if we're using one.
	if (useWasteMap)
		AddWasteMapArea(skylineNodeIndex, rect.width, rect.height, rect.y);

	SkylineNode newNode;
	newNode.x = rect.x;
	newNode.y = rect.y + rect.height;
	newNode.width = rect.width;
	skyLine.Insert(skylineNodeIndex, newNode);

	assert(newNode.x + newNode.width <= binWidth);
	assert(newNode.y <= binHeight);

	for(unsigned i = skylineNodeIndex+1; i < skyLine.Size(); ++i)
	{
		assert(skyLine[i-1].x <= skyLine[i].x);

		if (skyLine[i].x < skyLine[i-1].x + skyLine[i-1].width)
		{
			int shrink = skyLine[i-1].x + skyLine[i-1].width - skyLine[i].x;

			skyLine[i].x += shrink;
			skyLine[i].width -= shrink;

			if (skyLine[i].width <= 0)
			{
				skyLine.Delete(i);
				--i;
			}
			else
				break;
		}
		else
			break;
	}
	MergeSkylines();
}

void SkylineBinPack::MergeSkylines()
{
	for(unsigned i = 0; i < skyLine.Size()-1; ++i)
		if (skyLine[i].y == skyLine[i+1].y)
		{
			skyLine[i].width += skyLine[i+1].width;
			skyLine.Delete(i+1);
			--i;
		}
}

Rect SkylineBinPack::InsertBottomLeft(int width, int height)
{
	int bestHeight;
	int bestWidth;
	int bestIndex;
	Rect newNode = FindPositionForNewNodeBottomLeft(width, height, bestHeight, bestWidth, bestIndex);

	if (bestIndex != -1)
	{
#ifdef _DEBUG
		assert(disjointRects.Disjoint(newNode));
#endif
		// Perform the actual packing.
		AddSkylineLevel(bestIndex, newNode);

		usedSurfaceArea += width * height;
#ifdef _DEBUG
		disjointRects.Add(newNode);
#endif
	}
	else
		memset(&newNode, 0, sizeof(Rect));

	return newNode;
}

Rect SkylineBinPack::FindPositionForNewNodeBottomLeft(int width, int height, int &bestHeight, int &bestWidth, int &bestIndex) const
{
	bestHeight = INT_MAX;
	bestIndex = -1;
	// Used to break ties if there are nodes at the same level. Then pick the narrowest one.
	bestWidth = INT_MAX;
	Rect newNode = { 0, 0, 0, 0 };
	for(unsigned i = 0; i < skyLine.Size(); ++i)
	{
		int y;
		if (RectangleFits(i, width, height, y))
		{
			if (y + height < bestHeight || (y + height == bestHeight && skyLine[i].width < bestWidth))
			{
				bestHeight = y + height;
				bestIndex = i;
				bestWidth = skyLine[i].width;
				newNode.x = skyLine[i].x;
				newNode.y = y;
				newNode.width = width;
				newNode.height = height;
#ifdef _DEBUG
				assert(disjointRects.Disjoint(newNode));
#endif
			}
		}
/*		if (RectangleFits(i, height, width, y))
		{
			if (y + width < bestHeight || (y + width == bestHeight && skyLine[i].width < bestWidth))
			{
				bestHeight = y + width;
				bestIndex = i;
				bestWidth = skyLine[i].width;
				newNode.x = skyLine[i].x;
				newNode.y = y;
				newNode.width = height;
				newNode.height = width;
				assert(disjointRects.Disjoint(newNode));
			}
		}
*/	}

	return newNode;
}

Rect SkylineBinPack::InsertMinWaste(int width, int height)
{
	int bestHeight;
	int bestWastedArea;
	int bestIndex;
	Rect newNode = FindPositionForNewNodeMinWaste(width, height, bestHeight, bestWastedArea, bestIndex);

	if (bestIndex != -1)
	{
#ifdef _DEBUG
		assert(disjointRects.Disjoint(newNode));
#endif
		// Perform the actual packing.
		AddSkylineLevel(bestIndex, newNode);

		usedSurfaceArea += width * height;
#ifdef _DEBUG
		disjointRects.Add(newNode);
#endif
	}
	else
		memset(&newNode, 0, sizeof(newNode));

	return newNode;
}

Rect SkylineBinPack::FindPositionForNewNodeMinWaste(int width, int height, int &bestHeight, int &bestWastedArea, int &bestIndex) const
{
	bestHeight = INT_MAX;
	bestWastedArea = INT_MAX;
	bestIndex = -1;
	Rect newNode;
	memset(&newNode, 0, sizeof(newNode));
	for(unsigned i = 0; i < skyLine.Size(); ++i)
	{
		int y;
		int wastedArea;

		if (RectangleFits(i, width, height, y, wastedArea))
		{
			if (wastedArea < bestWastedArea || (wastedArea == bestWastedArea && y + height < bestHeight))
			{
				bestHeight = y + height;
				bestWastedArea = wastedArea;
				bestIndex = i;
				newNode.x = skyLine[i].x;
				newNode.y = y;
				newNode.width = width;
				newNode.height = height;
#ifdef _DEBUG
				assert(disjointRects.Disjoint(newNode));
#endif
			}
		}
/*		if (RectangleFits(i, height, width, y, wastedArea))
		{
			if (wastedArea < bestWastedArea || (wastedArea == bestWastedArea && y + width < bestHeight))
			{
				bestHeight = y + width;
				bestWastedArea = wastedArea;
				bestIndex = i;
				newNode.x = skyLine[i].x;
				newNode.y = y;
				newNode.width = height;
				newNode.height = width;
				assert(disjointRects.Disjoint(newNode));
			}
		}*/
	}

	return newNode;
}

/// Computes the ratio of used surface area.
float SkylineBinPack::Occupancy() const
{
	return (float)usedSurfaceArea / (binWidth * binHeight);
}
