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



#include "m_alloc.h"
#include <stdlib.h>
#include <math.h>

#include "doomdef.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"

#include "m_bbox.h"

#include "p_local.h"

#include "r_local.h"
#include "r_sky.h"

// For ST_Y
#include "st_stuff.h"

#include "c_cvars.h"
#include "v_video.h"

cvar_t *r_viewsize;

extern int dmflags;

// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 	2048



int 					viewangleoffset;

// increment every time a check is made
int 					validcount = 1; 		

lighttable_t*			basecolormap;	// [RH] colormap currently drawing with
int						fixedlightlev;
lighttable_t*			fixedcolormap;
extern int*				walllights;		// [RH] changed from lighttable_t** to int*

int 					centerx;
int 					centery;

fixed_t 				centerxfrac;
fixed_t 				centeryfrac;
fixed_t 				projection;
// [RH] fixing the aspect ratio stuff (from doom legacy)...
fixed_t 				projectiony;
// [RH] virtual top of the sky (for freelooking)
fixed_t					skytopfrac;

// just for profiling purposes
int 					framecount; 	

int 					linecount;
int 					loopcount;

fixed_t 				viewx;
fixed_t 				viewy;
fixed_t 				viewz;

angle_t 				viewangle;

fixed_t 				viewcos;
fixed_t 				viewsin;

mobj_t*					camera;		// [RH] camera to draw from. doesn't have to be a player

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

// [RH] Changed these from lighttable_t* to int.
int						scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
int						scalelightfixed[MAXLIGHTSCALE];
int						zlight[LIGHTLEVELS][MAXLIGHTZ];

int						lightscalexmul;	// [RH] used to keep hires modes dark enough
int						lightscaleymul;

int 					extralight;		// bumped light from gun blasts
BOOL					foggy;			// [RH] ignore extralight and fullbright
extern BOOL				DrawNewHUD;		// [RH] Defined in d_main.c.

cvar_t					*r_detail;		// [RH] Detail mode


void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*lucentcolfunc) (void);
void (*transcolfunc) (void);
void (*tlatedlucentcolfunc) (void);
void (*spanfunc) (void);

void (*hcolfunc_pre) (void);
void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post4) (int sx, int yl, int yh);


//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void R_AddPointToBox (int x, int y, fixed_t *box)
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
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front) or 1 (back).
// [RH] Re-arranged slightly.
//
int R_PointOnSide (fixed_t x, fixed_t y, node_t *node)
{
	if (!node->dx)
	{
		return (x <= node->x) ? (node->dy > 0) : (node->dy < 0);
	}
	else if (!node->dy)
	{
		return (y <= node->y) ? (node->dx < 0) : (node->dx > 0);
	}
	else
	{
		fixed_t dx = (x - node->x);
		fixed_t dy = (y - node->y);
		
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

		if (FixedMul (dy, node->dx>>FRACBITS) < FixedMul (node->dy>>FRACBITS, dx))
		{
			// front side
			return 0;
		}
		// back side
		return 1;
	}
}

// [RH] Rearranged this slightly, too.
int R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line)
{
	fixed_t lx = line->v1->x;
	fixed_t ly = line->v1->y;

	fixed_t ldx = line->v2->x - lx;
	fixed_t ldy = line->v2->y - ly;

	if (!ldx)
	{
		return (x <= lx) ? (ldy > 0) : (ldy < 0);
	}
	else if (!ldy)
	{
		return (y <= ly) ? (ldx < 0) : (ldx > 0);
	}
	else
	{
		fixed_t dx = (x - lx);
		fixed_t dy = (y - ly);

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

		if (FixedMul (dy, ldx>>FRACBITS) < FixedMul (ldy>>FRACBITS, dx))
		{
			// front side
			return 0;
		}
		// back side
		return 1;
	}
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
angle_t R_PointToAngle (fixed_t x, fixed_t y)
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
}


angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{		
	viewx = x1;
	viewy = y1;
	
	return R_PointToAngle (x2, y2);
}


