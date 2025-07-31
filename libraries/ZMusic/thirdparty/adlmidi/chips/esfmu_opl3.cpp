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

#include "esfmu_opl3.h"
#include "esfmu/esfm.h"
#include <cstring>

ESFMuOPL3::ESFMuOPL3() :
    OPLChipBaseT()
{
    m_chip = new esfm_chip;
    ESFMuOPL3::setRate(m_rate);
}

ESFMuOPL3::~ESFMuOPL3()
{
    esfm_chip *chip_r = reinterpret_cast<esfm_chip*>(m_chip);
    delete chip_r;
}

void ESFMuOPL3::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    esfm_chip *chip_r = reinterpret_cast<esfm_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(esfm_chip));
    ESFM_init(chip_r/*, rate*/);
}

void ESFMuOPL3::reset()
{
    OPLChipBaseT::reset();
    esfm_chip *chip_r = reinterpret_cast<esfm_chip*>(m_chip);
    std::memset(chip_r, 0, sizeof(esfm_chip));
    ESFM_init(chip_r/*, m_rate*/);
}

void ESFMuOPL3::writeReg(uint16_t addr, uint8_t data)
{
    esfm_chip *chip_r = reinterpret_cast<esfm_chip*>(m_chip);
    ESFM_write_reg_buffered_fast(chip_r, addr, data);
}

void ESFMuOPL3::writePan(uint16_t addr, uint8_t data)
{
    // FIXME: Implement panning support
    // esfm_chip *chip_r = reinterpret_cast<esfm_chip*>(m_chip);
    // ESFM_write?pan(chip_r, addr, data);
    (void)(addr);
    (void)(data);
}

void ESFMuOPL3::nativeGenerate(int16_t *frame)
{
    esfm_chip *chip_r = reinterpret_cast<esfm_chip*>(m_chip);
    ESFM_generate(chip_r, frame);
}

const char *ESFMuOPL3::emulatorName()
{
    return "ESFMu";
}

bool ESFMuOPL3::hasFullPanning()
{
    return false;
}

OPLChipBase::ChipType ESFMuOPL3::chipType()
{
    return CHIPTYPE_ESFM;
}
