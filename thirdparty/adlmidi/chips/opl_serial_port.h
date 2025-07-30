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


#ifndef OPL_SERIAL_PORT_H
#define OPL_SERIAL_PORT_H

#ifdef ENABLE_HW_OPL_SERIAL_PORT

#include <string>
#include "opl_chip_base.h"

class ChipSerialPortBase;

class OPL_SerialPort : public OPLChipBaseT<OPL_SerialPort>
{
public:
    OPL_SerialPort();
    virtual ~OPL_SerialPort() override;

    enum Protocol
    {
        ProtocolUnknown,
        ProtocolArduinoOPL2,
        ProtocolNukeYktOPL3,
        ProtocolRetroWaveOPL3
    };

    bool connectPort(const std::string &name, unsigned baudRate, unsigned protocol);

    bool canRunAtPcmRate() const override { return false; }
    void setRate(uint32_t /*rate*/) override {}
    void reset() override {}
    void writeReg(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerate(int16_t *frame) override;
    const char *emulatorName() override;
    ChipType chipType() override;
    bool hasFullPanning() override;

private:
    ChipSerialPortBase *m_port;
    int m_protocol;
};

#endif // ENABLE_HW_OPL_SERIAL_PORT

#endif // OPL_SERIAL_PORT_H