//
// R_InitPointToAngle
//
void R_InitPointToAngle (void)
{
	// UNUSED - now getting from tables.c
	// [RH] Actually, if you define CALC_TABLES, the game will use the FPU
	//		to calculate these tables at runtime so that a little space
	//		can be saved on disk.
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
	// UNUSED: now getting from tables.c
	// [RH] As with R_InitPointToAngle, you can #define CALC_TABLES
	//		to generate these tables at runtime.
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
	int 	i;
	int 	x;
	int 	t;
	fixed_t focallength;
	
	// Use tangent table to generate viewangletox:
	//	viewangletox will give the next greatest x
	//	after the view angle.
	//
	// Calc focallength
	//	so FIELDOFVIEW angles covers SCREENWIDTH.
	focallength = FixedDiv (centerxfrac,
							finetangent[FINEANGLES/4+FIELDOFVIEW/2] );
		
	for (i = 0; i < FINEANGLES/2; i++)
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
	for (x = 0; x <= viewwidth; x++)
	{
		i = 0;
		while (viewangletox[i]>x)
			i++;
		xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
	}
	
	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES/2; i++)
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
// [RH] This now sets up indices into a colormap rather than pointers into the
//		colormap, because the colormap can vary by sector, but the indices
//		into it don't.
//
#define DISTMAP 2

