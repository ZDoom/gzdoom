/*
** gl_vertex.cpp
**
**---------------------------------------------------------------------------
** Copyright 2006 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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



#include "gl/system/gl_system.h"
#include "gl/gl_functions.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_templates.h"

EXTERN_CVAR(Bool, gl_seamless)
extern int vertexcount;

//==========================================================================
//
// Split upper edge of wall
//
//==========================================================================

void GLWall::SplitUpperEdge(texcoord * tcs, bool glow)
{
	if (seg == NULL || seg->sidedef == NULL || (seg->sidedef->Flags & WALLF_POLYOBJ) || seg->sidedef->numsegs == 1) return;

	side_t *sidedef = seg->sidedef;
	float polyw = glseg.fracright - glseg.fracleft;
	float facu = (tcs[2].u - tcs[1].u) / polyw;
	float facv = (tcs[2].v - tcs[1].v) / polyw;
	float fact = (ztop[1] - ztop[0]) / polyw;
	float facc = (zceil[1] - zceil[0]) / polyw;
	float facf = (zfloor[1] - zfloor[0]) / polyw;

	for (int i=0; i < sidedef->numsegs - 1; i++)
	{
		seg_t *cseg = sidedef->segs[i];
		float sidefrac = cseg->sidefrac;
		if (sidefrac <= glseg.fracleft) continue;
		if (sidefrac >= glseg.fracright) return;

		float fracfac = sidefrac - glseg.fracleft;

		if (glow) glVertexAttrib2f(VATTR_GLOWDISTANCE, zceil[0] - ztop[0] + (facc - fact) * fracfac, 
									 ztop[0] - zfloor[0] + (fact - facf) * fracfac);

		glTexCoord2f(tcs[1].u + facu * fracfac, tcs[1].v + facv * fracfac);
		glVertex3f(cseg->v2->fx, ztop[0] + fact * fracfac, cseg->v2->fy);
	}
	vertexcount += sidedef->numsegs-1;
}

//==========================================================================
//
// Split upper edge of wall
//
//==========================================================================

void GLWall::SplitLowerEdge(texcoord * tcs, bool glow)
{
	if (seg == NULL || seg->sidedef == NULL || (seg->sidedef->Flags & WALLF_POLYOBJ) || seg->sidedef->numsegs == 1) return;

	side_t *sidedef = seg->sidedef;
	float polyw = glseg.fracright - glseg.fracleft;
	float facu = (tcs[3].u - tcs[0].u) / polyw;
	float facv = (tcs[3].v - tcs[0].v) / polyw;
	float facb = (zbottom[1] - zbottom[0]) / polyw;
	float facc = (zceil[1] - zceil[0]) / polyw;
	float facf = (zfloor[1] - zfloor[0]) / polyw;

	for (int i = sidedef->numsegs-2; i >= 0; i--)
	{
		seg_t *cseg = sidedef->segs[i];
		float sidefrac = cseg->sidefrac;
		if (sidefrac >= glseg.fracright) continue;
		if (sidefrac <= glseg.fracleft) return;

		float fracfac = sidefrac - glseg.fracleft;

		if (glow) glVertexAttrib2f(VATTR_GLOWDISTANCE, zceil[0] - zbottom[0] + (facc - facb) * fracfac, 
									 zbottom[0] - zfloor[0] + (facb - facf) * fracfac);

		glTexCoord2f(tcs[0].u + facu * fracfac, tcs[0].v + facv * fracfac);
		glVertex3f(cseg->v2->fx, zbottom[0] + facb * fracfac, cseg->v2->fy);
	}
	vertexcount += sidedef->numsegs-1;
}

//==========================================================================
//
// Split left edge of wall
//
//==========================================================================

void GLWall::SplitLeftEdge(texcoord * tcs, bool glow)
{
	if (vertexes[0]==NULL) return;

	vertex_t * vi=vertexes[0];

	if (vi->numheights)
	{
		int i=0;

		float polyh1=ztop[0] - zbottom[0];
		float factv1=polyh1? (tcs[1].v - tcs[0].v) / polyh1:0;
		float factu1=polyh1? (tcs[1].u - tcs[0].u) / polyh1:0;

		while (i<vi->numheights && vi->heightlist[i] <= zbottom[0] ) i++;
		while (i<vi->numheights && vi->heightlist[i] < ztop[0])
		{
			if (glow) glVertexAttrib2f(VATTR_GLOWDISTANCE, zceil[0] - vi->heightlist[i], vi->heightlist[i] - zfloor[0]);
			glTexCoord2f(factu1*(vi->heightlist[i] - ztop[0]) + tcs[1].u,
						 factv1*(vi->heightlist[i] - ztop[0]) + tcs[1].v);
			glVertex3f(glseg.x1, vi->heightlist[i], glseg.y1);
			i++;
		}
		vertexcount+=i;
	}
}

//==========================================================================
//
// Split right edge of wall
//
//==========================================================================

void GLWall::SplitRightEdge(texcoord * tcs, bool glow)
{
	if (vertexes[1]==NULL) return;

	vertex_t * vi=vertexes[1];

	if (vi->numheights)
	{
		int i=vi->numheights-1;

		float polyh2 = ztop[1] - zbottom[1];
		float factv2 = polyh2? (tcs[2].v - tcs[3].v) / polyh2:0;
		float factu2 = polyh2? (tcs[2].u - tcs[3].u) / polyh2:0;

		while (i>0 && vi->heightlist[i] >= ztop[1]) i--;
		while (i>0 && vi->heightlist[i] > zbottom[1])
		{
			if (glow) glVertexAttrib2f(VATTR_GLOWDISTANCE, zceil[1] - vi->heightlist[i], vi->heightlist[i] - zfloor[1]);
			glTexCoord2f(factu2 * (vi->heightlist[i] - ztop[1]) + tcs[2].u,
						 factv2 * (vi->heightlist[i] - ztop[1]) + tcs[2].v);
			glVertex3f(glseg.x2, vi->heightlist[i], glseg.y2);
			i--;
		}
		vertexcount+=i;
	}
}

