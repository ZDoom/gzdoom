//-----------------------------------------------------------------------------
//
// Copyright 2016-2018 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// VM thunks for simple functions.
//
//-----------------------------------------------------------------------------

#include "vm.h"
#include "r_defs.h"
#include "g_levellocals.h"
#include "s_sound.h"
#include "p_local.h"

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindShortestTextureAround, FindShortestTextureAround)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_FLOAT(FindShortestTextureAround(self));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindShortestUpperAround, FindShortestUpperAround)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_FLOAT(FindShortestUpperAround(self));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindModelFloorSector, FindModelFloorSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(fdh);
	auto h = FindModelFloorSector(self, fdh);
	ACTION_RETURN_POINTER(h);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, FindModelCeilingSector, FindModelCeilingSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(fdh);
	auto h = FindModelCeilingSector(self, fdh);
	ACTION_RETURN_POINTER(h);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetColor, SetColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_COLOR(color);
	PARAM_INT(desat);
	SetColor(self, color, desat);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetFade, SetFade)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_COLOR(fade);
	SetFade(self, fade);
	return 0;
}

static void SetSpecialColor(sector_t *self, int num, int color)
{
	if (num >= 0 && num < 5)
	{
		self->SetSpecialColor(num, color);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetSpecialColor, SetSpecialColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(num);
	PARAM_COLOR(color);
	SetSpecialColor(self, num, color);
	return 0;
}

static void SetFogDensity(sector_t *self, int dens)
{
	self->Colormap.FogDensity = dens;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetFogDensity, SetFogDensity)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(dens);
	self->Colormap.FogDensity = dens;
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PlaneMoving, PlaneMoving)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_BOOL(PlaneMoving(self, pos));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetFloorLight, GetFloorLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(self->GetFloorLight());
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetCeilingLight, GetCeilingLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(self->GetCeilingLight());
}

static sector_t *GetHeightSec(sector_t *self)
{
	return self->GetHeightSec();
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetHeightSec, GetHeightSec)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_POINTER(self->GetHeightSec());
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetSpecial, GetSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, secspecial_t);
	GetSpecial(self, spec);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetSpecial, SetSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, secspecial_t);
	SetSpecial(self, spec);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, TransferSpecial, TransferSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, sector_t);
	TransferSpecial(self, spec);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetTerrain, GetTerrain)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_INT(GetTerrain(self, pos));
}