void R_InitLightTables (void)
{
	int i;
	int j;
	int level;
	int startmap;		
	int scale;
	int lightmapsize = 8 + (screen.is8bit ? 0 : 2);

	// Calculate the light levels to use
	//	for each level / distance combination.
	for (i = 0; i < LIGHTLEVELS; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTZ ; j++)
		{
			scale = FixedDiv (160*FRACUNIT, (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT-LIGHTSCALEMULBITS;
			level = startmap - scale/DISTMAP;
			
			if (level < 0)
				level = 0;
			else if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			zlight[i][j] = level << lightmapsize;
		}
	}

	lightscalexmul = ((320<<detailyshift) * (1<<LIGHTSCALEMULBITS)) / screen.width;
}



//
// R_SetViewSize
// Do not really change anything here,
//	because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
BOOL setsizeneeded;
int	 setblocks;
int	 setdetail = -1;


void R_SetViewSize (int blocks)
{
	setsizeneeded = true;
	setblocks = blocks;
}


// [RH] Change detailmode
void R_DetailCallback (cvar_t *var)
{
	static BOOL badrecovery = false;

	if (badrecovery) {
		badrecovery = false;
		return;
	}

	if (var->value < 0.0 || var->value > 3.0) {
		Printf (PRINT_HIGH, "Bad detail mode. (Use 0-3)\n");
		badrecovery = true;
		SetCVarFloat (var, (float)((detailyshift << 1)|detailxshift));
		return;
	}

	setdetail = (int)var->value;
	setsizeneeded = true;
}

int freediff;
fixed_t freelookviewheight;

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
	int			virtheight, virtwidth;
	int			lightmapsize = 8 + (screen.is8bit ? 0 : 2);

	setsizeneeded = false;
	BorderNeedRefresh = true;

	if (setdetail >= 0) {
		if (!r_columnmethod->value) {
			R_RenderSegLoop = R_RenderSegLoop1;
			// [RH] x-doubling only works with the standard column drawer
			detailxshift = setdetail & 1;
		} else {
			R_RenderSegLoop = R_RenderSegLoop2;
			detailxshift = 0;
		}
		detailyshift = (setdetail >> 1) & 1;
		setdetail = -1;
	}

	if (setblocks == 11 || setblocks == 12)
	{
		realviewwidth = screen.width;
		freelookviewheight = realviewheight = screen.height;
	}
	else if (setblocks == 10) {
		realviewwidth = screen.width;
		realviewheight = ST_Y;
		freelookviewheight = screen.height;
	}
	else
	{
		realviewwidth = ((setblocks*screen.width)/10) & (~(15>>(screen.is8bit ? 0 : 2)));
		realviewheight = ((setblocks*ST_Y)/10)&~7;
		freelookviewheight = ((setblocks*screen.height)/10)&~7;
	}

	if (setblocks == 11)
		DrawNewHUD = true;
	else
		DrawNewHUD = false;
	
	freediff = (freelookviewheight - realviewheight) >> (detailyshift + 1);
	viewwidth = realviewwidth >> detailxshift;
	viewheight = realviewheight >> detailyshift;
	freelookviewheight >>= detailyshift;

	{
		char temp[16];

		sprintf (temp, "%d x %d", viewwidth, viewheight);
		SetCVar (r_viewsize, temp);
	}

	centery = viewheight/2;
	centerx = viewwidth/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	virtwidth = screen.width >> detailxshift;
	virtheight = screen.height >> detailyshift;

	// [RH] aspect ratio stuff (based on Doom Legacy's)
	aspectx = ((virtheight * centerx * 320) / 200) / virtwidth * FRACUNIT;

	projection = centerxfrac;
	projectiony = aspectx;

	colfunc = basecolfunc = R_DrawColumn;
	lucentcolfunc = R_DrawTranslucentColumn;
	fuzzcolfunc = R_DrawFuzzColumn;
	transcolfunc = R_DrawTranslatedColumn;
	tlatedlucentcolfunc = R_DrawTlatedLucentColumn;
	spanfunc = R_DrawSpan;

	// [RH] Horizontal column drawers
	hcolfunc_pre = R_DrawColumnHoriz;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post2 = rt_map2cols;
	hcolfunc_post4 = rt_map4cols;

	R_InitBuffer (viewwidth, viewheight);
	R_InitTextureMapping ();
	
	// psprite scales
	pspritescale = (viewwidth << FRACBITS) / 320;
	pspriteiscale = (320 << FRACBITS) / viewwidth;
	{
		// [RH] Aspect ratio fix (from Doom Legacy, sort of)
		float h = (float)virtheight;
		float w1 = (float)viewwidth;
		float w2 = (float)virtwidth;
		pspriteyscale = (fixed_t)((h*w1*0.005*FRACUNIT)/w2);
	}
	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap (r_stretchsky);

	// thing clipping
	for (i=0 ; i<viewwidth ; i++)
		screenheightarray[i] = (short)viewheight;
	
	// planes


	// [RH] Calculates yslopes for 2.5 times the view height
	// to allow for freelook (based on Doom Legacy)
	// This amount was got at after playing Blood for a
	// little bit: You can look half a screen height up
	// from straight ahead, and an entire screen height
	// below from straight ahead.
	for (i=0 ; i < (freelookviewheight<<1)+(freelookviewheight>>1) ; i++)
	{
		dy = ((i-freelookviewheight)<<FRACBITS)+FRACUNIT/2;
		dy = abs(dy);
		yslopetab[i] = FixedDiv (aspectx, dy);
	}
	yslope = yslopetab + (viewheight >> 1);
		
	for (i=0 ; i<viewwidth ; i++)
	{
		cosadj = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
		distscale[i] = FixedDiv (FRACUNIT,cosadj);
	}
	
	// Calculate the light levels to use for each level / scale combination.
	// [RH] This just stores indices into the colormap rather than pointers to a specific one.
	for (i=0 ; i< LIGHTLEVELS ; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j=0 ; j<MAXLIGHTSCALE ; j++)
		{
			level = startmap - (j*(screen.width>>detailxshift))/((viewwidth*DISTMAP));
			if (level < 0)
				level = 0;
			else if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			scalelight[i][j] = level << lightmapsize;
		}
	}

	// [RH] Initialize z-light tables here
	R_InitLightTables ();
}



//
// R_Init
//
extern cvar_t	*screenblocks;

static void screenblocksCallback (cvar_t *var)
{
	if (var->value > 12.0) {
		// SetCVarFloat() will call us again
		SetCVarFloat (var, 12.0);
		return;
	} else if (var->value < 3.0) {
		SetCVarFloat (var, 3.0);
		return;
	}

	R_SetViewSize ((int)var->value);
}

