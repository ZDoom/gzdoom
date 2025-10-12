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

#include "ymfm_opna.h"
#include "ymfm/ymfm_opn.h"
#include <cstring>
#include <assert.h>

struct YmFmOPNA_Private
{
    YmFmOPNA::Reg m_queue[YmFmOPNA::c_queueSize];
    size_t m_headPos = 0;
    size_t m_tailPos = 0;
    long m_queueCount = 0;
    unsigned long long m_step = 0;
    unsigned long long m_pos = 0;
    unsigned long long m_out_step = 0;

    void writeReg(uint32_t port, uint16_t addr, uint8_t data);
    void nativeGenerate(int16_t *frame, void *m_chip, void *m_output);
};

YmFmOPNA::YmFmOPNA(OPNFamily f) :
    OPNChipBaseT(f),
    p(new YmFmOPNA_Private)
{
    ymfm::ymfm_interface* intf = new ymfm::ymfm_interface;
    m_intf = intf;
    m_chip = new ymfm::ym2608(*intf);
    ymfm::ym2608::output_data* output = new ymfm::ym2608::output_data;
    output->clear();
    m_output = output;
    YmFmOPNA::setRate(m_rate, m_clock);
    writeReg(0, 0x29, 0x9f);  // enable channels 4-6
}

YmFmOPNA::~YmFmOPNA()
{
    delete p;
    ymfm::ym2608 *chip_r = reinterpret_cast<ymfm::ym2608*>(m_chip);
    ymfm::ymfm_interface *intf_r = reinterpret_cast<ymfm::ymfm_interface*>(m_intf);
    ymfm::ym2608::output_data *output_r = reinterpret_cast<ymfm::ym2608::output_data*>(m_output);
    delete chip_r;
    delete intf_r;
    delete output_r;
}

void YmFmOPNA::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseT::setRate(rate, clock);
    ymfm::ym2608 *chip_r = reinterpret_cast<ymfm::ym2608*>(m_chip);
    chip_r->reset();
    p->m_out_step = 0x100000000ull / nativeRate();
    p->m_step = 0x100000000ull / chip_r->sample_rate(m_clock);
    p->m_pos = 0;
    writeReg(0, 0x29, 0x9f);  // enable channels 4-6
}

void YmFmOPNA::reset()
{
    OPNChipBaseT::reset();
    ymfm::ym2608 *chip_r = reinterpret_cast<ymfm::ym2608*>(m_chip);
    chip_r->reset();
    p->m_out_step = 0x100000000ull / nativeRate();
    p->m_step = 0x100000000ull / chip_r->sample_rate(m_clock);
    p->m_pos = 0;
    writeReg(0, 0x29, 0x9f);  // enable channels 4-6
}

void YmFmOPNA::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    p->writeReg(port, addr, data);
}

void YmFmOPNA_Private::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    YmFmOPNA::Reg &back = m_queue[m_headPos++];
    back.addr = port > 0 ? addr | 0x100 : addr;
    back.data = data;

    if(m_headPos >= YmFmOPNA::c_queueSize)
        m_headPos = 0;

    ++m_queueCount;
    assert(m_queueCount < (long)YmFmOPNA::c_queueSize);
}

void YmFmOPNA::writePan(uint16_t /*addr*/, uint8_t /*data*/)
{
    // ymfm::ym2608 *chip_r = reinterpret_cast<ymfm::ym2608*>(m_chip);
    // OPL3_WritePan(chip_r, addr, data);
}

void YmFmOPNA::nativeGenerate(int16_t *frame)
{
    p->nativeGenerate(frame, m_chip, m_output);
}

void YmFmOPNA_Private::nativeGenerate(int16_t *frame, void *m_chip, void *m_output)
{
    ymfm::ym2608 *chip_r = reinterpret_cast<ymfm::ym2608*>(m_chip);
    ymfm::ym2608::output_data *output_r = reinterpret_cast<ymfm::ym2608::output_data*>(m_output);

    uint32_t addr1 = 0xffff, addr2 = 0xffff;
    uint8_t data1 = 0, data2 = 0;

    for ( ; m_pos <= m_out_step; m_pos += m_step)
    {
        // see if there is data to be written; if so, extract it and dequeue
        if(m_queueCount > 0)
        {
            const YmFmOPNA::Reg &front = m_queue[m_tailPos++];

            if(m_tailPos >= YmFmOPNA::c_queueSize)
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

        chip_r->generate(output_r);
    }

    int32_t o0 = output_r->data[0];
    int32_t o1 = output_r->data[1 % ymfm::ym2608::OUTPUTS];
    int32_t o2 = output_r->data[2 % ymfm::ym2608::OUTPUTS];

    frame[0] = static_cast<int16_t>(ymfm::clamp(static_cast<int32_t>((o0 + o2) * 0.8f), -32768, 32767));
    frame[1] = static_cast<int16_t>(ymfm::clamp(static_cast<int32_t>((o1 + o2) * 0.8f), -32768, 32767));

    m_pos = (m_pos % m_out_step);
}

const char *YmFmOPNA::emulatorName()
{
    return "YMFM OPNA";
}

bool YmFmOPNA::hasFullPanning()
{
    return false;
}
