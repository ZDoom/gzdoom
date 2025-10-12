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

#include "ymfm_opl3.h"
#include "ymfm/ymfm_opl.h"
#include <cstring>
#include <assert.h>

YmFmOPL3::YmFmOPL3() :
    OPLChipBaseT(),
    m_headPos(0),
    m_tailPos(0),
    m_queueCount(0)
{
    ymfm::ymfm_interface* intf = new ymfm::ymfm_interface;
    m_intf = intf;
    m_chip = new ymfm::ymf262(*intf);
    YmFmOPL3::setRate(m_rate);
}

YmFmOPL3::~YmFmOPL3()
{
    ymfm::ymf262 *chip_r = reinterpret_cast<ymfm::ymf262*>(m_chip);
    ymfm::ymfm_interface *intf_r = reinterpret_cast<ymfm::ymfm_interface*>(m_intf);
    delete chip_r;
    delete intf_r;
}

void YmFmOPL3::setRate(uint32_t rate)
{
    OPLChipBaseT::setRate(rate);
    ymfm::ymf262 *chip_r = reinterpret_cast<ymfm::ymf262*>(m_chip);
    chip_r->reset();
}

void YmFmOPL3::reset()
{
    OPLChipBaseT::reset();
    ymfm::ymf262 *chip_r = reinterpret_cast<ymfm::ymf262*>(m_chip);
    chip_r->reset();
}

void YmFmOPL3::writeReg(uint16_t addr, uint8_t data)
{
    Reg &back = m_queue[m_headPos++];
    back.addr = addr;
    back.data = data;

    if(m_headPos >= c_queueSize)
        m_headPos = 0;

    ++m_queueCount;
    assert(m_queueCount <= (long)c_queueSize);
}

void YmFmOPL3::nativeGenerate(int16_t *frame)
{
    ymfm::ymf262 *chip_r = reinterpret_cast<ymfm::ymf262*>(m_chip);
    ymfm::ymf262::output_data frames_i;

    uint16_t addr1 = 0xffff, addr2 = 0xffff;
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
    frame[0] = static_cast<int16_t>(ymfm::clamp(frames_i.data[0] / 2, -32768, 32767));
    frame[1] = static_cast<int16_t>(ymfm::clamp(frames_i.data[1] / 2, -32768, 32767));
}

const char *YmFmOPL3::emulatorName()
{
    return "YMFM OPL3";
}

OPLChipBase::ChipType YmFmOPL3::chipType()
{
    return CHIPTYPE_OPL3;
}

bool YmFmOPL3::hasFullPanning()
{
    return false;
}
