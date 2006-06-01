#include "doomdata.h"
#include "tarray.h"
#include "r_defs.h"

struct FEventInfo
{
	int Vertex;
	DWORD FrontSeg;
};

struct FEvent
{
	FEvent *Parent, *Left, *Right;
	enum { RED, BLACK } Color;
	double Distance;
	FEventInfo Info;
};

class FEventTree
{
public:
	FEventTree ();
	~FEventTree ();

	FEvent *GetMinimum ();
	FEvent *GetSuccessor (FEvent *event) const { FEvent *node = Successor(event); return node == &Nil ? NULL : node; }
	FEvent *GetPredecessor (FEvent *event) const { FEvent *node = Predecessor(event); return node == &Nil ? NULL : node; }

	FEvent *GetNewNode ();
	void Insert (FEvent *event);
	void Delete (FEvent *event);
	FEvent *FindEvent (double distance) const;
	void DeleteAll ();

private:
	FEvent Nil;
	FEvent *Root;
	FEvent *Spare;

	void LeftRotate (FEvent *event);
	void RightRotate (FEvent *event);
	void DeleteFixUp (FEvent *event);
	void DeletionTraverser (FEvent *event);
	FEvent *Successor (FEvent *event) const;
	FEvent *Predecessor (FEvent *event) const;
};

class FNodeBuilder
{
	struct FPrivSeg
	{
		int v1, v2;
		int sidedef;
		int linedef;
		sector_t *frontsector;
		sector_t *backsector;
		DWORD next;
		DWORD nextforvert;
		DWORD nextforvert2;
		int loopnum;		// loop number for split avoidance (0 means splitting is okay)
		DWORD partner;		// seg on back side
		DWORD storedseg;	// seg # in the GL_SEGS lump

		int planenum;
		bool planefront;
		FPrivSeg *hashnext;
	};
	struct FPrivVert
	{
		fixed_t x, y;
		DWORD segs;		// segs that use this vertex as v1
		DWORD segs2;	// segs that use this vertex as v2

		bool operator== (const FPrivVert &other)
		{
			return x == other.x && y == other.y;
		}
	};
	struct FSimpleLine
	{
		fixed_t x, y, dx, dy;
	};
	union USegPtr
	{
		DWORD SegNum;
		FPrivSeg *SegPtr;
	};
	struct FSplitSharer
	{
		double Distance;
		DWORD Seg;
		bool Forward;
	};
public:
	struct FLevel
	{
		vertex_t *Vertices;		int NumVertices;
		side_t *Sides;			int NumSides;
		line_t *Lines;			int NumLines;
	};

	struct FPolyStart
	{
		int polynum;
		fixed_t x, y;
	};

	FNodeBuilder (FLevel &level,
		TArray<FPolyStart> &polyspots, TArray<FPolyStart> &anchors,
		bool makeGLNodes);

	void Extract (node_t *&nodes, int &nodeCount,
		seg_t *&segs, int &segCount,
		subsector_t *&ssecs, int &subCount,
		vertex_t *&verts, int &vertCount);

	static angle_t PointToAngle (fixed_t dx, fixed_t dy);

private:
	TArray<node_t> Nodes;
	TArray<subsector_t> Subsectors;
	TArray<DWORD> SubsectorSets;
	TArray<FPrivSeg> Segs;
	TArray<FPrivVert> Vertices;
	TArray<USegPtr> SegList;
	TArray<BYTE> PlaneChecked;
	TArray<FSimpleLine> Planes;

	TArray<int> Touched;	// Loops a splitter touches on a vertex
	TArray<int> Colinear;	// Loops with edges colinear to a splitter
	FEventTree Events;		// Vertices intersected by the current splitter

	TArray<FSplitSharer> SplitSharers;	// Segs collinear with the current splitter

	DWORD HackSeg;			// Seg to force to back of splitter
	DWORD HackMate;			// Seg to use in front of hack seg
	FLevel &Level;
	bool GLNodes;			// Add minisegs to make GL nodes?

	// Progress meter stuff
	int SegsStuffed;

	void FindUsedVertices (vertex_t *vertices, int max);
	int SelectVertexExact (FPrivVert &vertex);
	void BuildTree ();
	void MakeSegsFromSides ();
	int CreateSeg (int linenum, int sidenum);
	void GroupSegPlanes ();
	void FindPolyContainers (TArray<FPolyStart> &spots, TArray<FPolyStart> &anchors);
	bool GetPolyExtents (int polynum, fixed_t bbox[4]);
	int MarkLoop (DWORD firstseg, int loopnum);
	void AddSegToBBox (fixed_t bbox[4], const FPrivSeg *seg);
	int CreateNode (DWORD set, fixed_t bbox[4]);
	int CreateSubsector (DWORD set, fixed_t bbox[4]);
	void CreateSubsectorsForReal ();
	bool CheckSubsector (DWORD set, node_t &node, DWORD &splitseg, int setsize);
	bool CheckSubsectorOverlappingSegs (DWORD set, node_t &node, DWORD &splitseg);
	bool ShoveSegBehind (DWORD set, node_t &node, DWORD seg, DWORD mate);	int SelectSplitter (DWORD set, node_t &node, DWORD &splitseg, int step, bool nosplit);
	void SplitSegs (DWORD set, node_t &node, DWORD splitseg, DWORD &outset0, DWORD &outset1);
	DWORD SplitSeg (DWORD segnum, int splitvert, int v1InFront);
	int Heuristic (node_t &node, DWORD set, bool honorNoSplit);
	int ClassifyLine (node_t &node, const FPrivSeg *seg, int &sidev1, int &sidev2);
	int CountSegs (DWORD set) const;

	void FixSplitSharers (const node_t &node);
	double AddIntersection (const node_t &node, int vertex);
	void AddMinisegs (const node_t &node, DWORD splitseg, DWORD &fset, DWORD &rset);
	DWORD CheckLoopStart (fixed_t dx, fixed_t dy, int vertex1, int vertex2);
	DWORD CheckLoopEnd (fixed_t dx, fixed_t dy, int vertex1, int vertex2);
	void RemoveSegFromVert1 (DWORD segnum, int vertnum);
	void RemoveSegFromVert2 (DWORD segnum, int vertnum);
	DWORD AddMiniseg (int v1, int v2, DWORD partner, DWORD seg1, DWORD splitseg);
	void SetNodeFromSeg (node_t &node, const FPrivSeg *pseg) const;

	int CloseSubsector (TArray<seg_t> &segs, int subsector, vertex_t *outVerts);
	DWORD PushGLSeg (TArray<seg_t> &segs, const FPrivSeg *seg, vertex_t *outVerts);
	void PushConnectingGLSeg (int subsector, TArray<seg_t> &segs, vertex_t *v1, vertex_t *v2);
	int OutputDegenerateSubsector (TArray<seg_t> &segs, int subsector, bool bForward, double lastdot, FPrivSeg *&prev, vertex_t *outVerts);

	static int STACK_ARGS SortSegs (const void *a, const void *b);

	//  < 0 : in front of line
	// == 0 : on line
	//  > 0 : behind line

	int PointOnSide (int x, int y, int x1, int y1, int dx, int dy);
	double InterceptVector (const node_t &splitter, const FPrivSeg &seg);

	void PrintSet (int l, DWORD set);
};
