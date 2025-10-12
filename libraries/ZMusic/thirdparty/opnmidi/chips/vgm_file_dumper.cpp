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

#include "vgm_file_dumper.h"
#include <cstring>
#include <cassert>

#include <opnmidi_private.hpp>

//! FIXME: Replace this ugly crap with proper public call
static const char *g_vgm_path = "kek.vgm";
extern "C"
{
    OPNMIDI_EXPORT void opn2_set_vgm_out_path(const char *path)
    {
        g_vgm_path = path;
    }
}


#define VGM_LOOP_START_BASE 0x1C
#define VGM_SONG_DATA_START 0x38

static void g_write_le(FILE *f_out, uint32_t &field)
{
    uint8_t out[4];
    out[0] = (field) & 0xFF;
    out[1] = (field >> 8) & 0xFF;
    out[2] = (field >> 16) & 0xFF;
    out[3] = (field >> 24) & 0xFF;
    std::fwrite(&out, 1, 4, f_out);
}

static void g_write_le(FILE *f_out, uint16_t &field)
{
    uint8_t out[4];
    out[0] = (field) & 0xFF;
    out[1] = (field >> 8) & 0xFF;
    std::fwrite(&out, 1, 2, f_out);
}

void VGMFileDumper::writeHead()
{
    if(!m_output)
        return;

    long offset = std::ftell(m_output);

    if(offset < 0)
        return; // FATAL ERROR:

    std::fseek(m_output, 0x00, SEEK_SET);
    std::fwrite(m_vgm_head.magic, 1, 4, m_output);
    g_write_le(m_output, m_vgm_head.eof_offset);
    g_write_le(m_output, m_vgm_head.version);
    g_write_le(m_output, m_vgm_head.clock_sn76489);
    g_write_le(m_output, m_vgm_head.clock_ym2413);
    g_write_le(m_output, m_vgm_head.offset_gd3);
    g_write_le(m_output, m_vgm_head.total_samples);
    g_write_le(m_output, m_vgm_head.offset_loop);
    g_write_le(m_output, m_vgm_head.loop_samples);
    g_write_le(m_output, m_vgm_head.rate);
    g_write_le(m_output, m_vgm_head.feedback_sn76489);
    std::fwrite(&m_vgm_head.shift_register_width_sn76489, 1, 1, m_output);
    std::fwrite(&m_vgm_head.flags_sn76489, 1, 1, m_output);
    g_write_le(m_output, m_vgm_head.clock_ym2612);
    g_write_le(m_output, m_vgm_head.clock_ym2151);
    g_write_le(m_output, m_vgm_head.offset_data);
    std::fseek(m_output, offset, SEEK_SET);
}

void VGMFileDumper::writeWait(uint_fast16_t value)
{
    if(!m_output)
        return;
    uint8_t out[3];
    out[0] = 0x61;
    if(value == 735)
    {
        out[0] = 0x62;
        std::fwrite(&out, 1, 1, m_output);
        m_bytes_written += 1;
    }
    else if(value == 882)
    {
        out[0] = 0x63;
        std::fwrite(&out, 1, 1, m_output);
        m_bytes_written += 1;
    }
    else
    {
        out[1] = value & 0xFF;
        out[2] = (value >> 8) & 0xFF;
        std::fwrite(&out, 1, 3, m_output);
        m_bytes_written += 3;
    }
    m_samples_written += value;
    m_samples_loop += value;
}

void VGMFileDumper::flushWait()
{
    if(!m_output)
        return;

    if(m_chip_index > 0)
        return;

    while(m_delay > 0)
    {
        uint16_t to_copy;
        if(m_delay > 65535)
        {
            to_copy = 65535;
            m_delay -= 65535;
        }
        else
        {
            to_copy = static_cast<uint16_t>(m_delay);
            m_delay = 0;
        }
        writeWait(to_copy);
    }
}

void VGMFileDumper::writeCommand(uint_fast8_t cmd, uint_fast16_t key, uint_fast8_t value)
{
    if(!m_output)
        return;
    uint8_t out[3];
    out[0] = static_cast<uint8_t>(cmd);
    out[1] = static_cast<uint8_t>(key);
    out[2] = static_cast<uint8_t>(value);
    std::fwrite(&out, 1, 3, m_output);
    m_bytes_written += 3;
}

