#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "w_wad.h"
#include "r_defs.h"
#include "m_swap.h"

static int WriteTHINGS (FILE *file);
static int WriteLINEDEFS (FILE *file);
static int WriteSIDEDEFS (FILE *file);
static int WriteVERTEXES (FILE *file);
static int WriteSEGS (FILE *file);
static int WriteSSECTORS (FILE *file);
static int WriteNODES (FILE *file);
static int WriteSECTORS (FILE *file);
static int WriteREJECT (FILE *file);
static int WriteBLOCKMAP (FILE *file);
static int WriteBEHAVIOR (FILE *file);

#define APPEND(pos,name) \
	lumps[pos].FilePos = LONG(ftell (file)); \
	lumps[pos].Size = LONG(Write##name (file)); \
	memcpy (lumps[pos].Name, #name, sizeof(#name)-1);

CCMD (dumpmap)
{
	char *mapname;
	FILE *file;

	if (argv.argc() < 2)
	{
		Printf ("Usage: dumpmap <wadname> [mapname]\n");
		return;
	}

	if (argv.argc() < 3)
	{
		if (gameinfo.flags & GI_MAPxx)
		{
			mapname = "MAP01";
		}
		else
		{
			mapname = "E1M1";
		}
	}
	else
	{
		mapname = argv[2];
	}

	file = fopen (argv[1], "wb");
	if (file == NULL)
	{
		Printf ("Cannot write %s\n", argv[1]);
		return;
	}

	wadinfo_t header = { PWAD_ID, 12 };
	wadlump_t lumps[12] = { {0} };

	fseek (file, 12, SEEK_SET);
	
	lumps[0].FilePos = LONG(12);
	lumps[0].Size = 0;
	uppercopy (lumps[0].Name, mapname);

	APPEND(1, THINGS);
	APPEND(2, LINEDEFS);
	APPEND(3, SIDEDEFS);
	APPEND(4, VERTEXES);
	APPEND(5, SEGS);
	APPEND(6, SSECTORS);
	APPEND(7, NODES);
	APPEND(8, SECTORS);
	APPEND(9, REJECT);
	APPEND(10, BLOCKMAP);
	APPEND(11, BEHAVIOR);

	header.InfoTableOfs = ftell (file);

	fwrite (lumps, 16, 12, file);
	fseek (file, 0, SEEK_SET);
	fwrite (&header, 12, 1, file);

	fclose (file);
}

static int WriteTHINGS (FILE *file)
{
	mapthing2_t mt = { 0 };
	AActor *mo = players[consoleplayer].mo;

	mt.x = SHORT(short(mo->x >> FRACBITS));
	mt.y = SHORT(short(mo->y >> FRACBITS));
	mt.angle = SHORT(short(MulScale32 (mo->angle >> ANGLETOFINESHIFT, 360)));
	mt.type = SHORT(1);
	mt.flags = SHORT(7|224|MTF_SINGLE);
	fwrite (&mt, sizeof(mt), 1, file);
	return sizeof (mt);
}

static int WriteLINEDEFS (FILE *file)
{
	maplinedef2_t mld;

	for (int i = 0; i < numlines; ++i)
	{
		mld.v1 = SHORT(short(lines[i].v1 - vertexes));
		mld.v2 = SHORT(short(lines[i].v2 - vertexes));
		mld.flags = SHORT(lines[i].flags);
		mld.special = lines[i].special;
		for (int j = 0; j < 5; ++j)
		{
			mld.args[j] = (BYTE)lines[i].args[j];
		}
		mld.sidenum[0] = SHORT(lines[i].sidenum[0]);
		mld.sidenum[1] = SHORT(lines[i].sidenum[1]);
		fwrite (&mld, sizeof(mld), 1, file);
	}
	return numlines * sizeof(mld);
}

static int WriteSIDEDEFS (FILE *file)
{
	mapsidedef_t msd;

	for (int i = 0; i < numsides; ++i)
	{
		msd.textureoffset = SHORT(short(sides[i].textureoffset >> FRACBITS));
		msd.rowoffset = SHORT(short(sides[i].rowoffset >> FRACBITS));
		msd.sector = SHORT(short(sides[i].sector - sectors));
		uppercopy (msd.toptexture, R_GetTextureName (sides[i].toptexture));
		uppercopy (msd.bottomtexture, R_GetTextureName (sides[i].bottomtexture));
		uppercopy (msd.midtexture, R_GetTextureName (sides[i].midtexture));
		fwrite (&msd, sizeof(msd), 1, file);
	}
	return numsides * sizeof(msd);
}

static int WriteVERTEXES (FILE *file)
{
	mapvertex_t mv;

	for (int i = 0; i < numvertexes; ++i)
	{
		mv.x = SHORT(short(vertexes[i].x >> FRACBITS));
		mv.y = SHORT(short(vertexes[i].y >> FRACBITS));
		fwrite (&mv, sizeof(mv), 1, file);
	}
	return numvertexes * sizeof(mv);
}

static int WriteSEGS (FILE *file)
{
	mapseg_t ms;

	ms.offset = 0;		// unused by ZDoom, so just leave it 0
	for (int i = 0; i < numsegs; ++i)
	{
		ms.v1 = SHORT(short(segs[i].v1 - vertexes));
		ms.v2 = SHORT(short(segs[i].v2 - vertexes));
		ms.linedef = SHORT(short(segs[i].linedef - lines));
		ms.side = SHORT(segs[i].sidedef - sides == segs[i].linedef->sidenum[0] ? 0 : 1);
		ms.angle = SHORT(short(R_PointToAngle2 (segs[i].v1->x, segs[i].v1->y, segs[i].v2->x, segs[i].v2->y)>>16));
		fwrite (&ms, sizeof(ms), 1, file);
	}
	return numsegs * sizeof(ms);
}

static int WriteSSECTORS (FILE *file)
{
	mapsubsector_t mss;

	for (int i = 0; i < numsubsectors; ++i)
	{
		mss.firstseg = SHORT((WORD)subsectors[i].firstline);
		mss.numsegs = SHORT((WORD)subsectors[i].numlines);
		fwrite (&mss, sizeof(mss), 1, file);
	}
	return numsubsectors * sizeof(mss);
}

static int WriteNODES (FILE *file)
{
	mapnode_t mn;

	for (int i = 0; i < numnodes; ++i)
	{
		mn.x = SHORT(short(nodes[i].x >> FRACBITS));
		mn.y = SHORT(short(nodes[i].y >> FRACBITS));
		mn.dx = SHORT(short(nodes[i].dx >> FRACBITS));
		mn.dy = SHORT(short(nodes[i].dy >> FRACBITS));
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				mn.bbox[j][k] = SHORT(short(nodes[i].bbox[j][k] >> FRACBITS));
			}
			WORD child;
			if ((size_t)nodes[i].children[j] & 1)
			{
				child = NF_SUBSECTOR | ((subsector_t *)((byte *)nodes[i].children[j] - 1) - subsectors);
			}
			else
			{
				child = (node_t *)nodes[i].children[j] - nodes;
			}
			mn.children[j] = SHORT(child);
		}
		fwrite (&mn, sizeof(mn), 1, file);
	}
	return numnodes * sizeof(mn);
}

static int WriteSECTORS (FILE *file)
{
	mapsector_t ms;

	for (int i = 0; i < numsectors; ++i)
	{
		ms.floorheight = SHORT(short(sectors[i].floortexz >> FRACBITS));
		ms.ceilingheight = SHORT(short(sectors[i].ceilingtexz >> FRACBITS));
		W_GetLumpName (ms.floorpic, firstflat + sectors[i].floorpic);
		W_GetLumpName (ms.ceilingpic, firstflat + sectors[i].ceilingpic);
		ms.lightlevel = SHORT(sectors[i].lightlevel);
		ms.special = SHORT(sectors[i].special);
		ms.tag = SHORT(sectors[i].tag);
		fwrite (&ms, sizeof(ms), 1, file);
	}
	return numsectors * sizeof(ms);
}

static int WriteREJECT (FILE *file)
{
	return 0;
}

static int WriteBLOCKMAP (FILE *file)
{
	return 0;
}

static int WriteBEHAVIOR (FILE *file)
{
	static const BYTE dummy[16] = { 'A', 'C', 'S', 0, 8 };
	fwrite (dummy, 16, 1, file);
	return 16;
}
