// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Rendering main loop and setup functions,
//		 utility functions (BSP, geometry, trigonometry).
//		See tables.c, too.
//
//-----------------------------------------------------------------------------


static const char rcsid[] = "$Id: r_main.c,v 1.5 1997/02/03 22:45:12 b1 Exp $";



#include <stdlib.h>
#include <math.h>


#include "doomdef.h"
#include "d_net.h"

#include "m_bbox.h"

#include "r_local.h"
#include "r_sky.h"

// For ST_Y
#include "st_stuff.h"

#include "c_cvars.h"
#include "v_video.h"

cvar_t *r_viewsize;


// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 			2048	



int 					viewangleoffset;

// increment every time a check is made
int 					validcount = 1; 		


lighttable_t*			fixedcolormap;
extern lighttable_t**	walllights;

int 					centerx;
int 					centery;

fixed_t 				centerxfrac;
fixed_t 				centeryfrac;
fixed_t 				projection;
// [RH] fixing the aspect ratio stuff (from doom legacy)...
fixed_t 				projectiony;

// just for profiling purposes
int 					framecount; 	

int 					sscount;
int 					linecount;
int 					loopcount;

fixed_t 				viewx;
fixed_t 				viewy;
fixed_t 				viewz;

angle_t 				viewangle;

fixed_t 				viewcos;
fixed_t 				viewsin;

player_t*				viewplayer;

//
// precalculated math tables
//
angle_t 				clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 
int 					viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t 				*xtoviewangle;


// UNUSED.
// The finetangentgent[angle+FINEANGLES/4] table
// holds the fixed_t tangent values for view angles,
// ranging from MININT to 0 to MAXINT.
// fixed_t				finetangent[FINEANGLES/2];

// fixed_t				finesine[5*FINEANGLES/4];
fixed_t*				finecosine = &finesine[FINEANGLES/4];


lighttable_t*			scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t*			scalelightfixed[MAXLIGHTSCALE];
lighttable_t*			zlight[LIGHTLEVELS][MAXLIGHTZ];

// bumped light from gun blasts
int 					extralight;



void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*lucentcolfunc) (void);
void (*transcolfunc) (void);
void (*spanfunc) (void);



