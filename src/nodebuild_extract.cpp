#include <string.h>

#include "nodebuild.h"
#include "templates.h"
#include "r_main.h"

void FNodeBuilder::Extract (node_t *&outNodes, int &nodeCount,
	seg_t *&outSegs, int &segCount,
	subsector_t *&outSubs, int &subCount,
	vertex_t *&outVerts, int &vertCount)
{
	int i;

	vertCount = Vertices.Size ();
	outVerts = new vertex_t[vertCount];

	for (i = 0; i < vertCount; ++i)
	{
		outVerts[i].x = Vertices[i].x;
		outVerts[i].y = Vertices[i].y;
	}

	subCount = Subsectors.Size();
	outSubs = new subsector_t[subCount];

	nodeCount = Nodes.Size ();
	outNodes = new node_t[nodeCount];

	memcpy (outNodes, &Nodes[0], nodeCount*sizeof(node_t));
	for (i = 0; i < nodeCount; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			if (outNodes[i].intchildren[j] & 0x80000000)
			{
				outNodes[i].children[j] = (BYTE *)(outSubs + (outNodes[i].intchildren[j] & 0x7fffffff)) + 1;
			}
			else
			{
				outNodes[i].children[j] = outNodes + outNodes[i].intchildren[j];
			}
		}
	}

	if (GLNodes)
	{
		TArray<seg_t> segs (Segs.Size()*5/4);

		for (i = 0; i < subCount; ++i)
		{
			int numsegs = CloseSubsector (segs, i, outVerts);
			outSubs[i].numlines = numsegs;
			outSubs[i].firstline = segs.Size() - numsegs;
			outSubs[i].poly = NULL;
		}

		segCount = segs.Size ();
		outSegs = new seg_t[segCount];
		memcpy (outSegs, &segs[0], segCount*sizeof(seg_t));

		for (i = 0; i < segCount; ++i)
		{
			if (outSegs[i].PartnerSeg != NULL)
			{
				outSegs[i].PartnerSeg = &outSegs[Segs[(size_t)outSegs[i].PartnerSeg-1].storedseg];
			}
		}
	}
	else
	{
		memcpy (outSubs, &Subsectors[0], subCount*sizeof(subsector_t));
		segCount = Segs.Size ();
		outSegs = new seg_t[segCount];
		for (i = 0; i < segCount; ++i)
		{
			const FPrivSeg *org = &Segs[SegList[i].SegNum];
			seg_t *out = &outSegs[i];

			out->v1 = outVerts + org->v1;
			out->v2 = outVerts + org->v2;
			out->backsector = org->backsector;
			out->frontsector = org->frontsector;
			out->linedef = Level.Lines + org->linedef;
			out->sidedef = Level.Sides + org->sidedef;
			out->PartnerSeg = NULL;
			out->bPolySeg = false;
		}
	}

	//Printf ("%i segs, %i nodes, %i subsectors\n", segCount, nodeCount, subCount);

	for (i = 0; i < Level.NumLines; ++i)
	{
		Level.Lines[i].v1 = outVerts + (size_t)Level.Lines[i].v1;
		Level.Lines[i].v2 = outVerts + (size_t)Level.Lines[i].v2;
	}
}

int FNodeBuilder::CloseSubsector (TArray<seg_t> &segs, int subsector, vertex_t *outVerts)
{
	FPrivSeg *seg, *prev;
	angle_t prevAngle;
	double accumx, accumy;
	fixed_t midx, midy;
	int i, j, first, max, count, firstVert;

	first = Subsectors[subsector].firstline;
	max = first + Subsectors[subsector].numlines;
	count = 0;

	accumx = accumy = 0.0;

	for (i = first; i < max; ++i)
	{
		seg = &Segs[SegList[i].SegNum];
		accumx += double(Vertices[seg->v1].x) + double(Vertices[seg->v2].x);
		accumy += double(Vertices[seg->v1].y) + double(Vertices[seg->v2].y);
	}

	midx = fixed_t(accumx / (max - first) / 2);
	midy = fixed_t(accumy / (max - first) / 2);

	seg = &Segs[SegList[first].SegNum];
	prevAngle = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
	seg->storedseg = PushGLSeg (segs, seg, outVerts);
	count = 1;
	prev = seg;
	firstVert = seg->v1;

	for (i = first + 1; i < max; ++i)
	{
		angle_t bestdiff = ANGLE_MAX;
		FPrivSeg *bestseg;
		int bestj = -1;
		for (j = first; j < max; ++j)
		{
			seg = &Segs[SegList[j].SegNum];
			angle_t ang = PointToAngle (Vertices[seg->v1].x - midx, Vertices[seg->v1].y - midy);
			angle_t diff = prevAngle - ang;
			if (seg->v1 == prev->v2)
			{
				bestdiff = diff;
				bestseg = seg;
				bestj = j;
				break;
			}
			if (diff < bestdiff && diff > 0)
			{
				bestdiff = diff;
				bestseg = seg;
				bestj = j;
			}
		}
		seg = bestseg;
		if (prev->v2 != seg->v1)
		{
			// Add a new miniseg to connect the two segs
			PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[seg->v1]);
			count++;
		}

		prevAngle -= bestdiff;
		seg->storedseg = PushGLSeg (segs, seg, outVerts);
		count++;
		prev = seg;
		if (seg->v2 == firstVert)
		{
			prev = seg;
			break;
		}
	}

	if (prev->v2 != firstVert)
	{
		PushConnectingGLSeg (subsector, segs, &outVerts[prev->v2], &outVerts[firstVert]);
		count++;
	}

	return count;
}

DWORD FNodeBuilder::PushGLSeg (TArray<seg_t> &segs, const FPrivSeg *seg, vertex_t *outVerts)
{
	seg_t newseg;

	newseg.v1 = outVerts + seg->v1;
	newseg.v2 = outVerts + seg->v2;
	newseg.backsector = seg->backsector;
	newseg.frontsector = seg->frontsector;
	if (seg->linedef != -1)
	{
		newseg.linedef = Level.Lines + seg->linedef;
		newseg.sidedef = Level.Sides + seg->sidedef;
	}
	else
	{
		newseg.linedef = NULL;
		newseg.sidedef = NULL;
	}
	newseg.PartnerSeg = (seg_t *)(seg->partner == DWORD_MAX ? NULL : seg->partner + 1);
	newseg.bPolySeg = false;
	return (DWORD)segs.Push (newseg);
}

void FNodeBuilder::PushConnectingGLSeg (int subsector, TArray<seg_t> &segs, vertex_t *v1, vertex_t *v2)
{
	seg_t newseg;

	newseg.v1 = v1;
	newseg.v2 = v2;
	newseg.backsector = NULL;
	newseg.frontsector = NULL;
	newseg.linedef = NULL;
	newseg.sidedef = NULL;
	newseg.PartnerSeg = NULL;
	newseg.bPolySeg = false;
	segs.Push (newseg);
}
