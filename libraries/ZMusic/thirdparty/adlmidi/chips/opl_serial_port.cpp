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


#ifdef ENABLE_HW_OPL_SERIAL_PORT

#include "opl_serial_port.h"
#include "opl_serial_misc.h"


static size_t retrowave_protocol_serial_pack(const uint8_t *buf_in, size_t len_in, uint8_t *buf_out)
{
    size_t in_cursor = 0;
    size_t out_cursor = 0;

    buf_out[out_cursor] = 0x00;
    out_cursor += 1;

    uint8_t shift_count = 0;

    while(in_cursor < len_in)
    {
        uint8_t cur_byte_out = buf_in[in_cursor] >> shift_count;
        if(in_cursor > 0)
            cur_byte_out |= (buf_in[in_cursor - 1] << (8 - shift_count));

        cur_byte_out |= 0x01;
        buf_out[out_cursor] = cur_byte_out;

        shift_count += 1;
        in_cursor += 1;
        out_cursor += 1;
        if(shift_count > 7)
        {
            shift_count = 0;
            in_cursor -= 1;
        }
    }

    if(shift_count)
    {
        buf_out[out_cursor] = buf_in[in_cursor - 1] << (8 - shift_count);
        buf_out[out_cursor] |= 0x01;
        out_cursor += 1;
    }

    buf_out[out_cursor] = 0x02;
    out_cursor += 1;

    return out_cursor;
}

OPL_SerialPort::OPL_SerialPort()
    : m_port(NULL), m_protocol(ProtocolUnknown)
{}

OPL_SerialPort::~OPL_SerialPort()
{
    delete m_port;
    m_port = NULL;
}

bool OPL_SerialPort::connectPort(const std::string& name, unsigned baudRate, unsigned protocol)
{
    delete m_port;
    m_port = NULL;

    // ensure audio thread reads protocol atomically and in order,
    // so chipType() will be correct after the port is live
    m_protocol = protocol;

    // QSerialPort *port = m_port = new QSerialPort(name);
    // port->setBaudRate(baudRate);
    // return port->open(QSerialPort::WriteOnly);

    m_port = new ChipSerialPort;
    return m_port->open(name, baudRate);
}

bool OPL_SerialPort::hasFullPanning()
{
    return false;
}

void OPL_SerialPort::writeReg(uint16_t addr, uint8_t data)
{
    uint8_t sendBuffer[16];
    ChipSerialPortBase *port = m_port;

    if(!port || !port->isOpen())
        return;

    switch(m_protocol)
    {
    default:
    case ProtocolArduinoOPL2:
    {
        if(addr >= 0x100)
            break;
        sendBuffer[0] = (uint8_t)addr;
        sendBuffer[1] = (uint8_t)data;
        port->write(sendBuffer, 2);
        break;
    }
    case ProtocolNukeYktOPL3:
    {
        sendBuffer[0] = (addr >> 6) | 0x80;
        sendBuffer[1] = ((addr & 0x3f) << 1) | (data >> 7);
        sendBuffer[2] = (data & 0x7f);
        port->write(sendBuffer, 3);
        break;
    }
    case ProtocolRetroWaveOPL3:
    {
        bool port1 = (addr & 0x100) != 0;
        uint8_t buf[8] =
        {
            static_cast<uint8_t>(0x21 << 1), 0x12,
            static_cast<uint8_t>(port1 ? 0xe5 : 0xe1), static_cast<uint8_t>(addr & 0xff),
            static_cast<uint8_t>(port1 ? 0xe7 : 0xe3), static_cast<uint8_t>(data),
            0xfb, static_cast<uint8_t>(data)
        };
        size_t packed_len = retrowave_protocol_serial_pack(buf, sizeof(buf), sendBuffer);
        port->write(sendBuffer, packed_len);
        break;
    }
    }
}

void OPL_SerialPort::nativeGenerate(int16_t *frame)
{
    frame[0] = 0;
    frame[1] = 0;
}

const char *OPL_SerialPort::emulatorName()
{
    return "OPL Serial Port Driver";
}

OPLChipBase::ChipType OPL_SerialPort::chipType()
{
    switch(m_protocol)
    {
    default:
    case ProtocolArduinoOPL2:
        return OPLChipBase::CHIPTYPE_OPL2;
    case ProtocolNukeYktOPL3:
    case ProtocolRetroWaveOPL3:
        return OPLChipBase::CHIPTYPE_OPL3;
    }
}

#endif // ENABLE_HW_OPL_SERIAL_PORT
