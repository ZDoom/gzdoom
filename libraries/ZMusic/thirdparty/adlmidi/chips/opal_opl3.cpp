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

#include "opal_opl3.h"
#include "opal/opal.h"
#include <cstring>

OpalOPL3::OpalOPL3() :
    OPLChipBaseT()
{
    m_chip = new Opal;
    OpalOPL3::setRate(m_rate);
}

OpalOPL3::~OpalOPL3()
{
    Opal *chip_r = reinterpret_cast<Opal *>(m_chip);
    delete chip_r;
}

void OpalOPL3::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    Opal *chip_r = reinterpret_cast<Opal *>(m_chip);
    std::memset(chip_r, 0, sizeof(Opal));
    Opal_Init(chip_r, effectiveRate());
}

void OpalOPL3::reset()
{
    OPLChipBaseT::reset();
    Opal *chip_r = reinterpret_cast<Opal *>(m_chip);
    std::memset(chip_r, 0, sizeof(Opal));
    Opal_Init(chip_r, effectiveRate());
}

void OpalOPL3::writeReg(uint16_t addr, uint8_t data)
{
    Opal *chip_r = reinterpret_cast<Opal *>(m_chip);
    Opal_Port(chip_r, addr, data);
}

void OpalOPL3::writePan(uint16_t addr, uint8_t data)
{
    Opal *chip_r = reinterpret_cast<Opal *>(m_chip);
    Opal_Pan(chip_r, addr, data);
}

void OpalOPL3::nativeGenerate(int16_t *frame)
{
    Opal *chip_r = reinterpret_cast<Opal *>(m_chip);
    Opal_Sample(chip_r, &frame[0], &frame[1]);
}

const char *OpalOPL3::emulatorName()
{
    return "Opal OPL3";
}

bool OpalOPL3::hasFullPanning()
{
    return true;
}

OPLChipBase::ChipType OpalOPL3::chipType()
{
    return CHIPTYPE_OPL3;
}