static void PickColumnMethod (cvar_t *var)
{
	if (var->value != 0 && var->value != 1)
		SetCVarFloat (var, 1);
	else
		// Trigger the change with a setdetail event
		SetCVarFloat (r_detail, r_detail->value);
}

void R_Init (void)
{
	// [RH] Automatically sense changes to screenblocks cvar
	screenblocks->u.callback = screenblocksCallback;

	// [RH] Automatically sense changes to r_detail cvar
	r_detail->u.callback = R_DetailCallback;
	// ...and apply it at startup.
	R_DetailCallback (r_detail);

	R_InitData ();
	R_InitPointToAngle ();
	R_InitTables ();
	// viewwidth / viewheight are set by the defaults

	R_SetViewSize ((int)screenblocks->value);
	R_InitPlanes ();
	R_InitLightTables ();
	R_InitTranslationTables ();

	R_InitParticles ();	// [RH] Setup particle engine

	// [RH] Setup the seg rendering loop
	r_columnmethod = cvar ("r_columnmethod", "1", CVAR_ARCHIVE|CVAR_CALLBACK);
	r_columnmethod->u.callback = PickColumnMethod;

	r_drawflat = cvar ("r_drawflat", "0", 0);

	framecount = 0;
}


//
// R_PointInSubsector
//
subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
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
extern dyncolormap_t NormalLight;
unsigned int R_OldBlend = ~0;

void R_SetupFrame (player_t *player)
{				
	unsigned int newblend;
	int dy;
	
	camera = player->camera;	// [RH] Use camera instead of viewplayer

	if (player->cheats & CF_CHASECAM) {
		// [RH] Use chasecam view
		P_AimCamera (camera);
		viewx = CameraX;
		viewy = CameraY;
		viewz = CameraZ;
	} else {
		viewx = camera->x;
		viewy = camera->y;
		viewz = camera->player ? camera->player->viewz : camera->z;
	}

	viewangle = camera->angle + viewangleoffset;

	if (camera->player && camera->player->xviewshift && !paused)
	{
		int intensity = camera->player->xviewshift;;
		viewx += ((M_Random() % (intensity<<2))
					-(intensity<<1))<<FRACBITS;
		viewy += ((M_Random()%(intensity<<2))
					-(intensity<<1))<<FRACBITS;
	}

	extralight = camera == player->mo ? player->extralight : 0;

	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
		
	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend

	if (camera->subsector->sector->heightsec != -1)
	{
		const sector_t *s = camera->subsector->sector->heightsec + sectors;
		newblend = viewz < s->floorheight ? s->bottommap : viewz > s->ceilingheight ?
				   s->topmap : s->midmap;
		if (!screen.is8bit)
			newblend = R_BlendForColormap (newblend);
		else if (APART(newblend) == 0 && newblend >= numfakecmaps)
			newblend = 0;
	} else {
		newblend = 0;
	}

	// [RH] Don't override testblend unless entering a sector with a
	//		blend different from the previous sector's. Same goes with
	//		NormalLight's maps pointer.
	if (R_OldBlend != newblend) {
		R_OldBlend = newblend;
		if (APART(newblend)) {
			BaseBlendR = RPART(newblend);
			BaseBlendG = GPART(newblend);
			BaseBlendB = BPART(newblend);
			BaseBlendA = APART(newblend) / 255.0f;
			NormalLight.maps = realcolormaps;
		} else {
			NormalLight.maps = realcolormaps + (NUMCOLORMAPS+1)*256*newblend;
			BaseBlendR = BaseBlendG = BaseBlendB = 0;
			BaseBlendA = 0.0f;
		}
	}

	fixedcolormap = NULL;
	fixedlightlev = 0;

	if (camera == player->mo && player->fixedcolormap)
	{
		if (player->fixedcolormap < NUMCOLORMAPS) {
			fixedlightlev = player->fixedcolormap*256;
			fixedcolormap = DefaultPalette->maps.colormaps;
		} else {
			if (screen.is8bit)
				fixedcolormap =
					DefaultPalette->maps.colormaps
					+ player->fixedcolormap*256;
			else
				fixedcolormap = (lighttable_t *)
					(DefaultPalette->maps.shades
					+ player->fixedcolormap*256);
		}
		
		walllights = scalelightfixed;

		// [RH] scalelightfixed is an int* now, not a lighttable_t**
		memset (scalelightfixed, 0, MAXLIGHTSCALE*sizeof(*scalelightfixed));
	}

	// [RH] freelook stuff
	dy = FixedMul (freelookviewheight << (FRACBITS/2), camera->pitch) >> 9;
	yslope = yslopetab + (freelookviewheight >> 1) + dy + freediff;
	centery = (viewheight >> 1) - dy;
	centeryfrac = centery << FRACBITS;
	skytopfrac = centeryfrac - ((freelookviewheight + freediff) << (FRACBITS - 1));

	framecount++;
	validcount++;

	if (BorderNeedRefresh)
	{
		if (setblocks < 10)
		{
			R_DrawViewBorder();
		}
		BorderNeedRefresh = false;
		BorderTopRefresh = false;
	}
	else if (BorderTopRefresh)
	{
		if (setblocks < 10)
		{
			R_DrawTopBorder();
		}
		BorderTopRefresh = false;
	}
}



