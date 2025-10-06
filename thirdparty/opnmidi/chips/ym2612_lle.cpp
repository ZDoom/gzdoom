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

#include "ym2612_lle.h"
#include "nuked_lle/nopn2.h"
#include <cstring>

Ym2612LLEOPN2::Ym2612LLEOPN2(OPNFamily f) :
    OPNChipBaseT(f)
{
    m_chip = nopn2_init(m_clock, m_rate);
    Ym2612LLEOPN2::setRate(m_rate, m_clock);
}

Ym2612LLEOPN2::~Ym2612LLEOPN2()
{
    nopn2_shutdown(m_chip);
}

void Ym2612LLEOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    nopn2_set_rate(m_chip, clock, rate);
}

void Ym2612LLEOPN2::reset()
{
    OPNChipBaseT::reset();
    nopn2_reset(m_chip);
}

void Ym2612LLEOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    nopn2_write_buffered(m_chip, 0 + (port) * 2, addr);
    nopn2_write_buffered(m_chip, 1 + (port) * 2, data);
}

void Ym2612LLEOPN2::writePan(uint16_t addr, uint8_t data)
{
    (void)addr;
    (void)data;
    // Uninmplemented
}

void Ym2612LLEOPN2::nativeGenerate(int16_t *frame)
{
    nopn2_getsample_one_native(m_chip, frame);
}

const char *Ym2612LLEOPN2::emulatorName()
{
    return "YM2612-LLE OPN2";
}

bool Ym2612LLEOPN2::hasFullPanning()
{
    return false;
}
