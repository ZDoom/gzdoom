/*
 * Interfaces over Yamaha OPL3 (ym3812) chip emulators
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

#include "ymfm_opl2.h"
#include "ymfm/ymfm_opl.h"
#include <cstring>

YmFmOPL2::YmFmOPL2() :
    OPLChipBaseT(),
    m_headPos(0),
    m_tailPos(0),
    m_queueCount(0)
{
    ymfm::ymfm_interface* intf = new ymfm::ymfm_interface;
    m_intf = intf;
    m_chip = new ymfm::ym3812(*intf);
    YmFmOPL2::setRate(m_rate);
}

YmFmOPL2::~YmFmOPL2()
{
    ymfm::ym3812 *chip_r = reinterpret_cast<ymfm::ym3812*>(m_chip);
    ymfm::ymfm_interface *intf_r = reinterpret_cast<ymfm::ymfm_interface*>(m_intf);
    delete chip_r;
    delete intf_r;
}

void YmFmOPL2::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    ymfm::ym3812 *chip_r = reinterpret_cast<ymfm::ym3812*>(m_chip);
    chip_r->reset();
}

void YmFmOPL2::reset()
{
    OPLChipBaseT::reset();
    ymfm::ym3812 *chip_r = reinterpret_cast<ymfm::ym3812*>(m_chip);
    chip_r->reset();
}

void YmFmOPL2::writeReg(uint16_t addr, uint8_t data)
{
    Reg &back = m_queue[m_headPos++];
    back.addr = addr;
    back.data = data;

    if(m_headPos >= c_queueSize)
        m_headPos = 0;

    ++m_queueCount;
}

void YmFmOPL2::nativeGenerate(int16_t *frame)
{
    ymfm::ym3812 *chip_r = reinterpret_cast<ymfm::ym3812*>(m_chip);
    ymfm::ym3812::output_data frames_i;

    uint32_t addr1 = 0xffff, addr2 = 0xffff;
    uint8_t data1 = 0, data2 = 0;

    // see if there is data to be written; if so, extract it and dequeue
    if(m_queueCount > 0)
    {
        const Reg &front = m_queue[m_tailPos++];

        if(m_tailPos >= c_queueSize)
            m_tailPos = 0;
        --m_queueCount;

        addr1 = 0 + 2 * ((front.addr >> 8) & 3);
        data1 = front.addr & 0xff;
        addr2 = addr1 + 1;
        data2 = front.data;
    }

    // write to the chip
    if(addr1 != 0xffff)
    {
        chip_r->write(addr1, data1);
        chip_r->write(addr2, data2);
    }

    chip_r->generate(&frames_i);
    frame[0] = static_cast<int16_t>(ymfm::clamp(frames_i.data[0], -32768, 32767));
    frame[1] = frame[0];
}

const char *YmFmOPL2::emulatorName()
{
    return "YMFM OPL2";
}

bool YmFmOPL2::hasFullPanning()
{
    return false;
}

OPLChipBase::ChipType YmFmOPL2::chipType()
{
    return CHIPTYPE_OPL2;
}