DEFINE_ACTION_FUNCTION_NATIVE(_Sector, CheckPortalPlane, CheckPortalPlane)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(plane);
	self->CheckPortalPlane(plane);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, RemoveForceField, RemoveForceField)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	RemoveForceField(self);
	return 0;
 }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, AdjustFloorClip, AdjustFloorClip)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 AdjustFloorClip(self);
	 return 0;
 }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PointInSector, P_PointInSectorXY)
 {
	 PARAM_PROLOGUE;
	 PARAM_FLOAT(x);
	 PARAM_FLOAT(y);
	 ACTION_RETURN_POINTER(P_PointInSector(x, y));
 }

  static void SetXOffset(sector_t *self, int pos, double o)
  {
	  self->SetXOffset(pos, o);
  }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetXOffset, SetXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXOffset(pos, o);
	 return 0;
 }

 static void AddXOffset(sector_t *self, int pos, double o)
 {
	 self->AddXOffset(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, AddXOffset, AddXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->AddXOffset(pos, o);
	 return 0;
 }

 static double GetXOffset(sector_t *self, int pos)
 {
	 return self->GetXOffset(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetXOffset, GetXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetXOffset(pos));
 }

 static void SetYOffset(sector_t *self, int pos, double o)
 {
	 self->SetYOffset(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetYOffset, SetYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXOffset(pos, o);
	 return 0;
 }

 static void AddYOffset(sector_t *self, int pos, double o)
 {
	 self->AddYOffset(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, AddYOffset, AddYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->AddYOffset(pos, o);
	 return 0;
 }

 static double GetYOffset(sector_t *self, int pos)
 {
	 return self->GetYOffset(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetYOffset, GetYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_BOOL(addbase);
	 ACTION_RETURN_FLOAT(self->GetYOffset(pos, addbase));
 }

 static void SetXScale(sector_t *self, int pos, double o)
 {
	 self->SetXScale(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetXScale, SetXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXScale(pos, o);
	 return 0;
 }

 static double GetXScale(sector_t *self, int pos)
 {
	 return self->GetXScale(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetXScale, GetXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetXScale(pos));
 }

 static void SetYScale(sector_t *self, int pos, double o)
 {
	 self->SetYScale(pos, o);
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetYScale(pos, o);
	 return 0;
 }

 static double GetYScale(sector_t *self, int pos)
 {
	 return self->GetYScale(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetYScale, GetYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetYScale(pos));
 }

 static void SetAngle(sector_t *self, int pos, double o)
 {
	 self->SetAngle(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetAngle, SetAngle)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_ANGLE(o);
	 self->SetAngle(pos, o);
	 return 0;
 }

 static double GetAngle(sector_t *self, int pos, bool addbase)
 {
	 return self->GetAngle(pos, addbase).Degrees;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetAngle, GetAngle)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_BOOL(addbase);
	 ACTION_RETURN_FLOAT(self->GetAngle(pos, addbase).Degrees);
 }

 static void SetBase(sector_t *self, int pos, double o, double a)
 {
	 self->SetBase(pos, o, a);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetBase, SetBase)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 PARAM_ANGLE(a);
	 self->SetBase(pos, o, a);
	 return 0;
 }

 static void SetAlpha(sector_t *self, int pos, double o)
 {
	 self->SetAlpha(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetAlpha, SetAlpha)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetAlpha(pos, o);
	 return 0;
 }

 static double GetAlpha(sector_t *self, int pos)
 {
	 return self->GetAlpha(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetAlpha, GetAlpha)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetAlpha(pos));
 }

 static int GetFlags(sector_t *self, int pos)
 {
	 return self->GetFlags(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetFlags, GetFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetFlags(pos));
 }

 static int GetVisFlags(sector_t *self, int pos)
 {
	 return self->GetVisFlags(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetVisFlags, GetVisFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetVisFlags(pos));
 }

 static void ChangeFlags(sector_t *self, int pos, int a, int o)
 {
	 self->ChangeFlags(pos, a, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, ChangeFlags, ChangeFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(a);
	 PARAM_INT(o);
	 self->ChangeFlags(pos, a, o);
	 return 0;
 }

 static void SetPlaneLight(sector_t *self, int pos, int o)
 {
	 self->SetPlaneLight(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetPlaneLight, SetPlaneLight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(o);
	 self->SetPlaneLight(pos, o);
	 return 0;
 }

 static int GetPlaneLight(sector_t *self, int pos)
 {
	 return self->GetPlaneLight(pos);
 }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPlaneLight, GetPlaneLight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetPlaneLight(pos));
 }

  static void SetTexture(sector_t *self, int pos, int o, bool adj)
  {
	  self->SetTexture(pos, FSetTextureID(o), adj);
  }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetTexture, SetTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(o);
	 PARAM_BOOL(adj);
	 self->SetTexture(pos, FSetTextureID(o), adj);
	 return 0;
 }

  static int GetSectorTexture(sector_t *self, int pos)
  {
	  return self->GetTexture(pos).GetIndex();
  }

  DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetTexture, GetSectorTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetTexture(pos).GetIndex());
 }

  static void SetPlaneTexZ(sector_t *self, int pos, double o, bool)
  {
	  self->SetPlaneTexZ(pos, o, true);	// not setting 'dirty' here is a guaranteed cause for problems.
  }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetPlaneTexZ, SetPlaneTexZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 PARAM_BOOL(dirty);
	 self->SetPlaneTexZ(pos, o, true);	// not setting 'dirty' here is a guaranteed cause for problems.
	 return 0;
 }

 static double GetPlaneTexZ(sector_t *self, int pos)
 {
	 return self->GetPlaneTexZ(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPlaneTexZ, GetPlaneTexZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetPlaneTexZ(pos));
 }

 static void SetLightLevel(sector_t *self, int o)
 {
	 self->SetLightLevel(o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetLightLevel, SetLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(o);
	 self->SetLightLevel(o);
	 return 0;
 }

 static void ChangeLightLevel(sector_t *self, int o)
 {
	 self->ChangeLightLevel(o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, ChangeLightLevel, ChangeLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(o);
	 self->ChangeLightLevel(o);
	 return 0;
 }

 static int GetLightLevel(sector_t *self)
 {
	 return self->GetLightLevel();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetLightLevel, GetLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_INT(self->GetLightLevel());
 }

 static int PortalBlocksView(sector_t *self, int pos)
 {
	 return self->PortalBlocksView(pos);
 }
 
 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksView, PortalBlocksView)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksView(pos));
 }

 static int PortalBlocksSight(sector_t *self, int pos)
 {
	 return self->PortalBlocksSight(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksSight, PortalBlocksSight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksSight(pos));
 }

 static int PortalBlocksMovement(sector_t *self, int pos)
 {
	 return self->PortalBlocksMovement(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksMovement, PortalBlocksMovement)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksMovement(pos));
 }

 static int PortalBlocksSound(sector_t *self, int pos)
 {
	 return self->PortalBlocksSound(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalBlocksSound, PortalBlocksSound)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksSound(pos));
 }

 static int PortalIsLinked(sector_t *self, int pos)
 {
	 return self->PortalIsLinked(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, PortalIsLinked, PortalIsLinked)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalIsLinked(pos));
 }

 static void ClearPortal(sector_t *self, int pos)
 {
	 self->ClearPortal(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, ClearPortal, ClearPortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 self->ClearPortal(pos);
	 return 0;
 }

 static double GetPortalPlaneZ(sector_t *self, int pos)
 {
	 return self->GetPortalPlaneZ(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPortalPlaneZ, GetPortalPlaneZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetPortalPlaneZ(pos));
 }

 static int GetPortalType(sector_t *self, int pos)
 {
	 return self->GetPortalType(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetPortalType, GetPortalType)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetPortalType(pos));
 }

 static int GetOppositePortalGroup(sector_t *self, int pos)
 {
	 return self->GetOppositePortalGroup(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetOppositePortalGroup, GetOppositePortalGroup)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetOppositePortalGroup(pos));
 }

 static double CenterFloor(sector_t *self)
 {
	 return self->CenterFloor();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, CenterFloor, CenterFloor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_FLOAT(self->CenterFloor());
 }

 static double CenterCeiling(sector_t *self)
 {
	 return self->CenterCeiling();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, CenterCeiling, CenterCeiling)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_FLOAT(self->CenterCeiling());
 }

 static int SectorIndex(sector_t *self)
 {
	 unsigned ndx = self->Index();
	 if (ndx >= level.sectors.Size()) return -1;	// This must not throw because it is the only means to check that the given pointer is valid.
	 return ndx;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, Index, SectorIndex)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(SectorIndex(self));
 }

 static void SetEnvironmentID(sector_t *self, int envnum)
 {
	 level.Zones[self->ZoneNumber].Environment = S_FindEnvironment(envnum);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetEnvironmentID, SetEnvironmentID)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(envnum);
	SetEnvironmentID(self, envnum);
	return 0;
 }

 static void SetEnvironment(sector_t *self, const FString &env)
 {
	 level.Zones[self->ZoneNumber].Environment = S_FindEnvironment(env);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetEnvironment, SetEnvironment)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_STRING(env);
	SetEnvironment(self, env);
	return 0;
 }

 static double GetGlowHeight(sector_t *self, int pos)
 {
	 return self->GetGlowHeight(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetGlowHeight, GetGlowHeight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetGlowHeight(pos));
 }

 static double GetGlowColor(sector_t *self, int pos)
 {
	 return self->GetGlowColor(pos);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, GetGlowColor, GetGlowColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetGlowColor(pos));
 }

 static void SetGlowHeight(sector_t *self, int pos, double o)
 {
	 self->SetGlowHeight(pos, float(o));
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetGlowHeight, SetGlowHeight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetGlowHeight(pos, float(o));
	 return 0;
 }

 static void SetGlowColor(sector_t *self, int pos, int o)
 {
	 self->SetGlowColor(pos, o);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Sector, SetGlowColor, SetGlowColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_COLOR(o);
	 self->SetGlowColor(pos, o);
	 return 0;
 }

 //===========================================================================
 //
 //  line_t exports
 //
 //===========================================================================

 static int isLinePortal(line_t *self)
 {
	 return self->isLinePortal();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, isLinePortal, isLinePortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_BOOL(self->isLinePortal());
 }

 static int isVisualPortal(line_t *self)
 {
	 return self->isVisualPortal();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, isVisualPortal, isVisualPortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_BOOL(self->isVisualPortal());
 }

 static line_t *getPortalDestination(line_t *self)
 {
	 return self->getPortalDestination();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, getPortalDestination, getPortalDestination)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_POINTER(self->getPortalDestination());
 }

 static int getPortalAlignment(line_t *self)
 {
	 return self->getPortalAlignment();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, getPortalAlignment, getPortalAlignment)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_INT(self->getPortalAlignment());
 }

 static int LineIndex(line_t *self)
 {
	 unsigned ndx = self->Index();
	 if (ndx >= level.lines.Size())
	 {
		 return -1;
	 }
	 return ndx;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Line, Index, LineIndex)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_INT(LineIndex(self));
 }

 //===========================================================================
 //
 // 
 //
 //===========================================================================

 static int GetSideTexture(side_t *self, int which)
 {
	return self->GetTexture(which).GetIndex();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTexture, GetSideTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_INT(self->GetTexture(which).GetIndex());
 }

 static void SetSideTexture(side_t *self, int which, int tex)
 {
	 self->SetTexture(which, FSetTextureID(tex));
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTexture, SetSideTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_INT(tex);
	 self->SetTexture(which, FSetTextureID(tex));
	 return 0;
 }

 static void SetTextureXOffset(side_t *self, int which, double ofs)
 {
	 self->SetTextureXOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureXOffset, SetTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureXOffset(which, ofs);
	 return 0;
 }

 static void AddTextureXOffset(side_t *self, int which, double ofs)
 {
	 self->AddTextureXOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, AddTextureXOffset, AddTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->AddTextureXOffset(which, ofs);
	 return 0;
 }

 static double GetTextureXOffset(side_t *self, int which)
 {
	 return self->GetTextureXOffset(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureXOffset, GetTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureXOffset(which));
 }

 static void SetTextureYOffset(side_t *self, int which, double ofs)
 {
	 self->SetTextureYOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureYOffset, SetTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureYOffset(which, ofs);
	 return 0;
 }

 static void AddTextureYOffset(side_t *self, int which, double ofs)
 {
	 self->AddTextureYOffset(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, AddTextureYOffset, AddTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->AddTextureYOffset(which, ofs);
	 return 0;
 }

 static double GetTextureYOffset(side_t *self, int which)
 {
	 return self->GetTextureYOffset(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureYOffset, GetTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureYOffset(which));
 }

 static void SetTextureXScale(side_t *self, int which, double ofs)
 {
	 self->SetTextureXScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureXScale, SetTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureXScale(which, ofs);
	 return 0;
 }

 static void MultiplyTextureXScale(side_t *self, int which, double ofs)
 {
	 self->MultiplyTextureXScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, MultiplyTextureXScale, MultiplyTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->MultiplyTextureXScale(which, ofs);
	 return 0;
 }

 static double GetTextureXScale(side_t *self, int which)
 {
	 return self->GetTextureXScale(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureXScale, GetTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureXScale(which));
 }

 static void SetTextureYScale(side_t *self, int which, double ofs)
 {
	 self->SetTextureYScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetTextureYScale, SetTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureYScale(which, ofs);
	 return 0;
 }

 static void MultiplyTextureYScale(side_t *self, int which, double ofs)
 {
	 self->MultiplyTextureYScale(which, ofs);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, MultiplyTextureYScale, MultiplyTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->MultiplyTextureYScale(which, ofs);
	 return 0;
 }

 static double GetTextureYScale(side_t *self, int which)
 {
	 return self->GetTextureYScale(which);
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, GetTextureYScale, GetTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureYScale(which));
 }

 static vertex_t *GetSideV1(side_t *self)
 {
	 return self->V1();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, V1, GetSideV1)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_POINTER(self->V1());
 }

 static vertex_t *GetSideV2(side_t *self)
 {
	 return self->V2();
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, V2, GetSideV2)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_POINTER(self->V2());
 }

 static void SetSideSpecialColor(side_t *self, int tier, int position, int color)
 {
	 if (tier >= 0 && tier < 3 && position >= 0 && position < 2)
	 {
		 self->SetSpecialColor(tier, position, color);
	 }
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, SetSpecialColor, SetSideSpecialColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(tier);
	 PARAM_INT(position);
	 PARAM_COLOR(color);
	 SetSideSpecialColor(self, tier, position, color);
	 return 0;
 }

 static int SideIndex(side_t *self)
 {
	 unsigned ndx = self->Index();
	 if (ndx >= level.sides.Size())
	 {
		 return -1;
	 }
	 return ndx;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Side, Index, SideIndex)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_INT(SideIndex(self));
 }

 //=====================================================================================
