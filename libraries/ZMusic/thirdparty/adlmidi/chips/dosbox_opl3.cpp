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

#include "dosbox_opl3.h"
#include "dosbox/dbopl.h"
#include <new>
#include <cstdlib>
#include <assert.h>

DosBoxOPL3::DosBoxOPL3() :
    OPLChipBaseBufferedT(),
    m_chip(new DBOPL::Handler)
{
    DosBoxOPL3::reset();
}

DosBoxOPL3::~DosBoxOPL3()
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    delete chip_r;
}

void DosBoxOPL3::globalPreInit()
{
    DBOPL::InitTables();
}

void DosBoxOPL3::setRate(uint32_t rate)
{
    OPLChipBaseBufferedT::setRate(rate);
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->~Handler();
    new(chip_r) DBOPL::Handler;
    chip_r->Init(effectiveRate());
}

void DosBoxOPL3::reset()
{
    OPLChipBaseBufferedT::reset();
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->~Handler();
    new(chip_r) DBOPL::Handler;
    chip_r->Init(effectiveRate());
}

void DosBoxOPL3::writeReg(uint16_t addr, uint8_t data)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->WriteReg(static_cast<Bit32u>(addr), data);
}

void DosBoxOPL3::writePan(uint16_t addr, uint8_t data)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    chip_r->WritePan(static_cast<Bit32u>(addr), data);
}

void DosBoxOPL3::nativeGenerateN(int16_t *output, size_t frames)
{
    DBOPL::Handler *chip_r = reinterpret_cast<DBOPL::Handler*>(m_chip);
    Bitu frames_i = frames;
    chip_r->GenerateArr(output, &frames_i);
}

const char *DosBoxOPL3::emulatorName()
{
    return "DOSBox 0.74-r4111 OPL3";
}

bool DosBoxOPL3::hasFullPanning()
{
    return true;
}

OPLChipBase::ChipType DosBoxOPL3::chipType()
{
    return CHIPTYPE_OPL3;
}
