/*
 * Interfaces over Yamaha OPN2 (YM2612) chip emulators
 *
 * Copyright (C) 2025 Vitaly Novichkov (Wohlstand)
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

#include "pmdwin_opna.h"
#include "pmdwin/opna.h"
#include <cstring>
#include <cassert>

PMDWinOPNA::PMDWinOPNA(OPNFamily f)
    : OPNChipBaseBufferedT(f)
{
    OPNA *opn = new OPNA;
    chip = reinterpret_cast<ChipType *>(opn);
    setRate(m_rate, m_clock);
}

PMDWinOPNA::~PMDWinOPNA()
{
    OPNA *opn = reinterpret_cast<OPNA *>(chip);
    delete opn;
}

void PMDWinOPNA::setRate(uint32_t rate, uint32_t clock)
{
    OPNA *opn = reinterpret_cast<OPNA *>(chip);
    OPNChipBaseBufferedT::setRate(rate, clock);

    uint32_t chipRate = isRunningAtPcmRate() ? rate : nativeRate();
    std::memset(chip, 0, sizeof(*opn));
    OPNAInit(opn, m_clock, chipRate, 0);
    OPNASetReg(opn, 0x29, 0x9f);
}

void PMDWinOPNA::reset()
{
    OPNChipBaseBufferedT::reset();
    OPNA *opn = reinterpret_cast<OPNA *>(chip);
    OPNAReset(opn);
    OPNASetReg(opn, 0x29, 0x9f);
}

void PMDWinOPNA::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    OPNA *opn = reinterpret_cast<OPNA *>(chip);
    OPNASetReg(opn, (port << 8) | addr, data);
}

void PMDWinOPNA::writePan(uint16_t chan, uint8_t data)
{
    OPNA *opn = reinterpret_cast<OPNA *>(chip);
    OPNASetPan(opn, chan, data);
}

void PMDWinOPNA::nativeGenerateN(int16_t *output, size_t frames)
{
    // be cautious to avoid overflowing stack buffer on PMDWin side!
    // (on OPNChipBaseBuffered it's fine)
    assert(frames < 16384 / 2);

    OPNA *opn = reinterpret_cast<OPNA *>(chip);
    OPNAMix(opn, output, static_cast<uint32_t>(frames));
}

const char *PMDWinOPNA::emulatorName()
{
    return "PMDWin OPNA";  // git 2018-05-11 rev 255ef52
}
