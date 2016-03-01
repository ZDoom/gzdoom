/*
** music_opl_mididevice.cpp
** Writes raw OPL commands from the emulated OPL MIDI output to disk.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "opl.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class OPLDump : public OPLEmul
{
public:
	OPLDump(FILE *file) : File(file), TimePerTick(0), CurTime(0),
		CurIntTime(0), TickMul(1), CurChip(0) {}

	// If we're doing things right, these should never be reset.
	virtual void Reset() { assert(0); }

	// Update() is only used for getting waveform data, which dumps don't do.
	virtual void Update(float *buffer, int length) { assert(0); }

	// OPL dumps don't pan beyond what OPL3 is capable of (which is
	// already written using registers from the original data).
	virtual void SetPanning(int c, float left, float right) {}

	// Only for the OPL dumpers, not the emulators
	virtual void SetClockRate(double samples_per_tick) {}
	virtual void WriteDelay(int ticks) = 0;

protected:
	FILE *File;
	double TimePerTick;	// in milliseconds
	double CurTime;
	int CurIntTime;
	int TickMul;
	BYTE CurChip;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

class OPL_RDOSdump : public OPLDump
{
public:
	OPL_RDOSdump(FILE *file) : OPLDump(file)
	{
		assert(File != NULL);
		fwrite("RAWADATA\0", 1, 10, File);
		NeedClockRate = true;
	}
	virtual ~OPL_RDOSdump()
	{
		if (File != NULL)
		{
			WORD endmark = 0xFFFF;
			fwrite(&endmark, 2, 1, File);
			fclose(File);
		}
	}

	virtual void WriteReg(int reg, int v)
	{
		assert(File != NULL);
		BYTE chipnum = reg >> 8;
		if (chipnum != CurChip)
		{
			BYTE switcher[2] = { (BYTE)(chipnum + 1), 2 };
			fwrite(switcher, 1, 2, File);
		}
		reg &= 255;
		if (reg != 0 && reg != 2 && (reg != 255 || v != 255))
		{
			BYTE cmd[2] = { BYTE(v), BYTE(reg) };
			fwrite(cmd, 1, 2, File);
		}
	}

	virtual void SetClockRate(double samples_per_tick)
	{
		TimePerTick = samples_per_tick / OPL_SAMPLE_RATE * 1000.0;

		double clock_rate;
		int clock_mul;
		WORD clock_word;

		clock_rate = samples_per_tick * ADLIB_CLOCK_MUL;
		clock_mul = 1;

		// The RDos raw format's clock rate is stored in a word. Therefore,
		// the longest tick that can be stored is only ~55 ms.
		while (clock_rate / clock_mul + 0.5 > 65535.0)
		{
			clock_mul++;
		}
		clock_word = WORD(clock_rate / clock_mul + 0.5);

		if (NeedClockRate)
		{ // Set the initial clock rate.
			clock_word = LittleShort(clock_word);
			fseek(File, 8, SEEK_SET);
			fwrite(&clock_word, 2, 1, File);
			fseek(File, 0, SEEK_END);
			NeedClockRate = false;
		}
		else
		{ // Change the clock rate in the middle of the song.
			BYTE clock_change[4] = { 0, 2, BYTE(clock_word & 255), BYTE(clock_word >> 8) };
			fwrite(clock_change, 1, 4, File);
		}
	}
	virtual void WriteDelay(int ticks)
	{
		if (ticks > 0)
		{ // RDos raw has very precise delays but isn't very efficient at
		  // storing long delays.
			BYTE delay[2];

			ticks *= TickMul;
			delay[1] = 0;
			while (ticks > 255)
			{
				ticks -= 255;
				delay[0] = 255;
				fwrite(delay, 1, 2, File);
			}
			delay[0] = BYTE(ticks);
			fwrite(delay, 1, 2, File);
		}
	}
protected:
	bool NeedClockRate;
};

class OPL_DOSBOXdump : public OPLDump
{
public:
	OPL_DOSBOXdump(FILE *file, bool dual) : OPLDump(file), Dual(dual)
	{
		assert(File != NULL);
		fwrite("DBRAWOPL"
			   "\0\0"		// Minor version number
			   "\1\0"		// Major version number
			   "\0\0\0\0"	// Total milliseconds
			   "\0\0\0",	// Total data
			   1, 20, File);
		char type[4] = { (char)(Dual * 2), 0, 0, 0 };	// Single or dual OPL-2
		fwrite(type, 1, 4, File);
	}
	virtual ~OPL_DOSBOXdump()
	{
		if (File != NULL)
		{
			long where_am_i = ftell(File);
			DWORD len[2];

			fseek(File, 12, SEEK_SET);
			len[0] = LittleLong(CurIntTime);
			len[1] = LittleLong(DWORD(where_am_i - 24));
			fwrite(len, 4, 2, File);
			fclose(File);
		}
	}
	virtual void WriteReg(int reg, int v)
	{
		assert(File != NULL);
		BYTE chipnum = reg >> 8;
		if (chipnum != CurChip)
		{
			CurChip = chipnum;
			fputc(chipnum + 2, File);
		}
		reg &= 255;
		BYTE cmd[3] = { 4, BYTE(reg), BYTE(v) };
		fwrite (cmd + (reg > 4), 1, 3 - (reg > 4), File);
	}
	virtual void WriteDelay(int ticks)
	{
		if (ticks > 0)
		{ // DosBox only has millisecond-precise delays.
			int delay;

			CurTime += TimePerTick * ticks;
			delay = int(CurTime + 0.5) - CurIntTime;
			CurIntTime += delay;
			while (delay > 65536)
			{
				BYTE cmd[3] = { 1, 255, 255 };
				fwrite(cmd, 1, 2, File);
				delay -= 65536;
			}
			delay--;
			if (delay <= 255)
			{
				BYTE cmd[2] = { 0, BYTE(delay) };
				fwrite(cmd, 1, 2, File);
			}
			else
			{
				assert(delay <= 65535);
				BYTE cmd[3] = { 1, BYTE(delay & 255), BYTE(delay >> 8) };
				fwrite(cmd, 1, 3, File);
			}
		}
	}
protected:
	bool Dual;
};

//==========================================================================
//
// OPLDumperMIDIDevice Constructor
//
//==========================================================================

OPLDumperMIDIDevice::OPLDumperMIDIDevice(const char *filename)
	: OPLMIDIDevice(NULL)
{
	// Replace the standard OPL device with a disk writer.
	delete io;
	io = new DiskWriterIO(filename);
}

//==========================================================================
//
// OPLDumperMIDIDevice Destructor
//
//==========================================================================

OPLDumperMIDIDevice::~OPLDumperMIDIDevice()
{
}

//==========================================================================
//
// OPLDumperMIDIDevice :: Resume
//
//==========================================================================

int OPLDumperMIDIDevice::Resume()
{
	int time;

	time = PlayTick();
	while (time != 0)
	{
		io->WriteDelay(time);
		time = PlayTick();
	}
	return 0;
}

//==========================================================================
//
// OPLDumperMIDIDevice :: Stop
//
//==========================================================================

void OPLDumperMIDIDevice::Stop()
{
}

//==========================================================================
//
// DiskWriterIO Constructor
//
//==========================================================================

DiskWriterIO::DiskWriterIO(const char *filename)
	: Filename(filename)
{
}

//==========================================================================
//
// DiskWriterIO Destructor
//
//==========================================================================

DiskWriterIO::~DiskWriterIO()
{
	OPLdeinit();
}

//==========================================================================
//
// DiskWriterIO :: OPLinit
//
//==========================================================================

int DiskWriterIO::OPLinit(uint numchips, bool, bool initopl3)
{
	FILE *file = fopen(Filename, "wb");
	if (file == NULL)
	{
		Printf("Could not open %s for writing.\n", Filename.GetChars());
		return 0;
	}

	numchips = clamp(numchips, 1u, 2u);
	memset(chips, 0, sizeof(chips));
	// If the file extension is unknown or not present, the default format
	// is RAW. Otherwise, you can use DRO.
	if (Filename.Len() < 5 || stricmp(&Filename[Filename.Len() - 4], ".dro") != 0)
	{
		chips[0] = new OPL_RDOSdump(file);
	}
	else
	{
		chips[0] = new OPL_DOSBOXdump(file, numchips > 1);
	}
	OPLchannels = OPL2CHANNELS * numchips;
	NumChips = numchips;
	IsOPL3 = numchips > 1;
	OPLwriteInitState(initopl3);
	return numchips;
}

//==========================================================================
//
// DiskWriterIO :: SetClockRate
//
//==========================================================================

void DiskWriterIO::SetClockRate(double samples_per_tick)
{
	static_cast<OPLDump *>(chips[0])->SetClockRate(samples_per_tick);
}

//==========================================================================
//
// DiskWriterIO :: WriteDelay
//
//==========================================================================

void DiskWriterIO :: WriteDelay(int ticks)
{
	static_cast<OPLDump *>(chips[0])->WriteDelay(ticks);
}
