#include <stdlib.h>
#include <assert.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "r_defs.h"
#include "r_main.h"
#include "templates.h"
#include "m_bbox.h"
#include "p_nodebuild.h"
#include "c_console.h"

static const int PO_LINE_START = 1;
static const int PO_LINE_EXPLICIT = 5;

#if 0
#define D(x) x
#else
#define D(x) do{}while(0)
#endif

extern sector_t *sectors;

FNodeBuilder::FNodeBuilder (line_t *lines, int numlines,
							vertex_t *vertices, int numvertices,
							side_t *sides, int numsides,
							TArray<FPolyStart> &polyspots, TArray<FPolyStart> &anchors)
	: Vertices (numvertices),
	  Lines (lines), NumLines (numlines),
	  Sides (sides), NumSides (numsides),
	  SegsStuffed (0), SubsecCount (0)
{
	FindUsedVertices (vertices, numvertices);
	MakeSegsFromSides ();
	FindPolyContainers (polyspots, anchors);
	GroupSegPlanes ();
	BuildTree ();
}

void FNodeBuilder::FindUsedVertices (vertex_t *oldverts, int max)
{
	size_t *map = (size_t *)alloca (max*sizeof(size_t));
	int i;
	FPrivVert newvert;

	clearbuf (&map[0], max, -1);

	newvert.segs = NO_INDEX;
	for (i = 0; i < NumLines; ++i)
	{
		int v1 = Lines[i].v1 - oldverts;
		int v2 = Lines[i].v2 - oldverts;

		if (map[v1] == (size_t)-1)
		{
			newvert.x = oldverts[v1].x;
			newvert.y = oldverts[v1].y;
			map[v1] = Vertices.Push (newvert);
		}
		if (map[v2] == (size_t)-1)
		{
			newvert.x = oldverts[v2].x;
			newvert.y = oldverts[v2].y;
			map[v2] = Vertices.Push (newvert);
		}

		Lines[i].v1 = (vertex_t *)map[v1];
		Lines[i].v2 = (vertex_t *)map[v2];
	}
}

void FNodeBuilder::MakeSegsFromSides ()
{
	FPrivSeg seg;
	int i, j;

	seg.next = NO_INDEX;
	seg.loopnum = 0;
	for (i = 0; i < NumLines; ++i)
	{
		seg.linedef = &Lines[i];
		seg.sidedef = &Sides[Lines[i].sidenum[0]];
		seg.frontsector = seg.linedef->frontsector;
		seg.backsector = seg.linedef->backsector;
		seg.v1 = (size_t)Lines[i].v1;
		seg.v2 = (size_t)Lines[i].v2;
		seg.nextforvert = Vertices[seg.v1].segs;
		j = (int)Segs.Push (seg);
		Vertices[seg.v1].segs = j;

		if (Lines[i].sidenum[1] != NO_INDEX)
		{
			seg.sidedef = &Sides[Lines[i].sidenum[1]];
			swap (seg.frontsector, seg.backsector);
			swap (seg.v1, seg.v2);
			seg.nextforvert = Vertices[seg.v1].segs;
			j = (int)Segs.Push (seg);
			Vertices[seg.v1].segs = j;
		}
	}
}

