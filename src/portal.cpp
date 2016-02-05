#include "portal.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "c_cvars.h"
#include "m_bbox.h"
#include "p_tags.h"
#include "farchive.h"

// simulation recurions maximum
CVAR(Int, sv_portal_recursions, 4, CVAR_ARCHIVE|CVAR_SERVERINFO)

TArray<FLinePortal> linePortals;


FArchive &operator<< (FArchive &arc, FLinePortal &port)
{
	arc << port.mOrigin
		<< port.mDestination
		<< port.mXDisplacement
		<< port.mYDisplacement
		<< port.mType
		<< port.mFlags
		<< port.mDefFlags
		<< port.mAlign;
	return arc;
}


void P_SpawnLinePortal(line_t* line)
{
	// portal destination is special argument #0
	line_t* dst = NULL;

	if (line->args[2] >= PORTT_VISUAL && line->args[2] <= PORTT_LINKED)
	{
		if (line->args[0] > 0)
		{
			int linenum = -1;

			for (int i = 0; i < numlines; i++)
			{
				if (&lines[i] == line)
					continue;
				if (tagManager.LineHasID(&lines[i], line->args[0]))
				{
					dst = &lines[i];
					break;
				}
			}
		}

		line->portalindex = linePortals.Reserve(1);
		FLinePortal *port = &linePortals.Last();

		memset(port, 0, sizeof(FLinePortal));
		port->mOrigin = line;
		port->mDestination = dst;
		port->mType = BYTE(line->args[2]);	// range check is done above.
		port->mAlign = BYTE(line->args[3] >= PORG_ABSOLUTE && line->args[3] <= PORG_CEILING ? line->args[3] : PORG_ABSOLUTE);
		if (port->mDestination != NULL)
		{
			port->mDefFlags = port->mType == PORTT_VISUAL ? PORTF_VISIBLE : port->mType == PORTT_TELEPORT ? PORTF_TYPETELEPORT : PORTF_TYPEINTERACTIVE;


		}
	}
	else if (line->args[2] == PORTT_LINKEDEE && line->args[0] == 0)
	{
		// EE-style portals require that the first line ID is identical and the first arg of the two linked linedefs are 0 and 1 respectively.

		int mytag = tagManager.GetFirstLineID(line);

		for (int i = 0; i < numlines; i++)
		{
			if (tagManager.GetFirstLineID(&lines[i]) == mytag && lines[i].args[0] == 1)
			{
				line->portalindex = linePortals.Reserve(1);
				FLinePortal *port = &linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = line;
				port->mDestination = &lines[i];
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;

				// we need to create the backlink here, too.
				lines[i].portalindex = linePortals.Reserve(1);
				port = &linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = &lines[i];
				port->mDestination = line;
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;

			}
		}
	}
	else
	{
		// undefined type
		return;
	}
}

void P_UpdatePortal(FLinePortal *port)
{
	if (port->mDestination == NULL)
	{
		// Portal has no destination: switch it off
		port->mFlags = 0;
	}
	else if (port->mDestination->getPortalDestination() != port->mOrigin)
	{
		//portal doesn't link back. This will be a simple teleporter portal.
		port->mFlags = port->mDefFlags & ~PORTF_INTERACTIVE;
		if (port->mType == PORTT_LINKED)
		{
			// this is illegal. Demote the type to TELEPORT
			port->mType = PORTT_TELEPORT;
			port->mDefFlags &= ~PORTF_INTERACTIVE;
		}
	}
	else
	{
		port->mFlags = port->mDefFlags;
	}
}

void P_FinalizePortals()
{
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		FLinePortal * port = &linePortals[i];
		P_UpdatePortal(port);
	}
}

// [ZZ] lots of floats here to avoid overflowing a lot
bool P_IntersectLines(fixed_t o1x, fixed_t o1y, fixed_t p1x, fixed_t p1y,
				      fixed_t o2x, fixed_t o2y, fixed_t p2x, fixed_t p2y,
				      fixed_t& rx, fixed_t& ry)
{
	double xx = FIXED2DBL(o2x) - FIXED2DBL(o1x);
	double xy = FIXED2DBL(o2y) - FIXED2DBL(o1y);

	double d1x = FIXED2DBL(p1x) - FIXED2DBL(o1x);
	double d1y = FIXED2DBL(p1y) - FIXED2DBL(o1y);

	if (d1x > d1y)
	{
		d1y = d1y / d1x * 32767.0f;
		d1x = 32767.0;
	}
	else
	{
		d1x = d1x / d1y * 32767.0f;
		d1y = 32767.0;
	}

	double d2x = FIXED2DBL(p2x) - FIXED2DBL(o2x);
	double d2y = FIXED2DBL(p2y) - FIXED2DBL(o2y);

	double cross = d1x*d2y - d1y*d2x;
	if (fabs(cross) < 1e-8)
		return false;

	double t1 = (xx * d2y - xy * d2x)/cross;
	rx = o1x + FLOAT2FIXED(d1x * t1);
	ry = o1y + FLOAT2FIXED(d1y * t1);
	return true;
}

