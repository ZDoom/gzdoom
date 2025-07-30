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

#include "gens_opn2.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include "gens/Ym2612.hpp"

#ifndef INT16_MIN
#define INT16_MIN   (-0x7fff - 1)
#endif
#ifndef INT16_MAX
#define INT16_MAX   0x7fff
#endif

GensOPN2::GensOPN2(OPNFamily f)
    : OPNChipBaseBufferedT(f),
      chip(new LibGens::Ym2612())
{
    GensOPN2::setRate(m_rate, m_clock);
}

GensOPN2::~GensOPN2()
{
    delete chip;
}

void GensOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseBufferedT::setRate(rate, clock);
    uint32_t chipRate = isRunningAtPcmRate() ? rate : nativeRate();
    chip->reInit(clock, chipRate);  // implies reset()
}

void GensOPN2::reset()
{
    OPNChipBaseBufferedT::reset();
    chip->reset();
}

void GensOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
     LibGens::Ym2612 *chip = this->chip;

    switch (port)
    {
    case 0:
        chip->write(0, addr);
        chip->write(1, data);
        break;
    case 1:
        chip->write(2, addr);
        chip->write(3, data);
        break;
    }
}

void GensOPN2::writePan(uint16_t chan, uint8_t data)
{
    chip->write_pan(static_cast<int>(chan), static_cast<int>(data));
}

void GensOPN2::nativeGenerateN(int16_t *output, size_t frames)
{
    enum { maxFrames = 256 };
    assert(frames <= maxFrames);

    LibGens::Ym2612 *chip = this->chip;
    int32_t bufLR[2 * maxFrames] = {};
    int32_t *bufL = &bufLR[0];
    int32_t *bufR = &bufLR[maxFrames];

    chip->resetBufferPtrs(bufL, bufR);
    chip->addWriteLen((int)frames);
    chip->specialUpdate();

    //TODO
    // chip->updateDacAndTimers(bufL, bufR, frames);

    for (size_t i = 0; i < 2 * frames; ++i)
    {
        int32_t sample = ((i & 1) ? bufR : bufL)[i / 2];
        sample /= 4; // has too high volume, attenuation needed
        sample = (sample < INT16_MIN) ? INT16_MIN : sample;
        sample = (sample > INT16_MAX) ? INT16_MAX : sample;
        output[i] = (int16_t)sample;
    }
}

const char *GensOPN2::emulatorName()
{
    return "GENS/GS II OPN2";
}
