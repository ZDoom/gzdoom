/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2017 Christoph Oelckers
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

#include "doomtype.h"
#include "s_sound.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "templates.h"
#include "w_wad.h"
#include "i_system.h"
#include "m_misc.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "vm.h"
#include "dobject.h"
#include "menu/menu.h"



void InitReverbMenu();
REVERB_PROPERTIES SavedProperties;
ReverbContainer *CurrentEnv;
extern ReverbContainer *ForcedEnvironment;

// These are for internal use only and not supposed to be user-settable
CVAR(String, reverbedit_name, "", CVAR_NOSET);
CVAR(Int, reverbedit_id1, 0, CVAR_NOSET);
CVAR(Int, reverbedit_id2, 0, CVAR_NOSET);
CVAR(String, reverbsavename, "", 0);

struct FReverbField
{
	int Min, Max;
	float REVERB_PROPERTIES::*Float;
	int REVERB_PROPERTIES::*Int;
	unsigned int Flag;
};


static const FReverbField ReverbFields[] =
{
	{        0,       25, 0, &REVERB_PROPERTIES::Environment, 0 },
	{     1000,   100000, &REVERB_PROPERTIES::EnvSize, 0, 0 },
	{        0,     1000, &REVERB_PROPERTIES::EnvDiffusion, 0, 0 },
	{   -10000,        0, 0, &REVERB_PROPERTIES::Room, 0 },
	{   -10000,        0, 0, &REVERB_PROPERTIES::RoomHF, 0 },
	{   -10000,        0, 0, &REVERB_PROPERTIES::RoomLF, 0 },
	{      100,    20000, &REVERB_PROPERTIES::DecayTime, 0, 0 },
	{      100,     2000, &REVERB_PROPERTIES::DecayHFRatio, 0, 0 },
	{      100,     2000, &REVERB_PROPERTIES::DecayLFRatio, 0, 0 },
	{   -10000,     1000, 0, &REVERB_PROPERTIES::Reflections, 0 },
	{        0,      300, &REVERB_PROPERTIES::ReflectionsDelay, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReflectionsPan0, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReflectionsPan1, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReflectionsPan2, 0, 0 },
	{   -10000,     2000, 0, &REVERB_PROPERTIES::Reverb, 0 },
	{        0,      100, &REVERB_PROPERTIES::ReverbDelay, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReverbPan0, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReverbPan1, 0, 0 },
	{ -2000000,  2000000, &REVERB_PROPERTIES::ReverbPan2, 0, 0 },
	{       75,      250, &REVERB_PROPERTIES::EchoTime, 0, 0 },
	{        0,     1000, &REVERB_PROPERTIES::EchoDepth, 0, 0 },
	{       40,     4000, &REVERB_PROPERTIES::ModulationTime, 0, 0 },
	{        0,     1000, &REVERB_PROPERTIES::ModulationDepth, 0, 0 },
	{  -100000,        0, &REVERB_PROPERTIES::AirAbsorptionHF, 0, 0 },
	{  1000000, 20000000, &REVERB_PROPERTIES::HFReference, 0, 0 },
	{    20000,  1000000, &REVERB_PROPERTIES::LFReference, 0, 0 },
	{        0,    10000, &REVERB_PROPERTIES::RoomRolloffFactor, 0, 0 },
	{        0,   100000, &REVERB_PROPERTIES::Diffusion, 0, 0 },
	{        0,   100000, &REVERB_PROPERTIES::Density, 0, 0 },
	{ 0, 0, 0, 0, 1 },
	{ 0, 0, 0, 0, 2 },
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 5 },
	{ 0, 0, 0, 0, 3 },
	{ 0, 0, 0, 0, 4 },
	{ 0, 0, 0, 0, 6 },
	{ 0, 0, 0, 0, 7 }
};
#define NUM_REVERB_FIELDS (int(countof(ReverbFields)))

