/*
 *	Name:		MUS Playing kernel
 *	Project:	MUS File Player Library
 *	Version:	1.70
 *	Author:		Vladimir Arnost (QA-Software)
 *	Last revision:	Oct-28-1995
 *	Compiler:	Borland C++ 3.1, Watcom C/C++ 10.0
 *
 */

/*
 * Revision History:
 *
 *	Aug-8-1994	V1.00	V.Arnost
 *		Written from scratch
 *	Aug-9-1994	V1.10	V.Arnost
 *		Some minor changes to improve sound quality. Tried to add
 *		stereo sound capabilities, but failed to -- my SB Pro refuses
 *		to switch to stereo mode.
 *	Aug-13-1994	V1.20	V.Arnost
 *		Stereo sound fixed. Now works also with Sound Blaster Pro II
 *		(chip OPL3 -- gives 18 "stereo" (ahem) channels).
 *		Changed code to handle properly notes without volume.
 *		(Uses previous volume on given channel.)
 *		Added cyclic channel usage to avoid annoying clicking noise.
 *	Aug-17-1994	V1.30	V.Arnost
 *		Completely rewritten time synchronization. Now the player runs
 *		on IRQ 8 (RTC Clock - 1024 Hz).
 *	Aug-28-1994	V1.40	V.Arnost
 *		Added Adlib and SB Pro II detection.
 *		Fixed bug that caused high part of 32-bit registers (EAX,EBX...)
 *		to be corrupted.
 *	Oct-30-1994	V1.50	V.Arnost
 *		Tidied up the source code
 *		Added C key - invoke COMMAND.COM
 *		Added RTC timer interrupt flag check (0000:04A0)
 *		Added BLASTER environment variable parsing
 *		FIRST PUBLIC RELEASE
 *	Apr-16-1995	V1.60	V.Arnost
 *		Moved into separate source file MUSLIB.C
 *	May-01-1995	V1.61	V.Arnost
 *		Added system timer (IRQ 0) support
 *	Jul-12-1995	V1.62	V.Arnost
 *		OPL2/OPL3-specific code moved to module MLOPL.C
 *		Module MUSLIB.C renamed to MLKERNEL.C
 *	Aug-04-1995	V1.63	V.Arnost
 *		Fixed stack-related bug occuring in big-code models in Watcom C
 *	Aug-16-1995	V1.64	V.Arnost
 *		Stack size changed from 256 to 512 words because of stack
 *		underflows caused by AWE32 driver
 *	Aug-28-1995	V1.65	V.Arnost
 *		Fixed a serious bug that caused the player to generate an
 *		exception in AWE32 driver under DOS/4GW: Register ES contained
 *		garbage instead of DGROUP. The compiler-generated prolog of
 *		interrupt handlers doesn't set ES register at all, thus any
 *		STOS/MOVS/SCAS/CMPS instruction used within the int. handler
 *		crashes the program.
 *	Oct-28-1995	V1.70	V.Arnost
 *		System-specific timer code moved separate modules
 */

#include "muslib.h"


char MLversion[] = "MUS Lib V"MLVERSIONSTR;
char MLcopyright[] = "Copyright (c) 1994-1996 QA-Software";

/* Program */
int musicBlock::playTick()
{
	int delay = 0;

    while (delay == 0)
    {
		uchar data = *score++;
		uchar command = (data >> 4) & 7;
		uchar channel = data & 0x0F;
		uchar last = data & 0x80;

		switch (command)
		{
		case 0:	// release note
			playingcount--;
			OPLreleaseNote(channel, *score++);
			break;
		case 1: {	// play note
			uchar note = *score++;
			playingcount++;
			if (note & 0x80)	// note with volume
				OPLplayNote(channel, note & 0x7F, *score++);
			else
				OPLplayNote(channel, note, -1);
				} break;
		case 2:	// pitch wheel
			// MUS pitch wheel is 8 bits, but MIDI is 14
			OPLpitchWheel(channel, *score++ << (14 - 8));
			break;
		case 3:	// system event (valueless controller)
			OPLchangeControl(channel, *score++, 0);
			break;
		case 4: {	// change control
			uchar ctrl = *score++;
			uchar value = *score++;
			OPLchangeControl(channel, ctrl, value);
				} break;
		case 6:	// end
			return 0;
		case 5:	// ???
		case 7:	// ???
			break;
		}
		if (last)
		{
			uchar t;
			
			do
			{
				t = *score++;
				delay = (delay << 7) | (t & 127);
			} while (t & 128);
		}
	}
    return delay;
}
