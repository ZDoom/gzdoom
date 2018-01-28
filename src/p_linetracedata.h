#ifndef P_LTRACEDATA_H
#define P_LTRACEDATA_H

//============================================================================
//
// Structure for passing detailed results of LineTrace to ZScript
//
//============================================================================
struct FLineTraceData
{
	AActor *HitActor;
	line_t *HitLine;
	sector_t *HitSector;
	F3DFloor *Hit3DFloor;
	FTextureID HitTexture;
	DVector3 HitLocation;
	DVector3 HitDir;
	double Distance;
	int NumPortals;
	int LineSide;
	int LinePart;
	int SectorPlane;
	ETraceResult HitType;
};

#endif
