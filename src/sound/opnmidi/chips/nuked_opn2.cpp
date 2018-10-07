/*
 * Interfaces over Yamaha OPN2 (YM2612) chip emulators
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

#include "nuked_opn2.h"
#include "nuked/ym3438.h"
#include <cstring>

NukedOPN2::NukedOPN2()
{
    OPN2_SetChipType(ym3438_type_asic);
    chip = new ym3438_t;
    setRate(m_rate, m_clock);
}

NukedOPN2::~NukedOPN2()
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    delete chip_r;
}

void NukedOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_Reset(chip_r, rate, clock);
}

void NukedOPN2::reset()
{
    OPNChipBaseT::reset();
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_Reset(chip_r, m_rate, m_clock);
}

void NukedOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_WriteBuffered(chip_r, 0 + (port) * 2, (uint8_t)addr);
    OPN2_WriteBuffered(chip_r, 1 + (port) * 2, data);
    //qDebug() << QString("%1: 0x%2 => 0x%3").arg(port).arg(addr, 2, 16, QChar('0')).arg(data, 2, 16, QChar('0'));
}

void NukedOPN2::writePan(uint16_t chan, uint8_t data)
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_WritePan(chip_r, (Bit32u)chan, data);
}

void NukedOPN2::nativeGenerate(int16_t *frame)
{
    ym3438_t *chip_r = reinterpret_cast<ym3438_t*>(chip);
    OPN2_Generate(chip_r, frame);
}

const char *NukedOPN2::emulatorName()
{
    return "Nuked OPN2";
}