VGMFileDumper::VGMFileDumper(OPNFamily f, int index, void *first)
    : OPNChipBaseBufferedT(f)
{
    m_output = NULL;
    m_bytes_written = 0;
    m_samples_written = 0;
    m_samples_loop = 0;
    m_actual_rate = 0;
    m_delay = 0;
    m_end_caught = false;
    m_chip_index = index;
    m_first = NULL;

    VGMFileDumper::setRate(m_rate, m_clock);
    std::memset(&m_vgm_head, 0, sizeof(VgmHead));

    if(m_chip_index == 0)
    {
        m_output = std::fopen(g_vgm_path, "wb");
        assert(m_output);
        std::memcpy(m_vgm_head.magic, "Vgm ", 4);
        m_vgm_head.version = 0x00000150;
        m_vgm_head.offset_loop = VGM_LOOP_START_BASE;
        std::fseek(m_output, VGM_SONG_DATA_START, SEEK_SET);
    }
    else
    {
        m_first = reinterpret_cast<VGMFileDumper*>(first);
    }
}

VGMFileDumper::~VGMFileDumper()
{
    if(m_chip_index > 0)
        return;

    uint8_t out[1];
    out[0] = 0x66;// end of sound data
    std::fwrite(&out, 1, 1, m_output);
    m_bytes_written += 1;

    m_vgm_head.total_samples = m_samples_written;
    m_vgm_head.loop_samples = m_samples_loop;
    m_vgm_head.eof_offset = (VGM_SONG_DATA_START + m_bytes_written - 4);
    m_vgm_head.offset_data = 0x04;

    writeHead();

    if(m_output)
        std::fclose(m_output);
}

void VGMFileDumper::setRate(uint32_t rate, uint32_t clock)
{
    OPNChipBaseBufferedT::setRate(rate, clock);
    m_actual_rate = isRunningAtPcmRate() ? rate : nativeRate();
    m_vgm_head.clock_ym2612 = m_clock;
}

void VGMFileDumper::reset()
{
    if(!m_output)
        return;
    OPNChipBaseBufferedT::reset();
    std::fseek(m_output, VGM_SONG_DATA_START, SEEK_SET);
    m_samples_written = 0;
    m_samples_loop = 0;
    m_bytes_written = 0;
    m_end_caught = false;
    m_delay = 0;
}

void VGMFileDumper::writeReg(uint32_t port, uint16_t addr, uint8_t data)
{
    if(m_chip_index > 0) // When it's a second chip
    {
        if(m_first)
            m_first->writeReg(port + 2, addr, data);
        return;
    }

    if(!m_output)
        return;

    if(port >= 4u)
        return; // VGM DOESN'T SUPPORTS MORE THAN 2 CHIPS

    if(port >= 2u && ((m_vgm_head.clock_ym2612 & 0x40000000) == 0))
        m_vgm_head.clock_ym2612 |= 0x40000000;

    flushWait();

    uint_fast8_t ports[] = {0x52, 0x53, 0xA2, 0xA3};
    writeCommand(ports[port], addr, data);
}

void VGMFileDumper::writePan(uint16_t /*chan*/, uint8_t /*data*/)
{}

void VGMFileDumper::nativeGenerateN(int16_t *output, size_t frames)
{
    if(!m_output)
        return;
    if(m_chip_index > 0 || m_end_caught) // When it's a second chip
        return;
    std::memset(output, 0, frames * sizeof(int16_t) * 2);
    m_delay += size_t(frames * (44100.0 / double(m_actual_rate)));
}

const char *VGMFileDumper::emulatorName()
{
    return "VGM Writer";
}

void VGMFileDumper::writeLoopStart()
{
    if(m_chip_index > 0)
        return;
    flushWait();
    m_vgm_head.offset_loop = VGM_LOOP_START_BASE + m_bytes_written;
    m_samples_loop = 0;
    std::printf(" - MIDI2VGM: Loop start at 0x%04X\n", m_vgm_head.offset_loop);
    std::fflush(stdout);
}

void VGMFileDumper::writeLoopEnd()
{
    if(m_chip_index > 0 || m_end_caught)
        return;
    m_end_caught = true;
    flushWait();
    std::printf(" - MIDI2VGM: Loop end with total wait in %u samples\n", m_samples_loop);
    std::fflush(stdout);
}

void VGMFileDumper::loopStartHook(void *self)
{
    VGMFileDumper *s = reinterpret_cast<VGMFileDumper*>(self);
    if(s)
        s->writeLoopStart();
}

void VGMFileDumper::loopEndHook(void *self)
{
    VGMFileDumper *s = reinterpret_cast<VGMFileDumper*>(self);
    if(s)
        s->writeLoopEnd();
}

bool VGMFileDumper::hasFullPanning()
{
    return false;
}
