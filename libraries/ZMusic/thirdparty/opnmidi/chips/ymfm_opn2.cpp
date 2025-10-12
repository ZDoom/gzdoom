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

#include "ymfm_opn2.h"
#include "ymfm/ymfm_opn.h"
#include <cstring>
#include <assert.h>

YmFmOPN2::YmFmOPN2(OPNFamily f) :
    OPNChipBaseT(f),
    m_headPos(0),
    m_tailPos(0),
    m_queueCount(0)
{
    ymfm::ymfm_interface* intf = new ymfm::ymfm_interface;
    m_intf = intf;
    m_chip = new ymfm::ym2612(*intf);
    YmFmOPN2::setRate(m_rate, m_clock);
}

YmFmOPN2::~YmFmOPN2()
{
    ymfm::ym2612 *chip_r = reinterpret_cast<ymfm::ym2612*>(m_chip);
    ymfm::ymfm_interface *intf_r = reinterpret_cast<ymfm::ymfm_interface*>(m_intf);
    delete chip_r;
    delete intf_r;
}

void YmFmOPN2::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    ymfm::ym2612 *chip_r = reinterpret_cast<ymfm::ym2612*>(m_chip);
    chip_r->reset();
}

void YmFmOPN2::reset()
{
    OPNChipBaseT::reset();
    ymfm::ym2612 *chip_r = reinterpret_cast<ymfm::ym2612*>(m_chip);
    chip_r->reset();
}

void YmFmOPN2::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    Reg &back = m_queue[m_headPos++];
    back.addr = port > 0 ? addr | 0x100 : addr;
    back.data = data;

    if(m_headPos >= c_queueSize)
        m_headPos = 0;

    ++m_queueCount;
    assert(m_queueCount < (long)c_queueSize);
}

void YmFmOPN2::writePan(uint16_t addr, uint8_t data)
{
    ymfm::ym2612 *chip_r = reinterpret_cast<ymfm::ym2612*>(m_chip);
    chip_r->write_pan(addr, data);
}

void YmFmOPN2::nativeGenerate(int16_t *frame)
{
    ymfm::ym2612 *chip_r = reinterpret_cast<ymfm::ym2612*>(m_chip);
    ymfm::ym2612::output_data frames_i;

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
    frame[0] = static_cast<int16_t>(ymfm::clamp(static_cast<int32_t>(frames_i.data[0] * 0.8f), -32768, 32767));
    frame[1] = static_cast<int16_t>(ymfm::clamp(static_cast<int32_t>(frames_i.data[1] * 0.8f), -32768, 32767));
}

const char *YmFmOPN2::emulatorName()
{
    return "YMFM OPN2";
}

bool YmFmOPN2::hasFullPanning()
{
    return true;
}