//
// R_RenderView
//
void R_RenderPlayerView (player_t *player)
{
	R_SetupFrame (player);

	// Clear buffers.
	R_ClearClipSegs ();
	R_ClearDrawSegs ();
	R_ClearPlanes ();
	R_ClearSprites ();
	
	// check for new console commands.
	NetUpdate ();

	// [RH] Show off segs if r_drawflat is 1
	if (r_drawflat->value) {
		hcolfunc_pre = R_FillColumnHorizP;
		hcolfunc_post1 = rt_copy1col;
		hcolfunc_post2 = rt_copy2cols;
		hcolfunc_post4 = rt_copy4cols;
		colfunc = R_FillColumnP;
		//spanfunc = R_FillSpan;
	}
	else
	{
		hcolfunc_pre = R_DrawColumnHoriz;
		colfunc = basecolfunc;
		hcolfunc_post1 = rt_map1col;
		hcolfunc_post2 = rt_map2cols;
		hcolfunc_post4 = rt_map4cols;
	}
	
	// Never draw the player unless in chasecam mode
	if (camera->player && !(camera->player->cheats & CF_CHASECAM)) {
		camera->flags2 |= MF2_DONTDRAW;
		R_RenderBSPNode (numnodes - 1);
		camera->flags2 &= ~MF2_DONTDRAW;
	} else
		R_RenderBSPNode (numnodes-1);	// The head node is the last node output.

	// Check for new console commands.
	NetUpdate ();
	
	R_DrawPlanes ();

	spanfunc = R_DrawSpan;
	
	// Check for new console commands.
	NetUpdate ();
	
	R_DrawMasked ();

	// Check for new console commands.
	NetUpdate ();

	// [RH] Apply detail mode doubling
	R_DetailDouble ();
}

// [RH] Do all multires stuff. Called from V_SetResolution()
void R_MultiresInit (void)
{
	int i;

	// in r_things.c
	extern short *r_dscliptop, *r_dsclipbot;
	// in r_draw.c
	extern byte **ylookup;
	extern int *columnofs;

	ylookup = Realloc (ylookup, screen.height * sizeof(byte *));
	columnofs = Realloc (columnofs, screen.width * sizeof(int));
	r_dscliptop = Realloc (r_dscliptop, screen.width * sizeof(short));
	r_dsclipbot = Realloc (r_dsclipbot, screen.width * sizeof(short));

	// Moved from R_InitSprites()
	negonearray = Realloc (negonearray, sizeof(short) * screen.width);

	for (i=0 ; i<screen.width ; i++)
	{
		negonearray[i] = -1;
	}

	// These get set in R_ExecuteSetViewSize()
	screenheightarray = Realloc (screenheightarray, sizeof(short) * screen.width);
	xtoviewangle = Realloc (xtoviewangle, sizeof(angle_t) * (screen.width + 1));

	R_InitFuzzTable ();
	R_PlaneInitData ();
	R_OldBlend = ~0;
}