void FNodeBuilder::GroupSegPlanes ()
{
	int i, planenum;

	for (i = 0; i < (int)Segs.Size(); ++i)
	{
		FPrivSeg *seg = &Segs[i];
		double dx, dy, invlen;

		// ax+by+0z=c
		dx = double(Vertices[seg->v2].y) - double(Vertices[seg->v1].y);
		dy = double(Vertices[seg->v1].x) - double(Vertices[seg->v2].x);

		// Since we don't care about direction, eliminate almost half
		// the possible planes by transforming lines that point south
		// into lines that point north. (I tried transforming east-facing
		// lines into west-facing ones too, but that didn't work.)
		if (dy < 0.0)
		{
			dx = -dx;
			dy = -dy;
		}

		invlen = double(1<<30) / sqrt (dx*dx+dy*dy);
		seg->plane[0] = fixed_t(invlen * dx);
		seg->plane[1] = fixed_t(invlen * dy);
		seg->plane[2] = DMulScale31 (seg->plane[0], Vertices[seg->v1].x,
									 seg->plane[1], Vertices[seg->v1].y);
	}

	qsort (&Segs[0], Segs.Size(), sizeof(FPrivSeg), SortPlanes);

	for (i = (int)(Segs.Size()-2); i >= 0; --i)
	{
		Segs[i].next = i+1;
	}

	Segs[0].planenum = planenum = 0;
	fixed_t *p1 = Segs[0].plane;
	for (i = 1; i < (int)Segs.Size(); ++i)
	{
		fixed_t *p2 = Segs[i].plane;
		if (p1[2] != p2[2] || p1[1] != p2[1] || p1[0] != p2[0])
		{
			++planenum;
			p1 = p2;
		}
		Segs[i].planenum = planenum;
	}

	D(Printf ("%d planes from %d segs\n", planenum+1, i));

	planenum = (planenum+8)/8;
	PlaneChecked.Reserve (planenum);
}

// The specific ordering is unimportant, so long as equivalent planes
// are sorted next to each other.
int STACK_ARGS FNodeBuilder::SortPlanes (const void *a, const void *b)
{
	const FPrivSeg *x = (FPrivSeg *)a;
	const FPrivSeg *y = (FPrivSeg *)b;

	if (x->plane[2] == y->plane[2])
	{
		if (x->plane[1] == y->plane[1])
		{
			return x->plane[0] - y->plane[0];
		}
		else
		{
			return x->plane[1] - y->plane[1];
		}
	}
	else
	{
		return x->plane[2] - y->plane[2];
	}
}

void FNodeBuilder::FindPolyContainers (TArray<FPolyStart> &spots, TArray<FPolyStart> &anchors)
{
	int loop = 1;

	for (size_t i = 0; i < spots.Size(); ++i)
	{
		FPolyStart *spot = &spots[i];
		fixed_t bbox[4];

		if (GetPolyExtents (spot->polynum, bbox))
		{
			FPolyStart *anchor;
			size_t j;

			for (j = 0; j < anchors.Size(); ++j)
			{
				anchor = &anchors[j];
				if (anchor->polynum == spot->polynum)
				{
					break;
				}
			}

			if (j < anchors.Size())
			{
				vertex_t mid;
				vertex_t center;

				mid.x = bbox[BOXLEFT] + (bbox[BOXRIGHT]-bbox[BOXLEFT])/2;
				mid.y = bbox[BOXBOTTOM] + (bbox[BOXTOP]-bbox[BOXBOTTOM])/2;

				center.x = mid.x - anchor->x + spot->x;
				center.y = mid.y - anchor->y + spot->y;

				// Scan right for the seg closest to the polyobject's center after it
				// gets moved to its start spot.
				fixed_t closestdist = FIXED_MAX;
				long closestseg = 0;

				D(Printf ("start %d,%d -- center %d, %d\n", spot->x>>16, spot->y>>16, center.x>>16, center.y>>16));

				for (size_t j = 0; j < Segs.Size(); ++j)
				{
					FPrivSeg *seg = &Segs[j];
					FPrivVert *v1 = &Vertices[seg->v1];
					FPrivVert *v2 = &Vertices[seg->v2];
					fixed_t dy = v2->y - v1->y;

					if (dy == 0)
					{ // Horizontal, so skip it
						continue;
					}
					if ((v1->y < center.y && v2->y < center.y) || (v1->y > center.y && v2->y > center.y))
					{ // Not crossed
						continue;
					}

					fixed_t dx = v2->x - v1->x;

					if (PointOnSide (center.x, center.y, v1->x, v1->y, dx, dy) <= 0)
					{
						fixed_t t = DivScale30 (center.y - v1->y, dy);
						fixed_t sx = v1->x + MulScale30 (dx, t);
						fixed_t dist = sx - spot->x;

						if (dist < closestdist && dist >= 0)
						{
							closestdist = dist;
							closestseg = (long)j;
						}
					}
				}
				if (closestseg >= 0)
				{
					loop = MarkLoop (closestseg, loop);
					D(Printf ("Found polyobj in sector %d (loop %d)\n", Segs[closestseg].frontsector-sectors,
						Segs[closestseg].loopnum));
				}
			}
		}
	}
}

