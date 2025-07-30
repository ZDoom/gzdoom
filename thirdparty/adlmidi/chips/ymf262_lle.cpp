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

#include "ymf262_lle.h"
#include "ymf262_lle/nuked_fmopl3.h"
#include "ymf262_lle/nopl3.h"
#include <cstring>

Ymf262LLEOPL3::Ymf262LLEOPL3() :
    OPLChipBaseT()
{
    m_chip = nopl3_init(14318182, m_rate);
    setRate(m_rate);
}

Ymf262LLEOPL3::~Ymf262LLEOPL3()
{
    fmopl3_t *chip_r = reinterpret_cast<fmopl3_t*>(m_chip);
    nopl3_shutdown(chip_r);
}

void Ymf262LLEOPL3::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    nopl3_set_rate(m_chip, 14318182, rate);
}

void Ymf262LLEOPL3::reset()
{
    OPLChipBaseT::reset();
    nopl3_reset(m_chip);
}

void Ymf262LLEOPL3::writeReg(uint16_t addr, uint8_t data)
{
    nopl3_write(m_chip, 2 * ((addr >> 8) & 3), addr);
    nopl3_write(m_chip, 1, data);
}

void Ymf262LLEOPL3::writePan(uint16_t addr, uint8_t data)
{
    (void)addr;
    (void)data;
    // Uninmplemented
}

void Ymf262LLEOPL3::nativeGenerate(int16_t *frame)
{
    nopl3_getsample_one_native(m_chip, frame);
}

const char *Ymf262LLEOPL3::emulatorName()
{
    return "YMF262-LLE OPL3";
}

bool Ymf262LLEOPL3::hasFullPanning()
{
    return false;
}

OPLChipBase::ChipType Ymf262LLEOPL3::chipType()
{
    return CHIPTYPE_OPL3;
}
