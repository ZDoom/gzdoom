// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_drawinfo.cpp
** Implements the draw info structure which contains most of the
** data in a scene and the draw lists - including a very thorough BSP 
** style sorting algorithm for translucent objects.
**
*/

#include "gl/system/gl_system.h"
#include "r_sky.h"
#include "r_utility.h"
#include "doomstat.h"
#include "g_levellocals.h"

#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/stereo3d/scoped_color_mask.h"
#include "gl/renderer/gl_quaddrawer.h"

FDrawInfo * gl_drawinfo;
FDrawInfoList di_list;

static FMemArena RenderDataAllocator(1024*1024);	// Use large blocks to reduce allocation time.

void ResetAllocator()
{
	RenderDataAllocator.FreeAll();
}

//==========================================================================
//
//
//
//==========================================================================
class StaticSortNodeArray : public TDeletingArray<SortNode*>
{
	unsigned usecount;
public:
	unsigned Size() { return usecount; }
	void Clear() { usecount=0; }
	void Release(int start) { usecount=start; }
	SortNode * GetNew();
};


SortNode * StaticSortNodeArray::GetNew()
{
	if (usecount==TArray<SortNode*>::Size())
	{
		Push(new SortNode);
	}
	return operator[](usecount++);
}


static StaticSortNodeArray SortNodes;

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::Reset()
{
	if (sorted) SortNodes.Release(SortNodeStart);
	sorted=NULL;
	walls.Clear();
	flats.Clear();
	sprites.Clear();
	drawitems.Clear();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Translucent polygon sorting - uses a BSP algorithm with an additional 'equal' branch

inline double GLSprite::CalcIntersectionVertex(GLWall * w2)
{
	float ax = x1, ay=y1;
	float bx = x2, by=y2;
	float cx = w2->glseg.x1, cy=w2->glseg.y1;
	float dx = w2->glseg.x2, dy=w2->glseg.y2;
	return ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy)) / ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
}



