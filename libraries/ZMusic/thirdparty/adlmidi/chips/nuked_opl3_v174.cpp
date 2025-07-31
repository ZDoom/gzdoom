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

#include "nuked_opl3_v174.h"
#include "nuked/nukedopl3_174.h"
#include <cstring>

NukedOPL3v174::NukedOPL3v174() :
    OPLChipBaseT()
{
    m_chip = new opl3_chip;
    NukedOPL3v174::setRate(m_rate);
}

NukedOPL3v174::~NukedOPL3v174()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
}

void NukedOPL3v174::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3v17_Reset(chip_r, rate);
}

void NukedOPL3v174::reset()
{
    OPLChipBaseT::reset();
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3v17_Reset(chip_r, m_rate);
}

void NukedOPL3v174::writeReg(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_WriteReg(chip_r, addr, data);
}

void NukedOPL3v174::writePan(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_WritePan(chip_r, addr, data);
}

void NukedOPL3v174::nativeGenerate(int16_t *frame)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3v17_Generate(chip_r, frame);
}

const char *NukedOPL3v174::emulatorName()
{
    return "Nuked OPL3 (v 1.7.4)";
}

bool NukedOPL3v174::hasFullPanning()
{
    return true;
}

OPLChipBase::ChipType NukedOPL3v174::chipType()
{
    return CHIPTYPE_OPL3;
}
