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

#ifndef YMFM_OPNA_H
#define YMFM_OPNA_H

#include "opn_chip_base.h"

struct YmFmOPNA_Private;

class YmFmOPNA final : public OPNChipBaseT<YmFmOPNA>
{
    friend struct YmFmOPNA_Private;
    void *m_chip;
    void *m_intf;
    void *m_output;

    static const size_t c_queueSize = 2048;

    struct Reg
    {
        uint32_t addr;
        uint8_t data;
    };

    YmFmOPNA_Private *p;

public:
    explicit YmFmOPNA(OPNFamily f);
    ~YmFmOPNA() override;

    bool canRunAtPcmRate() const override { return false; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void writePan(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
    bool hasFullPanning() override;
};

#endif // YMFM_OPNA_H
