/*
** gl_shaders.cpp
** Routines parsing/managing texture shaders.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
** Copyright 2009 Christoph Oelckers
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
#include "doomtype.h"
#include "c_cvars.h"
#include "sc_man.h"
#include "textures/textures.h"
#include "gl/shaders/gl_texshader.h"

CVAR(Bool, gl_texture_useshaders, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


//==========================================================================
//
//
//
//==========================================================================

FShaderLayer::FShaderLayer()
{
	animate = false;
	emissive = false;
	blendFuncSrc = GL_SRC_ALPHA;
	blendFuncDst = GL_ONE_MINUS_SRC_ALPHA;
	offsetX = 0.f;
	offsetY = 0.f;
	centerX = 0.0f;
	centerY = 0.0f;
	rotate = 0.f;
	rotation = 0.f;
	adjustX.SetParams(0.f, 0.f, 0.f);
	adjustY.SetParams(0.f, 0.f, 0.f);
	scaleX.SetParams(1.f, 1.f, 0.f);
	scaleY.SetParams(1.f, 1.f, 0.f);
	alpha.SetParams(1.f, 1.f, 0.f);
	r.SetParams(1.f, 1.f, 0.f);
	g.SetParams(1.f, 1.f, 0.f);
	b.SetParams(1.f, 1.f, 0.f);
	flags = 0;
	layerMask = NULL;
	texgen = SHADER_TexGen_None;
	warp = false;
	warpspeed = 0;
}

//==========================================================================
//
//
//
//==========================================================================

FShaderLayer::FShaderLayer(const FShaderLayer &layer)
{
	texture = layer.texture;
	animate = layer.animate;
	emissive = layer.emissive;
	adjustX = layer.adjustX;
	adjustY = layer.adjustY;
	blendFuncSrc = layer.blendFuncSrc;
	blendFuncDst = layer.blendFuncDst;
	offsetX = layer.offsetX;
	offsetY = layer.offsetY;
	centerX = layer.centerX;
	centerY = layer.centerX;
	rotate = layer.rotate;
	rotation = layer.rotation;
	scaleX = layer.scaleX;
	scaleY = layer.scaleY;
	vectorX = layer.vectorX;
	vectorY = layer.vectorY;
	alpha = layer.alpha;
	r = layer.r;
	g = layer.g;
	b = layer.b;
	flags = layer.flags;
	if (layer.layerMask)
	{
		layerMask = new FShaderLayer(*(layer.layerMask));
	}
	else
	{
		layerMask = NULL;
	}
	texgen = layer.texgen;
	warp = layer.warp;
	warpspeed = layer.warpspeed;
}

//==========================================================================
//
//
//
//==========================================================================

FShaderLayer::~FShaderLayer()
{
   if (layerMask)
   {
      delete layerMask;
      layerMask = NULL;
   }
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderLayer::Update(float diff)
{
	r.Update(diff);
	g.Update(diff);
	b.Update(diff);
	alpha.Update(diff);
	vectorY.Update(diff);
	vectorX.Update(diff);
	scaleX.Update(diff);
	scaleY.Update(diff);
	adjustX.Update(diff);
	adjustY.Update(diff);
	srcFactor.Update(diff);
	dstFactor.Update(diff);

	offsetX += vectorX * diff;
	if (offsetX >= 1.f) offsetX -= 1.f;
	if (offsetX < 0.f) offsetX += 1.f;

	offsetY += vectorY * diff;
	if (offsetY >= 1.f) offsetY -= 1.f;
	if (offsetY < 0.f) offsetY += 1.f;

	rotation += rotate * diff;
	if (rotation > 360.f) rotation -= 360.f;
	if (rotation < 0.f) rotation += 360.f;

	if (layerMask != NULL) layerMask->Update(diff);
}

//==========================================================================
//
//
//
//==========================================================================

struct FParseKey
{
	const char *name;
	int value;
};

static const FParseKey CycleTags[]=
{
	{"linear",		CYCLE_Linear},
	{"sin",			CYCLE_Sin},
	{"cos",			CYCLE_Cos},
	{"sawtooth",	CYCLE_SawTooth},
	{"square",		CYCLE_Square},
	{NULL}
};

static const FParseKey BlendTags[]=
{
	{"GL_ZERO",					GL_ZERO},
	{"GL_ONE",					GL_ONE},

	{"GL_DST_COLOR",			GL_DST_COLOR},
	{"GL_ONE_MINUS_DST_COLOR",	GL_ONE_MINUS_DST_COLOR},
	{"GL_DST_ALPHA",			GL_DST_ALPHA},
	{"GL_ONE_MINUS_DST_ALPHA",  GL_ONE_MINUS_DST_ALPHA},

	{"GL_SRC_COLOR",			GL_SRC_COLOR},
	{"GL_ONE_MINUS_SRC_COLOR",	GL_ONE_MINUS_SRC_COLOR},
	{"GL_SRC_ALPHA",			GL_SRC_ALPHA},
	{"GL_ONE_MINUS_SRC_ALPHA",	GL_ONE_MINUS_SRC_ALPHA},

	{"GL_SRC_ALPHA_SATURATE",   GL_SRC_ALPHA_SATURATE},
	{NULL}
};


//==========================================================================
//
//
//
//==========================================================================

CycleType FShaderLayer::ParseCycleType(FScanner &sc)
{
	if (sc.GetString())
	{
		int t = sc.MatchString(&CycleTags[0].name, sizeof(CycleTags[0]));
		if (t > -1) return CycleType(CycleTags[t].value);
		sc.UnGet();
	}
	return CYCLE_Linear;
}

//==========================================================================
//
//
//
//==========================================================================

bool FShaderLayer::ParseLayer(FScanner &sc)
{
	bool retval = true;
	float start, end, cycle, r1, r2, g1, g2, b1, b2;
	int type;

	if (sc.GetString())
	{
		texture = TexMan.CheckForTexture(sc.String, ETextureType::Wall);
		if (!texture.isValid())
		{
			sc.ScriptMessage("Unknown texture '%s'", sc.String);
			retval = false;
		}
		sc.MustGetStringName("{");
		while (!sc.CheckString("}"))
		{
			if (sc.End)
			{
				sc.ScriptError("Unexpected end of file encountered");
				return false;
			}

			if (sc.Compare("alpha"))
			{
				if (sc.CheckString("cycle"))
				{
					alpha.ShouldCycle(true);
					alpha.SetCycleType(ParseCycleType(sc));

					sc.MustGetFloat();
					start = sc.Float;
					sc.MustGetFloat();
					end = sc.Float;
					sc.MustGetFloat();
					cycle = sc.Float;

					alpha.SetParams(start, end, cycle);
				}
				else
				{
					sc.MustGetFloat();
					alpha.SetParams(float(sc.Float), float(sc.Float), 0.f);
				}
			}
			else if (sc.Compare("srcfactor"))
			{
				if (sc.CheckString("cycle"))
				{
					srcFactor.ShouldCycle(true);
					srcFactor.SetCycleType(ParseCycleType(sc));

					sc.MustGetFloat();
					start = sc.Float;
					sc.MustGetFloat();
					end = sc.Float;
					sc.MustGetFloat();
					cycle = sc.Float;

					srcFactor.SetParams(start, end, cycle);
				}
				else
				{
					sc.MustGetFloat();
					srcFactor.SetParams(float(sc.Float), float(sc.Float), 0.f);
				}
			}
			if (sc.Compare("destfactor"))
			{
				if (sc.CheckString("cycle"))
				{
					dstFactor.ShouldCycle(true);
					dstFactor.SetCycleType(ParseCycleType(sc));

					sc.MustGetFloat();
					start = sc.Float;
					sc.MustGetFloat();
					end = sc.Float;
					sc.MustGetFloat();
					cycle = sc.Float;

					dstFactor.SetParams(start, end, cycle);
				}
				else
				{
					sc.MustGetFloat();
					dstFactor.SetParams(float(sc.Float), float(sc.Float), 0.f);
				}
			}
			else if (sc.Compare("animate"))
			{
               sc.GetString();
               animate = sc.Compare("true");
			}
			else if (sc.Compare("blendfunc"))
			{
				sc.GetString();
				type = sc.MustMatchString(&BlendTags[0].name, sizeof(BlendTags[0]));
				blendFuncSrc = type;// BlendTags[type].value;

				sc.GetString();
				type = sc.MustMatchString(&BlendTags[0].name, sizeof(BlendTags[0]));
				blendFuncDst = type; //BlendTags[type].value;
			}
			else if (sc.Compare("color"))
			{
				if (sc.CheckString("cycle"))
				{
					CycleType type = ParseCycleType(sc);
					r.ShouldCycle(true);
					g.ShouldCycle(true);
					b.ShouldCycle(true);
					r.SetCycleType(type);
					g.SetCycleType(type);
					b.SetCycleType(type);

					sc.MustGetFloat();
					r1 = float(sc.Float);
					sc.MustGetFloat();
					g1 = float(sc.Float);
					sc.MustGetFloat();
					b1 = float(sc.Float);

					// get color2
					sc.MustGetFloat();
					r2 = float(sc.Float);
					sc.MustGetFloat();
					g2 = float(sc.Float);
					sc.MustGetFloat();
					b2 = float(sc.Float);

                  // get cycle time
					sc.MustGetFloat();
					cycle = sc.Float;

					r.SetParams(r1, r2, cycle);
					g.SetParams(g1, g2, cycle);
					b.SetParams(b1, b2, cycle);
               }
               else
               {
					sc.MustGetFloat();
					r1 = float(sc.Float);
					sc.MustGetFloat();
					g1 = sc.Float;
					sc.MustGetFloat();
					b1 = sc.Float;

					r.SetParams(r1, r1, 0.f);
					g.SetParams(g1, g1, 0.f);
					b.SetParams(b1, b1, 0.f);
               }
			}
			else if (sc.Compare("center"))
			{
               sc.MustGetFloat();
               centerX = sc.Float;
               sc.MustGetFloat();
               centerY = sc.Float;
			}
			else if (sc.Compare("emissive"))
			{
				sc.GetString();
				emissive = sc.Compare("true");
			}
			else if (sc.Compare("offset"))
			{
				if (sc.CheckString("cycle"))
				{
					adjustX.ShouldCycle(true);
					adjustY.ShouldCycle(true);

					sc.MustGetFloat();
					r1 = sc.Float;
					sc.MustGetFloat();
					r2 = sc.Float;

					sc.MustGetFloat();
					g1 = sc.Float;
					sc.MustGetFloat();
					g2 = sc.Float;

					sc.MustGetFloat();
					cycle = sc.Float;

					offsetX = r1;
					offsetY = r2;

					adjustX.SetParams(0.f, g1 - r1, cycle);
					adjustY.SetParams(0.f, g2 - r2, cycle);
				}
				else
				{
					sc.MustGetFloat();
					offsetX = sc.Float;
					sc.MustGetFloat();
					offsetY = sc.Float;
				}
			}
			else if (sc.Compare("offsetfunc"))
			{
				adjustX.SetCycleType(ParseCycleType(sc));
				adjustY.SetCycleType(ParseCycleType(sc));
			}
			else if (sc.Compare("mask"))
			{
				if (layerMask != NULL) delete layerMask;
				layerMask = new FShaderLayer;
				layerMask->ParseLayer(sc);
			}
			else if (sc.Compare("rotate"))
			{
               sc.MustGetFloat();
               rotate = sc.Float;
			}
			else if (sc.Compare("rotation"))
			{
               sc.MustGetFloat();
               rotation = sc.Float;
			}
			else if (sc.Compare("scale"))
			{
				if (sc.CheckString("cycle"))
				{
					scaleX.ShouldCycle(true);
					scaleY.ShouldCycle(true);

					sc.MustGetFloat();
					r1 = sc.Float;
					sc.MustGetFloat();
					r2 = sc.Float;

					sc.MustGetFloat();
					g1 = sc.Float;
					sc.MustGetFloat();
					g2 = sc.Float;

					sc.MustGetFloat();
					cycle = sc.Float;

					scaleX.SetParams(r1, g1, cycle);
					scaleY.SetParams(r2, g2, cycle);
				}
				else
				{
					sc.MustGetFloat();
					scaleX.SetParams(sc.Float, sc.Float, 0.f);
					sc.MustGetFloat();
					scaleY.SetParams(sc.Float, sc.Float, 0.f);
				}
			}
			else if (sc.Compare("scalefunc"))
			{
				scaleX.SetCycleType(ParseCycleType(sc));
				scaleY.SetCycleType(ParseCycleType(sc));
			}
			else if (sc.Compare("texgen"))
			{
				sc.MustGetString();
				if (sc.Compare("sphere"))
				{
					texgen = SHADER_TexGen_Sphere;
				}
				else
				{
					texgen = SHADER_TexGen_None;
				}
			}
			else if (sc.Compare("vector"))
			{
				if (sc.CheckString("cycle"))
				{
					vectorX.ShouldCycle(true);
					vectorY.ShouldCycle(true);

					sc.MustGetFloat();
					r1 = sc.Float;
					sc.MustGetFloat();
					g1 = sc.Float;
					sc.MustGetFloat();
					r2 = sc.Float;
					sc.MustGetFloat();
					g2 = sc.Float;
					sc.MustGetFloat();
					cycle = sc.Float;

					vectorX.SetParams(r1, r2, cycle);
					vectorY.SetParams(g1, g2, cycle);
				}
				else
				{
					sc.MustGetFloat();
					vectorX.SetParams(sc.Float, sc.Float, 0.f);
					sc.MustGetFloat();
					vectorY.SetParams(sc.Float, sc.Float, 0.f);
				}
			}
			else if (sc.Compare("vectorfunc"))
			{
				vectorX.SetCycleType(ParseCycleType(sc));
				vectorY.SetCycleType(ParseCycleType(sc));
			}
			else if (sc.Compare("warp"))
			{
				if (sc.CheckNumber())
				{
					warp = sc.Number >= 0 && sc.Number <= 2? sc.Number : 0;
				}
				else
				{
					// compatibility with ZDoomGL
					sc.MustGetString();
					warp = sc.Compare("true");
				}
			}
			else
			{
				sc.ScriptError("Unknown keyword '%s' in shader layer", sc.String);
			}
		}
	}
	return retval;
}

//==========================================================================
//
//
//
//==========================================================================

FTextureShader::FTextureShader()
{
	layers.Clear();
	lastUpdate = 0;
}

//==========================================================================
//
//
//
//==========================================================================

bool FTextureShader::ParseShader(FScanner &sc, TArray<FTextureID> &names)
{
	bool retval = true;

	if (sc.GetString())
	{
		name = sc.String;

		sc.MustGetStringName("{");
		while (!sc.CheckString("}"))
		{
			if (sc.End)
			{
				sc.ScriptError("Unexpected end of file encountered");
				return false;
			}
			else if (sc.Compare("layer"))
			{
				FShaderLayer *lay = new FShaderLayer;
				if (lay->ParseLayer(sc))
				{
					if (layers.Size() < 8)
					{
						layers.Push(lay);
					}
					else
					{
						delete lay;
						sc.ScriptMessage("Only 8 layers per texture allowed.");
					}
				}
				else
				{
					delete lay;
					retval = false;
				}
			}
			else
			{
				sc.ScriptError("Unknown keyword '%s' in shader", sc.String);
			}
		}
	}
	return retval;
}

//==========================================================================
//
//
//
//==========================================================================

void FTextureShader::Update(int framems)
{
	float diff = (framems - lastUpdate) / 1000.f;

	if (lastUpdate != 0) // && !paused && !bglobal.freeze)
	{
		for (unsigned int i = 0; i < layers.Size(); i++)
		{
			layers[i]->Update(diff);
		}
	}
	lastUpdate = framems;
}

//==========================================================================
//
//
//
//==========================================================================

void FTextureShader::FakeUpdate(int framems)
{
	lastUpdate = framems;
}

//==========================================================================
//
//
//
//==========================================================================

FString FTextureShader::CreateName()
{
	FString compose = "custom";
	for(unsigned i=0; i<layers.Size(); i++)
	{
		compose.AppendFormat("@%de%ds%ud%ut%dw%d", i, layers[i]->emissive, 
			layers[i]->blendFuncSrc, layers[i]->blendFuncDst, layers[i]->texgen, layers[i]->warp);
	}
	return compose;
}

//==========================================================================
//
//
//
//==========================================================================

FString FTextureShader::GenerateCode()
{
	static const char *funcnames[] = {"gettexel", "getwarp1", "getwarp2" };
	static const char *srcblend[] = { "vec4(0.0)", "src", "src*dest", "1.0-src*dest", "src*dest.a", "1.0-src*dest.a", 
										"src*src", "1.0-src*src", "src*src.a", "1.0-src*src", "vec4(src.rgb*src.a, 1)" };
	static const char *dstblend[] = { "vec4(0.0)", "dest", "dest*dest", "1.0-dest*dest", "dest*dest.a", "1.0-dest*dest.a", 
										"dest*src", "1.0-dest*src", "dest*src.a", "1.0-dest*src", "vec4(dest.rgb*src.a, 1)" };
	FString compose;
	for(unsigned i=0; i<layers.Size(); i++)
	{
		compose.AppendFormat("src = %s(texture%d, glTexCoord[%d].st) * colors[%d];\n",
			funcnames[layers[i]->warp], i+1, i, i);
		if (!layers[i]->emissive) compose.AppendFormat("src.rgb *= gl_Color.rgb;\n");
		compose.AppendFormat("dest = (%s)*srcfactor + (%s)*dstfactor;\n", 
			srcblend[layers[i]->blendFuncSrc], dstblend[layers[i]->blendFuncDst]);
	}
	return compose;
}