bool FNodeBuilder::GetPolyExtents (int polynum, fixed_t bbox[4])
{
	size_t i;

	bbox[BOXLEFT] = bbox[BOXBOTTOM] = FIXED_MAX;
	bbox[BOXRIGHT] = bbox[BOXTOP] = FIXED_MIN;

	// Try to find a polyobj marked with a start line
	for (i = 0; i < Segs.Size(); ++i)
	{
		if (Segs[i].linedef->special == PO_LINE_START &&
			Segs[i].linedef->args[0] == polynum)
		{
			break;
		}
	}

	if (i < Segs.Size())
	{
		vertex_t start;
		size_t vert;

		vert = Segs[i].v1;

		start.x = Vertices[vert].x;
		start.y = Vertices[vert].y;

		do
		{
			AddSegToBBox (bbox, &Segs[i]);
			vert = Segs[i].v2;
			i = Vertices[vert].segs;
		} while (i != NO_INDEX && (Vertices[vert].x != start.x || Vertices[vert].y != start.y));

		return true;
	}

	// Try to find a polyobj marked with explicit lines
	bool found = false;

	for (i = 0; i < Segs.Size(); ++i)
	{
		if (Segs[i].linedef->special == PO_LINE_EXPLICIT &&
			Segs[i].linedef->args[0] == polynum)
		{
			AddSegToBBox (bbox, &Segs[i]);
			found = true;
		}
	}
	return found;
}

void FNodeBuilder::AddSegToBBox (fixed_t bbox[4], const FPrivSeg *seg)
{
	FPrivVert *v1 = &Vertices[seg->v1];
	FPrivVert *v2 = &Vertices[seg->v2];

	if (v1->x < bbox[BOXLEFT])		bbox[BOXLEFT] = v1->x;
	if (v1->x > bbox[BOXRIGHT])		bbox[BOXRIGHT] = v1->x;
	if (v1->y < bbox[BOXBOTTOM])	bbox[BOXBOTTOM] = v1->y;
	if (v1->y > bbox[BOXTOP])		bbox[BOXTOP] = v1->y;

	if (v2->x < bbox[BOXLEFT])		bbox[BOXLEFT] = v2->x;
	if (v2->x > bbox[BOXRIGHT])		bbox[BOXRIGHT] = v2->x;
	if (v2->y < bbox[BOXBOTTOM])	bbox[BOXBOTTOM] = v2->y;
	if (v2->y > bbox[BOXTOP])		bbox[BOXTOP] = v2->y;
}

int FNodeBuilder::MarkLoop (int firstseg, int loopnum)
{
	int seg;
	sector_t *sec = Segs[firstseg].frontsector;

	if (Segs[firstseg].loopnum != 0)
	{ // already marked
		return loopnum;
	}

	seg = firstseg;

	do
	{
		FPrivSeg *s1 = &Segs[seg];

		s1->loopnum = loopnum;

		D(Printf ("Mark seg %d (%d,%d)-(%d,%d)\n", seg,
				Vertices[s1->v1].x>>16, Vertices[s1->v1].y>>16,
				Vertices[s1->v2].x>>16, Vertices[s1->v2].y>>16));

		int bestseg = -1;
		int tryseg = Vertices[s1->v2].segs;
		angle_t bestang = ANGLE_MAX;
		angle_t ang1 = R_PointToAngle2 (Vertices[s1->v2].x, Vertices[s1->v2].y,
			Vertices[s1->v1].x, Vertices[s1->v1].y);

		while (tryseg != -1)
		{
			FPrivSeg *s2 = &Segs[tryseg];

			if (s2->frontsector == sec)
			{
				angle_t ang2 = R_PointToAngle2 (Vertices[s2->v1].x, Vertices[s2->v1].y,
					Vertices[s2->v2].x, Vertices[s2->v2].y);
				angle_t angdiff = ang2 - ang1;
				if (angdiff < bestang && angdiff > 0)
				{
					bestang = angdiff;
					bestseg = tryseg;
				}
			}
			tryseg = s2->nextforvert;
		}

		seg = bestseg;
	} while (seg != -1 && Segs[seg].loopnum == 0);

	return loopnum + 1;
}