//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void
R_AddPointToBox
( int			x,
  int			y,
  fixed_t*		box )
{
	if (x< box[BOXLEFT])
		box[BOXLEFT] = x;
	if (x> box[BOXRIGHT])
		box[BOXRIGHT] = x;
	if (y< box[BOXBOTTOM])
		box[BOXBOTTOM] = y;
	if (y> box[BOXTOP])
		box[BOXTOP] = y;
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//	check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int
R_PointOnSide
( fixed_t		x,
  fixed_t		y,
  node_t*		node )
{
	fixed_t 	dx;
	fixed_t 	dy;
	fixed_t 	left;
	fixed_t 	right;
		
	if (!node->dx)
	{
		if (x <= node->x)
			return node->dy > 0;
		
		return node->dy < 0;
	}
	if (!node->dy)
	{
		if (y <= node->y)
			return node->dx < 0;
		
		return node->dx > 0;
	}
		
	dx = (x - node->x);
	dy = (y - node->y);
		
	// Try to quickly decide by looking at sign bits.
	if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
	{
		if	( (node->dy ^ dx) & 0x80000000 )
		{
			// (left is negative)
			return 1;
		}
		return 0;
	}

	left = FixedMul ( node->dy>>FRACBITS , dx );
	right = FixedMul ( dy , node->dx>>FRACBITS );
		
	if (right < left)
	{
		// front side
		return 0;
	}
	// back side
	return 1;					
}


int
R_PointOnSegSide
( fixed_t		x,
  fixed_t		y,
  seg_t*		line )
{
	fixed_t 	lx;
	fixed_t 	ly;
	fixed_t 	ldx;
	fixed_t 	ldy;
	fixed_t 	dx;
	fixed_t 	dy;
	fixed_t 	left;
	fixed_t 	right;
		
	lx = line->v1->x;
	ly = line->v1->y;
		
	ldx = line->v2->x - lx;
	ldy = line->v2->y - ly;
		
	if (!ldx)
	{
		if (x <= lx)
			return ldy > 0;
		
		return ldy < 0;
	}
	if (!ldy)
	{
		if (y <= ly)
			return ldx < 0;
		
		return ldx > 0;
	}
		
	dx = (x - lx);
	dy = (y - ly);
		
	// Try to quickly decide by looking at sign bits.
	if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
	{
		if	( (ldy ^ dx) & 0x80000000 )
		{
			// (left is negative)
			return 1;
		}
		return 0;
	}

	left = FixedMul ( ldy>>FRACBITS , dx );
	right = FixedMul ( dy , ldx>>FRACBITS );
		
	if (right < left)
	{
		// front side
		return 0;
	}
	// back side
	return 1;					
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//	the coordinates are flipped until they are in
//	the first octant of the coordinate system, then
//	the y (<=x) is scaled and divided by x to get a
//	tangent (slope) value which is looked up in the
//	tantoangle[] table.

//




angle_t
R_PointToAngle
( fixed_t		x,
  fixed_t		y )
{		
	x -= viewx;
	y -= viewy;
	
	if ( (!x) && (!y) )
		return 0;

	if (x>= 0)
	{
		// x >=0
		if (y>= 0)
		{
			// y>= 0

			if (x>y)
			{
				// octant 0
				return tantoangle[ SlopeDiv(y,x)];
			}
			else
			{
				// octant 1
				return ANG90-1-tantoangle[ SlopeDiv(x,y)];
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>y)
			{
				// octant 8
				return (angle_t) (-(int)tantoangle[SlopeDiv(y,x)]);
			}
			else
			{
				// octant 7
				return ANG270+tantoangle[ SlopeDiv(x,y)];
			}
		}
	}
	else
	{
		// x<0
		x = -x;

		if (y>= 0)
		{
			// y>= 0
			if (x>y)
			{
				// octant 3
				return ANG180-1-tantoangle[ SlopeDiv(y,x)];
			}
			else
			{
				// octant 2
				return ANG90+ tantoangle[ SlopeDiv(x,y)];
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>y)
			{
				// octant 4
				return ANG180+tantoangle[ SlopeDiv(y,x)];
			}
			else
			{
				 // octant 5
				return ANG270-1-tantoangle[ SlopeDiv(x,y)];
			}
		}
	}
	return 0;
}


angle_t
R_PointToAngle2
( fixed_t		x1,
  fixed_t		y1,
  fixed_t		x2,
  fixed_t		y2 )
{		
	viewx = x1;
	viewy = y1;
	
	return R_PointToAngle (x2, y2);
}


fixed_t
R_PointToDist
( fixed_t		x,
  fixed_t		y )
{
	int 		angle;
	fixed_t 	dx;
	fixed_t 	dy;
	fixed_t 	temp;
	fixed_t 	dist;
		
	dx = abs(x - viewx);
	dy = abs(y - viewy);
		
	if (dy>dx)
	{
		temp = dx;
		dx = dy;
		dy = temp;
	}
		
	angle = (tantoangle[ FixedDiv(dy,dx)>>DBITS ]+ANG90) >> ANGLETOFINESHIFT;

	// use as cosine
	dist = FixedDiv (dx, finesine[angle] ); 	
		
	return dist;
}




//
// R_InitPointToAngle
//
void R_InitPointToAngle (void)
{
	// UNUSED - now getting from tables.c [or are we? -RH]
#ifdef CALC_TABLES
	double		i, f;
//
// slope (tangent) to angle lookup
//
	for (i=0 ; i<=(double)SLOPERANGE ; i++)
	{
		f = atan2 (i, (double)SLOPERANGE) / (6.28318530718 /* 2*pi */);
		tantoangle[(int)i] = (angle_t)(0xffffffff*f);
	}
#endif
}


//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//	for the current line (horizontal span)
//	at the given angle.
// rw_distance must be calculated first.
//
fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
	fixed_t 			scale;
	int 				anglea;
	int 				angleb;
	int 				sinea;
	int 				sineb;
	fixed_t 			num;
	int 				den;

	// UNUSED
#if 0
{
	fixed_t 			dist;
	fixed_t 			z;
	fixed_t 			sinv;
	fixed_t 			cosv;
		
	sinv = finesine[(visangle-rw_normalangle)>>ANGLETOFINESHIFT];		
	dist = FixedDiv (rw_distance, sinv);
	cosv = finecosine[(viewangle-visangle)>>ANGLETOFINESHIFT];
	z = abs(FixedMul (dist, cosv));
	scale = FixedDiv(projection, z);
	return scale;
}
#endif

	anglea = ANG90 + (visangle-viewangle);
	angleb = ANG90 + (visangle-rw_normalangle);

	// both sines are allways positive
	sinea = finesine[anglea>>ANGLETOFINESHIFT]; 
	sineb = finesine[angleb>>ANGLETOFINESHIFT];
	// [RH] Use projectiony instead of projection to get correct
	// aspect ratio (assuming 320x200 is correct) (from doom legacy)
	num = FixedMul(projectiony,sineb);
	den = FixedMul(rw_distance,sinea);

	if (den > num>>16)
	{
		scale = FixedDiv (num, den);

		if (scale > 64*FRACUNIT)
			scale = 64*FRACUNIT;
		else if (scale < 256)
			scale = 256;
	}
	else
		scale = 64*FRACUNIT;
		
	return scale;
}



//
// R_InitTables
//
void R_InitTables (void)
{
	// UNUSED: now getting from tables.c [or are we? -RH]
#ifdef CALC_TABLES
	int 		i;
	double		a;
	double		fv;
	
	// viewangle tangent table
	for (i=0 ; i<FINEANGLES/2 ; i++)
	{
		a = (i-FINEANGLES/4+0.5)*PI*2/FINEANGLES;
		fv = FRACUNIT*tan (a);
		finetangent[i] = (angle_t)fv;
	}
	
	// finesine table
	for (i=0 ; i<5*FINEANGLES/4 ; i++)
	{
		// OPTIMIZE: mirror...
		a = (i+0.5)*PI*2/FINEANGLES;
		finesine[i] = (angle_t)(FRACUNIT * sin (a));
	}
#endif

}



//
// R_InitTextureMapping
//
void R_InitTextureMapping (void)
{
	int 				i;
	int 				x;
	int 				t;
	fixed_t 			focallength;
	
	// Use tangent table to generate viewangletox:
	//	viewangletox will give the next greatest x
	//	after the view angle.
	//
	// Calc focallength
	//	so FIELDOFVIEW angles covers SCREENWIDTH.
	focallength = FixedDiv (centerxfrac,
							finetangent[FINEANGLES/4+FIELDOFVIEW/2] );
		
	for (i=0 ; i<FINEANGLES/2 ; i++)
	{
		if (finetangent[i] > FRACUNIT*2)
			t = -1;
		else if (finetangent[i] < -FRACUNIT*2)
			t = viewwidth+1;
		else
		{
			t = FixedMul (finetangent[i], focallength);
			t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

			if (t < -1)
				t = -1;
			else if (t>viewwidth+1)
				t = viewwidth+1;
		}
		viewangletox[i] = t;
	}
	
	// Scan viewangletox[] to generate xtoviewangle[]:
	//	xtoviewangle will give the smallest view angle
	//	that maps to x. 
	for (x=0;x<=viewwidth;x++)
	{
		i = 0;
		while (viewangletox[i]>x)
			i++;
		xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
	}
	
	// Take out the fencepost cases from viewangletox.
	for (i=0 ; i<FINEANGLES/2 ; i++)
	{
		t = FixedMul (finetangent[i], focallength);
		t = centerx - t;
		
		if (viewangletox[i] == -1)
			viewangletox[i] = 0;
		else if (viewangletox[i] == viewwidth+1)
			viewangletox[i]  = viewwidth;
	}
		
	clipangle = xtoviewangle[0];
}



//
// R_InitLightTables
// Only inits the zlight table,
//	because the scalelight table changes with view size.
//
#define DISTMAP 		2

void R_InitLightTables (void)
{
	int 		i;
	int 		j;
	int 		level;
	int 		startmap;		
	int 		scale;
	
	// Calculate the light levels to use
	//	for each level / distance combination.
	for (i=0 ; i< LIGHTLEVELS ; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTZ ; j++)
		{
			scale = FixedDiv ((SCREENWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT;
			level = startmap - scale/DISTMAP;
			
			if (level < 0)
				level = 0;

			if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			zlight[i][j] = colormaps + level*256;
		}
	}
}



//
// R_SetViewSize
// Do not really change anything here,
//	because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean 		setsizeneeded;
int 			setblocks;


void R_SetViewSize (int blocks)
{
	setsizeneeded = true;
	setblocks = blocks;
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize (void)
{
	fixed_t 	cosadj;
	fixed_t 	dy;
	int 		i;
	int 		j;
	int 		level;
	int 		startmap;
	int			aspectx;

	setsizeneeded = false;

	if (setblocks == 11)
	{
		scaledviewwidth = SCREENWIDTH;
		viewheight = SCREENHEIGHT;
	}
	else if (setblocks == 10) {
		scaledviewwidth = SCREENWIDTH;
		viewheight = ST_Y;
	} else
	{
		scaledviewwidth = (setblocks*SCREENWIDTH)/10;
		viewheight = ((setblocks*ST_Y)/10)&~7;
	}
	
	viewwidth = scaledviewwidth;

	{
		char temp[16];

		sprintf (temp, "%d x %d", viewwidth, viewheight);
		SetCVar (r_viewsize, temp);
	}

	centery = viewheight/2;
	centerx = viewwidth/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	projection = centerxfrac;
	projectiony = (((SCREENHEIGHT*centerx*320)/200)/SCREENWIDTH)<<FRACBITS;

#ifndef USEASM
	colfunc = basecolfunc = R_DrawColumn;
	lucentcolfunc = R_DrawTranslucentColumn;
#else
// something strange going on here...
//	colfunc = basecolfunc = ASM_R_DrawColumn;
	colfunc = basecolfunc = R_DrawColumn;
// The assembly version has a bug which becomes
// even more apparent when an Archvile
// resurrects someone. (They are drawn fullbright.)
	lucentcolfunc = R_DrawTranslucentColumn;
#endif
	fuzzcolfunc = R_DrawFuzzColumn;
	transcolfunc = R_DrawTranslatedColumn;
	spanfunc = R_DrawSpan;

	R_InitBuffer (scaledviewwidth, viewheight);
		
	R_InitTextureMapping ();
	
	// psprite scales
//	  pspritescale = FRACUNIT*viewwidth/SCREENWIDTH;
//	  pspriteiscale = FRACUNIT*SCREENWIDTH/viewwidth;
	pspritescale = (viewwidth << FRACBITS) / 320;
	pspriteiscale = (320 << FRACBITS) / viewwidth;
	// [RH] Aspect ratio fix (from doom legacy)
	pspriteyscale = (((SCREENHEIGHT * viewwidth) / SCREENWIDTH) << FRACBITS) / 200;
	
	// thing clipping
	for (i=0 ; i<viewwidth ; i++)
		screenheightarray[i] = viewheight;
	
	// planes

	// [RH] aspect ratio stuff (from doom legacy)
	aspectx = (((SCREENHEIGHT * centerx * 320) / 200) / SCREENWIDTH);

	// [RH] Calculates yslopes for twice the view height
	// to allow for freelook
	for (i=0 ; i<viewheight*2 ; i++)
	{
		dy = ((i-viewheight)<<FRACBITS)+FRACUNIT/2;
		dy = abs(dy);
		yslopetab[i] = FixedDiv (aspectx * FRACUNIT, dy);
	}
	yslope = yslopetab + (viewheight >> 1);
		
	for (i=0 ; i<viewwidth ; i++)
	{
		cosadj = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
		distscale[i] = FixedDiv (FRACUNIT,cosadj);
	}
	
	// Calculate the light levels to use
	//	for each level / scale combination.
	for (i=0 ; i< LIGHTLEVELS ; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTSCALE ; j++)
		{
			level = startmap - j*SCREENWIDTH/viewwidth/DISTMAP;
			
			if (level < 0)
				level = 0;

			if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			scalelight[i][j] = colormaps + level*256;
		}
	}

	// Yeah, this is dumb...
	R_FillBackScreen ();
}



//
// R_Init
//
extern cvar_t	*screenblocks;



void R_Init (void)
{
	R_InitData ();
	Printf ("\nR_InitData");
	R_InitPointToAngle ();
	Printf ("\nR_InitPointToAngle");
	R_InitTables ();
	// viewwidth / viewheight are set by the defaults
	Printf ("\nR_InitTables");

	R_SetViewSize ((int)screenblocks->value);
	R_InitPlanes ();
	Printf ("\nR_InitPlanes");
	R_InitLightTables ();
	Printf ("\nR_InitLightTables");
	R_InitSkyMap ();
	Printf ("\nR_InitSkyMap");
	R_InitTranslationTables ();
	Printf ("\nR_InitTranslationsTables");
		
	framecount = 0;
}


//
// R_PointInSubsector
//
subsector_t*
R_PointInSubsector
( fixed_t		x,
  fixed_t		y )
{
	node_t* 	node;
	int 		side;
	int 		nodenum;

	// single subsector is a special case
	if (!numnodes)								
		return subsectors;
				
	nodenum = numnodes-1;

	while (! (nodenum & NF_SUBSECTOR) )
	{
		node = &nodes[nodenum];
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}
		
	return &subsectors[nodenum & ~NF_SUBSECTOR];
}



//
// R_SetupFrame
//
void R_SetupFrame (player_t* player)
{				
	int i;
	int dy;
	
	viewplayer = player;
	viewx = player->mo->x;
	viewy = player->mo->y;
	viewangle = player->mo->angle + viewangleoffset;
	extralight = player->extralight;

	viewz = player->viewz;

	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
		
	sscount = 0;
		
	if (player->fixedcolormap)
	{
		fixedcolormap =
			colormaps
			+ player->fixedcolormap*256*sizeof(lighttable_t);
		
		walllights = scalelightfixed;

		for (i=0 ; i<MAXLIGHTSCALE ; i++)
			scalelightfixed[i] = fixedcolormap;
	}
	else
		fixedcolormap = 0;

	// [RH] freelook stuff
	dy = FixedMul (viewheight << (FRACBITS/2), player->ysheer) >> 9;
	yslope = yslopetab + (viewheight >> 1) + dy;
	centery = (viewheight >> 1) - dy;
	centeryfrac = centery << FRACBITS;

	framecount++;
	validcount++;
}



//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{		
	R_SetupFrame (player);

	// Clear buffers.
	R_ClearClipSegs ();
	R_ClearDrawSegs ();
	R_ClearPlanes ();
	R_ClearSprites ();
	
	// check for new console commands.
	NetUpdate ();

	// The head node is the last node output.
	R_RenderBSPNode (numnodes-1);
	
	// Check for new console commands.
	NetUpdate ();
	
	R_DrawPlanes ();
	
	// Check for new console commands.
	NetUpdate ();
	
	R_DrawMasked ();

	// Check for new console commands.
	NetUpdate ();								
}
