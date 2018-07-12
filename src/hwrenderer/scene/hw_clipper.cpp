/*
*
** gl_clipper.cpp
**
** Handles visibility checks.
** Loosely based on the JDoom clipper.
**
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "hw_clipper.h"
#include "g_levellocals.h"

unsigned Clipper::starttime;

Clipper::Clipper()
{
	starttime++;
}

//-----------------------------------------------------------------------------
//
// RemoveRange
//
//-----------------------------------------------------------------------------

void Clipper::RemoveRange(ClipNode * range)
{
	if (range == cliphead)
	{
		cliphead = cliphead->next;
	}
	else
	{
		if (range->prev) range->prev->next = range->next;
		if (range->next) range->next->prev = range->prev;
	}
	
	Free(range);
}

//-----------------------------------------------------------------------------
//
// Clear
//
//-----------------------------------------------------------------------------

void Clipper::Clear()
{
	ClipNode *node = cliphead;
	ClipNode *temp;
	
	blocked = false;
	while (node != NULL)
	{
		temp = node;
		node = node->next;
		Free(temp);
	}
	node = silhouette;

	while (node != NULL)
	{
		temp = node;
		node = node->next;
		Free(temp);
	}
	
	cliphead = NULL;
	silhouette = NULL;
	starttime++;
}

//-----------------------------------------------------------------------------
//
// SetSilhouette
//
//-----------------------------------------------------------------------------

void Clipper::SetSilhouette()
{
	ClipNode *node = cliphead;
	ClipNode *last = NULL;

	while (node != NULL)
	{
		ClipNode *snode = NewRange(node->start, node->end);
		if (silhouette == NULL) silhouette = snode;
		snode->prev = last;
		if (last != NULL) last->next = snode;
		last = snode;
		node = node->next;
	}
}

//-----------------------------------------------------------------------------
//
// IsRangeVisible
//
//-----------------------------------------------------------------------------

bool Clipper::IsRangeVisible(angle_t startAngle, angle_t endAngle)
{
	ClipNode *ci;
	ci = cliphead;
	
	if (endAngle==0 && ci && ci->start==0) return false;
	
	while (ci != NULL && ci->start < endAngle)
	{
		if (startAngle >= ci->start && endAngle <= ci->end)
		{
			return false;
		}
		ci = ci->next;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
//
// AddClipRange
//
//-----------------------------------------------------------------------------

void Clipper::AddClipRange(angle_t start, angle_t end)
{
	ClipNode *node, *temp, *prevNode;

	if (cliphead)
	{
		//check to see if range contains any old ranges
		node = cliphead;
		while (node != NULL && node->start < end)
		{
			if (node->start >= start && node->end <= end)
			{
				temp = node;
				node = node->next;
				RemoveRange(temp);
			}
			else if (node->start<=start && node->end>=end)
			{
				return;
			}
			else
			{
				node = node->next;
			}
		}
		
		//check to see if range overlaps a range (or possibly 2)
		node = cliphead;
		while (node != NULL && node->start <= end)
		{
			if (node->end >= start)
			{
				// we found the first overlapping node
				if (node->start > start)
				{
					// the new range overlaps with this node's start point
					node->start = start;
				}

				if (node->end < end) 
				{
					node->end = end;
				}

				ClipNode *node2 = node->next;
				while (node2 && node2->start <= node->end)
				{
					if (node2->end > node->end) node->end = node2->end;
					ClipNode *delnode = node2;
					node2 = node2->next;
					RemoveRange(delnode);
				}
				return;
			}
			node = node->next;		
		}
		
		//just add range
		node = cliphead;
		prevNode = NULL;
		temp = NewRange(start, end);
		
		while (node != NULL && node->start < end)
		{
			prevNode = node;
			node = node->next;
		}
		
		temp->next = node;
		if (node == NULL)
		{
			temp->prev = prevNode;
			if (prevNode) prevNode->next = temp;
			if (!cliphead) cliphead = temp;
		}
		else
		{
			if (node == cliphead)
			{
				cliphead->prev = temp;
				cliphead = temp;
			}
			else
			{
				temp->prev = prevNode;
				prevNode->next = temp;
				node->prev = temp;
			}
		}
	}
	else
	{
		temp = NewRange(start, end);
		cliphead = temp;
		return;
	}
}


//-----------------------------------------------------------------------------
//
// RemoveClipRange
//
//-----------------------------------------------------------------------------

void Clipper::RemoveClipRange(angle_t start, angle_t end)
{
	ClipNode *node;

	if (silhouette)
	{
		node = silhouette;
		while (node != NULL && node->end <= start)
		{
			node = node->next;
		}
		if (node != NULL && node->start <= start)
		{
			if (node->end >= end) return;
			start = node->end;
			node = node->next;
		}
		while (node != NULL && node->start < end)
		{
			DoRemoveClipRange(start, node->start);
			start = node->end;
			node = node->next;
		}
		if (start >= end) return;
	}
	DoRemoveClipRange(start, end);
}
	
//-----------------------------------------------------------------------------
//
// RemoveClipRange worker function
//
//-----------------------------------------------------------------------------

void Clipper::DoRemoveClipRange(angle_t start, angle_t end)
{
	ClipNode *node, *temp;

	if (cliphead)
	{
		//check to see if range contains any old ranges
		node = cliphead;
		while (node != NULL && node->start < end)
		{
			if (node->start >= start && node->end <= end)
			{
				temp = node;
				node = node->next;
				RemoveRange(temp);
			}
			else
			{
				node = node->next;
			}
		}
		
		//check to see if range overlaps a range (or possibly 2)
		node = cliphead;
		while (node != NULL)
		{
			if (node->start >= start && node->start <= end)
			{
				node->start = end;
				break;
			}
			else if (node->end >= start && node->end <= end)
			{
				node->end=start;
			}
			else if (node->start < start && node->end > end)
			{
				temp = NewRange(end, node->end);
				node->end=start;
				temp->next=node->next;
				temp->prev=node;
				node->next=temp;
				if (temp->next) temp->next->prev=temp;
				break;
			}
			node = node->next;
		}
	}
}


//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

angle_t Clipper::AngleToPseudo(angle_t ang)
{
	double vecx = cos(ang * M_PI / ANGLE_180);
	double vecy = sin(ang * M_PI / ANGLE_180);

	double result = vecy / (fabs(vecx) + fabs(vecy));
	if (vecx < 0)
	{
		result = 2.f - result;
	}
	return xs_Fix<30>::ToFix(result);
}

//-----------------------------------------------------------------------------
//
// ! Returns the pseudoangle between the line p1 to (infinity, p1.y) and the 
// line from p1 to p2. The pseudoangle has the property that the ordering of 
// points by true angle around p1 and ordering of points by pseudoangle are the 
// same.
//
// For clipping exact angles are not needed. Only the ordering matters.
// This is about as fast as the fixed point R_PointToAngle2 but without
// the precision issues associated with that function.
//
//-----------------------------------------------------------------------------

angle_t Clipper::PointToPseudoAngle(double x, double y)
{
	double vecx = x - viewpoint->Pos.X;
	double vecy = y - viewpoint->Pos.Y;

	if (vecx == 0 && vecy == 0)
	{
		return 0;
	}
	else
	{
		double result = vecy / (fabs(vecx) + fabs(vecy));
		if (vecx < 0)
		{
			result = 2. - result;
		}
		return xs_Fix<30>::ToFix(result);
	}
}



//-----------------------------------------------------------------------------
//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
//-----------------------------------------------------------------------------
	static const uint8_t checkcoord[12][4] = // killough -- static const
	{
	  {3,0,2,1},
	  {3,0,2,0},
	  {3,1,2,0},
	  {0},
	  {2,0,2,1},
	  {0,0,0,0},
	  {3,1,3,0},
	  {0},
	  {2,0,3,1},
	  {2,1,3,1},
	  {2,1,3,0}
	};

bool Clipper::CheckBox(const float *bspcoord) 
{
	angle_t angle1, angle2;

	int        boxpos;
	const uint8_t* check;
	
	// Find the corners of the box
	// that define the edges from current viewpoint.
    auto &vp = viewpoint;
	boxpos = (vp->Pos.X <= bspcoord[BOXLEFT] ? 0 : vp->Pos.X < bspcoord[BOXRIGHT ] ? 1 : 2) +
		(vp->Pos.Y >= bspcoord[BOXTOP ] ? 0 : vp->Pos.Y > bspcoord[BOXBOTTOM] ? 4 : 8);
	
	if (boxpos == 5) return true;
	
	check = checkcoord[boxpos];
	angle1 = PointToPseudoAngle (bspcoord[check[0]], bspcoord[check[1]]);
	angle2 = PointToPseudoAngle (bspcoord[check[2]], bspcoord[check[3]]);
	
	return SafeCheckRange(angle2, angle1);
}

