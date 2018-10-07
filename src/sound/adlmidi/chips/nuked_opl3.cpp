/*
 * Interfaces over Yamaha OPL3 (YMF262) chip emulators
 *
 * Copyright (C) 2017-2018 Vitaly Novichkov (Wohlstand)
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

#include "nuked_opl3.h"
#include "nuked/nukedopl3.h"
#include <cstring>

NukedOPL3::NukedOPL3() :
    OPLChipBaseT()
{
    m_chip = new opl3_chip;
    setRate(m_rate);
}

NukedOPL3::~NukedOPL3()
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    delete chip_r;
}

void NukedOPL3::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3_Reset(chip_r, rate);
}

void NukedOPL3::reset()
{
    OPLChipBaseT::reset();
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(opl3_chip));
    OPL3_Reset(chip_r, m_rate);
}

void NukedOPL3::writeReg(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_WriteRegBuffered(chip_r, addr, data);
}

void NukedOPL3::writePan(uint16_t addr, uint8_t data)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_WritePan(chip_r, addr, data);
}

void NukedOPL3::nativeGenerate(int16_t *frame)
{
    opl3_chip *chip_r = reinterpret_cast<opl3_chip*>(m_chip);
    OPL3_Generate(chip_r, frame);
}

const char *NukedOPL3::emulatorName()
{
    return "Nuked OPL3 (v 1.8)";
}