//
//
//=====================================================================================

 static int VertexIndex(vertex_t *self)
 {
	 unsigned ndx = self->Index();
	 if (ndx >= level.vertexes.Size())
	 {
		 return -1;
	 }
	 return ndx;
 }

 DEFINE_ACTION_FUNCTION_NATIVE(_Vertex, Index, VertexIndex)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(vertex_t);
	 ACTION_RETURN_INT(VertexIndex(self));
 }

 // This is needed to convert the strings to char pointers.
 static void ReplaceTextures(const FString &from, const FString &to, int flags)
 {
	 P_ReplaceTextures(from, to, flags);
 }

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, ReplaceTextures, ReplaceTextures)
{
	PARAM_PROLOGUE;
	PARAM_STRING(from);
	PARAM_STRING(to);
	PARAM_INT(flags);
	P_ReplaceTextures(from, to, flags);
	return 0;
}

//=====================================================================================
//
//
//=====================================================================================

static int isSlope(secplane_t *self)
{
	return !self->normal.XY().isZero();
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, isSlope, isSlope)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	ACTION_RETURN_BOOL(!self->normal.XY().isZero());
}

static int PointOnSide(const secplane_t *self, double x, double y, double z)
{
	return self->PointOnSide(DVector3(x, y, z));
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, PointOnSide, PointOnSide)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_INT(self->PointOnSide(DVector3(x, y, z)));
}

