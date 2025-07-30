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

#include "np2_opna.h"
#include "np2/fmgen_opna.h"
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>

template <class ChipType>
NP2OPNA<ChipType>::NP2OPNA(OPNFamily f)
    : ChipBase(f)
{
    ChipType *opn = (ChipType *)std::calloc(1, sizeof(ChipType));
    chip = new(opn) ChipType;
    opn->Init(ChipBase::m_clock, ChipBase::m_rate);
    opn->SetReg(0x29, 0x9f);  // enable channels 4-6
}

template <class ChipType>
NP2OPNA<ChipType>::~NP2OPNA()
{
    chip->~ChipType();
    std::free(chip);
}

template <class ChipType>
void NP2OPNA<ChipType>::setRate(uint32_t rate, uint32_t clock)
{
    ChipBase::setRate(rate, clock);
    uint32_t chipRate = ChipBase::isRunningAtPcmRate() ? rate : ChipBase::nativeRate();
    chip->SetRate(clock, chipRate, false);  // implies Reset()
    chip->SetReg(0x29, 0x9f);  // enable channels 4-6
}

template <class ChipType>
void NP2OPNA<ChipType>::reset()
{
    ChipBase::reset();
    chip->Reset();
    chip->SetReg(0x29, 0x9f);  // enable channels 4-6
}

template <class ChipType>
void NP2OPNA<ChipType>::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    chip->SetReg((port << 8) | addr, data);
}

template <class ChipType>
void NP2OPNA<ChipType>::writePan(uint16_t chan, uint8_t data)
{
    chip->SetPan(chan, data);
}

template <class ChipType>
void NP2OPNA<ChipType>::nativeGenerateN(int16_t *output, size_t frames)
{
    std::memset(output, 0, 2 * frames * sizeof(output[0]));
    chip->Mix(output, static_cast<int>(frames));
}

template <>
const char *NP2OPNA<FM::OPNA>::emulatorName()
{
    return "Neko Project II Kai OPNA";  // git 2018-10-28 rev e1c0609
}

template <>
const char *NP2OPNA<FM::OPNB>::emulatorName()
{
    return "Neko Project II Kai OPNB";  // git 2018-10-28 rev e1c0609
}

// template class NP2OPNA<FM::OPN2>;
template class NP2OPNA<FM::OPNA>;
template class NP2OPNA<FM::OPNB>;
