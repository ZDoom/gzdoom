#ifndef __QDRAWER_H
#define __QDRAWER_H

#include "gl/data/gl_vertexbuffer.h"

class FQuadDrawer
{
	FFlatVertex *p;
	int ndx;
	static FFlatVertex buffer[4];
	
	void DoRender(int type);
public:

	FQuadDrawer()
	{
		if (gl.flags & RFL_QUADHACK)
		{
			p = buffer;
		}
		else
		{
			p = GLRenderer->mVBO->Alloc(4, &ndx);
		}
	}
	void Set(int ndx, float x, float y, float z, float s, float t)
	{
		p[ndx].Set(x, y, z, s, t);
	}
	void Render(int type)
	{
		if (gl.flags & RFL_QUADHACK)
		{
			DoRender(type);
		}
		else
		{
			GLRenderer->mVBO->RenderArray(type, ndx, 4);
		}
	}
};


#endif
