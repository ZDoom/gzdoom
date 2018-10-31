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

#include "gens_opn2.h"
#include <cstring>

#include "gens/Ym2612_Emu.h"

GensOPN2::GensOPN2()
    : chip(new Ym2612_Emu())
{
    setRate(m_rate, m_clock);
}

GensOPN2::~GensOPN2()
{
    delete chip;
}

void GensOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseBufferedT::setRate(rate, clock);
    uint32_t chipRate = isRunningAtPcmRate() ? rate : static_cast<uint32_t>(nativeRate);
    chip->set_rate(chipRate, clock);  // implies reset()
}

void GensOPN2::reset()
{
    OPNChipBaseBufferedT::reset();
    chip->reset();
}

void GensOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    switch (port)
    {
    case 0:
        chip->write0(addr, data);
        break;
    case 1:
        chip->write1(addr, data);
        break;
    }
}

void GensOPN2::writePan(uint16_t chan, uint8_t data)
{
    chip->write_pan(static_cast<int>(chan), static_cast<int>(data));
}

void GensOPN2::nativeGenerateN(int16_t *output, size_t frames)
{
    std::memset(output, 0, frames * sizeof(int16_t) * 2);
    chip->run((int)frames, output);
}

const char *GensOPN2::emulatorName()
{
    return "GENS 2.10 OPN2";
}