void FNodeBuilder::BuildTree ()
{
	fixed_t bbox[4];

	C_InitTicker ("Building BSP", FRACUNIT);
	CreateNode (0, bbox);
	C_SetTicker (65536, true);
	C_InitTicker (NULL, 0);
}

int FNodeBuilder::CreateNode (int set, fixed_t bbox[4])
{
	node_t node;
	int skip, count, selstat;

	count = CountSegs (set);
	skip = count / 64;

	if ((selstat = SelectSplitter (set, node, skip, true)) > 0 ||
		(skip > 1 && (selstat = SelectSplitter (set, node, 1, true)) > 0) ||
		(selstat < 0 && (SelectSplitter (set, node, skip, false) > 0) ||
						(skip > 1 && SelectSplitter (set, node, 1, false))) ||
		CheckSubsector (set, node, count))
	{
		// Create a normal node
		int set1, set2;

		SplitSegs (set, node, set1, set2);
		D(PrintSet (1, set1));
		D(Printf ("(%d,%d) delta (%d,%d)\n", node.x>>16, node.y>>16, node.dx>>16, node.dy>>16));
		D(PrintSet (2, set2));
		node.children[0] = CreateNode (set1, node.bbox[0]);
		node.children[1] = CreateNode (set2, node.bbox[1]);
		bbox[BOXTOP] = MAX (node.bbox[0][BOXTOP], node.bbox[1][BOXTOP]);
		bbox[BOXBOTTOM] = MIN (node.bbox[0][BOXBOTTOM], node.bbox[1][BOXBOTTOM]);
		bbox[BOXLEFT] = MIN (node.bbox[0][BOXLEFT], node.bbox[1][BOXLEFT]);
		bbox[BOXRIGHT] = MAX (node.bbox[0][BOXRIGHT], node.bbox[1][BOXRIGHT]);
		return (int)Nodes.Push (node);
	}
	else
	{
		return NF_SUBSECTOR | CreateSubsector (set, bbox);
	}
}

int FNodeBuilder::CreateSubsector (int set, fixed_t bbox[4])
{
	subsector_t sub;

	bbox[BOXTOP] = bbox[BOXRIGHT] = INT_MIN;
	bbox[BOXBOTTOM] = bbox[BOXLEFT] = INT_MAX;

	D(Printf ("Subsector from set %d\n", set));

	assert (set != -1);

	sub.firstline = (WORD)SegList.Size();
	sub.poly = NULL;
	sub.sector = Segs[set].frontsector;

	while (set != -1)
	{
		USegPtr ptr;

		ptr.SegPtr = &Segs[set];
		AddSegToBBox (bbox, ptr.SegPtr);
		SegList.Push (ptr);
		set = ptr.SegPtr->next;
	}
	sub.numlines = (WORD)(SegList.Size() - sub.firstline);

	// Sort segs by linedef for special effects
	qsort (&SegList[sub.firstline], sub.numlines, sizeof(int), SortSegs);

	// Convert seg pointers into indices
	for (size_t i = sub.firstline; i < SegList.Size(); ++i)
	{
		SegList[i].SegNum = SegList[i].SegPtr - &Segs[0];
	}

	SegsStuffed += sub.numlines;
	if (++SubsecCount == 32)
	{
		SubsecCount = 0;
		C_SetTicker (Scale (SegsStuffed, 65536, (SDWORD)Segs.Size()));
	}

	D(Printf ("bbox (%d,%d)-(%d,%d)\n", bbox[BOXLEFT]>>16, bbox[BOXBOTTOM]>>16, bbox[BOXRIGHT]>>16, bbox[BOXTOP]>>16));

	return (int)Subsectors.Push (sub);
}

