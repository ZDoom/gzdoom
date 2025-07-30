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

#include "mame_opn2.h"
#include "mame/mame_ym2612fm.h"
#include <cstdlib>
#include <assert.h>

MameOPN2::MameOPN2(OPNFamily f)
    : OPNChipBaseT(f)
{
    chip = NULL;
    MameOPN2::setRate(m_rate, m_clock);
}

MameOPN2::~MameOPN2()
{
    ym2612_shutdown(chip);
}

void MameOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    if(chip)
        ym2612_shutdown(chip);
    uint32_t chipRate = isRunningAtPcmRate() ? rate : nativeRate();
    chip = ym2612_init(NULL, (int)clock, (int)chipRate, NULL, NULL);
    ym2612_reset_chip(chip);
}

void MameOPN2::reset()
{
    OPNChipBaseT::reset();
    ym2612_reset_chip(chip);
}

void MameOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    ym2612_write(chip, 0 + (int)(port) * 2, (uint8_t)addr);
    ym2612_write(chip, 1 + (int)(port) * 2, data);
}

void MameOPN2::writePan(uint16_t chan, uint8_t data)
{
    ym2612_write_pan(chip, (int)chan, data);
}

void MameOPN2::nativePreGenerate()
{
    void *chip = this->chip;
    ym2612_pre_generate(chip);
}

void MameOPN2::nativeGenerate(int16_t *frame)
{
    void *chip = this->chip;
    ym2612_generate_one_native(chip, frame);
}

const char *MameOPN2::emulatorName()
{
    return "MAME YM2612";
}