//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::UnlinkFromChain()
{
	if (parent) parent->next=next;
	if (next) next->parent=parent;
	parent=next=NULL;
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::Link(SortNode * hook)
{
	if (hook)
	{
		parent=hook->parent;
		hook->parent=this;
	}
	next=hook;
	if (parent) parent->next=this;
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::AddToEqual(SortNode *child)
{
	child->UnlinkFromChain();
	child->equal=equal;
	equal=child;			
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::AddToLeft(SortNode * child)
{
	child->UnlinkFromChain();
	child->Link(left);
	left=child;
}

//==========================================================================
//
//
//
//==========================================================================
inline void SortNode::AddToRight(SortNode * child)
{
	child->UnlinkFromChain();
	child->Link(right);
	right=child;
}


//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::MakeSortList()
{
	SortNode * p, * n, * c;
	unsigned i;

	SortNodeStart=SortNodes.Size();
	p=NULL;
	n=SortNodes.GetNew();
	for(i=0;i<drawitems.Size();i++)
	{
		n->itemindex=(int)i;
		n->left=n->equal=n->right=NULL;
		n->parent=p;
		p=n;
		if (i!=drawitems.Size()-1)
		{
			c=SortNodes.GetNew();
			n->next=c;
			n=c;
		}
		else
		{
			n->next=NULL;
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::FindSortPlane(SortNode * head)
{
	while (head->next && drawitems[head->itemindex].rendertype!=GLDIT_FLAT) 
		head=head->next;
	if (drawitems[head->itemindex].rendertype==GLDIT_FLAT) return head;
	return NULL;
}


//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::FindSortWall(SortNode * head)
{
	float farthest = -FLT_MAX;
	float nearest = FLT_MAX;
	SortNode * best = NULL;
	SortNode * node = head;
	float bestdist = FLT_MAX;

	while (node)
	{
		GLDrawItem * it = &drawitems[node->itemindex];
		if (it->rendertype == GLDIT_WALL)
		{
			float d = walls[it->index]->ViewDistance;
			if (d > farthest) farthest = d;
			if (d < nearest) nearest = d;
		}
		node = node->next;
	}
	if (farthest == INT_MIN) return NULL;
	node = head;
	farthest = (farthest + nearest) / 2;
	while (node)
	{
		GLDrawItem * it = &drawitems[node->itemindex];
		if (it->rendertype == GLDIT_WALL)
		{
			float di = fabsf(walls[it->index]->ViewDistance - farthest);
			if (!best || di < bestdist)
			{
				best = node;
				bestdist = di;
			}
		}
		node = node->next;
	}
	return best;
}

//==========================================================================
//
// Note: sloped planes are a huge problem...
//
//==========================================================================
void GLDrawList::SortPlaneIntoPlane(SortNode * head,SortNode * sort)
{
	GLFlat * fh= flats[drawitems[head->itemindex].index];
	GLFlat * fs= flats[drawitems[sort->itemindex].index];

	if (fh->z==fs->z) 
		head->AddToEqual(sort);
	else if ( (fh->z<fs->z && fh->ceiling) || (fh->z>fs->z && !fh->ceiling)) 
		head->AddToLeft(sort);
	else 
		head->AddToRight(sort);
}


//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::SortWallIntoPlane(SortNode * head, SortNode * sort)
{
	GLFlat * fh = flats[drawitems[head->itemindex].index];
	GLWall * ws = walls[drawitems[sort->itemindex].index];

	bool ceiling = fh->z > r_viewpoint.Pos.Z;

	if ((ws->ztop[0] > fh->z || ws->ztop[1] > fh->z) && (ws->zbottom[0] < fh->z || ws->zbottom[1] < fh->z))
	{
		// We have to split this wall!

		GLWall *w = NewWall();
		*w = *ws;

		// Splitting is done in the shader with clip planes, if available
		if (gl.flags & RFL_NO_CLIP_PLANES)
		{
			ws->vertcount = 0;	// invalidate current vertices.
			float newtexv = ws->tcs[GLWall::UPLFT].v + ((ws->tcs[GLWall::LOLFT].v - ws->tcs[GLWall::UPLFT].v) / (ws->zbottom[0] - ws->ztop[0])) * (fh->z - ws->ztop[0]);

			// I make the very big assumption here that translucent walls in sloped sectors
			// and 3D-floors never coexist in the same level. If that were the case this
			// code would become extremely more complicated.
			if (!ceiling)
			{
				ws->ztop[1] = w->zbottom[1] = ws->ztop[0] = w->zbottom[0] = fh->z;
				ws->tcs[GLWall::UPRGT].v = w->tcs[GLWall::LORGT].v = ws->tcs[GLWall::UPLFT].v = w->tcs[GLWall::LOLFT].v = newtexv;
			}
			else
			{
				w->ztop[1] = ws->zbottom[1] = w->ztop[0] = ws->zbottom[0] = fh->z;
				w->tcs[GLWall::UPLFT].v = ws->tcs[GLWall::LOLFT].v = w->tcs[GLWall::UPRGT].v = ws->tcs[GLWall::LORGT].v = newtexv;
			}
		}

		SortNode * sort2 = SortNodes.GetNew();
		memset(sort2, 0, sizeof(SortNode));
		sort2->itemindex = drawitems.Size() - 1;

		head->AddToLeft(sort);
		head->AddToRight(sort2);
	}
	else if ((ws->zbottom[0] < fh->z && !ceiling) || (ws->ztop[0] > fh->z && ceiling))	// completely on the left side
	{
		head->AddToLeft(sort);
	}
	else
	{
		head->AddToRight(sort);
	}

}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::SortSpriteIntoPlane(SortNode * head, SortNode * sort)
{
	GLFlat * fh = flats[drawitems[head->itemindex].index];
	GLSprite * ss = sprites[drawitems[sort->itemindex].index];

	bool ceiling = fh->z > r_viewpoint.Pos.Z;

	auto hiz = ss->z1 > ss->z2 ? ss->z1 : ss->z2;
	auto loz = ss->z1 < ss->z2 ? ss->z1 : ss->z2;

	if ((hiz > fh->z && loz < fh->z) || ss->modelframe)
	{
		// We have to split this sprite
		GLSprite *s = NewSprite();
		*s = *ss;

		// Splitting is done in the shader with clip planes, if available.
		// The fallback here only really works for non-y-billboarded sprites.
		if (gl.flags & RFL_NO_CLIP_PLANES)
		{
			float newtexv = ss->vt + ((ss->vb - ss->vt) / (ss->z2 - ss->z1))*(fh->z - ss->z1);

			if (!ceiling)
			{
				ss->z1 = s->z2 = fh->z;
				ss->vt = s->vb = newtexv;
			}
			else
			{
				s->z1 = ss->z2 = fh->z;
				s->vt = ss->vb = newtexv;
			}
		}

		SortNode * sort2 = SortNodes.GetNew();
		memset(sort2, 0, sizeof(SortNode));
		sort2->itemindex = drawitems.Size() - 1;

		head->AddToLeft(sort);
		head->AddToRight(sort2);
	}
	else if ((ss->z2<fh->z && !ceiling) || (ss->z1>fh->z && ceiling))	// completely on the left side
	{
		head->AddToLeft(sort);
	}
	else
	{
		head->AddToRight(sort);
	}
}


//==========================================================================
//
//
//
//==========================================================================
#define MIN_EQ (0.0005f)

void GLDrawList::SortWallIntoWall(SortNode * head,SortNode * sort)
{
	GLWall * wh= walls[drawitems[head->itemindex].index];
	GLWall * ws= walls[drawitems[sort->itemindex].index];
	float v1=wh->PointOnSide(ws->glseg.x1,ws->glseg.y1);
	float v2=wh->PointOnSide(ws->glseg.x2,ws->glseg.y2);

	if (fabs(v1)<MIN_EQ && fabs(v2)<MIN_EQ) 
	{
		if (ws->type==RENDERWALL_FOGBOUNDARY && wh->type!=RENDERWALL_FOGBOUNDARY) 
		{
			head->AddToRight(sort);
		}
		else if (ws->type!=RENDERWALL_FOGBOUNDARY && wh->type==RENDERWALL_FOGBOUNDARY) 
		{
			head->AddToLeft(sort);
		}
		else 
		{
			head->AddToEqual(sort);
		}
	}
	else if (v1<MIN_EQ && v2<MIN_EQ) 
	{
		head->AddToLeft(sort);
	}
	else if (v1>-MIN_EQ && v2>-MIN_EQ) 
	{
		head->AddToRight(sort);
	}
	else
	{
		double r=ws->CalcIntersectionVertex(wh);

		float ix=(float)(ws->glseg.x1+r*(ws->glseg.x2-ws->glseg.x1));
		float iy=(float)(ws->glseg.y1+r*(ws->glseg.y2-ws->glseg.y1));
		float iu=(float)(ws->tcs[GLWall::UPLFT].u + r * (ws->tcs[GLWall::UPRGT].u - ws->tcs[GLWall::UPLFT].u));
		float izt=(float)(ws->ztop[0]+r*(ws->ztop[1]-ws->ztop[0]));
		float izb=(float)(ws->zbottom[0]+r*(ws->zbottom[1]-ws->zbottom[0]));

		ws->vertcount = 0;	// invalidate current vertices.
		GLWall *w= NewWall();
		*w = *ws;

		w->glseg.x1=ws->glseg.x2=ix;
		w->glseg.y1=ws->glseg.y2=iy;
		w->glseg.fracleft = ws->glseg.fracright = ws->glseg.fracleft + r*(ws->glseg.fracright - ws->glseg.fracleft);
		w->ztop[0]=ws->ztop[1]=izt;
		w->zbottom[0]=ws->zbottom[1]=izb;
		w->tcs[GLWall::LOLFT].u = w->tcs[GLWall::UPLFT].u = ws->tcs[GLWall::LORGT].u = ws->tcs[GLWall::UPRGT].u = iu;
		ws->MakeVertices(gl_drawinfo, false);
		w->MakeVertices(gl_drawinfo, false);

		SortNode * sort2=SortNodes.GetNew();
		memset(sort2,0,sizeof(SortNode));
		sort2->itemindex=drawitems.Size()-1;

		if (v1>0)
		{
			head->AddToLeft(sort2);
			head->AddToRight(sort);
		}
		else
		{
			head->AddToLeft(sort);
			head->AddToRight(sort2);
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
EXTERN_CVAR(Int, gl_billboard_mode)
EXTERN_CVAR(Bool, gl_billboard_faces_camera)
EXTERN_CVAR(Bool, gl_billboard_particles)

void GLDrawList::SortSpriteIntoWall(SortNode * head,SortNode * sort)
{
	GLWall *wh= walls[drawitems[head->itemindex].index];
	GLSprite * ss= sprites[drawitems[sort->itemindex].index];

	float v1 = wh->PointOnSide(ss->x1, ss->y1);
	float v2 = wh->PointOnSide(ss->x2, ss->y2);

	if (fabs(v1)<MIN_EQ && fabs(v2)<MIN_EQ) 
	{
		if (wh->type==RENDERWALL_FOGBOUNDARY) 
		{
			head->AddToLeft(sort);
		}
		else 
		{
			head->AddToEqual(sort);
		}
	}
	else if (v1<MIN_EQ && v2<MIN_EQ) 
	{
		head->AddToLeft(sort);
	}
	else if (v1>-MIN_EQ && v2>-MIN_EQ) 
	{
		head->AddToRight(sort);
	}
	else
	{
		const bool drawWithXYBillboard = ((ss->particle && gl_billboard_particles) || (!(ss->actor && ss->actor->renderflags & RF_FORCEYBILLBOARD)
			&& (gl_billboard_mode == 1 || (ss->actor && ss->actor->renderflags & RF_FORCEXYBILLBOARD))));

		const bool drawBillboardFacingCamera = gl_billboard_faces_camera;
		// [Nash] has +ROLLSPRITE
		const bool rotated = (ss->actor != nullptr && ss->actor->renderflags & (RF_ROLLSPRITE | RF_WALLSPRITE | RF_FLATSPRITE));

		// cannot sort them at the moment. This requires more complex splitting.
		if (drawWithXYBillboard || drawBillboardFacingCamera || rotated)
		{
			float v1 = wh->PointOnSide(ss->x, ss->y);
			if (v1 < 0)
			{
				head->AddToLeft(sort);
			}
			else
			{
				head->AddToRight(sort);
			}
			return;
		}
		double r=ss->CalcIntersectionVertex(wh);

		float ix=(float)(ss->x1 + r * (ss->x2-ss->x1));
		float iy=(float)(ss->y1 + r * (ss->y2-ss->y1));
		float iu=(float)(ss->ul + r * (ss->ur-ss->ul));

		GLSprite *s = NewSprite();
		*s = *ss;

		s->x1=ss->x2=ix;
		s->y1=ss->y2=iy;
		s->ul=ss->ur=iu;

		SortNode * sort2=SortNodes.GetNew();
		memset(sort2,0,sizeof(SortNode));
		sort2->itemindex=drawitems.Size()-1;

		if (v1>0)
		{
			head->AddToLeft(sort2);
			head->AddToRight(sort);
		}
		else
		{
			head->AddToLeft(sort);
			head->AddToRight(sort2);
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================

inline int GLDrawList::CompareSprites(SortNode * a,SortNode * b)
{
	GLSprite * s1= sprites[drawitems[a->itemindex].index];
	GLSprite * s2= sprites[drawitems[b->itemindex].index];

	int res = s1->depth - s2->depth;

	if (res != 0) return -res;
	else return (i_compatflags & COMPATF_SPRITESORT)? s1->index-s2->index : s2->index-s1->index;
}

//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::SortSpriteList(SortNode * head)
{
	SortNode * n;
	int count;
	unsigned i;

	static TArray<SortNode*> sortspritelist;

	SortNode * parent=head->parent;

	sortspritelist.Clear();
	for(count=0,n=head;n;n=n->next) sortspritelist.Push(n);
	std::stable_sort(sortspritelist.begin(), sortspritelist.end(), [=](SortNode *a, SortNode *b)
	{
		return CompareSprites(a, b) < 0;
	});

	for(i=0;i<sortspritelist.Size();i++)
	{
		sortspritelist[i]->next=NULL;
		if (parent) parent->equal=sortspritelist[i];
		parent=sortspritelist[i];
	}
	return sortspritelist[0];
}

//==========================================================================
//
//
//
//==========================================================================
SortNode * GLDrawList::DoSort(SortNode * head)
{
	SortNode * node, * sn, * next;

	sn=FindSortPlane(head);
	if (sn)
	{
		if (sn==head) head=head->next;
		sn->UnlinkFromChain();
		node=head;
		head=sn;
		while (node)
		{
			next=node->next;
			switch(drawitems[node->itemindex].rendertype)
			{
			case GLDIT_FLAT:
				SortPlaneIntoPlane(head,node);
				break;

			case GLDIT_WALL:
				SortWallIntoPlane(head,node);
				break;

			case GLDIT_SPRITE:
				SortSpriteIntoPlane(head,node);
				break;
			}
			node=next;
		}
	}
	else
	{
		sn=FindSortWall(head);
		if (sn)
		{
			if (sn==head) head=head->next;
			sn->UnlinkFromChain();
			node=head;
			head=sn;
			while (node)
			{
				next=node->next;
				switch(drawitems[node->itemindex].rendertype)
				{
				case GLDIT_WALL:
					SortWallIntoWall(head,node);
					break;

				case GLDIT_SPRITE:
					SortSpriteIntoWall(head,node);
					break;

				case GLDIT_FLAT: break;
				}
				node=next;
			}
		}
		else 
		{
			return SortSpriteList(head);
		}
	}
	if (head->left) head->left=DoSort(head->left);
	if (head->right) head->right=DoSort(head->right);
	return sn;
}


//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DoDraw(int pass, int i, bool trans)
{
	switch(drawitems[i].rendertype)
	{
	case GLDIT_FLAT:
		{
			GLFlat * f= flats[drawitems[i].index];
			RenderFlat.Clock();
			f->Draw(pass, trans);
			RenderFlat.Unclock();
		}
		break;

	case GLDIT_WALL:
		{
			GLWall * w= walls[drawitems[i].index];
			RenderWall.Clock();
			w->Draw(pass);
			RenderWall.Unclock();
		}
		break;

	case GLDIT_SPRITE:
		{
			GLSprite * s= sprites[drawitems[i].index];
			RenderSprite.Clock();
			s->Draw(pass);
			RenderSprite.Unclock();
		}
		break;
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DoDrawSorted(SortNode * head)
{
	float clipsplit[2];
	int relation = 0;
	float z = 0.f;

	gl_RenderState.GetClipSplit(clipsplit);

	if (drawitems[head->itemindex].rendertype == GLDIT_FLAT)
	{
		z = flats[drawitems[head->itemindex].index]->z;
		relation = z > r_viewpoint.Pos.Z ? 1 : -1;
	}


	// left is further away, i.e. for stuff above viewz its z coordinate higher, for stuff below viewz its z coordinate is lower
	if (head->left) 
	{
		if (relation == -1)
		{
			gl_RenderState.SetClipSplit(clipsplit[0], z);	// render below: set flat as top clip plane
		}
		else if (relation == 1)
		{
			gl_RenderState.SetClipSplit(z, clipsplit[1]);	// render above: set flat as bottom clip plane
		}
		DoDrawSorted(head->left);
		gl_RenderState.SetClipSplit(clipsplit);
	}
	DoDraw(GLPASS_TRANSLUCENT, head->itemindex, true);
	if (head->equal)
	{
		SortNode * ehead=head->equal;
		while (ehead)
		{
			DoDraw(GLPASS_TRANSLUCENT, ehead->itemindex, true);
			ehead=ehead->equal;
		}
	}
	// right is closer, i.e. for stuff above viewz its z coordinate is lower, for stuff below viewz its z coordinate is higher
	if (head->right)
	{
		if (relation == 1)
		{
			gl_RenderState.SetClipSplit(clipsplit[0], z);	// render below: set flat as top clip plane
		}
		else if (relation == -1)
		{
			gl_RenderState.SetClipSplit(z, clipsplit[1]);	// render above: set flat as bottom clip plane
		}
		DoDrawSorted(head->right);
		gl_RenderState.SetClipSplit(clipsplit);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DrawSorted()
{
	if (drawitems.Size()==0) return;

	if (!sorted)
	{
		GLRenderer->mVBO->Map();
		MakeSortList();
		sorted=DoSort(SortNodes[SortNodeStart]);
		GLRenderer->mVBO->Unmap();
	}
	gl_RenderState.ClearClipSplit();
	if (!(gl.flags & RFL_NO_CLIP_PLANES))
	{
		glEnable(GL_CLIP_DISTANCE1);
		glEnable(GL_CLIP_DISTANCE2);
	}
	DoDrawSorted(sorted);
	if (!(gl.flags & RFL_NO_CLIP_PLANES))
	{
		glDisable(GL_CLIP_DISTANCE1);
		glDisable(GL_CLIP_DISTANCE2);
	}
	gl_RenderState.ClearClipSplit();
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::Draw(int pass, bool trans)
{
	for(unsigned i=0;i<drawitems.Size();i++)
	{
		DoDraw(pass, i, trans);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DrawWalls(int pass)
{
	RenderWall.Clock();
	for(unsigned i=0;i<drawitems.Size();i++)
	{
		walls[drawitems[i].index]->Draw(pass);
	}
	RenderWall.Unclock();
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DrawFlats(int pass)
{
	RenderFlat.Clock();
	for(unsigned i=0;i<drawitems.Size();i++)
	{
		flats[drawitems[i].index]->Draw(pass, false);
	}
	RenderFlat.Unclock();
}

//==========================================================================
//
//
//
//==========================================================================
void GLDrawList::DrawDecals()
{
	for(unsigned i=0;i<drawitems.Size();i++)
	{
		gl_drawinfo->DoDrawDecals(walls[drawitems[i].index]);
	}
}

//==========================================================================
//
// Sorting the drawitems first by texture and then by light level.
//
//==========================================================================

void GLDrawList::SortWalls()
{
	if (drawitems.Size() > 1)
	{
		std::sort(drawitems.begin(), drawitems.end(), [=](const GLDrawItem &a, const GLDrawItem &b) -> bool
		{
			GLWall * w1 = walls[a.index];
			GLWall * w2 = walls[b.index];

			if (w1->gltexture != w2->gltexture) return w1->gltexture < w2->gltexture;
			return (w1->flags & 3) < (w2->flags & 3);

		});
	}
}

void GLDrawList::SortFlats()
{
	if (drawitems.Size() > 1)
	{
		std::sort(drawitems.begin(), drawitems.end(), [=](const GLDrawItem &a, const GLDrawItem &b)
		{
			GLFlat * w1 = flats[a.index];
			GLFlat* w2 = flats[b.index];
			return w1->gltexture < w2->gltexture;
		});
	}
}

//==========================================================================
//
//
//
//==========================================================================

GLWall *GLDrawList::NewWall()
{
	auto wall = (GLWall*)RenderDataAllocator.Alloc(sizeof(GLWall));
	drawitems.Push(GLDrawItem(GLDIT_WALL, walls.Push(wall)));
	return wall;
}

//==========================================================================
//
//
//
//==========================================================================
GLFlat *GLDrawList::NewFlat()
{
	auto flat = (GLFlat*)RenderDataAllocator.Alloc(sizeof(GLFlat));
	drawitems.Push(GLDrawItem(GLDIT_FLAT,flats.Push(flat)));
	return flat;
}

//==========================================================================
//
//
//
//==========================================================================
GLSprite *GLDrawList::NewSprite()
{	
	auto sprite = (GLSprite*)RenderDataAllocator.Alloc(sizeof(GLSprite));
	drawitems.Push(GLDrawItem(GLDIT_SPRITE, sprites.Push(sprite)));
	return sprite;
}


//==========================================================================
//
// Try to reuse the lists as often as possible as they contain resources that
// are expensive to create and delete.
//
// Note: If multithreading gets used, this class needs synchronization.
//
//==========================================================================

FDrawInfo *FDrawInfoList::GetNew()
{
	if (mList.Size() > 0)
	{
		FDrawInfo *di;
		mList.Pop(di);
		return di;
	}
	return new FDrawInfo;
}

void FDrawInfoList::Release(FDrawInfo * di)
{
	di->ClearBuffers();
	mList.Push(di);
}

//==========================================================================
//
//
//
//==========================================================================

FDrawInfo::FDrawInfo()
{
	next = NULL;
	if (gl.legacyMode)
	{
		dldrawlists = new GLDrawList[GLLDL_TYPES];
	}
}

FDrawInfo::~FDrawInfo()
{
	if (dldrawlists != NULL) delete[] dldrawlists;
	ClearBuffers();
}


//==========================================================================
//
// Sets up a new drawinfo struct
//
//==========================================================================
void FDrawInfo::StartDrawInfo(GLSceneDrawer *drawer)
{
	FDrawInfo *di=di_list.GetNew();
	di->mDrawer = drawer;
    di->FixedColormap = drawer->FixedColormap;
	di->StartScene();
}

void FDrawInfo::StartScene()
{
	ClearBuffers();

	sectorrenderflags.Resize(level.sectors.Size());
	ss_renderflags.Resize(level.subsectors.Size());
	no_renderflags.Resize(level.subsectors.Size());

	memset(&sectorrenderflags[0], 0, level.sectors.Size() * sizeof(sectorrenderflags[0]));
	memset(&ss_renderflags[0], 0, level.subsectors.Size() * sizeof(ss_renderflags[0]));
	memset(&no_renderflags[0], 0, level.nodes.Size() * sizeof(no_renderflags[0]));

	next = gl_drawinfo;
	gl_drawinfo = this;
	for (int i = 0; i < GLDL_TYPES; i++) drawlists[i].Reset();
	if (dldrawlists != NULL)
	{
		for (int i = 0; i < GLLDL_TYPES; i++) dldrawlists[i].Reset();
	}
}

//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::EndDrawInfo()
{
	FDrawInfo * di = gl_drawinfo;

	for(int i=0;i<GLDL_TYPES;i++) di->drawlists[i].Reset();
	if (di->dldrawlists != NULL)
	{
		for (int i = 0; i < GLLDL_TYPES; i++) di->dldrawlists[i].Reset();
	}
	gl_drawinfo=di->next;
	di_list.Release(di);
	if (gl_drawinfo == nullptr) 
		ResetAllocator();
}


//==========================================================================
//
// Flood gaps with the back side's ceiling/floor texture
// This requires a stencil because the projected plane interferes with
// the depth buffer
//
//==========================================================================

void FDrawInfo::SetupFloodStencil(wallseg * ws)
{
	int recursion = GLPortal::GetRecursion();

	// Create stencil 
	glStencilFunc(GL_EQUAL, recursion, ~0);		// create stencil
	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);		// increment stencil of valid pixels
	{
		// Use revertible color mask, to avoid stomping on anaglyph 3D state
		ScopedColorMask colorMask(0, 0, 0, 0); // glColorMask(0, 0, 0, 0);						// don't write to the graphics buffer
		gl_RenderState.EnableTexture(false);
		gl_RenderState.ResetColor();
		glEnable(GL_DEPTH_TEST);
		glDepthMask(true);

		gl_RenderState.Apply();
		FQuadDrawer qd;
		qd.Set(0, ws->x1, ws->z1, ws->y1, 0, 0);
		qd.Set(1, ws->x1, ws->z2, ws->y1, 0, 0);
		qd.Set(2, ws->x2, ws->z2, ws->y2, 0, 0);
		qd.Set(3, ws->x2, ws->z1, ws->y2, 0, 0);
		qd.Render(GL_TRIANGLE_FAN);

		glStencilFunc(GL_EQUAL, recursion + 1, ~0);		// draw sky into stencil
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);		// this stage doesn't modify the stencil

	} // glColorMask(1, 1, 1, 1);						// don't write to the graphics buffer
	gl_RenderState.EnableTexture(true);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);
}

void FDrawInfo::ClearFloodStencil(wallseg * ws)
{
	int recursion = GLPortal::GetRecursion();

	glStencilOp(GL_KEEP,GL_KEEP,GL_DECR);
	gl_RenderState.EnableTexture(false);
	{
		// Use revertible color mask, to avoid stomping on anaglyph 3D state
		ScopedColorMask colorMask(0, 0, 0, 0); // glColorMask(0,0,0,0);						// don't write to the graphics buffer
		gl_RenderState.ResetColor();

		gl_RenderState.Apply();
		FQuadDrawer qd;
		qd.Set(0, ws->x1, ws->z1, ws->y1, 0, 0);
		qd.Set(1, ws->x1, ws->z2, ws->y1, 0, 0);
		qd.Set(2, ws->x2, ws->z2, ws->y2, 0, 0);
		qd.Set(3, ws->x2, ws->z1, ws->y2, 0, 0);
		qd.Render(GL_TRIANGLE_FAN);

		// restore old stencil op.
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc(GL_EQUAL, recursion, ~0);
		gl_RenderState.EnableTexture(true);
	} // glColorMask(1, 1, 1, 1);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(true);
}

//==========================================================================
//
// Draw the plane segment into the gap
//
//==========================================================================
void FDrawInfo::DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling)
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap Colormap;
	FMaterial * gltexture;

	plane.GetFromSector(sec, ceiling);

	gltexture=FMaterial::ValidateTexture(plane.texture, false, true);
	if (!gltexture) return;

	if (mDrawer->FixedColormap) 
	{
		Colormap.Clear();
		lightlevel=255;
	}
	else
	{
		Colormap = sec->Colormap;
		if (gltexture->tex->isFullbright())
		{
			Colormap.MakeWhite();
			lightlevel=255;
		}
		else lightlevel=abs(ceiling? sec->GetCeilingLight() : sec->GetFloorLight());
	}

	int rel = getExtraLight();
	mDrawer->SetColor(lightlevel, rel, Colormap, 1.0f);
	mDrawer->SetFog(lightlevel, rel, &Colormap, false);
	gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);

	float fviewx = r_viewpoint.Pos.X;
	float fviewy = r_viewpoint.Pos.Y;
	float fviewz = r_viewpoint.Pos.Z;

	gl_SetPlaneTextureRotation(&plane, gltexture);
	gl_RenderState.Apply();

	float prj_fac1 = (planez-fviewz)/(ws->z1-fviewz);
	float prj_fac2 = (planez-fviewz)/(ws->z2-fviewz);

	float px1 = fviewx + prj_fac1 * (ws->x1-fviewx);
	float py1 = fviewy + prj_fac1 * (ws->y1-fviewy);

	float px2 = fviewx + prj_fac2 * (ws->x1-fviewx);
	float py2 = fviewy + prj_fac2 * (ws->y1-fviewy);

	float px3 = fviewx + prj_fac2 * (ws->x2-fviewx);
	float py3 = fviewy + prj_fac2 * (ws->y2-fviewy);

	float px4 = fviewx + prj_fac1 * (ws->x2-fviewx);
	float py4 = fviewy + prj_fac1 * (ws->y2-fviewy);

	FQuadDrawer qd;
	qd.Set(0, px1, planez, py1, px1 / 64, -py1 / 64);
	qd.Set(1, px2, planez, py2, px2 / 64, -py2 / 64);
	qd.Set(2, px3, planez, py3, px3 / 64, -py3 / 64);
	qd.Set(3, px4, planez, py4, px4 / 64, -py4 / 64);
	qd.Render(GL_TRIANGLE_FAN);

	gl_RenderState.EnableTextureMatrix(false);
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::FloodUpperGap(seg_t * seg)
{
	wallseg ws;
	sector_t ffake, bfake;
	sector_t * fakefsector = hw_FakeFlat(seg->frontsector, &ffake, mDrawer->in_area, true);
	sector_t * fakebsector = hw_FakeFlat(seg->backsector, &bfake, mDrawer->in_area, false);

	vertex_t * v1, * v2;

	// Although the plane can be sloped this code will only be called
	// when the edge itself is not.
	double backz = fakebsector->ceilingplane.ZatPoint(seg->v1);
	double frontz = fakefsector->ceilingplane.ZatPoint(seg->v1);

	if (fakebsector->GetTexture(sector_t::ceiling)==skyflatnum) return;
	if (backz < r_viewpoint.Pos.Z) return;

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1=seg->linedef->v1;
		v2=seg->linedef->v2;
	}
	else
	{
		v1=seg->linedef->v2;
		v2=seg->linedef->v1;
	}

	ws.x1 = v1->fX();
	ws.y1 = v1->fY();
	ws.x2 = v2->fX();
	ws.y2 = v2->fY();

	ws.z1= frontz;
	ws.z2= backz;

	// Step1: Draw a stencil into the gap
	SetupFloodStencil(&ws);

	// Step2: Project the ceiling plane into the gap
	DrawFloodedPlane(&ws, ws.z2, fakebsector, true);

	// Step3: Delete the stencil
	ClearFloodStencil(&ws);
}

//==========================================================================
//
//
//
//==========================================================================

void FDrawInfo::FloodLowerGap(seg_t * seg)
{
	wallseg ws;
	sector_t ffake, bfake;
	sector_t * fakefsector = hw_FakeFlat(seg->frontsector, &ffake, mDrawer->in_area, true);
	sector_t * fakebsector = hw_FakeFlat(seg->backsector, &bfake, mDrawer->in_area, false);

	vertex_t * v1, * v2;

	// Although the plane can be sloped this code will only be called
	// when the edge itself is not.
	double backz = fakebsector->floorplane.ZatPoint(seg->v1);
	double frontz = fakefsector->floorplane.ZatPoint(seg->v1);


	if (fakebsector->GetTexture(sector_t::floor) == skyflatnum) return;
	if (fakebsector->GetPlaneTexZ(sector_t::floor) > r_viewpoint.Pos.Z) return;

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1=seg->linedef->v1;
		v2=seg->linedef->v2;
	}
	else
	{
		v1=seg->linedef->v2;
		v2=seg->linedef->v1;
	}

	ws.x1 = v1->fX();
	ws.y1 = v1->fY();
	ws.x2 = v2->fX();
	ws.y2 = v2->fY();

	ws.z2= frontz;
	ws.z1= backz;

	// Step1: Draw a stencil into the gap
	SetupFloodStencil(&ws);

	// Step2: Project the ceiling plane into the gap
	DrawFloodedPlane(&ws, ws.z1, fakebsector, false);

	// Step3: Delete the stencil
	ClearFloodStencil(&ws);
}

// This was temporarily moved out of gl_renderhacks.cpp so that the dependency on GLWall could be eliminated until things have progressed a bit.
void FDrawInfo::ProcessLowerMinisegs(TArray<seg_t *> &lowersegs)
{
	for(unsigned int j=0;j<lowersegs.Size();j++)
	{
		seg_t * seg=lowersegs[j];
		GLWall wall(mDrawer);
		wall.ProcessLowerMiniseg(this, seg, seg->Subsector->render_sector, seg->PartnerSeg->Subsector->render_sector);
		rendered_lines++;
	}
}

// Same here for the dependency on the portal.
void FDrawInfo::AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub)
{
	portal->GetRenderState()->AddSubsector(sub);
}

std::pair<FFlatVertex *, unsigned int> FDrawInfo::AllocVertices(unsigned int count)
{
	unsigned int index = -1;
	auto p = GLRenderer->mVBO->Alloc(count, &index);
	return std::make_pair(p, index);
}
