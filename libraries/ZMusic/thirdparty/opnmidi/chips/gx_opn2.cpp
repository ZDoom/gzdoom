/*
 * Interfaces over Yamaha OPN2 (YM2612) chip emulators
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

#include "gx_opn2.h"
#include <cstring>

#include "gx/gx_ym2612.h"

GXOPN2::GXOPN2(OPNFamily f)
    : OPNChipBaseT(f),
      m_chip(YM2612GXAlloc()),
      m_framecount(0)
{
    YM2612GXInit(m_chip);
    YM2612GXConfig(m_chip, YM2612_DISCRETE);
    setRate(m_rate, m_clock);
}

GXOPN2::~GXOPN2()
{
    YM2612GXFree(m_chip);
}

void GXOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    YM2612GXResetChip(m_chip);
}

void GXOPN2::reset()
{
    OPNChipBaseT::reset();
    YM2612GXResetChip(m_chip);
}

void GXOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    YM2612GXWrite(m_chip, 0 + port * 2, addr);
    YM2612GXWrite(m_chip, 1 + port * 2, data);
}

void GXOPN2::writePan(uint16_t chan, uint8_t data)
{
    YM2612GXWritePan(m_chip, chan, data);
}

void GXOPN2::nativePreGenerate()
{
    YM2612GXPreGenerate(m_chip);
    m_framecount = 0;
}

void GXOPN2::nativePostGenerate()
{
    YM2612GXPostGenerate(m_chip, m_framecount);
}

void GXOPN2::nativeGenerate(int16_t *frame)
{
    YM2612GXGenerateOneNative(m_chip, frame);
    ++m_framecount;
}

const char *GXOPN2::emulatorName()
{
    return "Genesis Plus GX";
}

bool GXOPN2::hasFullPanning()
{
    return true;
}
