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
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_templates.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// Split upper edge of wall
//
//==========================================================================

void GLWall::SplitUpperEdge(FFlatVertex *&ptr)
{
	if (seg == NULL || seg->sidedef == NULL || (seg->sidedef->Flags & WALLF_POLYOBJ) || seg->sidedef->numsegs == 1) return;

	side_t *sidedef = seg->sidedef;
	float polyw = glseg.fracright - glseg.fracleft;
	float facu = (tcs[UPRGT].u - tcs[UPLFT].u) / polyw;
	float facv = (tcs[UPRGT].v - tcs[UPLFT].v) / polyw;
	float fact = (ztop[1] - ztop[0]) / polyw;
	float facc = (zceil[1] - zceil[0]) / polyw;
	float facf = (zfloor[1] - zfloor[0]) / polyw;

	for (int i = 0; i < sidedef->numsegs - 1; i++)
	{
		seg_t *cseg = sidedef->segs[i];
		float sidefrac = cseg->sidefrac;
		if (sidefrac <= glseg.fracleft) continue;
		if (sidefrac >= glseg.fracright) return;

		float fracfac = sidefrac - glseg.fracleft;

		ptr->x = cseg->v2->fX();
		ptr->y = cseg->v2->fY();
		ptr->z = ztop[0] + fact * fracfac;
		ptr->u = tcs[UPLFT].u + facu * fracfac;
		ptr->v = tcs[UPLFT].v + facv * fracfac;
		ptr++;
	}
}

//==========================================================================
//
// Split upper edge of wall
//
//==========================================================================

void GLWall::SplitLowerEdge(FFlatVertex *&ptr)
{
	if (seg == NULL || seg->sidedef == NULL || (seg->sidedef->Flags & WALLF_POLYOBJ) || seg->sidedef->numsegs == 1) return;

	side_t *sidedef = seg->sidedef;
	float polyw = glseg.fracright - glseg.fracleft;
	float facu = (tcs[LORGT].u - tcs[LOLFT].u) / polyw;
	float facv = (tcs[LORGT].v - tcs[LOLFT].v) / polyw;
	float facb = (zbottom[1] - zbottom[0]) / polyw;
	float facc = (zceil[1] - zceil[0]) / polyw;
	float facf = (zfloor[1] - zfloor[0]) / polyw;

	for (int i = sidedef->numsegs - 2; i >= 0; i--)
	{
		seg_t *cseg = sidedef->segs[i];
		float sidefrac = cseg->sidefrac;
		if (sidefrac >= glseg.fracright) continue;
		if (sidefrac <= glseg.fracleft) return;

		float fracfac = sidefrac - glseg.fracleft;

		ptr->x = cseg->v2->fX();
		ptr->y = cseg->v2->fY();
		ptr->z = zbottom[0] + facb * fracfac;
		ptr->u = tcs[LOLFT].u + facu * fracfac;
		ptr->v = tcs[LOLFT].v + facv * fracfac;
		ptr++;
	}
}

//==========================================================================
//
// Split left edge of wall
//
//==========================================================================

void GLWall::SplitLeftEdge(FFlatVertex *&ptr)
{
	if (vertexes[0] == NULL) return;

	vertex_t * vi = vertexes[0];

	if (vi->numheights)
	{
		int i = 0;

		float polyh1 = ztop[0] - zbottom[0];
		float factv1 = polyh1 ? (tcs[UPLFT].v - tcs[LOLFT].v) / polyh1 : 0;
		float factu1 = polyh1 ? (tcs[UPLFT].u - tcs[LOLFT].u) / polyh1 : 0;

		while (i<vi->numheights && vi->heightlist[i] <= zbottom[0]) i++;
		while (i<vi->numheights && vi->heightlist[i] < ztop[0])
		{
			ptr->x = glseg.x1;
			ptr->y = glseg.y1;
			ptr->z = vi->heightlist[i];
			ptr->u = factu1*(vi->heightlist[i] - ztop[0]) + tcs[UPLFT].u;
			ptr->v = factv1*(vi->heightlist[i] - ztop[0]) + tcs[UPLFT].v;
			ptr++;
			i++;
		}
	}
}

//==========================================================================
//
// Split right edge of wall
//
//==========================================================================

void GLWall::SplitRightEdge(FFlatVertex *&ptr)
{
	if (vertexes[1] == NULL) return;

	vertex_t * vi = vertexes[1];

	if (vi->numheights)
	{
		int i = vi->numheights - 1;

		float polyh2 = ztop[1] - zbottom[1];
		float factv2 = polyh2 ? (tcs[UPRGT].v - tcs[LORGT].v) / polyh2 : 0;
		float factu2 = polyh2 ? (tcs[UPRGT].u - tcs[LORGT].u) / polyh2 : 0;

		while (i>0 && vi->heightlist[i] >= ztop[1]) i--;
		while (i>0 && vi->heightlist[i] > zbottom[1])
		{
			ptr->x = glseg.x2;
			ptr->y = glseg.y2;
			ptr->z = vi->heightlist[i];
			ptr->u = factu2*(vi->heightlist[i] - ztop[1]) + tcs[UPRGT].u;
			ptr->v = factv2*(vi->heightlist[i] - ztop[1]) + tcs[UPRGT].v;
			ptr++;
			i--;
		}
	}
}

