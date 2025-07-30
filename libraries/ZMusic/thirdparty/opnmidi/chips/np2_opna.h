/*
 * Interfaces over Yamaha OPN2 (YM2612) chip emulators
 *
 * Copyright (c) 2018-2025 Vitaly Novichkov (Wohlstand)
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

#ifndef NP2_OPNA_H
#define NP2_OPNA_H

#include "opn_chip_base.h"

namespace FM { class OPN2; class OPNA; class OPNB; }
template <class ChipType = FM::OPNA>
class NP2OPNA final : public OPNChipBaseBufferedT<NP2OPNA<ChipType > >
{
    typedef OPNChipBaseBufferedT<NP2OPNA<ChipType > > ChipBase;
    ChipType *chip;
public:
    explicit NP2OPNA(OPNFamily f);
    ~NP2OPNA() override;

    bool canRunAtPcmRate() const override { return true; }
    void setRate(uint32_t rate, uint32_t clock) override;
    void reset() override;
    void writeReg(uint32_t port, uint16_t addr, uint8_t data) override;
    void writePan(uint16_t chan, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerateN(int16_t *output, size_t frames) override;
    const char *emulatorName() override;
    enum { resamplerPostAttenuate = 2 };
};

#endif