int STACK_ARGS FNodeBuilder::SortSegs (const void *a, const void *b)
{
	const FPrivSeg *x = ((const USegPtr *)a)->SegPtr;
	const FPrivSeg *y = ((const USegPtr *)b)->SegPtr;

	if (x->linedef == y->linedef)
	{
		return x - y;
	}
	else
	{
		return x->linedef - y->linedef;
	}
}

int FNodeBuilder::CountSegs (int set) const
{
	int count = 0;

	while (set != -1)
	{
		count++;
		set = Segs[set].next;
	}
	return count;
}

// Given a set of segs, checks to make sure they all belong to a single
// sector. If so, false is returned, and they become a subsector. If not,
// a splitter is synthesized, and true is returned to continue processing
// down this branch of the tree.

bool FNodeBuilder::CheckSubsector (int set, node_t &node, int setsize)
{
	sector_t *sec;
	int seg;

	sec = Segs[set].frontsector;
	seg = set;

	do
	{
		if (Segs[seg].frontsector != sec)
		{
			break;
		}
		seg = Segs[seg].next;
	} while (seg != -1);

	if (seg == -1)
	{ // It's a valid subsector
		return false;
	}

	// If there are only two segs in the set, and they form two sides
	// of a triangle, the splitter should pass through their shared
	// point and the (imaginary) third side of the triangle
	if (setsize == 2)
	{
		FPrivVert *v1, *v2, *v3;

		if (Vertices[Segs[set].v2] == Vertices[Segs[seg].v1])
		{
			v1 = &Vertices[Segs[set].v1];
			v2 = &Vertices[Segs[seg].v2];
			v3 = &Vertices[Segs[set].v2];
		}
		else if (Vertices[Segs[set].v1] == Vertices[Segs[seg].v2])
		{
			v1 = &Vertices[Segs[seg].v1];
			v2 = &Vertices[Segs[set].v2];
			v3 = &Vertices[Segs[seg].v2];
		}
		else
		{
			v1 = v2 = v3 = NULL;
		}
		if (v1 != NULL)
		{
			node.x = v3->x;
			node.y = v3->y;
			node.dx = v1->x + (v2->x-v1->x)/2 - node.x;
			node.dy = v1->y + (v2->y-v1->y)/2 - node.y;
			return Heuristic (node, set, false) != 0;
		}
	}

	bool nosplit = true;
	int firsthit = seg;

	do
	{
		seg = firsthit;
		do
		{
			if (Segs[seg].frontsector != sec)
			{
				node.x = Vertices[Segs[set].v1].x;
				node.y = Vertices[Segs[set].v1].y;
				node.dx = Vertices[Segs[seg].v2].x - node.x;
				node.dy = Vertices[Segs[seg].v2].y - node.y;

				if (Heuristic (node, set, nosplit) != 0)
				{
					return true;
				}

				node.dx = Vertices[Segs[seg].v1].x - node.x;
				node.dy = Vertices[Segs[seg].v1].y - node.y;

				if (Heuristic (node, set, nosplit) != 0)
				{
					return true;
				}
			}

			seg = Segs[seg].next;
		} while (seg != -1);
	} while ((nosplit ^= 1) == 0);

	// Give up.
	return false;
}

