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

#include "java_opl3.h"
#include "java/JavaOPL3.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

JavaOPL3::JavaOPL3() :
    OPLChipBaseBufferedT(),
    m_chip(new ADL_JavaOPL3::OPL3(true))
{
    JavaOPL3::reset();
}

JavaOPL3::~JavaOPL3()
{
    ADL_JavaOPL3::OPL3 *chip_r = reinterpret_cast<ADL_JavaOPL3::OPL3 *>(m_chip);
    delete chip_r;
}

void JavaOPL3::setRate(uint32_t rate)
{
    OPLChipBaseBufferedT::setRate(rate);
    ADL_JavaOPL3::OPL3 *chip_r = reinterpret_cast<ADL_JavaOPL3::OPL3 *>(m_chip);
    chip_r->Reset();

    float pan = sinf((float)M_SQRT1_2);
    for (unsigned channel = 0; channel < 18; ++channel)
        chip_r->SetPanning(channel, pan, pan);
}

void JavaOPL3::reset()
{
    OPLChipBaseBufferedT::reset();
    ADL_JavaOPL3::OPL3 *chip_r = reinterpret_cast<ADL_JavaOPL3::OPL3 *>(m_chip);
    chip_r->Reset();
}

void JavaOPL3::writeReg(uint16_t addr, uint8_t data)
{
    ADL_JavaOPL3::OPL3 *chip_r = reinterpret_cast<ADL_JavaOPL3::OPL3 *>(m_chip);
    chip_r->WriteReg(addr, data);
}

void JavaOPL3::writePan(uint16_t addr, uint8_t data)
{
    ADL_JavaOPL3::OPL3 *chip_r = reinterpret_cast<ADL_JavaOPL3::OPL3 *>(m_chip);

    unsigned high = (addr >> 8) & 0x01;
    unsigned regm = addr & 0xff;
    unsigned channel = 9 * high + (regm & 0x0f);

    float phase = (data == 63 || data == 64) ? 63.5f : (float)data;
    phase *= (float)(M_PI / 2 / 127);
    chip_r->SetPanning(channel, cosf(phase), sinf(phase));
}

void JavaOPL3::nativeGenerateN(int16_t *output, size_t frames)
{
    ADL_JavaOPL3::OPL3 *chip_r = reinterpret_cast<ADL_JavaOPL3::OPL3 *>(m_chip);

    enum { maxframes = 256 };

    float buf[2 * maxframes];
    while(frames > 0)
    {
        memset(buf, 0, sizeof(buf));

        size_t curframes = (frames < (size_t)maxframes) ? frames : (size_t)maxframes;
        chip_r->Update(buf, (int)curframes);

        size_t cursamples = 2 * curframes;
        for(size_t i = 0; i < cursamples; ++i)
        {
            int32_t sample = (int32_t)lround(4096 * buf[i]);
            sample = (sample > -32768) ? sample : -32768;
            sample = (sample < +32767) ? sample : +32767;
            output[i] = (int16_t)sample;
        }

        output += cursamples;
        frames -= curframes;
    }
}

const char *JavaOPL3::emulatorName()
{
    return "Java 1.0.6 OPL3";
}

bool JavaOPL3::hasFullPanning()
{
    return true;
}

OPLChipBase::ChipType JavaOPL3::chipType()
{
    return CHIPTYPE_OPL3;
}
