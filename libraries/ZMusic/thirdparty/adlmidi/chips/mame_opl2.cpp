/*
 * Interfaces over Yamaha OPL3 (ym3812) chip emulators
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

#include "mame_opl2.h"
#include "mame/opl.h"
#include <cstring>

MameOPL2::MameOPL2() :
    OPLChipBaseT()
{
    m_chip = YM3812Create(false);
    MameOPL2::setRate(m_rate);
}

MameOPL2::~MameOPL2()
{
    OPLEmul *chip_r = reinterpret_cast<OPLEmul*>(m_chip);
    delete chip_r;
}

void MameOPL2::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    OPLEmul *chip_r = reinterpret_cast<OPLEmul*>(m_chip);
    chip_r->Reset();
}

void MameOPL2::reset()
{
    OPLChipBaseT::reset();
    OPLEmul *chip_r = reinterpret_cast<OPLEmul*>(m_chip);
    chip_r->Reset();
}

void MameOPL2::writeReg(uint16_t addr, uint8_t data)
{
    OPLEmul *chip_r = reinterpret_cast<OPLEmul*>(m_chip);
    chip_r->WriteReg(addr, data);
}

void MameOPL2::nativeGenerate(int16_t *frame)
{
    OPLEmul *chip_r = reinterpret_cast<OPLEmul*>(m_chip);
    frame[0] = 0;
    chip_r->UpdateS(frame, 1);
    frame[1] = frame[0];
}

const char *MameOPL2::emulatorName()
{
    return "MAME OPL2";
}

bool MameOPL2::hasFullPanning()
{
    return false;
}

OPLChipBase::ChipType MameOPL2::chipType()
{
    return CHIPTYPE_OPL2;
}
