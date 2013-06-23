
#ifndef __GL_SECTIONS_H
#define __GL_SECTIONS_H

#include "tarray.h"
#include "r_defs.h"

struct FGLSectionLine
{
	vertex_t *start;
	vertex_t *end;
	side_t *sidedef;
	line_t *linedef;
	seg_t *refseg;			// we need to reference at least one seg for each line.
	subsector_t *polysub;	// If this is part of a polyobject we need a reference to the containing subsector
	int otherside;
};

struct FGLSectionLoop
{
	int startline;
	int numlines;

	FGLSectionLine *GetLine(int no);
};

struct FGLSection
{
	sector_t *sector;
	TArray<subsector_t *> subsectors;
	TArray<int> vertices;
	int startloop;
	int numloops;
	int validcount;

	FGLSectionLoop *GetLoop(int no);
};

extern TArray<FGLSectionLine> SectionLines;
extern TArray<FGLSectionLoop> SectionLoops;
extern TArray<FGLSection> Sections;
extern TArray<int> SectionForSubsector;

inline FGLSectionLine *FGLSectionLoop::GetLine(int no)
{
	return &SectionLines[startline + no];
}

inline FGLSectionLoop *FGLSection::GetLoop(int no)
{
	return &SectionLoops[startloop + no];
}

void gl_CreateSections();

#endif