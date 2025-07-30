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

#include "ym3812_lle.h"
#include "ym3812_lle/nuked_fmopl2.h"
#include "ym3812_lle/nopl2.h"
#include <cstring>

Ym3812LLEOPL2::Ym3812LLEOPL2() :
    OPLChipBaseT()
{
    m_chip = nopl2_init(14318182, m_rate);
    setRate(m_rate);
}

Ym3812LLEOPL2::~Ym3812LLEOPL2()
{
    fmopl2_t *chip_r = reinterpret_cast<fmopl2_t*>(m_chip);
    nopl2_shutdown(chip_r);
}

void Ym3812LLEOPL2::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    nopl2_set_rate(m_chip, 14318182, rate);
}

void Ym3812LLEOPL2::reset()
{
    OPLChipBaseT::reset();
    nopl2_reset(m_chip);
}

void Ym3812LLEOPL2::writeReg(uint16_t addr, uint8_t data)
{
    nopl2_write(m_chip, 2 * ((addr >> 8) & 3), addr);
    nopl2_write(m_chip, 1, data);
}

void Ym3812LLEOPL2::writePan(uint16_t addr, uint8_t data)
{
    (void)addr;
    (void)data;
    // Uninmplemented
}

void Ym3812LLEOPL2::nativeGenerate(int16_t *frame)
{
    nopl2_getsample_one_native(m_chip, frame);
}

const char *Ym3812LLEOPL2::emulatorName()
{
    return "YM3812-LLE OPL2";
}

bool Ym3812LLEOPL2::hasFullPanning()
{
    return false;
}

OPLChipBase::ChipType Ym3812LLEOPL2::chipType()
{
    return CHIPTYPE_OPL2;
}