int FNodeBuilder::SelectSplitter (int set, node_t &node, int step, bool nosplit)
{
	int stepleft;
	int bestvalue, bestseg;
	int seg;
	bool nosplitters = false;

	bestvalue = 0;
	bestseg = -1;

	seg = set;
	stepleft = 0;

	memset (&PlaneChecked[0], 0, PlaneChecked.Size());

	while (seg != -1)
	{
		const FPrivSeg *pseg = &Segs[seg];

		if (--stepleft <= 0)
		{
			int l = pseg->planenum >> 3;
			int r = pseg->planenum & 7;

			if ((PlaneChecked[l] & r) == 0)
			{
				PlaneChecked[l] |= r;

				stepleft = step;
				node.x = Vertices[pseg->v1].x;
				node.y = Vertices[pseg->v1].y;
				node.dx = Vertices[pseg->v2].x - node.x;
				node.dy = Vertices[pseg->v2].y - node.y;

				int value = Heuristic (node, set, nosplit);

				D(Printf ("Seg %d (%4d,%4d)-(%4d,%4d) scores %d\n", seg, node.x>>16, node.y>>16,
					(node.x+node.dx)>>16, (node.y+node.dy)>>16, value));

				if (value > bestvalue)
				{
					bestvalue = value;
					bestseg = seg;
				}
				else if (value < 0)
				{
					nosplitters = true;
				}
			}
		}

		seg = pseg->next;
	}

	if (bestseg < 0)
	{ // No lines split any others into two sets, so this is a convex region.
	D(Printf ("set %d, step %d, nosplit %d has no good splitter\n", set, step, nosplit));
		return nosplitters ? -1 : 0;
	}

	D(Printf ("split seg %d in set %d, score %d, step %d, nosplit %d\n", bestseg, set, bestvalue, step, nosplit));

	node.x = Vertices[Segs[bestseg].v1].x;
	node.y = Vertices[Segs[bestseg].v1].y;
	node.dx = Vertices[Segs[bestseg].v2].x - node.x;
	node.dy = Vertices[Segs[bestseg].v2].y - node.y;
	return 1;
}

int FNodeBuilder::Heuristic (node_t &node, int set, bool honorNoSplit)
{
	int score = 0;
	int segsInSet = 0;
	int counts[2] = { 0, 0 };
	int i = set;
	int sidev1, sidev2;
	int side;
	bool splitter = false;
	size_t max, m2, p, q;

	Touched.Clear ();
	Colinear.Clear ();

	while (i != -1)
	{
		const FPrivSeg *test = &Segs[i];

		switch ((side = ClassifyLine (node, test, sidev1, sidev2)))
		{
		case 0:
		case 1:
			// If we don't split this line, but it abuts the splitter, also reject it.
			// The "right" thing to do in this case is to only reject it if there is
			// another nosplit seg from the same sector at this vertex. Note that a line
			// that lies exactly on top of the splitter is okay.
			if (test->loopnum && honorNoSplit && (sidev1 == 0 || sidev2 == 0))
			{
				if ((sidev1 | sidev2) != 0)
				{
					max = Touched.Size();
					for (p = 0; p < max; ++p)
					{
						if (Touched[p] == test->loopnum)
						{
							break;
						}
					}
					if (p == max)
					{
						Touched.Push (test->loopnum);
					}
				}
				else
				{
					max = Colinear.Size();
					for (p = 0; p < max; ++p)
					{
						if (Colinear[p] == test->loopnum)
						{
							break;
						}
					}
					if (p == max)
					{
						Colinear.Push (test->loopnum);
					}
				}
			}

			counts[side]++;
			score++;
			break;

		default:
			// If we are not allowed to split this seg, reject this splitter
			if (test->loopnum)
			{
				if (honorNoSplit)
				{
					D(Printf ("Splits seg %d\n", i));
					return -1;
				}
				else
				{
					splitter = true;
				}
			}

			counts[0]++;
			counts[1]++;
			break;
		}

		segsInSet++;
		i = test->next;
	}

	// If this line is outside all the others, return a special score
	if (counts[0] == 0 || counts[1] == 0)
	{
		return 0;
	}

	// If this splitter intersects any vertices of segs that should not be split,
	// check if it is also colinear with another seg from the same sector. If it
	// is, the splitter is okay. If not, it should be rejected. Why? Assuming that
	// polyobject containers are convex (which they should be), a splitter that
	// is colinear with one of the sector's segs and crosses the vertex of another
	// seg of that sector must be crossing the container's corner and does not
	// actually intersect it.

	max = Touched.Size ();
	m2 = Colinear.Size ();

	// If honorNoSplit is false, then both these list will be empty.

	// If the splitter touches some vertices without being colinear to any, we
	// can skip further checks and reject this right away.
	if (m2 == 0 && max > 0)
	{
		return -1;
	}

	for (p = 0; p < max; ++p)
	{
		int look = Touched[p];
		for (q = 0; q < m2; ++q)
		{
			if (look == Colinear[q])
			{
				break;
			}
		}
		if (q == m2)
		{ // Not a good one
			return -1;
		}
	}

	// Add some weight to the score for unsplit lines
	score = score*8;

	// Doom maps are primarily axis-aligned lines, so it's usually a good
	// idea to prefer axis-aligned splitters over diagonal ones

	if ((node.dx == 0) || (node.dy == 0))
	{
		// If we have to split a seg we would prefer to keep unsplit, give
		// extra precedence to orthogonal lines so that the polyobjects
		// outside the entrance to MAP06 in Hexen MAP02 display properly.
		if (splitter)
		{
			score += segsInSet*8;
		}
		else
		{
			score += segsInSet/16;
		}
	}

	score += (counts[0] + counts[1]) - abs(counts[0] - counts[1]);

	return score;
}

