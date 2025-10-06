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

#ifndef YMF276LLE_OPN2_H
#define YMF276LLE_OPN2_H

#include "opn_chip_base.h"

class Ymf276LLEOPN2 final : public OPNChipBaseT<Ymf276LLEOPN2>
{
    void *m_chip;
    bool m_ymf276mode;

public:
    Ymf276LLEOPN2(OPNFamily f, bool ymf276mode);
    ~Ymf276LLEOPN2() override;

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
    // amplitude scale factors to use in resampling
    enum { resamplerPreAmplify = 11, resamplerPostAttenuate = 2 };
};

#endif // YMF276LLE_OPN2_H
