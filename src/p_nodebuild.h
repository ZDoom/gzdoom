#include "r_defs.h"

class FNodeBuilder
{
	struct FPrivSeg
	{
		size_t v1, v2;
		side_t *sidedef;
		line_t *linedef;
		sector_t *frontsector;
		sector_t *backsector;
		int next;
		int nextforvert;
		int loopnum;	// loop number for split avoidance (0 means splitting is okay)

		fixed_t plane[3];	// a=2.30, b=2.30, c=17.15
		int planenum;
	};
	struct FPrivVert
	{
		fixed_t x, y;
		WORD segs;		// segs that use this vertex as v1

		bool operator== (const FPrivVert &other)
		{
			return x == other.x && y == other.y;
		}
	};
	union USegPtr
	{
		int SegNum;
		FPrivSeg *SegPtr;
	};
public:
	struct FPolyStart
	{
		int polynum;
		fixed_t x, y;
	};

	FNodeBuilder (line_t *lines, int numlines,
		vertex_t *vertices, int numvertices,
		side_t *sides, int numsides,
		TArray<FPolyStart> &polyspots, TArray<FPolyStart> &anchors);

	void Create (node_t *&nodes, int &numnodes,
		seg_t *&segs, int &numsegs,
		subsector_t *&subsector, int &numsubsectors,
		vertex_t *&vertices, int &numvertices);

private:
	TArray<node_t> Nodes;
	TArray<subsector_t> Subsectors;
	TArray<FPrivSeg> Segs;
	TArray<FPrivVert> Vertices;
	TArray<USegPtr> SegList;
	TArray<BYTE> PlaneChecked;

	TArray<int> Touched;	// Loops a splitter touches on a vertex
	TArray<int> Colinear;	// Loops with edges colinear to a splitter 

	line_t *Lines;
	int NumLines;
	side_t *Sides;
	int NumSides;

	// Progress meter stuff
	int SegsStuffed;
	int SubsecCount;

	void FindUsedVertices (vertex_t *vertices, int max);
	void BuildTree ();
	void MakeSegsFromSides ();
	void GroupSegPlanes ();
	void FindPolyContainers (TArray<FPolyStart> &spots, TArray<FPolyStart> &anchors);
	bool GetPolyExtents (int polynum, fixed_t bbox[4]);
	int MarkLoop (int firstseg, int loopnum);
	void AddSegToBBox (fixed_t bbox[4], const FPrivSeg *seg);
	int CreateNode (int set, fixed_t bbox[4]);
	int CreateSubsector (int set, fixed_t bbox[4]);
	bool CheckSubsector (int set, node_t &node, int setsize);
	int SelectSplitter (int set, node_t &node, int step, bool nosplit);
	void SplitSegs (int set, node_t &node, int &outset0, int &outset1);
	int Heuristic (node_t &node, int set, bool honorNoSplit);
	int ClassifyLine (node_t &node, const FPrivSeg *seg, int &sidev1, int &sidev2);
	int CountSegs (int set) const;

	static int STACK_ARGS SortSegs (const void *a, const void *b);
	static int STACK_ARGS SortPlanes (const void *a, const void *b);

	//  < 0 : in front of line
	// == 0 : on line
	//  > 0 : behind line

	inline int PointOnSide (int x, int y, int x1, int y1, int dx, int dy);
	fixed_t InterceptVector (const node_t &splitter, const FPrivSeg &seg);

	void PrintSet (int l, int set);
};