static const char *ReverbFieldNames[NUM_REVERB_FIELDS+2] =
{
	"Environment",
	"EnvironmentSize",
	"EnvironmentDiffusion",
	"Room",
	"RoomHF",
	"RoomLF",
	"DecayTime",
	"DecayHFRatio",
	"DecayLFRatio",
	"Reflections",
	"ReflectionsDelay",
	"ReflectionsPanX",
	"ReflectionsPanY",
	"ReflectionsPanZ",
	"Reverb",
	"ReverbDelay",
	"ReverbPanX",
	"ReverbPanY",
	"ReverbPanZ",
	"EchoTime",
	"EchoDepth",
	"ModulationTime",
	"ModulationDepth",
	"AirAbsorptionHF",
	"HFReference",
	"LFReference",
	"RoomRolloffFactor",
	"Diffusion",
	"Density",
	"bReflectionsScale",
	"bReflectionsDelayScale",
	"bDecayTimeScale",
	"bDecayHFLimit",
	"bReverbScale",
	"bReverbDelayScale",
	"bEchoTimeScale",
	"bModulationTimeScale",
	"}",
	NULL
};

static const char *BoolNames[3] = { "False", "True", NULL };

static ReverbContainer DSPWater =
{
	// Based on the "off" reverb, this one uses the software water effect,
	// which is completely independant from EAX-like reverb.
	NULL,
	"DSP Water",
	0xffff,
	true,
	false,
	{0, 0,	7.5f,	1.00f, -10000, -10000, 0,   1.00f,  1.00f, 1.0f,  -2602, 0.007f, 0.0f,0.0f,0.0f,   200, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,   0.0f,   0.0f, 0x33f },
	true
};

static ReverbContainer Psychotic =
{
	&DSPWater,
	"Psychotic",
	0x1900,
	true,
	false,
	{0,25,	1.0f,	0.50f, -1000,  -151,   0,   7.56f,  0.91f, 1.0f,  -626,  0.020f, 0.0f,0.0f,0.0f,   774, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 4.00f, 1.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f },
	false
};

static ReverbContainer Dizzy =
{
	&Psychotic,
	"Dizzy",
	0x1800,
	true,
	false,
	{0,24,	1.8f,	0.60f, -1000,  -400,   0,   17.23f, 0.56f, 1.0f,  -1713, 0.020f, 0.0f,0.0f,0.0f,  -613, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 1.00f, 0.81f, 0.310f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f },
	false
};

static ReverbContainer Drugged =
{
	&Dizzy,
	"Drugged",
	0x1700,
	true,
	false,
	{0,23,	1.9f,	0.50f, -1000,  0,      0,   8.39f,  1.39f, 1.0f,  -115,  0.002f, 0.0f,0.0f,0.0f,   985, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 1.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f },
	false
};

