/*
 * Interfaces over Yamaha OPL3 (YMF262) chip emulators
 *
 * Copyright (c) 2017-2025 Vitaly Novichkov (Wohlstand)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef __DJGPP__
#   include <pc.h>
#endif

#include "dos_hw_opl.h"

static uint16_t                 s_OPLBase[2]    = {0x388, 0x38A};
static char                     s_devName[80]   = {0};
static OPLChipBase::ChipType    s_type          = OPLChipBase::CHIPTYPE_OPL3;
static bool                     s_detected      = false;

static void s_updateDevName()
{
    const char *oplName = (s_type == OPLChipBase::CHIPTYPE_OPL3) ? "OPL3" : "OPL2";
    memset(s_devName, 0, sizeof(s_devName));
    snprintf(s_devName, 80, "%s at 0x%03X", oplName, s_OPLBase[0]);
}

static void s_detect()
{
    if(s_detected)
        return;

    const char *blaster = getenv("BLASTER");
    if(blaster)
    {
        size_t len = strlen(blaster);
        for(size_t i = 0; i < len - 1; ++i)
        {
            if(blaster[i] == 'T')
            {
                switch(blaster[i + 1])
                {
                case '1':
                case '2':
                    s_type = OPLChipBase::CHIPTYPE_OPL2;
                    break;
                case '3':
                case '4':
                case '6':
                    s_type = OPLChipBase::CHIPTYPE_OPL3;
                    break;
                default:
                    s_type = OPLChipBase::CHIPTYPE_OPL2;
                    break;
                }

                // printf("-- Detected BLASTER T%c\n", blaster[i + 1]);

                break;
            }
        }
    }
    else
        s_type = OPLChipBase::CHIPTYPE_OPL2;

    s_detected = true;
}

DOS_HW_OPL::DOS_HW_OPL()
{
    s_detect();
    s_updateDevName();
}

void DOS_HW_OPL::setOplAddress(uint16_t address)
{
    s_OPLBase[0] = address;
    s_OPLBase[1] = address + 2;
    s_updateDevName();
}

void DOS_HW_OPL::setChipType(ChipType type)
{
    s_type = type;
    s_detected = true; // Assignd manually, no need to detect
}

DOS_HW_OPL::~DOS_HW_OPL()
{
    DOS_HW_OPL::writeReg(0x0BD, 0);
    if(s_type == CHIPTYPE_OPL3)
    {
        DOS_HW_OPL::writeReg(0x104, 0);
        DOS_HW_OPL::writeReg(0x105, 0);
    }
}

void DOS_HW_OPL::writeReg(uint16_t addr, uint8_t data)
{
    assert(m_id <= 1);
    unsigned o = addr >> 8;
    unsigned port = s_OPLBase[m_id] + o * 2;

#   ifdef __DJGPP__
    outportb(port, addr);

    for(unsigned c = 0; c < 6; ++c)
        inportb(port);

    outportb(port + 1, data);

    for(unsigned c = 0; c < 35; ++c)
        inportb(port);
#   endif

#   ifdef __WATCOMC__
    outp(port, addr);

    for(uint16_t c = 0; c < 6; ++c)
        inp(port);

    outp(port + 1, data);

    for(uint16_t c = 0; c < 35; ++c)
        inp(port);
#   endif//__WATCOMC__
}

void DOS_HW_OPL::nativeGenerate(int16_t *frame)
{
    frame[0] = 0;
    frame[1] = 0;
}

const char *DOS_HW_OPL::emulatorName()
{
    return s_devName;
}

bool DOS_HW_OPL::hasFullPanning()
{
    return false;
}

OPLChipBase::ChipType DOS_HW_OPL::chipType()
{
    return s_type;
}