inline int P_PointOnLineSideExplicit (fixed_t x, fixed_t y, fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
	return DMulScale32 (y-y1, x2-x1, x1-x, y2-y1) > 0;
}

bool P_ClipLineToPortal(line_t* line, line_t* portal, fixed_t viewx, fixed_t viewy, bool partial, bool samebehind)
{
	// check if this line is between portal and the viewer. clip away if it is.
	bool behind1 = !!P_PointOnLineSidePrecise(line->v1->x, line->v1->y, portal);
	bool behind2 = !!P_PointOnLineSidePrecise(line->v2->x, line->v2->y, portal);

	// [ZZ] update 16.12.2014: if a vertex equals to one of portal's vertices, it's treated as being behind the portal.
	//                         this is required in order to clip away diagonal lines around the portal (example: 1-sided triangle shape with a mirror on it's side)
	if ((line->v1->x == portal->v1->x && line->v1->y == portal->v1->y) ||
		(line->v1->x == portal->v2->x && line->v1->y == portal->v2->y))
			behind1 = samebehind;
	if ((line->v2->x == portal->v1->x && line->v2->y == portal->v1->y) ||
		(line->v2->x == portal->v2->x && line->v2->y == portal->v2->y))
			behind2 = samebehind;

	if (behind1 && behind2)
	{
		// line is behind the portal plane. now check if it's in front of two view plane borders (i.e. if it will get in the way of rendering)
		fixed_t dummyx, dummyy;
		bool infront1 = P_IntersectLines(line->v1->x, line->v1->y, line->v2->x, line->v2->y, viewx, viewy, portal->v1->x, portal->v1->y, dummyx, dummyy);
		bool infront2 = P_IntersectLines(line->v1->x, line->v1->y, line->v2->x, line->v2->y, viewx, viewy, portal->v2->x, portal->v2->y, dummyx, dummyy);
		if (infront1 && infront2)
			return true;
	}

	return false;
}

void P_TranslatePortalXY(line_t* src, line_t* dst, fixed_t& x, fixed_t& y)
{
	if (!src || !dst)
		return;

	fixed_t nposx, nposy;	// offsets from line

	// Get the angle between the two linedefs, for rotating
	// orientation and velocity. Rotate 180 degrees, and flip
	// the position across the exit linedef, if reversed.
	angle_t angle =
			R_PointToAngle2(0, 0, dst->dx, dst->dy) -
			R_PointToAngle2(0, 0, src->dx, src->dy);

	angle += ANGLE_180;

	// Sine, cosine of angle adjustment
	fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
	fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

	fixed_t tx, ty;

	nposx = x - src->v1->x;
	nposy = y - src->v1->y;

	// Rotate position along normal to match exit linedef
	tx = FixedMul(nposx, c) - FixedMul(nposy, s);
	ty = FixedMul(nposy, c) + FixedMul(nposx, s);

	tx += dst->v2->x;
	ty += dst->v2->y;

	x = tx;
	y = ty;
}

void P_TranslatePortalVXVY(line_t* src, line_t* dst, fixed_t& vx, fixed_t& vy)
{
	angle_t angle =
		R_PointToAngle2(0, 0, dst->dx, dst->dy) -
		R_PointToAngle2(0, 0, src->dx, src->dy);

	angle += ANGLE_180;

	// Sine, cosine of angle adjustment
	fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
	fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

	fixed_t orig_velx = vx;
	fixed_t orig_vely = vy;
	vx = FixedMul(orig_velx, c) - FixedMul(orig_vely, s);
	vy = FixedMul(orig_vely, c) + FixedMul(orig_velx, s);
}

void P_TranslatePortalAngle(line_t* src, line_t* dst, angle_t& angle)
{
	if (!src || !dst)
		return;

	// Get the angle between the two linedefs, for rotating
	// orientation and velocity. Rotate 180 degrees, and flip
	// the position across the exit linedef, if reversed.
	angle_t xangle =
			R_PointToAngle2(0, 0, dst->dx, dst->dy) -
			R_PointToAngle2(0, 0, src->dx, src->dy);

	xangle += ANGLE_180;
	angle += xangle;
}

