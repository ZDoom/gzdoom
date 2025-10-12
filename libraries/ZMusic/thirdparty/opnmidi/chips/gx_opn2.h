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

#ifndef GX_OPN2_H
#define GX_OPN2_H

#include "opn_chip_base.h"

struct YM2612GX;
class GXOPN2 final : public OPNChipBaseT<GXOPN2>
{
    YM2612GX *m_chip;
    unsigned int m_framecount;
public:
    explicit GXOPN2(OPNFamily f);
    ~GXOPN2() override;

    bool canRunAtPcmRate() const override { return false; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void writePan(uint16_t chan, uint8_t data) override;
    void nativePreGenerate() override;
    void nativePostGenerate() override;
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
    bool hasFullPanning() override;
};

#endif // GX_OPN2_H