int FNodeBuilder::ClassifyLine (node_t &node, const FPrivSeg *seg, int &sidev1, int &sidev2)
{
	const FPrivVert *v1 = &Vertices[seg->v1];
	const FPrivVert *v2 = &Vertices[seg->v2];
	sidev1 = PointOnSide (v1->x, v1->y, node.x, node.y, node.dx, node.dy);
	sidev2 = PointOnSide (v2->x, v2->y, node.x, node.y, node.dx, node.dy);

	if ((sidev1 | sidev2) == 0)
	{ // seg is coplanar with the splitter, so use its orientation to determine
	  // which child it ends up in. If it faces the same direction as the splitter,
	  // it goes in front. Otherwise, it goes in back.

		if (node.dx != 0)
		{
			if ((node.dx > 0 && v2->x > v1->x) || (node.dx < 0 && v2->x < v1->x))
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
		else
		{
			if ((node.dy > 0 && v2->y > v1->y) || (node.dy < 0 && v2->y < v1->y))
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}
	else if (sidev1 <= 0 && sidev2 <= 0)
	{
		return 0;
	}
	else if (sidev1 >= 0 && sidev2 >= 0)
	{
		return 1;
	}
	return -1;
}

void FNodeBuilder::SplitSegs (int set, node_t &node, int &outset0, int &outset1)
{
	outset0 = -1;
	outset1 = -1;

	while (set != -1)
	{
		FPrivSeg *seg = &Segs[set];
		int next = seg->next;

		int sidev1, sidev2, side;

		side = ClassifyLine (node, seg, sidev1, sidev2);

		switch (side)
		{
		case 0:
			seg->next = outset0;
			outset0 = set;
			break;

		case 1:
			seg->next = outset1;
			outset1 = set;
			break;

		default:
			fixed_t frac;
			FPrivVert newvert;
			FPrivSeg newseg;
			size_t vertnum;
			int seg2;

			if (seg->loopnum)
			{
				Printf ("Split seg %d of sector %d in loop %d\n", set, seg->frontsector-sectors, seg->loopnum);
			}

			frac = InterceptVector (node, *seg);
			newvert.x = Vertices[seg->v1].x;
			newvert.y = Vertices[seg->v1].y;
			newvert.x = newvert.x + MulScale30 (frac, Vertices[seg->v2].x - newvert.x);
			newvert.y = newvert.y + MulScale30 (frac, Vertices[seg->v2].y - newvert.y);
			vertnum = Vertices.Push (newvert);
			newseg = *seg;

			if (sidev1 > 0)
			{
				newseg.v1 = vertnum;
				seg->v2 = vertnum;
			}
			else
			{
				seg->v1 = vertnum;
				newseg.v2 = vertnum;
			}

			seg2 = (int)Segs.Push (newseg);

			Segs[seg2].next = outset0;
			outset0 = seg2;
			Segs[set].next = outset1;
			outset1 = set;
			break;
		}

		set = next;
	}
}

fixed_t FNodeBuilder::InterceptVector (const node_t &splitter, const FPrivSeg &seg)
{
	double v2x = (double)Vertices[seg.v1].x;
	double v2y = (double)Vertices[seg.v1].y;
	double v2dx = (double)Vertices[seg.v2].x - v2x;
	double v2dy = (double)Vertices[seg.v2].y - v2y;
	double v1dx = (double)splitter.dx;
	double v1dy = (double)splitter.dy;

	double den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0.0)
		return 0;		// parallel

	double v1x = (double)splitter.x;
	double v1y = (double)splitter.y;

	double num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	double frac = num / den;

	return (fixed_t)(1073741824.0*frac);
}

inline int FNodeBuilder::PointOnSide (int x, int y, int x1, int y1, int dx, int dy)
{
	int foo = DMulScale32 (y-y1, dx, x1-x, dy);
	return abs(foo) < 4 ? 0 : foo;
}


extern sector_t *sectors;

void FNodeBuilder::PrintSet (int l, int set)
{
	Printf ("set %d: ", l);
	for (; set != -1; set = Segs[set].next)
	{
		Printf ("%d(%d:%ld,%ld-%ld,%ld) ", set, Segs[set].frontsector-sectors,
			Vertices[Segs[set].v1].x>>16, Vertices[Segs[set].v1].y>>16,
			Vertices[Segs[set].v2].x>>16, Vertices[Segs[set].v2].y>>16);
	}
	Printf ("*\n");
}

#include "z_zone.h"

void FNodeBuilder::Create (node_t *&nodes, int &numnodes,
	seg_t *&segs, int &numsegs,
	subsector_t *&subsectors, int &numsubsectors,
	vertex_t *&vertices, int &numvertices)
{
	int i;

	numnodes = (int)Nodes.Size();
	nodes = (node_t *)Z_Malloc (numnodes*sizeof(node_t), PU_LEVEL, 0);
	numsegs = (int)SegList.Size();
	segs = (seg_t *)Z_Malloc (numsegs*sizeof(seg_t), PU_LEVEL, 0);
	numsubsectors = (int)Subsectors.Size();
	subsectors = (subsector_t *)Z_Malloc (numsubsectors*sizeof(subsector_t), PU_LEVEL, 0);
	numvertices = (int)Vertices.Size();
	vertices = (vertex_t *)Z_Malloc (numvertices*sizeof(vertex_t), PU_LEVEL, 0);

	memcpy (nodes, &Nodes[0], numnodes*sizeof(node_t));
	memcpy (subsectors, &Subsectors[0], numsubsectors*sizeof(subsector_t));

	for (i = 0; i < numvertices; ++i)
	{
		vertices[i].x = Vertices[i].x;
		vertices[i].y = Vertices[i].y;
	}

	for (i = 0; i < numsegs; ++i)
	{
		FPrivSeg *org = &Segs[SegList[i].SegNum];
		seg_t *out = &segs[i];

		out->backsector = org->backsector;
		out->frontsector = org->frontsector;
		out->linedef = org->linedef;
		out->sidedef = org->sidedef;
		out->v1 = vertices + org->v1;
		out->v2 = vertices + org->v2;
	}

	for (i = 0; i < NumLines; ++i)
	{
		Lines[i].v1 = vertices + (size_t)Lines[i].v1;
		Lines[i].v2 = vertices + (size_t)Lines[i].v2;
	}
}