void P_TranslatePortalZ(line_t* src, line_t* dst, fixed_t& z)
{
	// args[2] = 0 - no adjustment
	// args[2] = 1 - adjust by floor difference
	// args[2] = 2 - adjust by ceiling difference

	switch (src->getPortalAlignment())
	{
	case PORG_FLOOR:
		z = z - src->frontsector->floorplane.ZatPoint(src->v1->x, src->v1->y) + dst->frontsector->floorplane.ZatPoint(dst->v2->x, dst->v2->y);
		return;

	case PORG_CEILING:
		z = z - src->frontsector->ceilingplane.ZatPoint(src->v1->x, src->v1->y) + dst->frontsector->ceilingplane.ZatPoint(dst->v2->x, dst->v2->y);
		return;

	default:
		return;
	}
}

// calculate shortest distance from a point (x,y) to a linedef
fixed_t P_PointLineDistance(line_t* line, fixed_t x, fixed_t y)
{
	angle_t angle = R_PointToAngle2(0, 0, line->dx, line->dy);
	angle += ANGLE_180;

	fixed_t dx = line->v1->x - x;
	fixed_t dy = line->v1->y - y;

	fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
	fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

	fixed_t d2x = FixedMul(dx, c) - FixedMul(dy, s);

	return abs(d2x);
}

void P_NormalizeVXVY(fixed_t& vx, fixed_t& vy)
{
	double _vx = FIXED2DBL(vx);
	double _vy = FIXED2DBL(vy);
	double len = sqrt(_vx*_vx+_vy*_vy);
	vx = FLOAT2FIXED(_vx/len);
	vy = FLOAT2FIXED(_vy/len);
}

// portal tracer code
PortalTracer::PortalTracer(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy)
{
	this->startx = startx;
	this->starty = starty;
	this->endx = endx;
	this->endy = endy;
	intx = endx;
	inty = endy;
	intxIn = intx;
	intyIn = inty;
	z = 0;
	angle = 0;
	depth = 0;
	frac = 0;
	in = NULL;
	out = NULL;
	vx = 0;
	vy = 0;
}

bool PortalTracer::TraceStep()
{
	if (depth > sv_portal_recursions)
		return false;

	this->in = NULL;
	this->out = NULL;
	this->vx = 0;
	this->vy = 0;

	int oDepth = depth;

	fixed_t dirx = endx-startx;
	fixed_t diry = endy-starty;
	P_NormalizeVXVY(dirx, diry);

	dirx = 0;
	diry = 0;

	FPathTraverse it(startx-dirx, starty-diry, endx+dirx, endy+diry, PT_ADDLINES | PT_COMPATIBLE);

	intercept_t *in;
	while ((in = it.Next()))
	{
		line_t* li;

		if (in->isaline)
		{
			li = in->d.line;

			if (li->isLinePortal())
			{
				if (P_PointOnLineSide(startx-dirx, starty-diry, li))
					continue; // we're at the back side of this line

				line_t* out = li->getPortalDestination();

				this->in = li;
				this->out = out;

				// we only know that we crossed it, but we also need to know WHERE we crossed it
				fixed_t vx = it.Trace().dx;
				fixed_t vy = it.Trace().dy;

				fixed_t x = it.Trace().x + FixedMul(vx, in->frac);
				fixed_t y = it.Trace().y + FixedMul(vy, in->frac);

				P_NormalizeVXVY(vx, vy);

				this->vx = vx;
				this->vy = vy;

				// teleport our trace

				if (!out->backsector)
				{
					intx = x + vx;
					inty = y + vy;
				}
				else
				{
					intx = x - vx;
					inty = y - vy;
				}

				//P_TranslateCoordinatesAndAngle(li, out, startx, starty, noangle);
				//P_TranslateCoordinatesAndAngle(li, out, endx, endy, angle);
				//if (hdeltaZ)
				//	P_TranslateZ(li, out, deltaZ);
				//P_TranslateCoordinatesAndAngle(li, out, vx, vy, noangle);

				P_TranslatePortalXY(li, out, startx, starty);
				P_TranslatePortalVXVY(li, out, this->vx, this->vy);
				intxIn = intx;
				intyIn = inty;
				P_TranslatePortalXY(li, out, intx, inty);
				P_TranslatePortalXY(li, out, endx, endy);
				P_TranslatePortalAngle(li, out, angle);
				P_TranslatePortalZ(li, out, z);
				frac += in->frac;
				depth++;
				break; // breaks to outer loop
			}

			if (!(li->flags & ML_TWOSIDED) || (li->flags & ML_BLOCKEVERYTHING))
				return false; // stop tracing, 2D blocking line
		}
	}

	//Printf("returning %d; vx = %.2f; vy = %.2f\n", (oDepth != depth), FIXED2DBL(this->vx), FIXED2DBL(this->vy));

	return (oDepth != depth); // if a portal has been found, return false
}