static double ZatPoint(const secplane_t *self, double x, double y)
{
	return self->ZatPoint(x, y);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, ZatPoint, ZatPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	ACTION_RETURN_FLOAT(self->ZatPoint(x, y));
}

static double ZatPointDist(const secplane_t *self, double x, double y, double d)
{
	return (d + self->normal.X*x + self->normal.Y*y) * self->negiC;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, ZatPointDist, ZatPointDist)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(d);
	ACTION_RETURN_FLOAT(ZatPointDist(self, x, y, d));
}

static int isPlaneEqual(const secplane_t *self, const secplane_t *other)
{
	return *self == *other;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, isEqual, isPlaneEqual)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_POINTER(other, secplane_t);
	ACTION_RETURN_BOOL(*self == *other);
}

static void ChangeHeight(secplane_t *self, double hdiff)
{
	self->ChangeHeight(hdiff);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, ChangeHeight, ChangeHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(hdiff);
	self->ChangeHeight(hdiff);
	return 0;
}

static double GetChangedHeight(const secplane_t *self, double hdiff)
{
	return self->GetChangedHeight(hdiff);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, GetChangedHeight, GetChangedHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(hdiff);
	ACTION_RETURN_FLOAT(self->GetChangedHeight(hdiff));
}

static double HeightDiff(const secplane_t *self, double oldd, double newd)
{
	if (newd != 1e37)
	{
		return self->HeightDiff(oldd);
	}
	else
	{
		return self->HeightDiff(oldd, newd);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, HeightDiff, HeightDiff)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(oldd);
	PARAM_FLOAT(newd);
	ACTION_RETURN_FLOAT(HeightDiff(self, oldd, newd));
}

static double PointToDist(const secplane_t *self, double x, double y, double z)
{
	return self->PointToDist(DVector2(x, y), z);
}


DEFINE_ACTION_FUNCTION_NATIVE(_Secplane, PointToDist, PointToDist)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_FLOAT(self->PointToDist(DVector2(x, y), z));
}

