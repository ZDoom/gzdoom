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
** hw_drawlist.cpp
** The main container type for draw items.
**
*/

#include "r_sky.h"
#include "r_utility.h"
#include "doomstat.h"
#include "actor.h"
#include "g_levellocals.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_drawlist.h"
#include "flatvertices.h"
#include "hw_clock.h"
#include "hw_renderstate.h"
#include "hw_drawinfo.h"
#include "hw_fakeflat.h"
#include "hw_walldispatcher.h"

FMemArena RenderDataAllocator(1024*1024);	// Use large blocks to reduce allocation time.

void ResetRenderDataAllocator()
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
void HWDrawList::Reset()
{
	if (sorted) SortNodes.Release(SortNodeStart);
	sorted=NULL;
	walls.Clear();
	flats.Clear();
	sprites.Clear();
	drawitems.Clear();
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
void HWDrawList::MakeSortList()
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
SortNode * HWDrawList::FindSortPlane(SortNode * head)
{
	while (head->next && drawitems[head->itemindex].rendertype!=DrawType_FLAT) 
		head=head->next;
	if (drawitems[head->itemindex].rendertype==DrawType_FLAT) return head;
	return NULL;
}


//==========================================================================
//
//
//
//==========================================================================
SortNode * HWDrawList::FindSortWall(SortNode * head)
{
	float farthest = -FLT_MAX;
	float nearest = FLT_MAX;
	SortNode * best = NULL;
	SortNode * node = head;
	float bestdist = FLT_MAX;

	while (node)
	{
		HWDrawItem * it = &drawitems[node->itemindex];
		if (it->rendertype == DrawType_WALL)
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
		HWDrawItem * it = &drawitems[node->itemindex];
		if (it->rendertype == DrawType_WALL)
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
void HWDrawList::SortPlaneIntoPlane(SortNode * head,SortNode * sort)
{
	HWFlat * fh= flats[drawitems[head->itemindex].index];
	HWFlat * fs= flats[drawitems[sort->itemindex].index];

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
void HWDrawList::SortWallIntoPlane(HWDrawInfo* di, SortNode * head, SortNode * sort)
{
	HWFlat * fh = flats[drawitems[head->itemindex].index];
	HWWall * ws = walls[drawitems[sort->itemindex].index];

	bool ceiling = fh->z > SortZ;

	if ((ws->ztop[0] > fh->z || ws->ztop[1] > fh->z) && (ws->zbottom[0] < fh->z || ws->zbottom[1] < fh->z))
	{
		// We have to split this wall!

		HWWall *w = NewWall();
		*w = *ws;

		// Splitting is done in the shader with clip planes, if available
		if (screen->hwcaps & RFL_NO_CLIP_PLANES)
		{
			ws->vertcount = 0;	// invalidate current vertices.
			float newtexv = ws->tcs[HWWall::UPLFT].v + ((ws->tcs[HWWall::LOLFT].v - ws->tcs[HWWall::UPLFT].v) / (ws->zbottom[0] - ws->ztop[0])) * (fh->z - ws->ztop[0]);
			float newlmv = ws->lightuv[HWWall::UPLFT].v + ((ws->lightuv[HWWall::LOLFT].v - ws->lightuv[HWWall::UPLFT].v) / (ws->zbottom[0] - ws->ztop[0])) * (fh->z - ws->ztop[0]);

			// I make the very big assumption here that translucent walls in sloped sectors
			// and 3D-floors never coexist in the same level - If that were the case this
			// code would become extremely more complicated.
			if (!ceiling)
			{
				ws->ztop[1] = w->zbottom[1] = ws->ztop[0] = w->zbottom[0] = fh->z;
				ws->tcs[HWWall::UPRGT].v = w->tcs[HWWall::LORGT].v = ws->tcs[HWWall::UPLFT].v = w->tcs[HWWall::LOLFT].v = newtexv;
				ws->lightuv[HWWall::UPRGT].v = w->lightuv[HWWall::LORGT].v = ws->lightuv[HWWall::UPLFT].v = w->lightuv[HWWall::LOLFT].v = newlmv;
			}
			else
			{
				w->ztop[1] = ws->zbottom[1] = w->ztop[0] = ws->zbottom[0] = fh->z;
				w->tcs[HWWall::UPLFT].v = ws->tcs[HWWall::LOLFT].v = w->tcs[HWWall::UPRGT].v = ws->tcs[HWWall::LORGT].v = newtexv;
				w->lightuv[HWWall::UPLFT].v = ws->lightuv[HWWall::LOLFT].v = w->lightuv[HWWall::UPRGT].v = ws->lightuv[HWWall::LORGT].v = newlmv;
			}
			w->MakeVertices(false);
			ws->MakeVertices(false);
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
void HWDrawList::SortSpriteIntoPlane(SortNode * head, SortNode * sort)
{
	HWFlat * fh = flats[drawitems[head->itemindex].index];
	HWSprite * ss = sprites[drawitems[sort->itemindex].index];

	bool ceiling = fh->z > SortZ;

	auto hiz = ss->z1 > ss->z2 ? ss->z1 : ss->z2;
	auto loz = ss->z1 < ss->z2 ? ss->z1 : ss->z2;

	if ((hiz > fh->z && loz < fh->z) || ss->modelframe)
	{
		// We have to split this sprite
		HWSprite *s = NewSprite();
		*s = *ss;

		// Splitting is done in the shader with clip planes, if available.
		// The fallback here only really works for non-y-billboarded sprites.
		if (screen->hwcaps & RFL_NO_CLIP_PLANES)
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

// Lines start-end and fdiv must intersect.
inline double CalcIntersectionVertex(HWWall *w1, HWWall * w2)
{
	float ax = w1->glseg.x1, ay = w1->glseg.y1;
	float bx = w1->glseg.x2, by = w1->glseg.y2;
	float cx = w2->glseg.x1, cy = w2->glseg.y1;
	float dx = w2->glseg.x2, dy = w2->glseg.y2;
	return ((ay - cy)*(dx - cx) - (ax - cx)*(dy - cy)) / ((bx - ax)*(dy - cy) - (by - ay)*(dx - cx));
}

void HWDrawList::SortWallIntoWall(HWDrawInfo *di, SortNode * head,SortNode * sort)
{
	HWWall * wh= walls[drawitems[head->itemindex].index];
	HWWall * ws= walls[drawitems[sort->itemindex].index];
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
		double r = CalcIntersectionVertex(ws, wh);

		float ix=(float)(ws->glseg.x1+r*(ws->glseg.x2-ws->glseg.x1));
		float iy=(float)(ws->glseg.y1+r*(ws->glseg.y2-ws->glseg.y1));
		float iu=(float)(ws->tcs[HWWall::UPLFT].u + r * (ws->tcs[HWWall::UPRGT].u - ws->tcs[HWWall::UPLFT].u));
		float ilmu=(float)(ws->lightuv[HWWall::UPLFT].u + r * (ws->lightuv[HWWall::UPRGT].u - ws->lightuv[HWWall::UPLFT].u));
		float izt=(float)(ws->ztop[0]+r*(ws->ztop[1]-ws->ztop[0]));
		float izb=(float)(ws->zbottom[0]+r*(ws->zbottom[1]-ws->zbottom[0]));

		ws->vertcount = 0;	// invalidate current vertices.
		HWWall *w= NewWall();
		*w = *ws;

		w->glseg.x1=ws->glseg.x2=ix;
		w->glseg.y1=ws->glseg.y2=iy;
		w->glseg.fracleft = ws->glseg.fracright = ws->glseg.fracleft + r*(ws->glseg.fracright - ws->glseg.fracleft);
		w->ztop[0]=ws->ztop[1]=izt;
		w->zbottom[0]=ws->zbottom[1]=izb;
		w->tcs[HWWall::LOLFT].u = w->tcs[HWWall::UPLFT].u = ws->tcs[HWWall::LORGT].u = ws->tcs[HWWall::UPRGT].u = iu;
		w->lightuv[HWWall::LOLFT].u = w->lightuv[HWWall::UPLFT].u = ws->lightuv[HWWall::LORGT].u = ws->lightuv[HWWall::UPRGT].u = iu;
		ws->MakeVertices(false);
		w->MakeVertices(false);

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
EXTERN_CVAR(Bool, hw_force_cambbpref)
EXTERN_CVAR(Bool, gl_billboard_particles)

inline double CalcIntersectionVertex(HWSprite *s, HWWall * w2)
{
	float ax = s->x1, ay = s->y1;
	float bx = s->x2, by = s->y2;
	float cx = w2->glseg.x1, cy = w2->glseg.y1;
	float dx = w2->glseg.x2, dy = w2->glseg.y2;
	return ((ay - cy)*(dx - cx) - (ax - cx)*(dy - cy)) / ((bx - ax)*(dy - cy) - (by - ay)*(dx - cx));
}

void HWDrawList::SortSpriteIntoWall(HWDrawInfo *di, SortNode * head,SortNode * sort)
{
	HWWall *wh= walls[drawitems[head->itemindex].index];
	HWSprite * ss= sprites[drawitems[sort->itemindex].index];

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

		const bool drawBillboardFacingCamera = hw_force_cambbpref ? gl_billboard_faces_camera :
			(gl_billboard_faces_camera && (ss->actor && !(ss->actor->renderflags2 & RF2_BILLBOARDNOFACECAMERA)))
			|| (ss->actor && ss->actor->renderflags2 & RF2_BILLBOARDFACECAMERA);

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
		double r=CalcIntersectionVertex(ss, wh);

		float ix=(float)(ss->x1 + r * (ss->x2-ss->x1));
		float iy=(float)(ss->y1 + r * (ss->y2-ss->y1));
		float iu=(float)(ss->ul + r * (ss->ur-ss->ul));

		HWSprite *s = NewSprite();
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
		if (screen->BuffersArePersistent())
		{
			s->vertexindex = ss->vertexindex = -1;
		}
		else
		{
			s->CreateVertices(di);
			ss->CreateVertices(di);
		}

	}
}


//==========================================================================
//
//
//
//==========================================================================

inline int HWDrawList::CompareSprites(SortNode * a,SortNode * b)
{
	HWSprite * s1= sprites[drawitems[a->itemindex].index];
	HWSprite * s2= sprites[drawitems[b->itemindex].index];

	if (s1->depth < s2->depth) return 1;
	if (s1->depth > s2->depth) return -1;
	return reverseSort? s2->index-s1->index : s1->index-s2->index;
}

//==========================================================================
//
//
//
//==========================================================================
SortNode * HWDrawList::SortSpriteList(SortNode * head)
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
SortNode * HWDrawList::DoSort(HWDrawInfo *di, SortNode * head)
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
			case DrawType_FLAT:
				SortPlaneIntoPlane(head,node);
				break;

			case DrawType_WALL:
				SortWallIntoPlane(di,head,node);
				break;

			case DrawType_SPRITE:
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
				case DrawType_WALL:
					SortWallIntoWall(di, head,node);
					break;

				case DrawType_SPRITE:
					SortSpriteIntoWall(di, head, node);
					break;

				case DrawType_FLAT: break;
				}
				node=next;
			}
		}
		else 
		{
			return SortSpriteList(head);
		}
	}
	if (head->left) head->left=DoSort(di, head->left);
	if (head->right) head->right=DoSort(di, head->right);
	return sn;
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawList::Sort(HWDrawInfo *di)
{
	reverseSort = !!(di->Level->i_compatflags & COMPATF_SPRITESORT);
    SortZ = di->Viewpoint.Pos.Z;
	MakeSortList();
	sorted = DoSort(di, SortNodes[SortNodeStart]);
}

//==========================================================================
//
// Sorting the drawitems first by texture and then by light level
//
//==========================================================================

void HWDrawList::SortWalls()
{
	if (drawitems.Size() > 1)
	{
		std::sort(drawitems.begin(), drawitems.end(), [=](const HWDrawItem &a, const HWDrawItem &b) -> bool
		{
			HWWall * w1 = walls[a.index];
			HWWall * w2 = walls[b.index];

			if (w1->texture != w2->texture) return w1->texture < w2->texture;
			return (w1->flags & 3) < (w2->flags & 3);

		});
	}
}

void HWDrawList::SortFlats()
{
	if (drawitems.Size() > 1)
	{
		std::sort(drawitems.begin(), drawitems.end(), [=](const HWDrawItem &a, const HWDrawItem &b)
		{
			HWFlat * w1 = flats[a.index];
			HWFlat* w2 = flats[b.index];
			return w1->texture < w2->texture;
		});
	}
}


//==========================================================================
//
//
//
//==========================================================================

HWWall *HWDrawList::NewWall()
{
	auto wall = (HWWall*)RenderDataAllocator.Alloc(sizeof(HWWall));
	drawitems.Push(HWDrawItem(DrawType_WALL, walls.Push(wall)));
	return wall;
}

//==========================================================================
//
//
//
//==========================================================================
HWFlat *HWDrawList::NewFlat()
{
	auto flat = (HWFlat*)RenderDataAllocator.Alloc(sizeof(HWFlat));
	drawitems.Push(HWDrawItem(DrawType_FLAT,flats.Push(flat)));
	return flat;
}

//==========================================================================
//
//
//
//==========================================================================
HWSprite *HWDrawList::NewSprite()
{	
	auto sprite = (HWSprite*)RenderDataAllocator.Alloc(sizeof(HWSprite));
	drawitems.Push(HWDrawItem(DrawType_SPRITE, sprites.Push(sprite)));
	return sprite;
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawList::DoDraw(HWDrawInfo *di, FRenderState &state, bool translucent, int i)
{
	switch(drawitems[i].rendertype)
	{
	case DrawType_FLAT:
		{
			HWFlat * f= flats[drawitems[i].index];
			RenderFlat.Clock();
			f->DrawFlat(di, state, translucent);
			RenderFlat.Unclock();
		}
		break;

	case DrawType_WALL:
		{
			HWWall * w= walls[drawitems[i].index];
			HWWallDispatcher dis(di);
			RenderWall.Clock();
			w->DrawWall(&dis, state, translucent);
			RenderWall.Unclock();
		}
		break;

	case DrawType_SPRITE:
		{
			HWSprite * s= sprites[drawitems[i].index];
			RenderSprite.Clock();
			s->DrawSprite(di, state, translucent);
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
void HWDrawList::Draw(HWDrawInfo *di, FRenderState &state, bool translucent)
{
	for (unsigned i = 0; i < drawitems.Size(); i++)
	{
		DoDraw(di, state, translucent, i);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawList::DrawWalls(HWDrawInfo *di, FRenderState &state, bool translucent)
{
	HWWallDispatcher dis(di);
	RenderWall.Clock();
	for (auto &item : drawitems)
	{
		walls[item.index]->DrawWall(&dis, state, translucent);
	}
	RenderWall.Unclock();
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawList::DrawFlats(HWDrawInfo *di, FRenderState &state, bool translucent)
{
	RenderFlat.Clock();
	for (unsigned i = 0; i<drawitems.Size(); i++)
	{
		flats[drawitems[i].index]->DrawFlat(di, state, translucent);
	}
	RenderFlat.Unclock();
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawList::DrawSorted(HWDrawInfo *di, FRenderState &state, SortNode * head)
{
	float clipsplit[2];
	int relation = 0;
	float z = 0.f;

	state.GetClipSplit(clipsplit);

	if (drawitems[head->itemindex].rendertype == DrawType_FLAT)
	{
		z = flats[drawitems[head->itemindex].index]->z;
		relation = z > di->Viewpoint.Pos.Z ? 1 : -1;
	}


	// left is further away, i.e. for stuff above viewz its z coordinate higher, for stuff below viewz its z coordinate is lower
	if (head->left)
	{
		if (relation == -1)
		{
			state.SetClipSplit(clipsplit[0], z);	// render below: set flat as top clip plane
		}
		else if (relation == 1)
		{
			state.SetClipSplit(z, clipsplit[1]);	// render above: set flat as bottom clip plane
		}
		DrawSorted(di, state, head->left);
		state.SetClipSplit(clipsplit);
	}
	DoDraw(di, state, true, head->itemindex);
	if (head->equal)
	{
		SortNode * ehead = head->equal;
		while (ehead)
		{
			DoDraw(di, state, true, ehead->itemindex);
			ehead = ehead->equal;
		}
	}
	// right is closer, i.e. for stuff above viewz its z coordinate is lower, for stuff below viewz its z coordinate is higher
	if (head->right)
	{
		if (relation == 1)
		{
			state.SetClipSplit(clipsplit[0], z);	// render below: set flat as top clip plane
		}
		else if (relation == -1)
		{
			state.SetClipSplit(z, clipsplit[1]);	// render above: set flat as bottom clip plane
		}
		DrawSorted(di, state, head->right);
		state.SetClipSplit(clipsplit);
	}
}

//==========================================================================
//
//
//
//==========================================================================
void HWDrawList::DrawSorted(HWDrawInfo *di, FRenderState &state)
{
	if (drawitems.Size() == 0) return;

	if (!sorted)
	{
		screen->mVertexData->Map();
		Sort(di);
		screen->mVertexData->Unmap();
	}
	state.ClearClipSplit();
	state.EnableClipDistance(1, true);
	state.EnableClipDistance(2, true);
	DrawSorted(di, state, sorted);
	state.EnableClipDistance(1, false);
	state.EnableClipDistance(2, false);
	state.ClearClipSplit();
}