static ReverbContainer Underwater =
{
	&Drugged,
	"Underwater",
	0x1600,
	true,
	false,
	{0,22,	1.8f,	1.00f, -1000,  -4000,  0,   1.49f,  0.10f, 1.0f,   -449, 0.007f, 0.0f,0.0f,0.0f,  1700, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 1.18f, 0.348f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer SewerPipe =
{
	&Underwater,
	"Sewer Pipe",
	0x1500,
	true,
	false,
	{0,21,	1.7f,	0.80f, -1000,  -1000,  0,   2.81f,  0.14f, 1.0f,    429, 0.014f, 0.0f,0.0f,0.0f,  1023, 0.021f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  80.0f,  60.0f, 0x3f },
	false
};

static ReverbContainer ParkingLot =
{
	&SewerPipe,
	"Parking Lot",
	0x1400,
	true,
	false,
	{0,20,	8.3f,	1.00f, -1000,  0,      0,   1.65f,  1.50f, 1.0f,  -1363, 0.008f, 0.0f,0.0f,0.0f, -1153, 0.012f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f },
	false
};

static ReverbContainer Plain =
{
	&ParkingLot,
	"Plain",
	0x1300,
	true,
	false,
	{0,19,	42.5f,	0.21f, -1000,  -2000,  0,   1.49f,  0.50f, 1.0f,  -2466, 0.179f, 0.0f,0.0f,0.0f, -1926, 0.100f, 0.0f,0.0f,0.0f, 0.250f, 1.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  21.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Quarry =
{
	&Plain,
	"Quarry",
	0x1200,
	true,
	false,
	{0,18,	17.5f,	1.00f, -1000,  -1000,  0,   1.49f,  0.83f, 1.0f, -10000, 0.061f, 0.0f,0.0f,0.0f,   500, 0.025f, 0.0f,0.0f,0.0f, 0.125f, 0.70f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Mountains =
{
	&Quarry,
	"Mountains",
	0x1100,
	true,
	false,
	{0,17,	100.0f, 0.27f, -1000,  -2500,  0,   1.49f,  0.21f, 1.0f,  -2780, 0.300f, 0.0f,0.0f,0.0f, -1434, 0.100f, 0.0f,0.0f,0.0f, 0.250f, 1.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  27.0f, 100.0f, 0x1f },
	false
};

static ReverbContainer City =
{
	&Mountains,
	"City",
	0x1000,
	true,
	false,
	{0,16,	7.5f,	0.50f, -1000,  -800,   0,   1.49f,  0.67f, 1.0f,  -2273, 0.007f, 0.0f,0.0f,0.0f, -1691, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  50.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Forest =
{
	&City,
	"Forest",
	0x0F00,
	true,
	false,
	{0,15,	38.0f,	0.30f, -1000,  -3300,  0,   1.49f,  0.54f, 1.0f,  -2560, 0.162f, 0.0f,0.0f,0.0f,  -229, 0.088f, 0.0f,0.0f,0.0f, 0.125f, 1.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,  79.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Alley =
{
	&Forest,
	"Alley",
	0x0E00,
	true,
	false,
	{0,14,	7.5f,	0.30f, -1000,  -270,   0,   1.49f,  0.86f, 1.0f,  -1204, 0.007f, 0.0f,0.0f,0.0f,    -4, 0.011f, 0.0f,0.0f,0.0f, 0.125f, 0.95f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer StoneCorridor =
{
	&Alley,
	"Stone Corridor",
	0x0D00,
	true,
	false,
	{0,13,	13.5f,	1.00f, -1000,  -237,   0,   2.70f,  0.79f, 1.0f,  -1214, 0.013f, 0.0f,0.0f,0.0f,   395, 0.020f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Hallway =
{
	&StoneCorridor,
	"Hallway",
	0x0C00,
	true,
	false,
	{0,12,	1.8f,	1.00f, -1000,  -300,   0,   1.49f,  0.59f, 1.0f,  -1219, 0.007f, 0.0f,0.0f,0.0f,   441, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer CarpettedHallway =
{
	&Hallway,
	"Carpetted Hallway",
	0x0B00,
	true,
	false,
	{0,11,	1.9f,	1.00f, -1000,  -4000,  0,   0.30f,  0.10f, 1.0f,  -1831, 0.002f, 0.0f,0.0f,0.0f, -1630, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Hangar =
{
	&CarpettedHallway,
	"Hangar",
	0x0A00,
	true,
	false,
	{0,10,	50.3f,	1.00f, -1000,  -1000,  0,   10.05f, 0.23f, 1.0f,   -602, 0.020f, 0.0f,0.0f,0.0f,   198, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Arena =
{
	&Hangar,
	"Arena",
	0x0900,
	true,
	false,
	{0, 9,	36.2f,	1.00f, -1000,  -698,   0,   7.24f,  0.33f, 1.0f,  -1166, 0.020f, 0.0f,0.0f,0.0f,    16, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Cave =
{
	&Arena,
	"Cave",
	0x0800,
	true,
	false,
	{0, 8,	14.6f,	1.00f, -1000,  0,      0,   2.91f,  1.30f, 1.0f,   -602, 0.015f, 0.0f,0.0f,0.0f,  -302, 0.022f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x1f },
	false
};

static ReverbContainer ConcertHall =
{
	&Cave,
	"Concert Hall",
	0x0700,
	true,
	false,
	{0, 7,	19.6f,	1.00f, -1000,  -500,   0,   3.92f,  0.70f, 1.0f,  -1230, 0.020f, 0.0f,0.0f,0.0f,    -2, 0.029f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Auditorium =
{
	&ConcertHall,
	"Auditorium",
	0x0600,
	true,
	false,
	{0, 6,	21.6f,	1.00f, -1000,  -476,   0,   4.32f,  0.59f, 1.0f,   -789, 0.020f, 0.0f,0.0f,0.0f,  -289, 0.030f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer StoneRoom =
{
	&Auditorium,
	"Stone Room",
	0x0500,
	true,
	false,
	{0, 5,	11.6f,	1.00f, -1000,  -300,   0,   2.31f,  0.64f, 1.0f,   -711, 0.012f, 0.0f,0.0f,0.0f,    83, 0.017f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer LivingRoom =
{
	&StoneRoom,
	"Living Room",
	0x0400,
	true,
	false,
	{0, 4,	2.5f,	1.00f, -1000,  -6000,  0,   0.50f,  0.10f, 1.0f,  -1376, 0.003f, 0.0f,0.0f,0.0f, -1104, 0.004f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Bathroom =
{
	&LivingRoom,
	"Bathroom",
	0x0300,
	true,
	false,
	{0, 3,	1.4f,	1.00f, -1000,  -1200,  0,   1.49f,  0.54f, 1.0f,   -370, 0.007f, 0.0f,0.0f,0.0f,  1030, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f,  60.0f, 0x3f },
	false
};

static ReverbContainer Room =
{
	&Bathroom,
	"Room",
	0x0200,
	true,
	false,
	{0, 2,	1.9f,	1.00f, -1000,  -454,   0,   0.40f,  0.83f, 1.0f,  -1646, 0.002f, 0.0f,0.0f,0.0f,    53, 0.003f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer PaddedCell =
{
	&Room,
	"Padded Cell",
	0x0100,
	true,
	false,
	{0, 1,	1.4f,	1.00f, -1000,  -6000,  0,   0.17f,  0.10f, 1.0f,  -1204, 0.001f, 0.0f,0.0f,0.0f,   207, 0.002f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Generic =
{
	&PaddedCell,
	"Generic",
	0x0001,
	true,
	false,
	{0, 0,	7.5f,	1.00f, -1000,  -100,   0,   1.49f,  0.83f, 1.0f,  -2602, 0.007f, 0.0f,0.0f,0.0f,   200, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 100.0f, 100.0f, 0x3f },
	false
};

static ReverbContainer Off =
{
	&Generic,
	"Off",
	0x0000,
	true,
	false,
	{0, 0,	7.5f,	1.00f, -10000, -10000, 0,   1.00f,  1.00f, 1.0f,  -2602, 0.007f, 0.0f,0.0f,0.0f,   200, 0.011f, 0.0f,0.0f,0.0f, 0.250f, 0.00f, 0.25f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f,   0.0f,   0.0f, 0x33f },
	false
};

ReverbContainer *DefaultEnvironments[26] =
{
	&Off, &PaddedCell, &Room, &Bathroom, &LivingRoom, &StoneRoom, &Auditorium,
	&ConcertHall, &Cave, &Arena, &Hangar, &CarpettedHallway, &Hallway, &StoneCorridor,
	&Alley, &Forest, &City, &Mountains, &Quarry, &Plain, &ParkingLot, &SewerPipe,
	&Underwater, &Drugged, &Dizzy, &Psychotic
};

ReverbContainer *Environments = &Off;

ReverbContainer *S_FindEnvironment (const char *name)
{
	ReverbContainer *probe = Environments;

	if (name == NULL)
		return NULL;

	while (probe != NULL)
	{
		if (stricmp (probe->Name, name) == 0)
		{
			return probe;
		}
		probe = probe->Next;
	}
	return NULL;
}

ReverbContainer *S_FindEnvironment (int id)
{
	ReverbContainer *probe = Environments;

	while (probe != NULL && probe->ID < id)
	{
		probe = probe->Next;
	}
	return (probe && probe->ID == id ? probe : NULL);
}

void S_AddEnvironment (ReverbContainer *settings)
{
	ReverbContainer *probe = Environments;
	ReverbContainer **ptr = &Environments;

	while (probe != NULL && probe->ID < settings->ID)
	{
		ptr = &probe->Next;
		probe = probe->Next;
	}

	if (probe != NULL && probe->ID == settings->ID)
	{
		// Built-in environments cannot be changed
		if (!probe->Builtin)
		{
			settings->Next = probe->Next;
			*ptr = settings;
			delete[] const_cast<char *>(probe->Name);
			delete probe;
		}
	}
	else
	{
		settings->Next = probe;
		*ptr = settings;
	}
}

static void ReadReverbDef (int lump)
{
	FScanner sc;
	const ReverbContainer *def;
	ReverbContainer *newenv;
	REVERB_PROPERTIES props;
	char *name;
	int id1, id2, i, j;
	bool inited[NUM_REVERB_FIELDS];
	uint8_t bools[32];

	sc.OpenLumpNum(lump);
	while (sc.GetString ())
	{
		name = copystring (sc.String);
		sc.MustGetNumber ();
		id1 = sc.Number;
		sc.MustGetNumber ();
		id2 = sc.Number;
		sc.MustGetStringName ("{");
		memset (inited, 0, sizeof(inited));
		props.Instance = 0;
		props.Flags = 0;
		while (sc.MustGetString (), NUM_REVERB_FIELDS > (i = sc.MustMatchString (ReverbFieldNames)))
		{
			if (ReverbFields[i].Float)
			{
				sc.MustGetFloat ();
				props.*ReverbFields[i].Float = (float)clamp (sc.Float,
					double(ReverbFields[i].Min)/1000,
					double(ReverbFields[i].Max)/1000);
			}
			else if (ReverbFields[i].Int)
			{
				sc.MustGetNumber ();
				props.*ReverbFields[i].Int = (j = clamp (sc.Number,
					ReverbFields[i].Min, ReverbFields[i].Max));
				if (i == 0 && j != sc.Number)
				{
					sc.ScriptError ("The Environment field is out of range.");
				}
			}
			else
			{
				sc.MustGetString ();
				bools[ReverbFields[i].Flag] = sc.MustMatchString (BoolNames);
			}
			inited[i] = true;
		}
		if (!inited[0])
		{
			sc.ScriptError ("Sound %s is missing an Environment field.", name);
		}

		// Add the new environment to the list, filling in uninitialized fields
		// with values from the standard environment specified.
		def = DefaultEnvironments[props.Environment];
		for (i = 0; i < NUM_REVERB_FIELDS; ++i)
		{
			if (ReverbFields[i].Float)
			{
				if (!inited[i])
				{
					props.*ReverbFields[i].Float = def->Properties.*ReverbFields[i].Float;
				}
			}
			else if (ReverbFields[i].Int)
			{
				if (!inited[i])
				{
					props.*ReverbFields[i].Int = def->Properties.*ReverbFields[i].Int;
				}
			}
			else
			{
				if (!inited[i])
				{
					int mask = 1 << ReverbFields[i].Flag;
					if (def->Properties.Flags & mask)
					{
						props.Flags |= mask;
					}
				}
				else
				{
					if (bools[ReverbFields[i].Flag])
					{
						props.Flags |= 1 << ReverbFields[i].Flag;
					}
				}
			}
		}

		newenv = new ReverbContainer;
		newenv->Next = NULL;
		newenv->Name = name;
		newenv->ID = (id1 << 8) | id2;
		newenv->Builtin = false;
		newenv->Properties = props;
		newenv->SoftwareWater = false;
		S_AddEnvironment (newenv);
	}
}

void S_ParseReverbDef ()
{
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("REVERBS", &lastlump)) != -1)
	{
		ReadReverbDef (lump);
	}
	InitReverbMenu();
}

void S_UnloadReverbDef ()
{
	ReverbContainer *probe = Environments;
	ReverbContainer **pNext = NULL;

	while (probe != NULL)
	{
		ReverbContainer *next = probe->Next;
		if (!probe->Builtin)
		{
			if (pNext != NULL) *pNext = probe->Next;
			delete[] const_cast<char *>(probe->Name);
			delete probe;
		}
		else
		{
			pNext = &probe->Next;
		}
		probe = next;
	}
	Environments = &Off;
}

CUSTOM_CVAR(Bool, eaxedit_test, false, CVAR_NOINITCALL)
{
	if (self)
	{
		ForcedEnvironment = CurrentEnv;
	}
	else
	{
		ForcedEnvironment = nullptr;
	}
}

struct EnvFlag
{
	const char *Name;
	int CheckboxControl;
	unsigned int Flag;
};

inline int HIBYTE(int i)
{
	return (i >> 8) & 255;
}

inline int LOBYTE(int i)
{
	return i & 255;
}

uint16_t FirstFreeID(uint16_t base, bool builtin)
{
	int tryCount = 0;
	int priID = HIBYTE(base);

	// If the original sound is built-in, start searching for a new
	// primary ID at 30.
	if (builtin)
	{
		for (priID = 30; priID < 256; ++priID)
		{
			if (S_FindEnvironment(priID << 8) == nullptr)
			{
				break;
			}
		}
		if (priID == 256)
		{ // Oh well.
			priID = 30;
		}
	}

	for (;;)
	{
		uint16_t lastID = Environments->ID;
		const ReverbContainer *env = Environments->Next;

		// Find the lowest-numbered free ID with the same primary ID as base
		// If none are available, add 100 to base's primary ID and try again.
		// If that fails, then the primary ID gets incremented
		// by 1 until a match is found. If all the IDs searchable by this
		// algorithm are in use, then you're in trouble.

		while (env != nullptr)
		{
			if (HIBYTE(env->ID) > priID)
			{
				break;
			}
			if (HIBYTE(env->ID) == priID)
			{
				if (HIBYTE(lastID) == priID)
				{
					if (LOBYTE(env->ID) - LOBYTE(lastID) > 1)
					{
						return lastID + 1;
					}
				}
				lastID = env->ID;
			}
			env = env->Next;
		}
		if (LOBYTE(lastID) == 255)
		{
			if (tryCount == 0)
			{
				base += 100 * 256;
				tryCount = 1;
			}
			else
			{
				base += 256;
			}
		}
		else if (builtin && lastID == 0)
		{
			return priID << 8;
		}
		else
		{
			return lastID + 1;
		}
	}
}

FString SuggestNewName(const ReverbContainer *env)
{
	const ReverbContainer *probe = nullptr;
	char text[32];
	size_t len;
	int number, numdigits;

	strncpy(text, env->Name, 31);
	text[31] = 0;

	len = strlen(text);
	while (text[len - 1] >= '0' && text[len - 1] <= '9')
	{
		len--;
	}
	number = atoi(text + len);
	if (number < 1)
	{
		number = 1;
	}

	if (text[len - 1] != ' ' && len < 31)
	{
		text[len++] = ' ';
	}

	for (; number < 100000; ++number)
	{
		if (number < 10)	numdigits = 1;
		else if (number < 100)	numdigits = 2;
		else if (number < 1000)	numdigits = 3;
		else if (number < 10000)numdigits = 4;
		else					numdigits = 5;
		if (len + numdigits > 31)
		{
			len = 31 - numdigits;
		}
		mysnprintf(text + len, countof(text) - len, "%d", number);

		probe = Environments;
		while (probe != nullptr)
		{
			if (stricmp(probe->Name, text) == 0)
				break;
			probe = probe->Next;
		}
		if (probe == nullptr)
		{
			break;
		}
	}
	return text;
}

void ExportEnvironments(const char *filename, uint32_t count, const ReverbContainer **envs)
{
	FString dest = M_GetDocumentsPath() + filename;

	FileWriter *f = FileWriter::Open(dest);

	if (f != nullptr)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const ReverbContainer *env = envs[i];
			const ReverbContainer *base;
			size_t j;

			if ((unsigned int)env->Properties.Environment < 26)
			{
				base = DefaultEnvironments[env->Properties.Environment];
			}
			else
			{
				base = nullptr;
			}
			f->Printf("\"%s\" %u %u\n{\n", env->Name, HIBYTE(env->ID), LOBYTE(env->ID));
			for (j = 0; j < countof(ReverbFields); ++j)
			{
				const FReverbField *ctl = &ReverbFields[j];
				const char *ctlName = ReverbFieldNames[j];
				if (ctlName)
				{
					if (j == 0 ||
						(ctl->Float && base->Properties.*ctl->Float != env->Properties.*ctl->Float) ||
						(ctl->Int && base->Properties.*ctl->Int != env->Properties.*ctl->Int))
					{
						f->Printf("\t%s ", ctlName);
						if (ctl->Float)
						{
							float v = env->Properties.*ctl->Float * 1000;
							int vi = int(v >= 0.0 ? v + 0.5 : v - 0.5);
							f->Printf("%d.%03d\n", vi / 1000, abs(vi % 1000));
						}
						else
						{
							f->Printf("%d\n", env->Properties.*ctl->Int);
						}
					}
					else
					{
						if ((1 << ctl->Flag) & (env->Properties.Flags ^ base->Properties.Flags))
						{
							f->Printf("\t%s %s\n", ctlName, ctl->Flag & env->Properties.Flags ? "true" : "false");
						}
					}
				}
			}
			f->Printf("}\n\n");
		}
		delete f;
	}
	else
	{
		M_StartMessage("Save failed", 1);
	}
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GetValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	float v = 0;

	if (index >= 0 && index < (int)countof(ReverbFields))
	{
		auto rev = &ReverbFields[index];
		if (rev->Int != nullptr)
		{
			v = float(CurrentEnv->Properties.*(rev->Int));
		}
		else if (rev->Float != nullptr)
		{
			v = CurrentEnv->Properties.*(rev->Float);
		}
		else
		{
			v = !!(CurrentEnv->Properties.Flags & (1 << int(rev->Flag)));
		}
	}
	ACTION_RETURN_FLOAT(v);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, SetValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	PARAM_FLOAT(v);

	if (index >= 0 && index < (int)countof(ReverbFields))
	{
		auto rev = &ReverbFields[index];
		if (rev->Int != nullptr)
		{
			v = CurrentEnv->Properties.*(rev->Int) = clamp<int>(int(v), rev->Min, rev->Max);
		}
		else if (rev->Float != nullptr)
		{
			v = CurrentEnv->Properties.*(rev->Float) = clamp<float>(float(v), rev->Min / 1000.f, rev->Max / 1000.f);
		}
		else
		{
			if (v == 0) CurrentEnv->Properties.Flags &= ~(1 << int(rev->Flag));
			else CurrentEnv->Properties.Flags |= (1 << int(rev->Flag));
		}
	}

	ACTION_RETURN_FLOAT(v);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GrayCheck)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_BOOL(CurrentEnv->Builtin);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GetSelectedEnvironment)
{
	PARAM_PROLOGUE;
	if (numret > 1)
	{
		numret = 2;
		ret[1].SetInt(CurrentEnv ? CurrentEnv->ID : -1);
	}
	if (numret > 0)
	{
		ret[0].SetString(CurrentEnv ? CurrentEnv->Name : "");
	}
	return numret;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, FillSelectMenu)
{
	PARAM_PROLOGUE;
	PARAM_STRING(ccmd);
	PARAM_OBJECT(desc, DOptionMenuDescriptor);
	desc->mItems.Clear();
	for (auto env = Environments; env != nullptr; env = env->Next)
	{
		FStringf text("(%d, %d) %s", HIBYTE(env->ID), LOBYTE(env->ID), env->Name);
		FStringf cmd("%s \"%s\"", ccmd.GetChars(), env->Name);
		PClass *cls = PClass::FindClass("OptionMenuItemCommand");
		if (cls != nullptr && cls->IsDescendantOf("OptionMenuItem"))
		{
			auto func = dyn_cast<PFunction>(cls->FindSymbol("Init", true));
			if (func != nullptr)
			{
				DMenuItemBase *item = (DMenuItemBase*)cls->CreateNew();
				VMValue params[] = { item, &text, FName(cmd).GetIndex(), false, true };
				VMCall(func->Variants[0].Implementation, params, 5, nullptr, 0);
				desc->mItems.Push((DMenuItemBase*)item);
			}
		}
	}
	return 0;
}

static TArray<std::pair<ReverbContainer*, bool>> SaveState;

DEFINE_ACTION_FUNCTION(DReverbEdit, FillSaveMenu)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(desc, DOptionMenuDescriptor);
	desc->mItems.Resize(4);
	SaveState.Clear();
	for (auto env = Environments; env != nullptr; env = env->Next)
	{
		if (!env->Builtin)
		{
			int index = (int)SaveState.Push(std::make_pair(env, false));

			FStringf text("(%d, %d) %s", HIBYTE(env->ID), LOBYTE(env->ID), env->Name);
			PClass *cls = PClass::FindClass("OptionMenuItemReverbSaveSelect");
			if (cls != nullptr && cls->IsDescendantOf("OptionMenuItem"))
			{
				auto func = dyn_cast<PFunction>(cls->FindSymbol("Init", true));
				if (func != nullptr)
				{
					DMenuItemBase *item = (DMenuItemBase*)cls->CreateNew();
					VMValue params[] = { item, &text, index, FName("OnOff").GetIndex() };
					VMCall(func->Variants[0].Implementation, params, 4, nullptr, 0);
					desc->mItems.Push((DMenuItemBase*)item);
				}
			}
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GetSaveSelection)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	bool res = false;
	if ((unsigned)index <= SaveState.Size())
	{
		res = SaveState[index].second;
	}
	ACTION_RETURN_BOOL(res);
}

DEFINE_ACTION_FUNCTION(DReverbEdit, ToggleSaveSelection)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	if ((unsigned)index <= SaveState.Size())
	{
		SaveState[index].second = !SaveState[index].second;
	}
	return 0;
}


CCMD(savereverbs)
{
	if (SaveState.Size() == 0) return;

	TArray<const ReverbContainer*> toSave;

	for (auto &p : SaveState)
	{
		if (p.second) toSave.Push(p.first);
	}
	ExportEnvironments(reverbsavename, toSave.Size(), &toSave[0]);
	SaveState.Clear();
}

static void SelectEnvironment(const char *envname)
{
	for (auto env = Environments; env != nullptr; env = env->Next)
	{
		if (!strcmp(env->Name, envname))
		{
			CurrentEnv = env;
			SavedProperties = env->Properties;
			if (eaxedit_test) ForcedEnvironment = env;

			// Set up defaults for a new environment based on this one.
			int newid = FirstFreeID(env->ID, env->Builtin);
			UCVarValue cv;
			cv.Int = HIBYTE(newid);
			reverbedit_id1.ForceSet(cv, CVAR_Int);
			cv.Int = LOBYTE(newid);
			reverbedit_id2.ForceSet(cv, CVAR_Int);
			FString selectname = SuggestNewName(env);
			cv.String = selectname.GetChars();
			reverbedit_name.ForceSet(cv, CVAR_String);
			return;
		}
	}
}

void InitReverbMenu()
{
	// Make sure that the editor's variables are properly initialized.
	SelectEnvironment("Off");
}

CCMD(selectenvironment)
{
	if (argv.argc() > 1)
	{
		auto str = argv[1];
		SelectEnvironment(str);
	}
	else
		InitReverbMenu();
}

CCMD(revertenvironment)
{
	if (CurrentEnv != nullptr)
	{
		CurrentEnv->Properties = SavedProperties;
	}
}

CCMD(createenvironment)
{
	if (S_FindEnvironment(reverbedit_name))
	{
		M_StartMessage(FStringf("An environment with the name '%s' already exists", *reverbedit_name), 1);
		return;
	}
	int id = (reverbedit_id1 << 8) + reverbedit_id2;
	if (S_FindEnvironment(id))
	{
		M_StartMessage(FStringf("An environment with the ID (%d, %d) already exists", *reverbedit_id1, *reverbedit_id2), 1);
		return;
	}

	auto newenv = new ReverbContainer;
	newenv->Builtin = false;
	newenv->ID = id;
	newenv->Name = copystring(reverbedit_name);
	newenv->Next = nullptr;
	newenv->Properties = CurrentEnv->Properties;
	S_AddEnvironment(newenv);
	SelectEnvironment(newenv->Name);
}

CCMD(reverbedit)
{
	C_DoCommand("openmenu reverbedit");
}

